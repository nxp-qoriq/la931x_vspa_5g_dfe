#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""Gate the co-simulation kernels: each one against an independent Python model.

The co-simulation is only worth what its stages are worth. Before believing a BER
curve, prove that every stage the curve is built from computes what it claims:

  dfe_tx        vs  reorder + r_dif_fft_sc128(inv=1) + bit-reversal + windowing
  dfe_rx        vs  r_dif_fft_sc128(inv=0) + bit-reversal + extraction
  x2_interp     vs  r_interpolator_2x
  decim_2x_8tap vs  r_decimator_2x
  qec_td        vs  r_qec_td   (and: does it actually undo an I/Q imbalance?)

The models share no code with the firmware — they are a re-derivation, not a
wrapper — so agreement is evidence, not tautology. A round-trip check follows: a
symbol pushed through all four must come back as itself.

    python3 scripts/kernel_check.py
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "cosim"))

from dfe.kernel_runner import NativeKernelRunner, Port  # noqa: E402
from vspa_model.fft import r_dif_fft_sc128              # noqa: E402
from vspa_model.filter import r_interpolator_2x, r_decimator_2x  # noqa: E402
from vspa_model.qec import (r_qec_td, imbalance_to_params,       # noqa: E402
                            apply_imbalance, qec_params_words)

BUILD = REPO / "cosim" / "kernels" / "build"
N, NSC = 1024, 61
W = 16
WIN = np.sin(np.pi * np.arange(W) / (2 * W)) ** 2

#: The interpolator's taps are stored the way the HARDWARE wants them —
#: reverse PAIR order: h[M-2],h[M-1], h[M-4],h[M-3], ..., h[0],h[1]. The model
#: wants natural order. Note this is NOT a no-op even though the filter is
#: palindromic: it reverses the order of the pairs, not of the taps.
_INTERP_HW = np.array([
    0x3BF66D0F, 0x3B145943, 0xBC2029E4, 0x3BC6E0FE, 0x3C1D0E2D, 0xBC914B26,
    0xBB7807E3, 0x3D136719, 0xBC4E8404, 0xBD814983, 0x3D4EF22B, 0x3DD52378,
    0xBE1B9A7D, 0xBE34750A, 0x3F82D7F9, 0x3ECB4074, 0x3ECB4074, 0x3F82D7F9,
    0xBE34750A, 0xBE1B9A7D, 0x3DD52378, 0x3D4EF22B, 0xBD814983, 0xBC4E8404,
    0x3D136719, 0xBB7807E3, 0xBC914B26, 0x3C1D0E2D, 0x3BC6E0FE, 0xBC2029E4,
    0x3B145943, 0x3BF66D0F], dtype=np.uint32).view(np.float32).astype(np.float64)


def unreverse_pairs(hw: np.ndarray) -> np.ndarray:
    nat = np.empty_like(hw)
    pairs = len(hw) // 2
    for i in range(pairs):
        k = pairs - 1 - i
        nat[2 * k], nat[2 * k + 1] = hw[2 * i], hw[2 * i + 1]
    return nat


INTERP_TAPS = unreverse_pairs(_INTERP_HW)
DECIM_TAPS = np.array([                       # already natural order (symmetric)
    0xBE0263BE, 0xBDE8A4F0, 0x3EA45618, 0x3F5B3BFF,
    0x3F5B3BFF, 0x3EA45618, 0xBDE8A4F0, 0xBE0263BE],
    dtype=np.uint32).view(np.float32).astype(np.float64)

#: The rate converters' SX kernels process whole 64-sample blocks, so their input
#: length MUST be a multiple of 64 — a non-multiple leaves the tail unprocessed
#: (measured: with 1044 in, outputs 2048..2087 are garbage). 1024 + a 64-sample
#: cyclic prefix = 1088 = 17*64 satisfies it.
CP_LEN = 64
N_CP = N + CP_LEN                             # 1088 = 17 * 64
N_UP = 2 * N_CP                               # 2176 = 34 * 64


#: The models work in NORMALISED Q15 (a sample is a fraction of full scale); the
#: kernels work in raw SM16 counts. Everything below is compared as counts.
Q = 32768.0


def as_counts(z) -> np.ndarray:
    z = np.asarray(z, dtype=complex) * Q
    return np.round(np.real(z)) + 1j * np.round(np.imag(z))


def snr_db(ref: np.ndarray, got: np.ndarray) -> float:
    err = float((np.abs(got - ref) ** 2).sum())
    sig = float((np.abs(ref) ** 2).sum())
    if err == 0:
        return np.inf
    if sig == 0:
        return -np.inf
    return 10 * np.log10(sig / err)


def report(name: str, ref: np.ndarray, got: np.ndarray, floor: float = 40.0) -> bool:
    s = snr_db(ref, got)
    exact = np.array_equal(got, ref)
    ok = exact or s >= floor
    verdict = "BIT-EXACT" if exact else ("PASS" if ok else "FAIL")
    shown = "inf" if s == np.inf else ("-inf" if s == -np.inf else f"{s:.1f}")
    print(f"  {name:<16s} SNR vs model {shown:>6s} dB   {verdict}")
    return ok


def tx_runner():
    return NativeKernelRunner(
        BUILD / "compute_dfe_tx_vspa2_16au.eld",
        [Port("input_sc", "input_sc_packed", NSC)],
        [Port("output_td", "work_buf_out", N)])


def rx_runner():
    return NativeKernelRunner(
        BUILD / "compute_dfe_rx_vspa2_16au.eld",
        [Port("input_td", "input_td", N)],
        [Port("output_sc", "rx_out", NSC)])


def interp_runner():
    return NativeKernelRunner(
        BUILD / "compute_x2_interp_vspa2_16au.eld",
        [Port("input_td", "input_td", N_CP)],
        [Port("output_td", "output_td", N_UP)])


def decim_runner():
    return NativeKernelRunner(
        BUILD / "compute_decim_2x_8tap_vspa2_16au.eld",
        [Port("input_td", "input_td", N_UP)],
        [Port("output_td", "output_td", N_CP)])


def qec_runner():
    """The QEC stage. Note the second INPUT port: the correction coefficients are
    written into DMEM per call, like the samples — not baked into the ELF."""
    return NativeKernelRunner(
        BUILD / "compute_qec_td_vspa2_16au.eld",
        [Port("input_td", "input_td", N),
         Port("qec_params", "qec_params", 32, enc="u32")],
        [Port("output_td", "output_td", N)])


def add_cp(td: np.ndarray) -> np.ndarray:
    """Prepend the cyclic prefix — the last CP_LEN samples of the symbol."""
    return np.concatenate([td[N - CP_LEN:], td])


def model_tx(sc: np.ndarray) -> np.ndarray:
    """reorder -> iFFT -> bit-reversal -> windowing. `sc` is in counts."""
    grid = np.zeros(N, dtype=complex)
    half = NSC // 2
    grid[:NSC - half] = sc[half:] / Q             # pos freqs -> low bins
    grid[N - half:] = sc[:half] / Q               # neg freqs -> high bins
    out = np.array(r_dif_fft_sc128(grid, inv_flag=1))
    out[:W] *= WIN
    out[N - W:] *= (1.0 - WIN)
    return as_counts(out)


def model_rx(td: np.ndarray) -> np.ndarray:
    fd = np.array(r_dif_fft_sc128(td / Q, inv_flag=0))
    half = NSC // 2
    return as_counts(np.concatenate([fd[N - half:], fd[:NSC - half]]))


def main() -> int:
    rng = np.random.default_rng(7)
    ok = True

    # QPSK subcarriers, scaled well clear of SM16 saturation.
    sc = (rng.choice([-1, 1], NSC) + 1j * rng.choice([-1, 1], NSC)) * 4096

    print("Kernels vs an independent Python model")
    tx = tx_runner()
    td = tx.compute({"input_sc": sc})["output_td"]
    ok &= report("dfe_tx", model_tx(sc), td)

    rx = rx_runner()
    got_sc = rx.compute({"input_td": td})["output_sc"]
    ok &= report("dfe_rx", model_rx(td), got_sc)

    # The rate converters run on the symbol plus its cyclic prefix. Both model
    # functions return (output, next_history) — the co-simulation filters each
    # symbol from a cold start, so the history is discarded.
    x = add_cp(td)
    up = interp_runner().compute({"input_td": x})["output_td"]
    ok &= report("x2_interp", as_counts(r_interpolator_2x(x / Q, INTERP_TAPS)[0]), up)

    dn = decim_runner().compute({"input_td": up})["output_td"]
    ok &= report("decim_2x_8tap", as_counts(r_decimator_2x(up / Q, DECIM_TAPS)[0]), dn)

    # QEC. Two claims, and the second is the one that matters: (a) the kernel
    # computes what the model says, and (b) feeding it an imbalanced signal and the
    # coefficients derived from that imbalance gives the CLEAN signal back. A
    # kernel that quietly ignored its parameter line would pass (a) and fail (b).
    eps, phi = 0.08, 0.05                        # 8% gain error, 50 mrad phase
    f1, f4, f2 = imbalance_to_params(eps, phi)
    imb = as_counts(apply_imbalance(td, eps, phi) / Q)
    fixed = qec_runner().compute({"input_td": imb,
                                  "qec_params": qec_params_words(f1, f4, f2)})["output_td"]
    ok &= report("qec_td", r_qec_td(imb, f1, f4, f2), fixed)
    before, after = snr_db(td, imb), snr_db(td, fixed)
    good = after >= 40.0 and after > before + 20.0
    print(f"  {'qec_td (undo)':<16s} imbalance {before:.1f} dB -> corrected "
          f"{after:.1f} dB   {'PASS' if good else 'FAIL'}")
    ok &= good

    # Round trip: the chain the co-simulation actually runs. The 32-tap
    # interpolator and 8-tap decimator together delay the signal by ~9.5 samples
    # at the 1x rate; the FFT window is opened past the CP to absorb it.
    print("\nRound trip through all four kernels (no channel)")
    # A cyclic roll, not a later slice: the samples the delay "eats" come from the
    # CP, which is by construction a copy of the payload's own tail.
    delay = 10
    rt = rx.compute({"input_td": np.roll(dn[CP_LEN:], -delay)})["output_sc"]
    corr = np.vdot(sc, rt) / np.vdot(sc, sc)          # the known net gain
    resid = snr_db(sc * corr, rt)
    shown = "inf" if resid == np.inf else f"{resid:.1f}"
    good = resid >= 20.0
    print(f"  tx -> interp -> decim -> rx   gain {abs(corr):.4f}  "
          f"SNR {shown} dB   {'PASS' if good else 'FAIL'}")
    ok &= good

    print("\n" + ("ALL KERNELS VERIFIED" if ok else "SOME KERNELS FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
