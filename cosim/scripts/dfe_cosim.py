#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""BER vs SNR — the DFE transmit and receive chains, in the loop, over a channel.

    TX (VSPA) -> +CP -> 2x interpolate (VSPA) -> [I/Q imbalance] -> AWGN
              -> 2x decimate (VSPA) -> [CFO] -> [CFO correction] -> -CP
              -> [timing offset] -> [QEC (VSPA)] -> RX (VSPA) -> [phase comp]

Every DSP stage is the real kernel running on the instruction-set simulator; only
the channel is Python. Where the verification tiers ask *"is the firmware
bit-correct?"*, this asks the other question: *"does the radio actually carry
bits?"* — and answers it against the textbook QPSK curve.

Three curves are plotted:
  theory   0.5*erfc(sqrt(Eb/N0))         — what AWGN QPSK must do
  model    the fixed-point Python models — what the arithmetic should cost
  VSPA     the kernels on the simulator  — what the silicon actually does

The impairments in brackets are off by default and CLI-selectable. Each is applied
to BOTH the VSPA path and the Python-model path, at the same point and in the same
order — otherwise the "model" curve quietly stops being a model of the chain under
test and becomes decoration that will be read as agreement.

WHY THE LOOP IS ORDERED THE WAY IT IS
    TX and the interpolator do not depend on the SNR, so they run ONCE per symbol
    rather than once per (symbol, SNR) — the noise goes in after them. That alone
    removes ~75% of the simulator work. The I/Q imbalance is deterministic and
    noise-independent, so it too is applied once, while the symbol is built.

    python3 cosim/scripts/dfe_cosim.py [--symbols 10] [--no-model]
    python3 cosim/scripts/dfe_cosim.py --iq-imbalance 1,5 --qec on
    python3 cosim/scripts/dfe_cosim.py --symbols 1000 --checkpoint runs/deep.json
"""
from __future__ import annotations

import argparse
import json
import os
import signal as _signal
import sys
import time
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from scipy.special import erfc

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "cosim"))

from dfe.kernel_runner import NativeKernelRunner, Port  # noqa: E402
from vspa_model.fft import r_dif_fft_sc128              # noqa: E402
from vspa_model.filter import r_interpolator_2x, r_decimator_2x  # noqa: E402
from vspa_model.qec import (imbalance_to_params, apply_imbalance,  # noqa: E402
                            qec_params_words, r_qec_td)

BUILD = REPO / "cosim" / "kernels" / "build"
FIGURE = REPO / "docs" / "figures" / "ber_vs_snr.png"

N, NSC = 1024, 61
W = 16
WIN = np.sin(np.pi * np.arange(W) / (2 * W)) ** 2

#: The rate-converter kernels process whole 64-sample blocks, so the symbol +
#: cyclic prefix MUST be a multiple of 64. (The original co-simulation used a
#: 20-sample CP -> 1044, which is not, and the interpolator silently returned
#: garbage for the last 40 output samples — inside the CP the receiver relies on.)
CP_LEN = 64
N_CP = N + CP_LEN                 # 1088 = 17 * 64
N_UP = 2 * N_CP                   # 2176 = 34 * 64

#: Not a comment — an assert. X2_interp_tap32_filter.sx and ddc2x4x.sx both do
#: `sr g0, g0, 6` — floor(n/64) iterations — and silently DROP a partial trailing
#: block. The native backend zero-prefills the output buffer, so those samples come
#: back as a clean 0.0 rather than as garbage, which is why the defect reads as a
#: smooth ~2%-per-symbol loss instead of an error. This assert is the only thing
#: standing between a future geometry change and the same silent corruption.
assert N_CP % 64 == 0, f"symbol+CP = {N_CP} is not a whole number of 64-sample blocks"
assert N_UP % 64 == 0, f"2x symbol+CP = {N_UP} is not a whole number of 64-sample blocks"

#: The 32-tap interpolator and 8-tap decimator together delay the signal by ~9.5
#: samples at the 1x rate. A cyclic roll by 9 recovers it: the samples the delay
#: eats come from the CP, which is a copy of the payload's own tail. The residual
#: HALF sample is irreducible by an integer roll and sets the chain's noise floor
#: at ~21.6 dB — far above anything that matters for QPSK.
DELAY = 9

AMPLITUDE = 8192.0                # QPSK radius, in SM16 counts (1/4 of full scale)
Q = 32768.0
SNR_RANGE_DB = np.array([0, 2, 4, 6, 8, 10, 12])
RNG_SEED = 42

_INTERP_HW = np.array([
    0x3BF66D0F, 0x3B145943, 0xBC2029E4, 0x3BC6E0FE, 0x3C1D0E2D, 0xBC914B26,
    0xBB7807E3, 0x3D136719, 0xBC4E8404, 0xBD814983, 0x3D4EF22B, 0x3DD52378,
    0xBE1B9A7D, 0xBE34750A, 0x3F82D7F9, 0x3ECB4074, 0x3ECB4074, 0x3F82D7F9,
    0xBE34750A, 0xBE1B9A7D, 0x3DD52378, 0x3D4EF22B, 0xBD814983, 0xBC4E8404,
    0x3D136719, 0xBB7807E3, 0xBC914B26, 0x3C1D0E2D, 0x3BC6E0FE, 0xBC2029E4,
    0x3B145943, 0x3BF66D0F], dtype=np.uint32).view(np.float32).astype(np.float64)


def _unreverse_pairs(hw: np.ndarray) -> np.ndarray:
    """Hardware tap order is reverse-PAIR: h[M-2],h[M-1], h[M-4],h[M-3], ...
    The Python model wants natural order. Not a no-op, palindrome or not."""
    nat = np.empty_like(hw)
    pairs = len(hw) // 2
    for i in range(pairs):
        k = pairs - 1 - i
        nat[2 * k], nat[2 * k + 1] = hw[2 * i], hw[2 * i + 1]
    return nat


TX_TAPS = _unreverse_pairs(_INTERP_HW)
RX_TAPS = np.array([
    0xBE0263BE, 0xBDE8A4F0, 0x3EA45618, 0x3F5B3BFF,
    0x3F5B3BFF, 0x3EA45618, 0xBDE8A4F0, 0xBE0263BE],
    dtype=np.uint32).view(np.float32).astype(np.float64)

#: Signal and noise see different gains through the halfband decimator: the DC
#: gain (sum h)^2 for the in-band signal, the impulse energy sum|h|^2 for white
#: noise. Computing the ratio from the real taps keeps the AWGN calibration exact.
_DECIM_SNR_CORR = float(np.sum(RX_TAPS) ** 2 / np.sum(RX_TAPS ** 2))


# ── QPSK ─────────────────────────────────────────────────────────────────────
def qpsk_mod(bits: np.ndarray) -> np.ndarray:
    si = 1.0 - 2.0 * bits[0::2].astype(np.float64)
    sq = 1.0 - 2.0 * bits[1::2].astype(np.float64)
    return (AMPLITUDE / np.sqrt(2)) * (si + 1j * sq)


def qpsk_demod(sym: np.ndarray) -> np.ndarray:
    bits = np.zeros(2 * len(sym), dtype=np.uint8)
    bits[0::2] = (np.real(sym) < 0).astype(np.uint8)
    bits[1::2] = (np.imag(sym) < 0).astype(np.uint8)
    return bits


def theory_ber(snr_db: np.ndarray) -> np.ndarray:
    return 0.5 * erfc(np.sqrt(10.0 ** (snr_db / 10.0)))


def awgn(sig: np.ndarray, snr_db: float, rng: np.random.Generator) -> np.ndarray:
    """AWGN calibrated so that the per-subcarrier Eb/N0 after the RX FFT is snr_db.

    Two corrections: N/NSC, because time-domain noise spreads over all N bins
    while the signal sits in only NSC of them; and the decimator's signal-vs-noise
    gain ratio, because the noise is injected at the 2x rate.

    Note what happens when an I/Q imbalance is injected: the variance is calibrated
    from the mean power of the ALREADY-imbalanced signal, so an imbalanced run has
    a marginally different noise variance from a balanced one ((1+eps)^2 on the Q
    branch). That is fine — but it is only fine because the VSPA path and the model
    path do it in exactly the same order. Do not "fix" it in one of them.
    """
    es_n0 = 2.0 * 10.0 ** (snr_db / 10.0)         # QPSK: 2 bits per symbol
    var = _DECIM_SNR_CORR * np.mean(np.abs(sig) ** 2) * (N / NSC) / es_n0
    noise = rng.standard_normal(len(sig)) + 1j * rng.standard_normal(len(sig))
    return sig + noise * np.sqrt(var / 2.0)


def add_cp(td: np.ndarray) -> np.ndarray:
    return np.concatenate([td[N - CP_LEN:], td])


def strip_cp(td: np.ndarray) -> np.ndarray:
    return np.roll(td[CP_LEN:], -DELAY)


# ── impairments ──────────────────────────────────────────────────────────────
#
# All four model the analogue world the DSP chain sits in, so all four are pure
# NumPy — there is no kernel to run for them. The CORRECTIONS are a different
# matter: QEC is the real vspa-lib kernel on the simulator; the CFO estimator and
# the phase ramp are Python, and are named as stand-ins where they are defined.

def eps_phi_from_cli(spec: str | None) -> tuple[float, float] | None:
    """'EPS_DB,PHI_DEG' -> (eps linear, phi radians); None when disabled."""
    if not spec:
        return None
    try:
        eps_db, phi_deg = (float(x) for x in spec.split(","))
    except ValueError:
        raise SystemExit(f"--iq-imbalance expects 'EPS_DB,PHI_DEG', got {spec!r}")
    return 10.0 ** (eps_db / 20.0) - 1.0, float(np.deg2rad(phi_deg))


# I/Q IMBALANCE.  An analogue quadrature mixer with a gain mismatch and a
# non-quadrature phase between its I and Q branches distorts the complex baseband.
# I is the reference branch and Q carries both errors:
#
#     I_imb = I ;   Q_imb = (1 + eps) * (Q*cos(phi) + I*sin(phi))
#
# (vspa_model.qec.apply_imbalance).  It is injected on the 2x-rate TX signal — but
# it is memoryless, real-linear and noise-independent, so it is applied ONCE, when
# the symbol is built, not once per SNR point.
#
# The correction (QEC — the real qec_opt_asm kernel) runs at the 1x rate, after the
# decimator.  Correcting a 2x-rate impairment at 1x is EXACT, not an approximation:
# the imbalance is a per-sample real-linear map, and the decimator's taps are REAL,
# so the filter acts identically and independently on the I and Q streams and the
# two operations commute.

def qec_params_for(imb: tuple[float, float] | None) -> tuple[float, float, float]:
    """(f1, f4, f2) that invert `imb`; the identity (1, 1, 0) when there is none."""
    return imbalance_to_params(*imb) if imb else (1.0, 1.0, 0.0)


# CFO.  A carrier-frequency offset of `frac` subcarrier spacings rotates the
# baseband by exp(+j*2*pi*frac*n/N).  One subcarrier spacing is exactly one phase
# cycle per N time-domain samples, so the scale is set by the FFT SIZE and nothing
# here scales with the CP length.  It is injected on the decimator's CP-PRESENT
# output, on signal and noise together: that is where a receiver LO offset
# physically sits, and it is the only ordering in which the CP<->payload-tail
# periodicity the estimator needs is real rather than manufactured.
#
# HONESTY NOTE: this cosim processes each OFDM symbol as an independent buffer with
# no cross-symbol phase continuity, so the ramp restarts at n=0 every symbol. A
# real receiver sees a phase that accumulates across the slot. The per-symbol reset
# makes CFO look more benign than it is — there is no inter-symbol common-phase
# drift to track. Making it continuous needs the slot structure this cosim does not
# yet carry.

def apply_cfo(buf: np.ndarray, frac: float) -> np.ndarray:
    return buf * np.exp(1j * 2.0 * np.pi * frac * np.arange(len(buf)) / N)


def correct_cfo(buf: np.ndarray, frac: float) -> np.ndarray:
    return buf * np.exp(-1j * 2.0 * np.pi * frac * np.arange(len(buf)) / N)


# THE CP-BASED ESTIMATOR'S WINDOW, and why it only exists at this geometry.
#
# The van-de-Beek / Moose autocorrelation pairs each CP sample with the copy of it
# that sits N samples later in the payload tail: in an ideal CP-present buffer,
# b[n] and b[N+n], for n in [0, CP).  The decimator delays everything by DELAY, so
# in the raw rx_cp the pair is (rx_cp[DELAY+n], rx_cp[DELAY+N+n]) — and the second
# index runs off the end of the 1088-sample buffer as soon as n > N_CP-1-N-DELAY.
# The usable length is therefore
#
#     L = N_CP - N - DELAY = 1088 - 1024 - 9 = 55
#
# and the first two samples are filter transient (measured: 92% and 30% mismatch at
# offsets 9 and 10, under 3% from 11 on), so the window starts at DELAY + 2.
#
# AT THE OLD GEOMETRY THIS WINDOW DOES NOT EXIST.  With CP_LEN=20 (N_CP=1044) the
# same arithmetic gives L = 11, of which 9 are usable.  Nine samples is not an
# estimator — which is why the old co-simulation instead RECONSTRUCTED the CP by
# copying the payload's own tail, and correlated a buffer against itself.  Both
# windows were then the same samples with the same noise, so it returned the
# injected CFO exactly, at every SNR (measured std 0.0000 at 0 dB Eb/N0).  That is
# not an estimator, it is a read-back, and it is not ported.  The 64-sample CP is
# what makes a real one possible.
CFO_EST_START = DELAY + 2
CFO_EST_LEN = N_CP - N - DELAY - 2          # 53


def estimate_cfo_cp(rx_cp: np.ndarray) -> float:
    """Fractional CFO from the CP autocorrelation of a RAW decimator output.

    The two correlation windows carry INDEPENDENT noise, so this has real variance
    (40 trials, injected +0.0500: 12 dB -> mean +0.0523 std 0.0218; 6 dB -> +0.0548
    std 0.0667; 0 dB -> +0.0242 std 0.1828).

    THAT VARIANCE IS THE POINT, and it has a consequence you should expect rather
    than debug: 53 samples is a short correlation, so a PER-SYMBOL estimate is
    noise-dominated below ~10 dB, and --cfo-correct can make the BER WORSE than
    leaving the offset alone. Measured here, injected +0.0500, 8 symbols/point:
    12 dB -> mean +0.0521 (and the BER floor still holds); 6 dB -> +0.0641;
    0 dB -> -0.0004, i.e. the estimate is noise. A receiver fixes this by
    ACCUMULATING the correlation across the symbols of a frame before taking the
    angle; this cosim has no frame, so it does not, and the honest per-symbol
    behaviour is what you see.

    Python STAND-IN for the vspa-lib ``fineCFO_rc`` kernel. fineCFO_rc *is* in
    firmware/lib, but it estimates from PSS/SSS frequency-domain channel estimates
    and needs an NR sync burst this flat-AWGN cosim does not carry.

    Unambiguous only for |frac| < 0.5 — np.angle wraps beyond that.
    """
    s, L = CFO_EST_START, CFO_EST_LEN
    c = np.sum(np.conj(rx_cp[s:s + L]) * rx_cp[s + N:s + N + L])
    return float(np.angle(c) / (2.0 * np.pi))


# TIMING OFFSET and its PHASE COMPENSATION.  Advancing or retarding the RX FFT
# window by K samples is meaningful only because the CP guards the symbol: for
# |K| <= CP it is an ISI-free CYCLIC shift of the N-sample window, which is exactly
# what np.roll models.  (It is a model of a window shift, not a true re-slice of
# the 1088-sample buffer — a true re-slice for K>0 would need samples past index
# 1087, which the group delay has already truncated.  The cyclic roll is the right
# call and is exact for |K| <= CP.)  DELAY eats 9 of the 64, so the honest range is
# |K| <~ 55.
#
# The FFT turns that shift into a linear per-subcarrier phase ramp: np.roll(x, K)
# multiplies bin m by exp(-j*2*pi*m*K/N), so the compensation multiplies by
# exp(+j*2*pi*m*K/N).  Both m_rx and compute_dfe_rx.c place subcarrier j at signed
# bin m = j - NSC//2, which is what fixes the sign.  (Measured on the model: K=+3
# EVM 0.281 -> 0.0057; K=-5 0.461 -> 0.0069; K=+20 1.387 -> 0.0062.)
#
# THIS IS NOT phcom_asm, despite what the gap list calls it.  The real phcom_asm —
# which IS in firmware/lib — takes ONE complex scalar per symbol ("the coeff for
# this symbol") and broadcasts it across the samples: it is an NR common-phase-error
# rotator, not a per-bin linear ramp, and it cannot implement this correction.  The
# float ramp is not a placeholder for a kernel we declined to wire.

def phase_comp_ramp(k: int) -> np.ndarray:
    m = np.arange(NSC) - NSC // 2
    return np.exp(1j * 2.0 * np.pi * m * k / N)


@dataclass(frozen=True)
class Impairments:
    """What the channel does to a symbol, and what the receiver does back."""
    imbalance: tuple[float, float] | None = None    # (eps, phi)
    qec: tuple[float, float, float] | None = None   # (f1, f4, f2); None = off
    cfo: float = 0.0                                # subcarrier spacings
    cfo_correct: bool = False
    timing: int = 0                                 # samples
    phase_comp: bool = False

    def check(self) -> None:
        if abs(self.cfo) >= 0.5:
            raise SystemExit(
                f"--cfo {self.cfo}: the CP autocorrelation is unambiguous only for "
                f"|frac| < 0.5 — np.angle wraps beyond that.")
        room = CP_LEN - DELAY
        if abs(self.timing) > room:
            print(f"WARNING: --timing-offset {self.timing:+d} exceeds the guard the "
                  f"CP actually provides ({room} samples, once the {DELAY}-sample "
                  f"group delay is paid). The shift is no longer ISI-free.",
                  file=sys.stderr)


# ── the Python model of the same chain (no simulator) ────────────────────────
def m_tx(sc: np.ndarray) -> np.ndarray:
    grid = np.zeros(N, dtype=complex)
    h = NSC // 2
    grid[:NSC - h] = sc[h:] / Q
    grid[N - h:] = sc[:h] / Q
    out = np.array(r_dif_fft_sc128(grid, inv_flag=1))
    out[:W] *= WIN
    out[N - W:] *= (1.0 - WIN)
    return (np.round(np.real(out * Q)) + 1j * np.round(np.imag(out * Q)))


def m_rx(td: np.ndarray) -> np.ndarray:
    fd = np.array(r_dif_fft_sc128(td / Q, inv_flag=0))
    h = NSC // 2
    return np.concatenate([fd[N - h:], fd[:NSC - h]]) * Q


# ── the receive half, twice: once on the VSPA, once in Python ───────────────
#
# These two are deliberately line-for-line parallel, and share _rx_common for the
# part that is Python in both. Every impairment and every correction happens at the
# same point, in the same order — which is the only reason the two curves are worth
# plotting on the same axes.

def _rx_common(rx_cp: np.ndarray, imp: Impairments) -> tuple[np.ndarray, float | None]:
    """Decimator output -> FFT input: CFO, CFO correction, CP removal, window shift."""
    cfo_est = None
    if imp.cfo:
        rx_cp = apply_cfo(rx_cp, imp.cfo)
    if imp.cfo_correct:
        # Estimated and corrected BEFORE strip_cp, for two reasons. The raw buffer
        # is the only place where the CP and the payload tail carry INDEPENDENT
        # noise. And strip_cp's cyclic roll(-DELAY) is only valid on a cyclic
        # signal: a CFO ramp is not one, so rolling an uncorrected ramp would put a
        # -2*pi*frac phase STEP on the last DELAY samples of the payload.
        cfo_est = estimate_cfo_cp(rx_cp)
        rx_cp = correct_cfo(rx_cp, cfo_est)
    rx_td = strip_cp(rx_cp)
    if imp.timing:
        rx_td = np.roll(rx_td, imp.timing)
    return rx_td, cfo_est


def rx_chain(sig: np.ndarray, snr_db: float, rng: np.random.Generator,
             dn: NativeKernelRunner, rx: NativeKernelRunner,
             qec: NativeKernelRunner | None,
             imp: Impairments) -> tuple[np.ndarray, float | None]:
    """channel -> decimate (VSPA) -> impairments -> QEC (VSPA) -> RX FFT (VSPA)."""
    rx_cp = dn.compute({"up": awgn(sig, snr_db, rng)})["td"]     # 1088, CP present
    rx_td, cfo_est = _rx_common(rx_cp, imp)
    if imp.qec is not None:
        f1, f4, f2 = imp.qec
        # The coefficients are a runtime PORT, written at the breakpoint on main()
        # like any other input — one ELF serves every (f1, f4, f2). (The old cosim
        # baked them into the ELF from a generated params.hex and rebuilt it on
        # every parameter change.)
        rx_td = qec.compute({"input_td": rx_td,
                             "qec_params": qec_params_words(f1, f4, f2)})["output_td"]
    sc = rx.compute({"td": rx_td})["sc"]
    if imp.phase_comp:
        sc = sc * phase_comp_ramp(imp.timing)
    return sc, cfo_est


def m_rx_chain(sig: np.ndarray, snr_db: float, rng: np.random.Generator,
               imp: Impairments) -> tuple[np.ndarray, float | None]:
    """The same, with every VSPA kernel replaced by its fixed-point Python model."""
    d, _ = r_decimator_2x(awgn(sig, snr_db, rng) / Q, RX_TAPS)
    rx_td, cfo_est = _rx_common(np.asarray(d) * Q, imp)
    if imp.qec is not None:
        rx_td = r_qec_td(rx_td, *imp.qec)      # bit-exact model of qec_opt_asm
    sc = m_rx(rx_td)
    if imp.phase_comp:
        sc = sc * phase_comp_ramp(imp.timing)
    return sc, cfo_est


# ── checkpointing, sharding, and the RNG that makes both sound ───────────────
#
# A deep run is hours of simulator time and must not be all-or-nothing: a crash, a
# suspend or a stray Ctrl-C at hour 4 cannot be allowed to cost 4 hours. The sweep
# is cut into CHUNKS of symbols, and the accumulated (errors, bits) are flushed to
# JSON after every chunk — so at most one chunk of work is ever lost.
#
# THE INVARIANT that makes a resumed run, and a sharded one, ELEMENT-WISE equal to
# a single sequential sweep: every random draw must be a pure function of
# (seed, point index, chunk index, position within the chunk) and of NOTHING else —
# in particular not of how many draws happened before it. A streaming Generator
# does not have that property; a fresh Generator derived per chunk from a
# SeedSequence does.
#
# THE SPLIT — this is where the old co-simulation's keying does not transplant.
# It used one stream keyed on (seed, point, chunk). But this loop TXes and
# interpolates each symbol ONCE and reuses it across every SNR point (the ~4x
# saving in the docstring), so a symbol's BITS are shared by all points and CANNOT
# be keyed by point without destroying that. Hence two streams:
#
#     bits  <- SeedSequence([seed, 0, chunk])           point-INdependent
#     noise <- SeedSequence([seed, 1, point, chunk])    point-keyed
#
# Consequences to respect if this is ever edited: the number and order of draws
# inside a chunk must not depend on the SNR value or on which process runs it (so
# an early `continue` must never consume a draw); the chunk SIZE partitions the
# streams, so it is in the fingerprint and changing it invalidates a resume; and
# --only-points must never renumber or shorten the grid, because the point index is
# the key.
CHECKPOINT_VERSION = 1
CHUNK_SYMBOLS = 25

_BITS, _NOISE = 0, 1


def rng_bits(seed: int, chunk_idx: int) -> np.random.Generator:
    return np.random.default_rng(np.random.SeedSequence([seed, _BITS, chunk_idx]))


def rng_noise(seed: int, point_idx: int, chunk_idx: int) -> np.random.Generator:
    return np.random.default_rng(
        np.random.SeedSequence([seed, _NOISE, point_idx, chunk_idx]))


def chunk_plan(n_symbols: int, chunk: int) -> list[int]:
    """Split n_symbols into chunk-sized pieces (the last one short)."""
    if chunk <= 0:
        return [n_symbols]
    full, rest = divmod(n_symbols, chunk)
    return [chunk] * full + ([rest] if rest else [])


def ckpt_load(path: str) -> dict | None:
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return json.load(f)


def ckpt_save(path: str, state: dict) -> None:
    """Atomic write. os.replace is atomic and the fsync happens first, so a crash
    mid-flush leaves either the old checkpoint or the new one — never a truncated
    file where hours of results used to be."""
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(state, f, indent=1)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)


def fingerprint(snr: np.ndarray, n_symbols: int, chunk: int,
                imp: Impairments) -> dict:
    """The configuration that determines the NUMBERS.

    A --resume against a checkpoint with a different fingerprint would silently
    splice two experiments together, so it is refused. The GEOMETRY is in here on
    purpose: it is exactly what stops someone resuming a CP_LEN=64 run into a
    checkpoint written under a different — or broken — one.

    The shard selection (--only-points) is deliberately NOT in here: shards of one
    experiment must compare EQUAL, or --merge could never prove they belong
    together.
    """
    return {
        "snr_db":        [round(float(x), 6) for x in snr],
        "n_symbols":     n_symbols,
        "chunk":         chunk,
        "seed":          RNG_SEED,
        "amplitude":     AMPLITUDE,
        "geometry":      [N, NSC, CP_LEN, N_CP, N_UP, DELAY],
        "imbalance":     [round(x, 12) for x in imp.imbalance] if imp.imbalance else None,
        "qec":           [round(x, 12) for x in imp.qec] if imp.qec else None,
        "cfo":           round(float(imp.cfo), 12),
        "cfo_correct":   imp.cfo_correct,
        "timing_offset": imp.timing,
        "phase_comp":    imp.phase_comp,
    }


def new_state(snr: np.ndarray, cfg: dict) -> dict:
    return {"version": CHECKPOINT_VERSION, "config": cfg, "complete": False,
            "symbols_run": 0, "seconds": 0.0,
            "points": [{"snr_db": round(float(s), 6), "errors": 0, "bits": 0,
                        "cfo_sum": 0.0, "cfo_cnt": 0, "chunks_done": []}
                       for s in snr]}


def parse_points(spec: str, n_points: int) -> list[int]:
    """'0,3,5' -> [0, 3, 5], validated against the grid."""
    try:
        idx = sorted({int(x) for x in spec.split(",") if x.strip()})
    except ValueError:
        raise SystemExit(f"--only-points: not a comma-separated index list: {spec!r}")
    bad = [i for i in idx if not 0 <= i < n_points]
    if bad or not idx:
        raise SystemExit(f"--only-points: index/indices {bad or spec!r} outside the "
                         f"{n_points}-point grid (valid: 0..{n_points - 1})")
    return idx


def merge_checkpoints(paths: list[str]) -> dict:
    """Fold shard checkpoints (written with --only-points) into one result.

    Sharding is sound only because the noise is keyed by the GLOBAL point index, so
    a point computed by shard 3 gets exactly the noise it would have had in a single
    sequential sweep. Three things can still go wrong, and all three are refused:

      * shards from different experiments — the config fingerprints differ;
      * the same point contributed twice — summing would double-count its bits and
        halve its BER, with no other symptom (a --merge glob that catches a stale
        checkpoint is exactly how that happens);
      * a grid point no shard covered — the table is partial, and says so.
    """
    merged: dict | None = None
    owner: dict[int, str] = {}
    for p in paths:
        c = ckpt_load(p)
        if c is None:
            raise SystemExit(f"--merge: no such checkpoint: {p}")
        if c.get("version") != CHECKPOINT_VERSION:
            raise SystemExit(f"--merge: {p}: checkpoint version {c.get('version')} "
                             f"!= {CHECKPOINT_VERSION}")
        if merged is None:
            merged = {"version": CHECKPOINT_VERSION, "config": c["config"],
                      "complete": False, "symbols_run": 0, "seconds": 0.0,
                      "merged_from": list(paths),
                      "points": [{"snr_db": pt["snr_db"], "errors": 0, "bits": 0,
                                  "cfo_sum": 0.0, "cfo_cnt": 0, "chunks_done": []}
                                 for pt in c["points"]]}
        elif c["config"] != merged["config"]:
            diff = [k for k in c["config"] if merged["config"].get(k) != c["config"][k]]
            raise SystemExit(f"--merge: {p} was written with a different config "
                             f"(differing: {', '.join(diff)}) — refusing to merge two "
                             f"different experiments.")

        for i, pt in enumerate(c["points"]):
            if not pt["chunks_done"]:
                continue                        # this shard did not own this point
            if i in owner:
                raise SystemExit(
                    f"--merge: SNR point {i} ({pt['snr_db']} dB) appears in both "
                    f"{owner[i]} and {p}. Merging would double-count its bits.")
            owner[i] = p
            m = merged["points"][i]
            m["errors"] += pt["errors"]
            m["bits"] += pt["bits"]
            m["cfo_sum"] += pt["cfo_sum"]
            m["cfo_cnt"] += pt["cfo_cnt"]
            m["chunks_done"] = sorted(pt["chunks_done"])
        # Shards run CONCURRENTLY: the run took as long as the SLOWEST one, not the
        # sum. symbols_run does sum.
        merged["seconds"] = max(merged["seconds"], c.get("seconds", 0.0))
        merged["symbols_run"] += c.get("symbols_run", 0)

    if merged is None:
        raise SystemExit("--merge: no checkpoints given")

    n_chunks = len(chunk_plan(merged["config"]["n_symbols"],
                              merged["config"]["chunk"]))
    missing = [i for i, m in enumerate(merged["points"])
               if len(m["chunks_done"]) != n_chunks]
    merged["complete"] = not missing
    if missing:
        print(f"WARNING: incomplete after merge — SNR point(s) "
              f"{', '.join(map(str, missing))} are missing chunks. The table below is "
              f"a PARTIAL result.", file=sys.stderr)
    return merged


def _hms(seconds: float) -> str:
    s = int(max(0.0, seconds))
    return f"{s // 3600:d}:{(s % 3600) // 60:02d}:{s % 60:02d}"


def ber_of(points: list[dict]) -> np.ndarray:
    return np.array([p["errors"] / p["bits"] if p["bits"] else float("nan")
                     for p in points])


def print_table(snr: np.ndarray, points: list[dict], sel: list[int],
                cfo_injected: float) -> None:
    ber = ber_of(points)
    print(f"\n{'Eb/N0':>6} {'VSPA BER':>12} {'errors':>8} {'bits':>8}   {'theory':>9}")
    for i in sel:
        p = points[i]
        b = (f"{ber[i]:12.2e}" if p["bits"] and ber[i] > 0
             else f"{'0 (floor)':>12}" if p["bits"] else f"{'-':>12}")
        print(f"{snr[i]:5.1f} {b} {p['errors']:8d} {p['bits']:8d}   "
              f"{theory_ber(np.array([snr[i]]))[0]:9.2e}")
    est = [p["cfo_sum"] / p["cfo_cnt"] for p in points if p["cfo_cnt"]]
    if est:
        print(f"\n  CFO estimate (CP autocorrelation): mean {np.mean(est):+.4f} vs "
              f"injected {cfo_injected:+.4f} subcarrier spacing(s) "
              f"[{len(est)} SNR point(s)]")


# ── plot ─────────────────────────────────────────────────────────────────────
def plot(figure: Path, snr: np.ndarray, vspa_ber, vspa_bits,
         model_ber=None, model_bits=0, subtitle: str = "") -> None:
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("\n(matplotlib not installed — no figure)")
        return

    fine = np.linspace(snr[0], snr[-1], 200)
    fig, ax = plt.subplots(figsize=(7.5, 5))
    ax.semilogy(fine, theory_ber(fine), color="#8a8a8a", lw=2,
                label="theory  QPSK / AWGN")

    def draw(ber, n_bits, colour, marker, style, label):
        """Points where NO errors were seen are not measurements — they are upper
        bounds ("< 1/n_bits"). Drawing them at some floor as if measured would
        invent a curve the data does not support, so they get a downward arrow at
        the detection limit instead."""
        ber = np.asarray(ber, dtype=float)
        bits = np.broadcast_to(np.asarray(n_bits, dtype=float), ber.shape)
        seen = ber > 0
        none = (ber == 0) & (bits > 0)
        ax.semilogy(snr[seen], ber[seen], marker + style, lw=2, ms=8, color=colour,
                    mfc="white", mew=2, label=label)
        if none.any():
            ax.semilogy(snr[none], 1.0 / bits[none], "v", ms=8, color=colour,
                        mfc=colour, alpha=0.55)

    # Depth belongs ON the curve.  Points with no observed errors are drawn as
    # upper-bound arrows, and an arrow's height IS the depth — so a reader who
    # cannot see how many symbols went into a curve cannot judge it at all.
    def depth(bits):
        n = int(np.max(np.asarray(bits, dtype=float)))
        return f"  ({n // (2 * NSC)} sym/pt)" if n else ""

    if model_ber is not None:
        draw(model_ber, model_bits, "#4c6ef5", "s", "--",
             "Python model" + depth(model_bits))
    draw(vspa_ber, vspa_bits, "#e8590c", "o", "-",
         "VSPA kernels on the ISS" + depth(vspa_bits))
    ax.plot([], [], "v", color="#8a8a8a", mfc="#8a8a8a", ms=8, alpha=0.55,
            label="no errors observed (< 1 / n bits)")

    floor = max(float(np.max(np.asarray(vspa_bits, dtype=float))), 1.0)
    ax.set_xlabel("Eb/N0  (dB)")
    ax.set_ylabel("bit error rate")
    ax.set_title("LA9310 DFE — TX → AWGN → RX, every DSP stage on the simulator"
                 + (f"\n{subtitle}" if subtitle else ""))
    ax.set_ylim(0.2 / floor, 1)
    ax.grid(True, which="both", alpha=0.25, lw=0.6)
    ax.legend(frameon=False)
    fig.tight_layout()
    figure.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(figure, dpi=140)
    rel = figure.relative_to(REPO) if figure.is_absolute() and figure.is_relative_to(REPO) else figure
    print(f"\nwrote {rel}")


# ── CLI ──────────────────────────────────────────────────────────────────────
def build_argparser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(
        description="DFE TX -> AWGN -> RX BER-vs-SNR co-simulation")
    ap.add_argument("--symbols", type=int, default=None,
                    help="OFDM symbols per SNR point on the VSPA path (default: 10)")
    ap.add_argument("--model-symbols", type=int, default=60)
    ap.add_argument("--no-model", action="store_true",
                    help="skip the Python-model curve")
    ap.add_argument("--snr-points", type=int, default=None, metavar="P",
                    help="number of Eb/N0 points, evenly spaced over the range")
    ap.add_argument("--snr-min", type=float, default=float(SNR_RANGE_DB[0]))
    ap.add_argument("--snr-max", type=float, default=float(SNR_RANGE_DB[-1]))
    ap.add_argument("--quick", action="store_true",
                    help="smoke grid: 2 symbols over 3 SNR points")
    ap.add_argument("--figure", type=Path, default=FIGURE,
                    help="where to write the BER figure")

    g = ap.add_argument_group("impairments — applied to BOTH curves")
    g.add_argument("--iq-imbalance", metavar="EPS_DB,PHI_DEG", default=None,
                   help="inject an analogue I/Q imbalance on the 2x TX signal: gain "
                        "error EPS_DB in dB, phase error PHI_DEG in degrees ('1,5')")
    g.add_argument("--qec", choices=("off", "on"), default="off",
                   help="correct it at the RX with the real vspa-lib qec_opt_asm "
                        "kernel; the coefficients follow from --iq-imbalance")
    g.add_argument("--cfo", type=float, default=0.0, metavar="FRAC",
                   help="inject a carrier frequency offset of FRAC subcarrier "
                        "spacings, |FRAC| < 0.5 (e.g. 0.05)")
    g.add_argument("--cfo-correct", choices=("off", "on"), default="off",
                   help="estimate it from the CP autocorrelation and de-rotate. A "
                        "Python stand-in for fineCFO_rc, and a real estimator with "
                        "real variance: the per-symbol estimate is noise-dominated "
                        "below ~10 dB and can make the BER worse than not correcting "
                        "at all — see estimate_cfo_cp")
    g.add_argument("--timing-offset", type=int, default=0, metavar="K",
                   help="shift the RX FFT window by K samples — ISI-free while the "
                        "CP guards it")
    g.add_argument("--phase-comp", choices=("off", "on"), default="off",
                   help="invert the per-subcarrier phase ramp that window shift "
                        "produces")

    c = ap.add_argument_group("checkpointing and sharding")
    c.add_argument("--checkpoint", metavar="PATH", default=None,
                   help="flush the accumulated errors/bits to PATH after every chunk, "
                        "so a long run survives a crash or a Ctrl-C; doubles as the "
                        "machine-readable results file")
    c.add_argument("--resume", action="store_true",
                   help="continue the run recorded in --checkpoint; refuses if the "
                        "config fingerprint differs, naming the differing field")
    c.add_argument("--restart", action="store_true",
                   help="discard an existing --checkpoint and start over")
    c.add_argument("--chunk", type=int, default=CHUNK_SYMBOLS, metavar="N",
                   help=f"symbols per checkpoint flush (default {CHUNK_SYMBOLS}). "
                        f"NOTE: the chunk size PARTITIONS the RNG streams, so changing "
                        f"it changes every noise realisation — it is in the "
                        f"fingerprint, and a resume across a change is refused.")
    c.add_argument("--only-points", metavar="I[,J,...]", default=None,
                   help="compute only these SNR-point indices. The grid is unchanged, "
                        "so the point index — which keys the noise — stays GLOBAL: "
                        "sharding across processes gives element-wise the same numbers "
                        "as one sequential run. Combine with --merge.")
    c.add_argument("--merge", nargs="+", metavar="CKPT", default=None,
                   help="fold shard checkpoints into one result: validates that they "
                        "share a config and cover the grid without overlap, then prints "
                        "the table and plots. Computes nothing.")
    return ap


def main() -> int:
    a = build_argparser().parse_args()

    # ── --merge: fold the shards and report; compute nothing ─────────────────
    if a.merge:
        merged = merge_checkpoints(a.merge)
        cfg = merged["config"]
        snr = np.array(cfg["snr_db"], dtype=float)
        pts = merged["points"]
        if a.checkpoint:
            ckpt_save(a.checkpoint, merged)
            print(f"Merged {len(a.merge)} shard(s) -> {a.checkpoint}")
        print(f"\nMerged {merged['symbols_run']} symbol(s) ({cfg['n_symbols']} per "
              f"point), slowest shard {_hms(merged['seconds'])}, "
              f"complete={merged['complete']}")
        print_table(snr, pts, list(range(len(snr))), cfg["cfo"])
        plot(a.figure, snr, ber_of(pts), [p["bits"] for p in pts],
             subtitle=f"merged from {len(a.merge)} shard(s)")
        return 0 if merged["complete"] else 1

    # ── grid, impairments, chunk plan ────────────────────────────────────────
    n_points = a.snr_points if a.snr_points is not None else (
        3 if a.quick else len(SNR_RANGE_DB))
    n_symbols = a.symbols if a.symbols is not None else (2 if a.quick else 10)
    snr = (np.linspace(a.snr_min, a.snr_max, n_points) if n_points > 1
           else np.array([a.snr_min]))

    imb = eps_phi_from_cli(a.iq_imbalance)
    imp = Impairments(imbalance=imb,
                      qec=qec_params_for(imb) if a.qec == "on" else None,
                      cfo=a.cfo,
                      cfo_correct=a.cfo_correct == "on",
                      timing=a.timing_offset,
                      phase_comp=a.phase_comp == "on")
    imp.check()

    if (a.resume or a.restart) and not a.checkpoint:
        print("ERROR: --resume/--restart need --checkpoint PATH to act on.",
              file=sys.stderr)
        return 2
    if a.resume and a.restart:
        print("ERROR: --resume and --restart are mutually exclusive.", file=sys.stderr)
        return 2

    chunk = max(1, min(a.chunk, n_symbols))
    plan = chunk_plan(n_symbols, chunk)
    # Shard selection. NOT part of the fingerprint — see fingerprint().
    sel = (parse_points(a.only_points, len(snr)) if a.only_points
           else list(range(len(snr))))
    sharded = a.only_points is not None

    print("=" * 74)
    print(f"DFE co-simulation   N={N}  NSC={NSC}  CP={CP_LEN}  "
          f"N_CP={N_CP}={N_CP // 64}x64  delay={DELAY}")
    print(f"  VSPA  : {n_symbols} symbol(s)/point, {2 * NSC * n_symbols} bits/point, "
          f"{len(plan)} chunk(s) of {chunk}")
    print(f"  grid  : {snr[0]:.1f} .. {snr[-1]:.1f} dB, {len(snr)} point(s)"
          + (f"   [shard: {', '.join(map(str, sel))}]" if sharded else ""))
    print(f"  imbal : eps={imb[0]:+.5f}  phi={np.rad2deg(imb[1]):.2f} deg  "
          f"(from --iq-imbalance {a.iq_imbalance})" if imb else "  imbal : off")
    print(f"  QEC   : ON — real qec_opt_asm, f1={imp.qec[0]:.5f} f4={imp.qec[1]:.5f} "
          f"f2={imp.qec[2]:.5f}" if imp.qec else "  QEC   : off")
    print(f"  CFO   : {imp.cfo:+.4f} subcarrier spacing(s), correction "
          f"{'ON (CP autocorrelation — Python stand-in)' if imp.cfo_correct else 'off'}"
          if imp.cfo else "  CFO   : off")
    print(f"  timing: {imp.timing:+d} sample(s), phase comp "
          f"{'ON' if imp.phase_comp else 'off'}"
          if imp.timing or imp.phase_comp else "  timing: off")
    print("=" * 74)

    # ── checkpoint setup ─────────────────────────────────────────────────────
    cfg = fingerprint(snr, n_symbols, chunk, imp)
    state = None
    if a.checkpoint:
        old = ckpt_load(a.checkpoint)
        if old is not None and not (a.resume or a.restart):
            print(f"ERROR: checkpoint already exists: {a.checkpoint}\n"
                  f"       {old.get('symbols_run', 0)} symbol(s) recorded, "
                  f"complete={old.get('complete')}\n"
                  f"       Pass --resume to continue it, or --restart to discard it.",
                  file=sys.stderr)
            return 2
        if old is not None and a.resume:
            if old.get("version") != CHECKPOINT_VERSION:
                print(f"ERROR: checkpoint version {old.get('version')} != "
                      f"{CHECKPOINT_VERSION} — cannot resume.", file=sys.stderr)
                return 2
            if old.get("config") != cfg:
                diff = [k for k in cfg if old.get("config", {}).get(k) != cfg[k]]
                print(f"ERROR: {a.checkpoint} was written with a different config; "
                      f"resuming would splice two experiments together.\n"
                      f"       Differing field(s): {', '.join(diff)}\n"
                      f"       Re-run with the original settings, or --restart.",
                      file=sys.stderr)
                return 2
            state = old
            done = sum(len(state["points"][i]["chunks_done"]) for i in sel)
            print(f"Resuming {a.checkpoint}: {state['symbols_run']} symbol(s) done "
                  f"({done}/{len(sel) * len(plan)} chunks, {_hms(state['seconds'])} "
                  f"of simulator time banked).")
        if state is None:
            state = new_state(snr, cfg)
            print(f"Checkpointing to {a.checkpoint}.")
    if state is None:
        state = new_state(snr, cfg)             # ephemeral — the same code path

    # Ctrl-C / SIGTERM: finish the chunk in flight, bank it, then stop. The handler
    # sets a flag and returns; it does not raise. Python defers a signal handler
    # until the current C call returns, so a signal that arrives inside the ISS
    # (libvspa2_16au.so, via ctypes) cannot interrupt the simulator — which is
    # exactly the "finish the chunk" semantics we want, and is why no process-group
    # gymnastics are needed: there are no worker subprocesses, the ISS is in here.
    stopping = {"flag": False}

    def _on_signal(signum, _frame):
        if stopping["flag"]:                    # second Ctrl-C: go now
            raise KeyboardInterrupt
        stopping["flag"] = True
        print(f"\n[signal {signum}] finishing the chunk in flight, then stopping "
              f"(again to abort immediately) ...", flush=True)

    if a.checkpoint:
        _signal.signal(_signal.SIGINT, _on_signal)
        _signal.signal(_signal.SIGTERM, _on_signal)

    # ── the kernels ──────────────────────────────────────────────────────────
    tx = NativeKernelRunner(BUILD / "compute_dfe_tx_vspa2_16au.eld",
                            [Port("sc", "input_sc_packed", NSC)],
                            [Port("td", "work_buf_out", N)])
    up = NativeKernelRunner(BUILD / "compute_x2_interp_vspa2_16au.eld",
                            [Port("td", "input_td", N_CP)],
                            [Port("up", "output_td", N_UP)])
    dn = NativeKernelRunner(BUILD / "compute_decim_2x_8tap_vspa2_16au.eld",
                            [Port("up", "input_td", N_UP)],
                            [Port("td", "output_td", N_CP)])
    rx = NativeKernelRunner(BUILD / "compute_dfe_rx_vspa2_16au.eld",
                            [Port("td", "input_td", N)],
                            [Port("sc", "rx_out", NSC)])
    qec = (NativeKernelRunner(BUILD / "compute_qec_td_vspa2_16au.eld",
                              [Port("input_td", "input_td", N),
                               Port("qec_params", "qec_params", 32, enc="u32")],
                              [Port("output_td", "output_td", N)])
           if imp.qec is not None else None)

    # ── the sweep: chunk outer, SNR point inner ──────────────────────────────
    # TX + interpolate (+ the imbalance, which is noise-independent) run ONCE per
    # symbol of the chunk; every SNR point then reuses those signals. That is the
    # ~4x saving — and it survives checkpointing precisely because the bits are
    # keyed by chunk alone, so the same chunk always yields the same symbols no
    # matter which points a given process happens to own.
    total_chunks = len(sel) * len(plan)
    done_chunks = sum(len(state["points"][i]["chunks_done"]) for i in sel)
    banked = state["seconds"]
    t_sweep = time.time()

    for j, n_sym in enumerate(plan):
        need = [i for i in sel if j not in state["points"][i]["chunks_done"]]
        if not need:
            continue                            # every point already banked this chunk

        t_tx = time.time()
        rb = rng_bits(RNG_SEED, j)
        sent = []
        for _ in range(n_sym):
            bits = rb.integers(0, 2, size=2 * NSC, dtype=np.uint8)
            td = tx.compute({"sc": qpsk_mod(bits)})["td"]
            sig = up.compute({"td": add_cp(td)})["up"]          # 2x rate, 2176
            if imp.imbalance:
                sig = apply_imbalance(sig, *imp.imbalance)
            sent.append((bits, sig))
        print(f"  chunk {j + 1}/{len(plan)}: TX + interpolate {n_sym} symbol(s) in "
              f"{_hms(time.time() - t_tx)}", flush=True)

        for i in need:
            snr_db = float(snr[i])
            pt = state["points"][i]
            t_chunk = time.time()
            err = tot = 0
            cfo_sum, cfo_cnt = 0.0, 0
            rn = rng_noise(RNG_SEED, i, j)
            for bits, sig in sent:
                sc, cfo_est = rx_chain(sig, snr_db, rn, dn, rx, qec, imp)
                err += int(np.sum(bits != qpsk_demod(sc)))
                tot += len(bits)
                if cfo_est is not None:
                    cfo_sum += cfo_est
                    cfo_cnt += 1

            pt["errors"] += err
            pt["bits"] += tot
            pt["cfo_sum"] += cfo_sum
            pt["cfo_cnt"] += cfo_cnt
            pt["chunks_done"].append(j)
            state["symbols_run"] += n_sym
            state["seconds"] = banked + (time.time() - t_sweep)
            done_chunks += 1
            if a.checkpoint:
                ckpt_save(a.checkpoint, state)

            per_sym = (time.time() - t_chunk) / max(1, n_sym)
            left = (total_chunks - done_chunks) * chunk * per_sym
            so_far = pt["errors"] / pt["bits"] if pt["bits"] else float("nan")
            print(f"  [{done_chunks:>4}/{total_chunks}] {snr_db:>5.1f} dB  chunk "
                  f"{j + 1}/{len(plan)}  {n_sym} sym in {_hms(time.time() - t_chunk)} "
                  f"({per_sym:.1f} s/sym)  BER so far {so_far:.2e}  "
                  f"ETA {_hms(left)}", flush=True)

            if stopping["flag"]:
                break
        if stopping["flag"]:
            break

    state["seconds"] = banked + (time.time() - t_sweep)

    if stopping["flag"]:
        if a.checkpoint:
            ckpt_save(a.checkpoint, state)
            print(f"\nStopped cleanly. {state['symbols_run']} symbol(s) banked in "
                  f"{a.checkpoint}.\nResume with the SAME arguments plus --resume.")
        else:
            print("\nStopped. No --checkpoint was given, so this run's results are "
                  "LOST. Use --checkpoint next time.")
        return 130

    state["complete"] = all(len(state["points"][i]["chunks_done"]) == len(plan)
                            for i in sel)
    if a.checkpoint:
        ckpt_save(a.checkpoint, state)

    print_table(snr, state["points"], sel, imp.cfo)
    print(f"\n  {_hms(state['seconds'])} of simulator time")

    if sharded:
        # A shard owns a slice of the grid. A BER floor, a summary or a figure over
        # a partial grid would be misleading — and every shard writing the same PNG
        # would race on it. Report and stop; the whole picture exists only after
        # --merge.
        print(f"\nShard done: SNR point(s) {', '.join(map(str, sel))}.")
        if a.checkpoint:
            print(f"Combine the shards with:\n  python3 {Path(__file__).name} "
                  f"--merge <shard>.json ... --checkpoint merged.json")
        return 0

    # ── the same chain in Python, for reference ──────────────────────────────
    model_ber = None
    if not a.no_model:
        print(f"\nPython model, {a.model_symbols} symbols/point ...")
        rng2 = np.random.default_rng(RNG_SEED + 1)
        msent = []
        for _ in range(a.model_symbols):
            bits = rng2.integers(0, 2, size=2 * NSC, dtype=np.uint8)
            td = m_tx(qpsk_mod(bits))
            u, _ = r_interpolator_2x(add_cp(td) / Q, TX_TAPS)
            sig = np.asarray(u) * Q
            if imp.imbalance:
                sig = apply_imbalance(sig, *imp.imbalance)
            msent.append((bits, sig))
        model_ber = []
        for snr_db in snr:
            err = tot = 0
            for bits, sig in msent:
                sc, _ = m_rx_chain(sig, float(snr_db), rng2, imp)
                err += int(np.sum(bits != qpsk_demod(sc)))
                tot += len(bits)
            model_ber.append(err / tot)
            print(f"{snr_db:5.1f}  {err / tot:10.2e} {err:8d} {tot:8d}")
        model_ber = np.array(model_ber)

    plot(a.figure, snr, ber_of(state["points"]),
         [p["bits"] for p in state["points"]],
         model_ber, a.model_symbols * 2 * NSC)
    return 0


if __name__ == "__main__":
    sys.exit(main())
