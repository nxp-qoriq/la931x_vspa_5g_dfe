#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""SSB co-simulation — once the cell is found, can we actually demodulate it?

    build SSB -> TX (VSPA) -> +CP -> MULTIPATH channel + CFO + AWGN -> -CP
             -> RX (VSPA) -> chEst on the DMRS (VSPA) -> ZF equalise (VSPA)
             -> EVM / channel-MSE / PBCH BER

Cell *search* answers "is there a cell, and which one?". This answers the next
question, and it is the one that actually needs a channel estimator: the PBCH
carries the MIB, and you cannot decode the MIB through a multipath channel
without estimating and inverting that channel. An AWGN link never has to.

WHAT IS REAL AND WHAT IS MODELLED
---------------------------------
Real assembly out of firmware/lib/lib_DFE_Ref_vspa2.elb, run on the in-process
ISS, one ELF per stage:

    ifftDIF1024_sc128_hfx_hfx_vspa2   TX                (compute_ssb_tx)
    fftDIF1024_sc128_hfx_hfx_vspa2    RX                (compute_ssb_rx)
    chEst                             LS channel estimate on the DMRS
    csp_chf_rcpv_asm                  equaliser weights = 1/H
    chp_chf_csp_vmult_asm             apply the weights          (compute_ssb_demod)

There is no "zf_equalizer" routine in the library and none is invented here:
zero forcing is composed from rcpv and vmult exactly as firmware/src/DFE_func_rx.c
composes it.

Modelled in Python, and these are the ONLY modelled steps:
  * the channel itself (multipath taps, CFO, AWGN) — that is the point of a
    co-simulation;
  * the CP add/strip, which on silicon is DMA address arithmetic, not DSP;
  * INTERPOLATION OF THE CHANNEL ESTIMATE ACROSS THE DMRS COMB. The DMRS sits on
    every 4th RE, so chEst — which is elementwise — produces a usable estimate
    only on those REs. Interpolating between them needs a filter, and there is no
    such kernel in the library: `ssbSmooth` is declared in the NXP headers but is
    NOT exported by lib_DFE_Ref_vspa2.elb (and its only call site in DFE_func_rx.c
    is commented out). So the comb interpolation is a documented host-side model.
    Everything on either side of it — the LS estimate, the reciprocal, the
    equalisation — is the real kernel.

TWO CALIBRATIONS, BOTH MEASURED RATHER THAN ASSUMED
---------------------------------------------------
1. The sc128 FFT pair's round-trip gain is NOT the nominal 1/16 = 0.0625. It
   measures 0.0613 — 1.9% low, because the fixed-point butterflies truncate
   toward zero at every stage. That bias is a property of the transform, not an
   error made by the estimator, so it is measured once (--calibrate, run with no
   channel and no noise, through the real kernels) and divided out of the
   reference. Charging the channel estimator for it would put a fake -34 dB floor
   under the channel MSE.

2. The AWGN variance is set from that same measured gain so that the requested
   SNR is the per-RE Es/N0 *after* the RX FFT. The script then MEASURES the SNR
   it actually realised, off the received grid, and checks it against the request.

    python3 cosim/scripts/ssb_cosim.py --snr 20 --delay-spread 8
    python3 cosim/scripts/ssb_cosim.py --sweep

Needs LD_LIBRARY_PATH=<repo>/environment/ccssim2/bin.
"""
from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import numpy as np
from scipy.special import erfc

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "cosim"))

from dfe.kernel_runner import NativeKernelRunner, Port          # noqa: E402
from vspa_model.nr5g import (                                    # noqa: E402
    nr_pss_sequence, nr_sss_sequence, nr_pbch_dmrs, nr_ssb_re_map,
    RE_PSS, RE_SSS, RE_PBCH, RE_DMRS,
    NR_SSB_NSYM, NR_SSB_NRE, NR_PBCH_DMRS_NRE, NR_PBCH_DATA_NRE,
)

BUILD = REPO / "cosim" / "kernels" / "build"
FIGDIR = REPO / "docs" / "figures"

# ── the radio ────────────────────────────────────────────────────────────────
N        = 1024               # FFT size (the DFE's own iFFT/FFT)
NSC      = 624               # carrier REs — the 9310 build's NSC, and the width
                             # of NXP's own FR1 10 MHz / 15 kHz fd vectors
CP       = 72                # cyclic prefix (~7%, normal CP). Also the hard
                             # upper bound on the channel's delay spread.
SCS      = 30_000.0          # subcarrier spacing (Hz)
FS       = N * SCS           # 30.72 MHz

#: Carrier RE index of SSB RE 0. 372 is one of the RE_offsets NXP's own FR1 test
#: vectors use, and it keeps all 240 SSB REs on one side of DC — the SSB does not
#: straddle bin 0, which would only complicate the plots without testing anything.
SSB_RE0  = 372
KRE      = 8                 # NR_SSB_KRE: the 256-pt window starts 8 REs early
NFFT_SSB = 256               # NR_SSB_NFFT — what chEst and the equaliser see
WIN0     = SSB_RE0 - KRE     # carrier RE where that window starts
NLINES   = NFFT_SSB // 32    # = 8 DMEM lines, chEst's N argument

#: RE amplitude in SM16 counts. 12000 puts the time-domain peak at ~4200 of
#: 32767 — enough headroom for the channel's peaks plus AWGN down to 0 dB — while
#: keeping the post-FFT REs at ~736 counts, well clear of the quantiser.
AMP = 12000.0

NID1, NID2 = 200, 1
CELL_ID = 3 * NID1 + NID2     # NR_CELLID(id1,id2) — 601

SNR_SWEEP = [0, 5, 10, 15, 20, 25, 30]


# ── QPSK ─────────────────────────────────────────────────────────────────────
def qpsk_mod(bits: np.ndarray) -> np.ndarray:
    """Unit-power QPSK: two bits -> (±1 ± 1j)/sqrt(2)."""
    return ((1 - 2 * bits[0::2].astype(float))
            + 1j * (1 - 2 * bits[1::2].astype(float))) / np.sqrt(2)


def qpsk_demod(sym: np.ndarray) -> np.ndarray:
    bits = np.zeros(2 * len(sym), dtype=np.uint8)
    bits[0::2] = (np.real(sym) < 0)
    bits[1::2] = (np.imag(sym) < 0)
    return bits


def rms(v: np.ndarray) -> float:
    return float(np.sqrt(np.mean(np.abs(np.asarray(v)) ** 2)))


def db(x: float) -> float:
    return 20.0 * np.log10(max(x, 1e-30))


# ── the SSB ──────────────────────────────────────────────────────────────────
class SSB:
    """One SSB burst: 4 OFDM symbols x 240 REs, unit-power symbols.

    grid[l, k] is the modulation symbol on SSB symbol l, subcarrier k — not yet
    scaled by AMP and not yet placed on the carrier.
    """

    def __init__(self, nid1: int, nid2: int, rng: np.random.Generator):
        self.nid1, self.nid2 = nid1, nid2
        self.cell_id = 3 * nid1 + nid2
        self.re_map = nr_ssb_re_map(self.cell_id)
        self.grid = np.zeros((NR_SSB_NSYM, NR_SSB_NRE), dtype=complex)

        # PSS (symbol 0) and SSS (symbol 2): real ±1 BPSK on k = 56..182.
        self.grid[0, 56:183] = nr_pss_sequence(nid2)
        self.grid[2, 56:183] = nr_sss_sequence(nid1, nid2)

        # PBCH DMRS: every 4th PBCH RE, in (symbol, subcarrier) order.
        self.dmrs_idx = [(l, k) for l in (1, 2, 3)
                         for k in np.where(self.re_map[l] == RE_DMRS)[0]]
        assert len(self.dmrs_idx) == NR_PBCH_DMRS_NRE
        self.dmrs = nr_pbch_dmrs(self.cell_id)
        for (l, k), d in zip(self.dmrs_idx, self.dmrs):
            self.grid[l, k] = d

        # PBCH payload: random bits -> QPSK on every remaining PBCH RE.
        self.pbch_idx = [(l, k) for l in (1, 2, 3)
                         for k in np.where(self.re_map[l] == RE_PBCH)[0]]
        assert len(self.pbch_idx) == NR_PBCH_DATA_NRE
        self.bits = rng.integers(0, 2, size=2 * NR_PBCH_DATA_NRE).astype(np.uint8)
        self.pbch = qpsk_mod(self.bits)
        for (l, k), d in zip(self.pbch_idx, self.pbch):
            self.grid[l, k] = d

    def carrier(self, l: int) -> np.ndarray:
        """SSB symbol l placed on the 624-RE carrier, scaled to SM16 counts."""
        sc = np.zeros(NSC, dtype=complex)
        sc[SSB_RE0:SSB_RE0 + NR_SSB_NRE] = AMP * self.grid[l]
        return sc

    def pilot(self, l: int) -> np.ndarray:
        """chEst's pPilot for SSB symbol l, on the 256-pt window.

        DMRS where there is DMRS; 1+0j everywhere else — the padding convention
        the real sssConvert() kernel uses, and the one that keeps rcpv's 1/H
        finite off the pilot REs. chEst computes h = y*conj(pilot), so the REs
        that are not DMRS come back holding the received symbol itself; they are
        ignored (see comb_interp).
        """
        p = np.ones(NFFT_SSB, dtype=complex)
        for k in np.where(self.re_map[l] == RE_DMRS)[0]:
            p[KRE + k] = self.grid[l, k]
        return p

    def dmrs_window_idx(self, l: int) -> np.ndarray:
        """Indices into the 256-pt window that carry DMRS on SSB symbol l."""
        return KRE + np.where(self.re_map[l] == RE_DMRS)[0]


# ── the channel ──────────────────────────────────────────────────────────────
def make_channel(delay_spread: int, rng: np.random.Generator) -> np.ndarray:
    """Multipath taps: exponential power-delay profile, unit total power.

    delay_spread is the maximum excess delay in samples (0 = flat/single-tap).
    Tap l has average power exp(-3l/D), so the last tap is 13 dB down. The taps
    are complex Gaussian — a Rayleigh channel — and they are normalised so the
    channel neither adds nor removes energy, which keeps the SNR calibration
    meaning what it says.
    """
    if delay_spread <= 0:
        return np.array([1.0 + 0j])
    if delay_spread >= CP:
        raise SystemExit(f"--delay-spread {delay_spread} >= CP {CP}: the CP can "
                         f"no longer absorb the channel and the OFDM link breaks "
                         f"by construction. That is not a bug worth simulating.")
    l = np.arange(delay_spread + 1)
    p = np.exp(-3.0 * l / delay_spread)
    g = (rng.standard_normal(len(l)) + 1j * rng.standard_normal(len(l))) / np.sqrt(2)
    g = g * np.sqrt(p)
    return g / np.sqrt(np.sum(np.abs(g) ** 2))


def channel_freq_response(taps: np.ndarray) -> np.ndarray:
    """H[bin] for every one of the N DFT bins."""
    return np.fft.fft(taps, N)


def re_to_bin(j: np.ndarray | int) -> np.ndarray:
    """Carrier RE index -> DFT bin. The mapping compute_ssb_tx.c performs."""
    return (np.asarray(j) - NSC // 2) % N


def cfo_common_phase(cfo_hz: float, sym: int) -> complex:
    """The complex scalar a carrier frequency offset puts on every RE of symbol l.

    With r[m] = x[m]·exp(j2πε(n0+m)/N), ε = cfo/SCS, the DFT bin k picks up

        c = exp(j2πε·n0/N) · exp(jπε(N-1)/N) · sin(πε) / (N·sin(πε/N))

    from its own subcarrier (the rest is ICI, which is left in as error). n0 is
    the first sample of the symbol's FFT window in the transmitted stream. This
    is derived, not fitted: it is what lets the channel-MSE stay a measure of the
    ESTIMATOR rather than of the frequency offset.
    """
    if cfo_hz == 0.0:
        return 1.0 + 0j
    eps = cfo_hz / SCS
    n0 = sym * (N + CP) + CP
    amp = np.sin(np.pi * eps) / (N * np.sin(np.pi * eps / N))
    return amp * np.exp(2j * np.pi * eps * n0 / N) * np.exp(1j * np.pi * eps * (N - 1) / N)


# ── the VSPA kernels ─────────────────────────────────────────────────────────
class Chain:
    def __init__(self) -> None:
        self.tx = NativeKernelRunner(
            BUILD / "compute_ssb_tx_vspa2_16au.eld",
            [Port("sc", "input_sc_packed", NSC)],
            [Port("td", "work_buf_out", N)])
        self.rx = NativeKernelRunner(
            BUILD / "compute_ssb_rx_vspa2_16au.eld",
            [Port("td", "input_td", N)],
            [Port("sc", "rx_out", NSC)])
        self.demod = NativeKernelRunner(
            BUILD / "compute_ssb_demod_vspa2_16au.eld",
            [Port("rx1", "rx_sym1", NFFT_SSB, "sm16_complex"),
             Port("rx2", "rx_sym2", NFFT_SSB, "sm16_complex"),
             Port("rx3", "rx_sym3", NFFT_SSB, "sm16_complex"),
             Port("dmrs1", "dmrs1", NFFT_SSB, "chf_complex"),
             Port("dmrs3", "dmrs3", NFFT_SSB, "chf_complex"),
             Port("h1_in", "h1_in", NFFT_SSB, "sm16_complex"),
             Port("h2_in", "h2_in", NFFT_SSB, "sm16_complex"),
             Port("h3_in", "h3_in", NFFT_SSB, "sm16_complex")],
            [Port("h1_raw", "h1_raw", NFFT_SSB, "sm16_complex"),
             Port("h3_raw", "h3_raw", NFFT_SSB, "sm16_complex"),
             Port("eq1", "eq1", NFFT_SSB, "chf_complex"),
             Port("eq2", "eq2", NFFT_SSB, "chf_complex"),
             Port("eq3", "eq3", NFFT_SSB, "chf_complex")])
        self.calls = 0

    def transmit(self, sc: np.ndarray) -> np.ndarray:
        self.calls += 1
        return self.tx.compute({"sc": sc})["td"]

    def receive(self, td: np.ndarray) -> np.ndarray:
        self.calls += 1
        return self.rx.compute({"td": td})["sc"]


def sm16_clip(td: np.ndarray) -> tuple[np.ndarray, int]:
    """Quantise time samples into cfixed16's integer grid; report clipping.

    SM16 holds a 15-bit magnitude, so |re|,|im| <= 32767. If the channel plus the
    noise drives the signal past that, the ADC clips — and a clipped run is not
    the run you asked for, so it is counted and reported rather than swallowed.
    """
    re = np.round(td.real)
    im = np.round(td.imag)
    n_clip = int(np.sum((np.abs(re) > 32767) | (np.abs(im) > 32767)))
    return np.clip(re, -32767, 32767) + 1j * np.clip(im, -32767, 32767), n_clip


# ── the one modelled DSP step ────────────────────────────────────────────────
def comb_interp(h_raw: np.ndarray, idx: np.ndarray) -> np.ndarray:
    """Interpolate the LS estimate across the DMRS comb — HOST-SIDE MODEL.

    chEst is elementwise, so h_raw is a channel estimate only on the DMRS REs
    (`idx`); between them it holds the received PBCH data multiplied by the
    pilot's 1+0j filler. Piecewise-linear interpolation in the real and imaginary
    parts fills the gaps, holding the edge values outside the comb.

    THIS IS THE ONLY DSP STEP IN THE WHOLE CHAIN WITH NO KERNEL BEHIND IT. The
    library ships no interpolator or smoother for it — `ssbSmooth` is declared in
    the NXP headers but is not exported by lib_DFE_Ref_vspa2.elb. Real firmware
    would need one written; it is modelled here rather than faked, and it is
    labelled at every point where its output is used.
    """
    i = np.arange(NFFT_SSB)
    return (np.interp(i, idx, h_raw[idx].real)
            + 1j * np.interp(i, idx, h_raw[idx].imag))


def to_cfixed16(h: np.ndarray) -> np.ndarray:
    """Round a channel estimate back into the cfixed16 integer grid the kernel
    reads (the interpolated value goes back to the VSPA for rcpv to invert)."""
    return (np.clip(np.round(h.real), -32767, 32767)
            + 1j * np.clip(np.round(h.imag), -32767, 32767))


# ── one link-level run ───────────────────────────────────────────────────────
def transmit_ssb(chain: Chain, ssb: SSB) -> list[np.ndarray]:
    """The 4 SSB symbols through the real iFFT. SNR-independent — done once."""
    return [chain.transmit(ssb.carrier(l)) for l in range(NR_SSB_NSYM)]


def stream_with_cp(symbols: list[np.ndarray]) -> np.ndarray:
    return np.concatenate([np.concatenate([s[N - CP:], s]) for s in symbols])


def run_point(chain: Chain, ssb: SSB, td_syms: list[np.ndarray],
              taps: np.ndarray, snr_db: float | None, cfo_hz: float,
              gain: complex, rng: np.random.Generator,
              rx_all: bool = False) -> dict:
    """One (channel, CFO, SNR) point, end to end. Returns everything measured."""
    H = channel_freq_response(taps)
    ssb_bins = re_to_bin(np.arange(SSB_RE0, SSB_RE0 + NR_SSB_NRE))
    win_bins = re_to_bin(np.arange(WIN0, WIN0 + NFFT_SSB))
    H_ssb = H[ssb_bins]

    # ── channel: multipath, then CFO, then AWGN, on the whole 4-symbol stream ──
    stream = stream_with_cp(td_syms)
    rx = np.convolve(stream, taps)[:len(stream)]

    if cfo_hz:
        rx = rx * np.exp(2j * np.pi * cfo_hz * np.arange(len(rx)) / FS)

    if snr_db is None:
        sigma2 = 0.0
    else:
        # Noise reaches the RE through the forward FFT only (gain 1/128, so its
        # variance is scaled by N/128^2 = 1/16); the signal reaches it through
        # both transforms (measured gain `gain`). Setting the two equal at the
        # requested Es/N0 gives:
        sigma2 = (16.0 * (AMP * abs(gain)) ** 2 * float(np.mean(np.abs(H_ssb) ** 2))
                  / 10.0 ** (snr_db / 10.0))
        rx = rx + np.sqrt(sigma2 / 2.0) * (rng.standard_normal(len(rx))
                                           + 1j * rng.standard_normal(len(rx)))

    rx, n_clip = sm16_clip(rx)

    # ── RX: strip the CP, real FFT-1024, back to the carrier RE grid ──────────
    want = range(NR_SSB_NSYM) if rx_all else (1, 2, 3)
    grid = {}
    for l in want:
        n0 = l * (N + CP) + CP
        grid[l] = chain.receive(rx[n0:n0 + N])
    win = {l: grid[l][WIN0:WIN0 + NFFT_SSB] for l in want}

    # Realised SNR, measured off the grid itself: signal on the SSB REs, noise on
    # the empty carrier REs beside them. A check on the calibration, not an input.
    empty = np.concatenate([grid[1][:SSB_RE0 - 16],
                            grid[1][SSB_RE0 + NR_SSB_NRE + 16:]])
    p_sig = rms(grid[1][SSB_RE0:SSB_RE0 + NR_SSB_NRE]) ** 2
    p_noi = rms(empty) ** 2
    snr_meas = 10 * np.log10(max(p_sig - p_noi, 1e-9) / max(p_noi, 1e-30))

    # ── pass 1: LS channel estimate on the DMRS, on the real chEst kernel ─────
    ones = np.ones(NFFT_SSB, dtype=complex)     # h_in don't-care: NOT zeros, or
    p1 = chain.demod.compute({                  # rcpv would be asked for 1/0
        "rx1": win[1], "rx2": win[2], "rx3": win[3],
        "dmrs1": ssb.pilot(1), "dmrs3": ssb.pilot(3),
        "h1_in": ones, "h2_in": ones, "h3_in": ones})
    chain.calls += 1

    # ── the modelled step: interpolate across the comb ────────────────────────
    h1 = comb_interp(p1["h1_raw"], ssb.dmrs_window_idx(1))
    h3 = comb_interp(p1["h3_raw"], ssb.dmrs_window_idx(3))
    h2 = 0.5 * (h1 + h3)      # symbol 2 sits between them; its own DMRS covers
                              # only the band edges, so interpolate in TIME.

    # ── pass 2: 1/H on the real rcpv, apply on the real vmult ─────────────────
    p2 = chain.demod.compute({
        "rx1": win[1], "rx2": win[2], "rx3": win[3],
        "dmrs1": ssb.pilot(1), "dmrs3": ssb.pilot(3),
        "h1_in": to_cfixed16(h1), "h2_in": to_cfixed16(h2),
        "h3_in": to_cfixed16(h3)})
    chain.calls += 1
    eq = {1: p2["eq1"], 2: p2["eq2"], 3: p2["eq3"]}
    h_est = {1: h1, 2: h2, 3: h3}

    # ── measure ───────────────────────────────────────────────────────────────
    # A perfect estimate of symbol l would be AMP * gain * H * (CFO's phase).
    # Everything is compared against that; nothing is fitted.
    ref = {l: AMP * gain * H[win_bins] * cfo_common_phase(cfo_hz, l)
           for l in (1, 2, 3)}

    ssb_win = slice(KRE, KRE + NR_SSB_NRE)      # the 240 real REs of the window
    chan_err = np.concatenate([h_est[l][ssb_win] - ref[l][ssb_win] for l in (1, 3)])
    chan_ref = np.concatenate([ref[l][ssb_win] for l in (1, 3)])
    chan_nmse = db(rms(chan_err) / rms(chan_ref))   # 20log10 of an amplitude
                                                    # ratio == the power NMSE in dB

    after = np.array([eq[l][KRE + k] for (l, k) in ssb.pbch_idx])
    before = np.array([win[l][KRE + k] / (AMP * gain) for (l, k) in ssb.pbch_idx])
    d = ssb.pbch

    evm_after = rms(after - d) / rms(d)
    evm_before = rms(before - d) / rms(d)
    n_err = int(np.sum(qpsk_demod(after) != ssb.bits))

    # ── what a ZF equaliser with PERFECT channel knowledge would do here ──────
    # Zero forcing divides each RE by its own H, so it divides the noise by H
    # too: an RE sitting in a fade comes out with its SNR cut by |H_k|^2. Both
    # bounds below follow from that one fact and from the known channel — they
    # are derived, not tuned, and they are what the measured numbers are held to.
    if snr_db is None:
        evm_ideal, ber_ideal = 0.0, 0.0
    else:
        hk = np.array([abs(H[re_to_bin(SSB_RE0 + k)]) for (_, k) in ssb.pbch_idx])
        snr_re = 10.0 ** (snr_db / 10.0) * hk ** 2 / float(np.mean(np.abs(H_ssb) ** 2))
        evm_ideal = float(np.sqrt(np.mean(1.0 / snr_re)))
        # uncoded QPSK: BER = Q(sqrt(Es/N0)) per RE, averaged over the band
        ber_ideal = float(np.mean(0.5 * erfc(np.sqrt(snr_re / 2.0))))

    return dict(snr_db=snr_db, snr_meas=snr_meas, cfo_hz=cfo_hz,
                evm_before=evm_before, evm_after=evm_after, evm_ideal=evm_ideal,
                chan_nmse=chan_nmse, n_err=n_err, n_bits=len(ssb.bits),
                ber=n_err / len(ssb.bits), ber_ideal=ber_ideal, n_clip=n_clip,
                before=before, after=after, h_est=h_est, ref=ref, H=H,
                grid=grid, win=win, h_raw={1: p1["h1_raw"], 3: p1["h3_raw"]})


# ── calibration ──────────────────────────────────────────────────────────────
def calibrate(chain: Chain, ssb: SSB, td_syms: list[np.ndarray]) -> tuple[complex, float]:
    """The real TX->RX chain's gain and noise floor: no channel, no noise.

    Returns (complex gain, EVM floor). The gain is NOT 1/16 — see the module
    docstring. Measuring it is what keeps the channel-MSE honest.
    """
    rx = chain.receive(td_syms[1])          # symbol 1: all 240 REs occupied
    y = rx[SSB_RE0:SSB_RE0 + NR_SSB_NRE]
    x = AMP * ssb.grid[1]
    gain = complex(np.mean(y / x))
    floor = rms(y - gain * x) / rms(gain * x)
    return gain, floor


# ── figures ──────────────────────────────────────────────────────────────────
FG, BG = "#1f2933", "#ffffff"
C_TRUE, C_EST, C_BEFORE, C_AFTER = "#8a8a8a", "#e8590c", "#adb5bd", "#4c6ef5"


def _style(ax):
    ax.grid(True, alpha=0.22, lw=0.6)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)


def fig_grid(ssb: SSB, path: Path) -> None:
    import matplotlib.pyplot as plt
    from matplotlib.colors import ListedColormap, BoundaryNorm
    from matplotlib.patches import Patch

    cols = ["#f1f3f5", "#e8590c", "#f59f00", "#4c6ef5", "#12b886"]
    names = ["unused", "PSS", "SSS", "PBCH", "PBCH DMRS"]
    cmap = ListedColormap(cols)
    norm = BoundaryNorm(np.arange(-0.5, 5.5), 5)

    fig, ax = plt.subplots(figsize=(11, 3.1))
    ax.imshow(ssb.re_map, aspect="auto", cmap=cmap, norm=norm,
              interpolation="nearest", extent=(-0.5, 239.5, 3.5, -0.5))
    ax.set_yticks(range(4))
    ax.set_yticklabels([f"l = {i}" for i in range(4)])
    ax.set_xlabel("SSB subcarrier  k  (carrier RE = k + %d)" % SSB_RE0)
    ax.set_title(f"SSB resource grid — N_id_1 = {ssb.nid1}, N_id_2 = {ssb.nid2}, "
                 f"NCellID = {ssb.cell_id}  (DMRS on k mod 4 = {ssb.cell_id % 4})",
                 color=FG)
    ax.legend(handles=[Patch(facecolor=c, label=n) for c, n in zip(cols, names)],
              ncol=5, frameon=False, loc="upper center",
              bbox_to_anchor=(0.5, -0.30))
    fig.tight_layout()
    fig.savefig(path, dpi=140, facecolor=BG, bbox_inches="tight")
    plt.close(fig)


def fig_channel(res: dict, ssb: SSB, path: Path, snr_db: float) -> None:
    """|H| and arg(H): the true channel, the raw LS estimate on the DMRS REs, and
    the interpolated estimate the equaliser actually uses — all overlaid."""
    import matplotlib.pyplot as plt

    k = np.arange(NR_SSB_NRE)
    H_true = res["H"][re_to_bin(SSB_RE0 + k)]
    scale = res["ref"][1][KRE + k] / H_true            # AMP * gain * CFO phase
    h_est = res["h_est"][1][KRE + k] / scale           # interpolated, in H units

    # the raw kernel output, at the REs where it is actually a channel estimate
    dm = ssb.dmrs_window_idx(1)                        # window indices
    dk = dm - KRE                                      # -> SSB subcarrier
    h_dmrs = res["h_raw"][1][dm] / (res["ref"][1][dm] / res["H"][re_to_bin(SSB_RE0 + dk)])

    # Phase must be UNWRAPPED consistently on all three curves. Plotting the
    # scatter with a wrapped angle() against unwrapped lines makes the DMRS points
    # jump by 360 deg where the true phase crosses -180 and hang in the air as a
    # detached arc — a lie about the data, which is in fact right on the curve.
    def unwrap_deg(z):
        return np.rad2deg(np.unwrap(np.angle(z)))

    ph_true, ph_est = unwrap_deg(H_true), unwrap_deg(h_est)
    ph_dmrs = unwrap_deg(h_dmrs)                      # unwrapped along the comb
    ph_dmrs += 360.0 * round((ph_true[dk].mean() - ph_dmrs.mean()) / 360.0)

    fig, axes = plt.subplots(2, 1, figsize=(10.5, 6.6), sharex=True)
    for ax, yt, ye, yd, lab in (
            (axes[0], np.abs(H_true), np.abs(h_est), np.abs(h_dmrs), "|H|"),
            (axes[1], ph_true, ph_est, ph_dmrs, "arg(H)  (deg)")):
        ax.plot(k, yt, color=C_TRUE, lw=2.6, label="true channel", zorder=1)
        ax.plot(k, ye, color=C_EST, lw=1.4, zorder=2,
                label="estimate used by the equaliser (comb-interpolated)")
        ax.scatter(dk, yd, s=16, color="#c92a2a", zorder=3,
                   label="raw chEst output, on the DMRS REs (VSPA)")
        ax.set_ylabel(lab)
        _style(ax)
    axes[0].legend(frameon=False, fontsize=9, ncol=1)
    axes[0].set_title(f"Channel estimate vs truth — SSB symbol 1, PBCH DMRS, "
                      f"SNR {snr_db:.0f} dB   (NMSE {res['chan_nmse']:.1f} dB)",
                      color=FG)
    axes[1].set_xlabel("SSB subcarrier  k")
    fig.tight_layout()
    fig.savefig(path, dpi=140, facecolor=BG, bbox_inches="tight")
    plt.close(fig)


def fig_constellation(res: dict, path: Path, snr_db: float, ds: int) -> None:
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(1, 2, figsize=(10.6, 5.3))
    lim = max(2.2, 1.15 * float(np.abs(res["before"]).max()))

    for ax, v, c, t in (
            (axes[0], res["before"], C_BEFORE,
             f"BEFORE equalisation\nEVM {100*res['evm_before']:.1f} %"),
            (axes[1], res["after"], C_AFTER,
             f"AFTER equalisation (real chEst + rcpv + vmult)\n"
             f"EVM {100*res['evm_after']:.1f} %")):
        ax.scatter(v.real, v.imag, s=9, c=c, alpha=0.55, linewidths=0)
        ideal = np.array([1 + 1j, 1 - 1j, -1 + 1j, -1 - 1j]) / np.sqrt(2)
        ax.scatter(ideal.real, ideal.imag, s=90, facecolors="none",
                   edgecolors="#e03131", linewidths=1.6, zorder=5)
        ax.set_xlim(-lim, lim)
        ax.set_ylim(-lim, lim)
        ax.set_aspect("equal")
        ax.axhline(0, color="#ced4da", lw=0.8, zorder=0)
        ax.axvline(0, color="#ced4da", lw=0.8, zorder=0)
        ax.set_title(t, color=FG, fontsize=11)
        ax.set_xlabel("I")
        _style(ax)
    axes[0].set_ylabel("Q")
    fig.suptitle(f"PBCH ({NR_PBCH_DATA_NRE} REs) through a {ds}-sample multipath "
                 f"channel at {snr_db:.0f} dB — red circles are the ideal QPSK points",
                 color=FG, fontsize=12)
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    fig.savefig(path, dpi=140, facecolor=BG, bbox_inches="tight")
    plt.close(fig)


def fig_evm_vs_snr(rows: list[dict], path: Path, ds: int) -> None:
    import matplotlib.pyplot as plt

    snr = np.array([r["snr_db"] for r in rows], dtype=float)
    fig, axes = plt.subplots(1, 2, figsize=(11.4, 4.6))

    ax = axes[0]
    ax.semilogy(snr, 100 * np.array([r["evm_before"] for r in rows]), "s--",
                color=C_BEFORE, lw=2, ms=7, mfc="white", mew=1.8,
                label="before equalisation")
    ax.semilogy(snr, 100 * np.array([r["evm_after"] for r in rows]), "o-",
                color=C_AFTER, lw=2, ms=7, mfc="white", mew=1.8,
                label="after equalisation (VSPA kernels)")
    ax.semilogy(snr, 100 * np.array([r["evm_ideal"] for r in rows]), "-",
                color=C_TRUE, lw=2, label="ZF with perfect channel knowledge")
    ax.set_xlabel("SNR per SSB resource element  (dB)")
    ax.set_ylabel("PBCH EVM  (%)")
    ax.set_title("EVM vs SNR", color=FG)
    ax.legend(frameon=False, fontsize=9)
    _style(ax)

    ax = axes[1]
    ax.plot(snr, [r["chan_nmse"] for r in rows], "o-", color=C_EST, lw=2, ms=7,
            mfc="white", mew=1.8, label="chEst + comb interp")
    ax.plot(snr, -snr, "-", color=C_TRUE, lw=2, label="-SNR (LS on the pilot RE)")
    ax.set_xlabel("SNR per SSB resource element  (dB)")
    ax.set_ylabel("channel-estimate NMSE  (dB)")
    ax.set_title("Channel-estimate error vs SNR", color=FG)
    ax.legend(frameon=False, fontsize=9)
    _style(ax)

    # Be exact about what ran where. "Every DSP stage on the real kernels" was the
    # earlier wording and it was NOT true: the comb interpolation of the channel
    # estimate is a host-side model (NXP's own is commented out in the library —
    # see comb_interp). A figure that overstates its provenance is how a caveat
    # gets lost between the document and the slide deck.
    fig.suptitle(f"SSB demodulation through a {ds}-sample multipath channel\n"
                 f"FFT · chEst · 1/H · equalise — real VSPA kernels;   "
                 f"comb interpolation of the estimate — host-side model",
                 color=FG, fontsize=11)
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    fig.savefig(path, dpi=140, facecolor=BG, bbox_inches="tight")
    plt.close(fig)


# ── main ─────────────────────────────────────────────────────────────────────
def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--snr", type=float, default=20.0,
                    help="SNR per SSB resource element, dB (default 20)")
    ap.add_argument("--delay-spread", type=int, default=8, metavar="SAMPLES",
                    help="max excess delay of the multipath channel, in samples "
                         f"(0 = flat; must be < CP = {CP}; default 8)")
    ap.add_argument("--cfo", type=float, default=0.0, metavar="HZ",
                    help="carrier frequency offset in Hz (default 0; SCS is "
                         f"{SCS/1e3:.0f} kHz)")
    ap.add_argument("--sweep", action="store_true",
                    help="sweep the SNR and print the table + EVM-vs-SNR figure")
    ap.add_argument("--seed", type=int, default=20260714)
    ap.add_argument("--no-figures", action="store_true")
    a = ap.parse_args()

    t0 = time.time()
    rng = np.random.default_rng(a.seed)

    ssb = SSB(NID1, NID2, rng)
    taps = make_channel(a.delay_spread, np.random.default_rng(a.seed + 1))
    H = channel_freq_response(taps)
    H_ssb = H[re_to_bin(np.arange(SSB_RE0, SSB_RE0 + NR_SSB_NRE))]

    print(f"SSB co-simulation — NCellID {ssb.cell_id} "
          f"(N_id_1 {ssb.nid1}, N_id_2 {ssb.nid2})")
    print(f"  carrier      {NSC} REs, {N}-pt FFT, CP {CP}, SCS {SCS/1e3:.0f} kHz, "
          f"fs {FS/1e6:.2f} MHz")
    print(f"  SSB          {NR_SSB_NRE} REs x {NR_SSB_NSYM} symbols at carrier RE "
          f"{SSB_RE0}; {NR_PBCH_DMRS_NRE} DMRS + {NR_PBCH_DATA_NRE} PBCH REs "
          f"({len(ssb.bits)} bits)")
    print(f"  channel      {len(taps)} taps, max excess delay {a.delay_spread} "
          f"samples, |H| over the SSB band {np.abs(H_ssb).min():.2f} .. "
          f"{np.abs(H_ssb).max():.2f}  ({db(float(np.abs(H_ssb).max()/np.abs(H_ssb).min())):.1f} dB "
          f"of frequency selectivity)")
    if a.cfo:
        print(f"  CFO          {a.cfo:.0f} Hz = {a.cfo/SCS*100:.2f} % of the SCS")

    chain = Chain()

    print("\nTX: 4 SSB symbols through the real ifftDIF1024_sc128 ...", flush=True)
    td_syms = transmit_ssb(chain, ssb)
    print(f"  time-domain rms {rms(td_syms[1]):.0f}, peak "
          f"{np.abs(np.concatenate(td_syms)).max():.0f} of 32767")

    gain, floor = calibrate(chain, ssb, td_syms)
    print(f"\nCalibration (TX -> RX, no channel, no noise, real kernels):")
    print(f"  chain gain   {abs(gain):.5f} @ {np.rad2deg(np.angle(gain)):+.2f} deg "
          f"— nominal 1/16 = {1/16:.5f}, so the truncating fixed-point FFT pair "
          f"runs {100*(1-abs(gain)*16):.1f} % low")
    print(f"  EVM floor    {100*floor:.3f} %  ({db(floor):.1f} dB) — the chain's "
          f"own noise, before any channel")

    fails: list[str] = []

    def check(name: str, ok: bool, detail: str = "") -> None:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}" + (f"  {detail}" if detail else ""))
        if not ok:
            fails.append(name)

    # ── controls: prove the kernel is really seeing our inputs ────────────────
    # The #1 trap of this harness: inputs are written at a breakpoint on main(),
    # after crt0 zeroes .bss. Get that wrong and the kernel computes on zeros and
    # returns a clean, plausible, WRONG answer. So before believing any number,
    # prove the outputs actually track the inputs.
    print("\nControls (the .bss trap: a kernel computing on zeros still 'works')")
    win_probe = chain.receive(stream_with_cp(td_syms)[N + 2 * CP:2 * N + 2 * CP]
                             )[WIN0:WIN0 + NFFT_SSB]
    ones = np.ones(NFFT_SSB, dtype=complex)
    p = chain.demod.compute({"rx1": win_probe, "rx2": win_probe, "rx3": win_probe,
                             "dmrs1": ones, "dmrs3": ones,
                             "h1_in": ones, "h2_in": ones, "h3_in": ones})
    chain.calls += 1
    d0 = float(np.abs(p["h1_raw"] - win_probe).max())
    check("chEst with a 1+0j pilot returns the RX symbol itself", d0 == 0.0,
          f"max|h_raw - rx| = {d0:g} LSB  (so both ports are live)")

    p2 = chain.demod.compute({"rx1": -win_probe, "rx2": win_probe, "rx3": win_probe,
                              "dmrs1": ones, "dmrs3": ones,
                              "h1_in": ones, "h2_in": ones, "h3_in": ones})
    chain.calls += 1
    moved = float(np.abs(p2["h1_raw"] - p["h1_raw"]).max())
    check("negating the RX symbol moves the estimate", moved > 100,
          f"max|delta| = {moved:.0f} LSB  (not a stale buffer)")

    # ...and the same for the OTHER half of the chain. rcpv computes 1/h_in, so
    # doubling h_in must exactly halve the equalised output. If h_in never reached
    # DMEM, eq would not move — and eq would still look like a perfectly plausible
    # constellation, which is the whole danger.
    h_probe = to_cfixed16(4000.0 * np.exp(1j * np.linspace(0, 3, NFFT_SSB)))
    e1 = chain.demod.compute({"rx1": win_probe, "rx2": win_probe, "rx3": win_probe,
                              "dmrs1": ones, "dmrs3": ones, "h1_in": h_probe,
                              "h2_in": h_probe, "h3_in": h_probe})["eq1"]
    e2 = chain.demod.compute({"rx1": win_probe, "rx2": win_probe, "rx3": win_probe,
                              "dmrs1": ones, "dmrs3": ones, "h1_in": 2 * h_probe,
                              "h2_in": h_probe, "h3_in": h_probe})["eq1"]
    chain.calls += 2
    ratio = float(np.abs(rms(e1) / max(rms(e2), 1e-30)))
    check("doubling h_in halves the equalised output (rcpv+vmult see it)",
          abs(ratio - 2.0) < 0.02, f"|eq1|/|eq1'| = {ratio:.4f}, expected 2.0")

    # ── the main point ────────────────────────────────────────────────────────
    print(f"\nLink at SNR = {a.snr:.0f} dB, delay spread {a.delay_spread} samples"
          + (f", CFO {a.cfo:.0f} Hz" if a.cfo else ""), flush=True)
    res = run_point(chain, ssb, td_syms, taps, a.snr, a.cfo, gain,
                    np.random.default_rng(a.seed + 2), rx_all=True)
    print(f"  SNR asked {a.snr:5.1f} dB   measured off the grid "
          f"{res['snr_meas']:5.1f} dB")
    print(f"  EVM   before {100*res['evm_before']:7.2f} %    "
          f"after {100*res['evm_after']:6.2f} %    "
          f"perfect-CSI ZF {100*res['evm_ideal']:6.2f} %")
    print(f"  channel NMSE {res['chan_nmse']:7.1f} dB")
    print(f"  PBCH  {res['n_err']} bit errors in {res['n_bits']} "
          f"(BER {res['ber']:.2e}; perfect-CSI ZF would make {res['ber_ideal']:.2e})")
    if res["n_clip"]:
        print(f"  !! {res['n_clip']} time samples clipped at the 15-bit SM16 limit")

    check("SNR calibration: measured == requested (+/- 1.5 dB)",
          abs(res["snr_meas"] - a.snr) < 1.5,
          f"|{res['snr_meas']:.1f} - {a.snr:.0f}| = {abs(res['snr_meas']-a.snr):.2f} dB")

    # What "equalisation works" means depends on whether there is anything TO
    # equalise. On a flat channel the equaliser cannot improve the EVM — there is
    # no distortion to undo — and it can only add its own estimation noise. Demanding
    # an improvement there would be demanding a result physics does not offer, and
    # would only ever be met by a bug. So the flat case is held to the opposite
    # standard, and it doubles as the control that proves the improvement seen on a
    # selective channel really does come from inverting the channel.
    sel = db(float(np.abs(H_ssb).max() / np.abs(H_ssb).min()))
    gain_db = db(res["evm_after"] / res["evm_before"])
    if sel > 3.0:
        check(f"equalisation improves EVM by > 6 dB (channel is {sel:.0f} dB selective)",
              gain_db < -6.0,
              f"{100*res['evm_before']:.1f} % -> {100*res['evm_after']:.2f} % "
              f"({gain_db:+.1f} dB)")
    else:
        check("flat channel: equalisation is a near no-op, adding only estimator noise",
              res["evm_after"] < 1.5 * res["evm_before"],
              f"{100*res['evm_before']:.2f} % -> {100*res['evm_after']:.2f} % "
              f"({gain_db:+.1f} dB — nothing to undo)")

    check("EVM within 2.5x of ZF with perfect channel knowledge",
          res["evm_after"] < 2.5 * res["evm_ideal"],
          f"{res['evm_after']/res['evm_ideal']:.2f} x")
    check("no time samples clipped", res["n_clip"] == 0)

    rows: list[dict] = []
    if a.sweep:
        print(f"\nSNR sweep — {NR_PBCH_DATA_NRE} PBCH REs ({len(ssb.bits)} uncoded "
              f"bits) per point\n")
        print(f"  {'SNR':>4} {'meas':>5} | {'EVM before':>10} {'EVM after':>9} "
              f"{'ideal ZF':>8} | {'chan NMSE':>9} | {'PBCH BER':>9} {'err':>4} "
              f"{'ideal BER':>9}")
        print("  " + "-" * 82)
        for snr in SNR_SWEEP:
            r = (res if snr == a.snr else
                 run_point(chain, ssb, td_syms, taps, float(snr), a.cfo, gain,
                           np.random.default_rng(a.seed + 100 + snr)))
            rows.append(r)
            print(f"  {snr:4.0f} {r['snr_meas']:5.1f} | "
                  f"{100*r['evm_before']:9.1f}% {100*r['evm_after']:8.2f}% "
                  f"{100*r['evm_ideal']:7.2f}% | {r['chan_nmse']:8.1f} dB | "
                  f"{r['ber']:9.2e} {r['n_err']:4d} {r['ber_ideal']:9.2e}",
                  flush=True)

        # The PBCH here is UNCODED. Real PBCH carries rate-1/3 polar coding, which
        # is there precisely to survive the deep fades a ZF equaliser amplifies —
        # so "error-free at 20 dB" is the wrong bar and would only be met by
        # picking a flatter channel. The right bar is the one physics sets: the
        # BER a ZF receiver that KNEW the channel exactly would itself make.
        cmp_rows = [r for r in rows if r["ber_ideal"] * r["n_bits"] >= 3.0]
        ratios = [(r["snr_db"], r["ber"] / r["ber_ideal"]) for r in cmp_rows]
        check("uncoded PBCH BER tracks the perfect-CSI ZF bound (< 3x)",
              all(x < 3.0 for _, x in ratios),
              ", ".join(f"{s:.0f} dB: {x:.2f}x" for s, x in ratios))
        check("PBCH decodes error-free at 30 dB", rows[-1]["n_err"] == 0,
              f"{rows[-1]['n_err']} errors in {rows[-1]['n_bits']} bits")
        check("channel NMSE < -25 dB at 30 dB SNR",
              rows[-1]["chan_nmse"] < -25.0, f"{rows[-1]['chan_nmse']:.1f} dB")
        mono = all(rows[i + 1]["chan_nmse"] < rows[i]["chan_nmse"] + 0.5
                   for i in range(len(rows) - 1))
        check("channel NMSE falls monotonically with SNR", mono,
              " -> ".join(f"{r['chan_nmse']:.0f}" for r in rows) + " dB")

    # ── figures ───────────────────────────────────────────────────────────────
    if not a.no_figures:
        import matplotlib
        matplotlib.use("Agg")
        FIGDIR.mkdir(parents=True, exist_ok=True)
        fig_grid(ssb, FIGDIR / "ssb_resource_grid.png")
        fig_channel(res, ssb, FIGDIR / "ssb_channel_estimate.png", a.snr)
        fig_constellation(res, FIGDIR / "ssb_constellation.png", a.snr,
                          a.delay_spread)
        wrote = ["ssb_resource_grid.png", "ssb_channel_estimate.png",
                 "ssb_constellation.png"]
        if rows:
            fig_evm_vs_snr(rows, FIGDIR / "ssb_evm_vs_snr.png", a.delay_spread)
            wrote.append("ssb_evm_vs_snr.png")
        print("\nwrote " + ", ".join(f"docs/figures/{w}" for w in wrote))

    print(f"\n{chain.calls} kernel calls on the ISS, {time.time()-t0:.0f} s")
    print(f"\n{'FAIL: ' + ', '.join(fails) if fails else 'PASS — all checks green'}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
