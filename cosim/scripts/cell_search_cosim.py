#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""cell_search_cosim.py — 5G NR cell search, end to end, on the real VSPA kernels.

Answers one question: GIVEN A RAW IQ STREAM, WHERE IS THE CELL AND WHAT IS ITS ID?

    1. take an IQ stream                       (a real NXP FR1 vector; see below)
    2. optionally add AWGN and a timing offset
    3. PSS search  -> N_id_2 and the sample index of the peak
    4. SSS search  -> N_id_1
    5. N_id = 3*N_id_1 + N_id_2, compared against ground truth

It is a MEASUREMENT, not a demo:

    --snr DB            run one detection at a given SNR
    --sweep             detection probability vs SNR, N trials per point
    --timing-offset K   shift the stream by K samples and see if the peak follows

Run (the native ISS needs its shared library on the path):

    LD_LIBRARY_PATH=$PWD/environment/ccssim2/bin \
        python3 cosim/scripts/cell_search_cosim.py --snr 0
    LD_LIBRARY_PATH=$PWD/environment/ccssim2/bin \
        python3 cosim/scripts/cell_search_cosim.py --sweep


WHERE THE SIGNAL COMES FROM — A REAL BURST, NOT A SYNTHETIC ONE
===============================================================
The brief allowed synthesising an SSB if no vector carried one. That fallback is
NOT used, and it is worth saying why. Eight of NXP's FR1 vectors carry a genuine,
labelled 5G NR SSB, and all eight were verified before any of this was written: a
phase-ramp-immune differential correlation recovers both N_id_2 and N_id_1 exactly
as the filename encodes them (corr 0.93-0.99). They also carry deliberately
injected CFO (300..1000 Hz) and timing offset (toi -40..80) — which is precisely
what makes them a test rather than a demonstration. A synthesised SSB would have
been strictly weaker.

The vectors are FREQUENCY-domain RE grids, so this script OFDM-modulates the grid
back to a time-domain IQ stream (1024-point IFFT + cyclic prefix) to get the raw
stream the search consumes. That reconstruction is numpy, and it is the STIMULUS,
not the thing under test. Everything downstream of it — the FFT, the correlation,
the channel estimate, the equaliser, the SSS detector — is the real assembly out
of firmware/lib/lib_DFE_Ref_vspa2.elb. The output says which source was used.


WHAT IS A REAL KERNEL AND WHAT IS NOT
=====================================
Real assembly (lib_DFE_Ref_vspa2.elb), via cosim/kernels/compute_*.c:

    fftDIF1024_sc128_hfx_hfx_vspa2   window FFT               (compute_ssb_fft.c)
    chEst                            the conjugate multiply   (compute_ssb_corr.c)
    ifftDIF1024_sc128_hfx_hfx_vspa2  back to the lag domain   (compute_ssb_corr.c)
    chEst / csp_chf_rcpv_asm / chp_chf_csp_vmult_asm
                                     LS estimate + 1/H + equalise (compute_ssb_chest.c)
    sssdet_targeted_hpgemv_rc_v1     SSS detection, swept over all 336 N_id_1
    sssConvert                                                (compute_sss_det.c)

There is NO PSS detector in the library — a full symbol dump of all 27 archive
members has zero hits for pss/xcorr/corr/peak/search, and the shipping firmware
does cell TRACKING (it reads N_id_2 from a host config word), not cell SEARCH. So
the PSS correlator is BUILT here — but built out of real kernels, via the
cross-correlation theorem:

    corr[d] = IDFT( DFT(rx_window) . conj(PSS_spectrum) ) [d]

which is exactly fftDIF1024 -> chEst -> ifftDIF1024. See compute_ssb_corr.c.

On the host, in Python, only: the OFDM modulation of the stimulus; the AWGN; the
bit-reversal permutation between the two transforms (a reindex, no arithmetic —
the firmware does it with the IPPU); |corr|^2; and the argmax. No detector, no
correlator, no channel estimator.


THE ONE TRAP THAT MATTERS
=========================
Kernel buffers are .bss globals and crt0 zeroes .bss AFTER the image is loaded.
NativeKernelRunner writes inputs at a breakpoint on main() for that reason. A
kernel fed zeros still runs and still returns a clean, plausible, WRONG answer —
so every number here is checked against a ground truth that comes from outside
the simulator (the vector's filename).
"""
from __future__ import annotations

import argparse
import os
import re
import sys
import time
from dataclasses import dataclass

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "cosim"))

from dfe.kernel_runner import NativeKernelRunner, Port          # noqa: E402
from fr1fr2_vectors import load_fd_grid                          # noqa: E402
from vspa_model.nr5g import (                                    # noqa: E402
    nr_pss_sequence, nr_sss_sequence, nr_ssb_pilot_cfloat16,
    nr_ssb_sss_ref_cfloat16, pack_sss_d0, pack_sss_d1,
)

BUILD = os.path.join(ROOT, "cosim/kernels/build")
VECDIR = os.path.join(ROOT, "nxp/fr1_fr2_test_tool/test_vectors")
FIGDIR = os.path.join(ROOT, "docs/figures")

NFFT = 1024          # OFDM FFT: 1024 x 15 kHz = 15.36 MHz, 1024 x 30 kHz = 30.72 MHz
CP = 72              # normal cyclic prefix at a 1024-point FFT
SYMLEN = NFFT + CP
CP_BACKOFF = 16      # start the SSB analysis window this many samples EARLY, i.e.
                     # inside the CP. See sss_search — a late window is ISI and is
                     # unrecoverable, an early one is just a phase ramp that the ZF
                     # weights cancel. ~CP/4.
STREAM_MAX = 16384   # must match compute_ssb_fft.c
NWIN_MAX = 24        # must match compute_ssb_fft.c / compute_ssb_corr.c

NSSB = 256           # NR_SSB_NFFT — the SSB analysis window
KRE = 8              # NR_SSB_KRE: the window starts 8 REs before SSB RE 0
OFS = 64             # SLOTTED_PSS_SSS_OFST = KRE(8) + NR_PSS_KRE(56)
NRE = 127            # PSS / SSS resource elements
N_ID2 = 3
N_ID1 = 336

# ATAN2_COEFF_BUFF: 4 coefficients, each DUPLICATED for the AU's S2i1r1i1r1 mode.
# From vspa-lib/atan/src/atan_coeff.txt; the firmware DMAs this in from the host.
ATAN2_COEFF = np.array([0xBC54B5E1, 0xBC54B5E1, 0x3D422AF3, 0x3D422AF3,
                        0xBDD21235, 0xBDD21235, 0x3EA2DCD7, 0x3EA2DCD7],
                       dtype=np.uint32)

# The detector's tuning constants, exactly as DFE_func_rx.c sets them.
THRE_SSS = 0.05
SCALING_SSS = 0.0310058593750     # = 127 * (1/8)^4, undoing ld.qam's 1/8

DEFAULT_VECTOR = ("NR-FR1-TM3.3_10MHz_15kHz_SSB_l_8_id1_200_id2_1"
                  "_cfo_800_toi_80_RE_offset_300_FDD_fd.bin")


# ===========================================================================
# Bit reversal — a pure index permutation, done on the host.
#
# Both library transforms are decimation-in-frequency: natural order in,
# bit-reversed order out. Chaining two of them needs a reversal in between. On
# the VCPU that is a 1024-iteration scalar loop, and the ISS costs ~50 us per
# phase, so it would cost more than the FFT it serves. The library's bitRev_*
# kernels are IPPU microcode, and the IPPU only advances when explicitly stepped.
# A reindex has no arithmetic in it, so it belongs on the host. (main_test.c does
# the same thing with a software sw_bitrev() and says the IPPU does it for real.)
# ===========================================================================
def _bitrev_index(n: int) -> np.ndarray:
    bits = int(np.log2(n))
    i = np.arange(n)
    r = np.zeros(n, dtype=np.int64)
    for b in range(bits):
        r |= ((i >> b) & 1) << (bits - 1 - b)
    return r


BR = _bitrev_index(NFFT)


def f2b(x) -> int:
    return int(np.float32(x).view(np.uint32))


def b2f(w) -> np.ndarray:
    return np.asarray(w, dtype=np.uint32).view(np.float32)


def pack_u16(a) -> np.ndarray:
    """uint16[] -> uint32 words, low halfword first (the DMEM convention)."""
    a = np.asarray(a, dtype=np.uint16)
    return a[0::2].astype(np.uint32) | (a[1::2].astype(np.uint32) << 16)


# ===========================================================================
# The stimulus: a real NXP vector, OFDM-modulated back to an IQ stream
# ===========================================================================
@dataclass
class Truth:
    """Ground truth, from the vector's filename — i.e. from outside the simulator."""
    name: str
    scs_khz: int
    bw_mhz: int
    pss_sym: int      # grid symbol carrying PSS (filename's l is 1-based)
    sss_sym: int      # = pss_sym + 2
    id1: int
    id2: int
    cfo_hz: int
    toi: int
    re_offset: int    # grid RE index of SSB RE 0

    @property
    def n_id(self) -> int:
        return 3 * self.id1 + self.id2      # NR_CELLID(id1,id2), 38.211


def parse_vector(name: str) -> Truth:
    m = re.search(r"_(\d+)MHz_(\d+)kHz", name)
    bw, scs = int(m.group(1)), int(m.group(2))
    m = re.search(r"_l_(\d+)_id1_(\d+)_id2_(\d+)_cfo_(-?\d+)_toi_(-?\d+)"
                  r"_RE_offset_(\d+)", name)
    if not m:
        raise ValueError(f"{name}: not a labelled SSB vector (no id1/id2 in the name)")
    l, id1, id2, cfo, toi, reo = (int(x) for x in m.groups())
    return Truth(name=name, scs_khz=scs, bw_mhz=bw,
                 pss_sym=l - 1, sss_sym=l + 1,     # l is 1-based; SSS is PSS+2
                 id1=id1, id2=id2, cfo_hz=cfo, toi=toi, re_offset=reo)


class Stimulus:
    """A real FR1 vector, turned into a time-domain IQ stream.

    The grid is frequency domain (post-FFT), which is what the firmware's cell
    tracking consumes. Cell SEARCH consumes a raw stream, so the grid is
    OFDM-modulated: map each symbol's REs onto the 1024-point FFT grid, IFFT,
    prepend the cyclic prefix, concatenate.

    Subcarrier mapping is NXP's own, from main_test.c:
        buf[0..311]    = sc[312..623]   positive frequencies, DC at bin 0
        buf[712..1023] = sc[0..311]     negative frequencies
    i.e. grid RE i -> FFT bin (i - n_sc/2) mod 1024.
    """

    def __init__(self, truth: Truth, n_symbols: int = 5, amp_rms: float = 5000.0):
        self.t = truth
        path = os.path.join(VECDIR, truth.name)
        if not os.path.exists(path):
            raise FileNotFoundError(
                f"{path}\nFetch the vectors first: cosim/scripts/fetch_fr1fr2.sh")
        self.grid = load_fd_grid(path, truth.scs_khz, truth.bw_mhz)
        self.n_sc = self.grid.n_sc

        # Include one symbol of context before PSS and after SSS, so the search
        # has somewhere wrong to go.
        self.sym0 = max(0, truth.pss_sym - 1)
        self.n_symbols = n_symbols
        self.fs_hz = NFFT * truth.scs_khz * 1000

        syms = []
        for m in range(self.sym0, self.sym0 + n_symbols):
            td = self._modulate(m)
            syms.append(np.concatenate([td[-CP:], td]))     # CP + body
        clean = np.concatenate(syms)

        # Scale into the cfixed16 integer range. OFDM PAPR is ~10-12 dB, so an
        # RMS of 5000 leaves headroom to 32767 for the peaks.
        rms = np.sqrt(np.mean(np.abs(clean) ** 2))
        self.gain = amp_rms / rms
        self.clean = clean * self.gain
        self.sig_power = float(np.mean(np.abs(self.clean) ** 2))

        if len(self.clean) > STREAM_MAX:
            raise ValueError(f"stream {len(self.clean)} > STREAM_MAX {STREAM_MAX}; "
                             f"reduce --symbols")

    def _modulate(self, sym_idx: int) -> np.ndarray:
        re_grid = self.grid.symbol_iq(sym_idx)[:self.n_sc]
        X = np.zeros(NFFT, dtype=np.complex128)
        k = (np.arange(self.n_sc) - self.n_sc // 2) % NFFT
        X[k] = re_grid
        return np.fft.ifft(X) * NFFT

    # --- geometry -------------------------------------------------------
    def body_start(self, grid_sym: int) -> int:
        """Sample index where that grid symbol's BODY (past its CP) begins."""
        return (grid_sym - self.sym0) * SYMLEN + CP

    @property
    def pss_start(self) -> int:
        return self.body_start(self.t.pss_sym)

    def bins_for_ssb_window(self) -> np.ndarray:
        """FFT bins of the 256-RE SSB analysis window.

        The firmware takes the window 8 REs before SSB RE 0 — [8 guard][240 SSB]
        [8 guard] = 256 — which is why NR_SSB_KRE is 8.
        """
        res = self.t.re_offset - KRE + np.arange(NSSB)
        return (res - self.n_sc // 2) % NFFT

    def pss_ref_spectra(self) -> np.ndarray:
        """The three PSS matched-filter references, on the 1024-point FFT grid.

        ZERO off the PSS subcarriers — that is what makes this a matched filter.
        (chEst's *pilot* in the gap-12 chain pads with 1+0j instead, because rcpv
        then computes 1/H. Same kernel, different reference, different job.)
        """
        refs = np.zeros((N_ID2, NFFT), dtype=np.complex128)
        res = self.t.re_offset + 56 + np.arange(NRE)        # PSS REs in the grid
        bins = (res - self.n_sc // 2) % NFFT
        for h in range(N_ID2):
            refs[h, bins] = nr_pss_sequence(h)
        return refs

    # --- impairments ----------------------------------------------------
    def realise(self, snr_db=None, timing_offset=0, rng=None) -> np.ndarray:
        """The stream as the receiver sees it: shifted, noisy, quantised."""
        x = self.clean
        if timing_offset:
            # Delay by K samples (K>0 pushes the cell later in the stream).
            x = np.roll(x, timing_offset)
            if timing_offset > 0:
                x[:timing_offset] = 0
            else:
                x[timing_offset:] = 0
        if snr_db is not None:
            rng = rng or np.random.default_rng()
            npow = self.sig_power / (10.0 ** (snr_db / 10.0))
            n = (rng.normal(size=x.size) + 1j * rng.normal(size=x.size))
            x = x + n * np.sqrt(npow / 2.0)
        # cfixed16 is a 15-bit magnitude plus a sign — saturate, do not wrap.
        re = np.clip(np.round(x.real), -32767, 32767)
        im = np.clip(np.round(x.imag), -32767, 32767)
        out = np.zeros(STREAM_MAX, dtype=np.complex128)
        out[:x.size] = re + 1j * im
        return out


# ===========================================================================
# The kernels
# ===========================================================================
class Kernels:
    """Lazily-built runners for the five ELFs this pipeline uses."""

    def __init__(self):
        self._fft = None
        self._corr = None
        self._chest = None
        self._sss = None
        self.phases = 0

    def _elf(self, name):
        p = os.path.join(BUILD, f"compute_{name}_vspa2_16au.eld")
        if not os.path.exists(p):
            raise FileNotFoundError(
                f"kernel not built: {p}\nRun: make -C cosim/kernels")
        return p

    @property
    def fft(self):
        if self._fft is None:
            self._fft = NativeKernelRunner(
                self._elf("ssb_fft"),
                inputs=[Port("stream", "stream", STREAM_MAX, "sm16_complex"),
                        Port("ctrl", "ctrl", 4, "u32")],
                outputs=[Port("fd", "fd_out", NWIN_MAX * NFFT, "sm16_complex")])
        return self._fft

    @property
    def corr(self):
        if self._corr is None:
            self._corr = NativeKernelRunner(
                self._elf("ssb_corr"),
                inputs=[Port("fd_in", "fd_in", NWIN_MAX * NFFT, "sm16_complex"),
                        Port("ref", "ref", N_ID2 * NFFT, "chf_complex"),
                        Port("ctrl", "ctrl", 4, "u32")],
                outputs=[Port("corr", "corr_out", NWIN_MAX * N_ID2 * NFFT,
                              "sm16_complex")])
        return self._corr

    @property
    def chest(self):
        if self._chest is None:
            self._chest = NativeKernelRunner(
                self._elf("ssb_chest"),
                inputs=[Port("pss_in", "pss_in", NSSB, "sm16_complex"),
                        Port("pss_ref", "pss_ref", NSSB, "chf_complex"),
                        Port("sss_in", "sss_in", NSSB, "sm16_complex"),
                        Port("sss_ref", "sss_ref", NSSB, "chf_complex"),
                        Port("atan2_coeff", "atan2_coeff", 8, "u32")],
                outputs=[Port("pss_csi", "pss_csi", NSSB, "sm16_complex"),
                         Port("pss_eqw", "pss_eqw", NSSB, "csp_complex"),
                         Port("sss_eq", "sss_eq", NSSB, "chf_complex"),
                         Port("sss_csi", "sss_csi", NSSB, "sm16_complex"),
                         Port("msmt", "msmt", 4, "u32"),
                         Port("est", "est", 4, "u32")])
        return self._chest

    @property
    def sss(self):
        if self._sss is None:
            self._sss = NativeKernelRunner(
                self._elf("sss_det"),
                inputs=[Port("sss_eq", "sss_eq", NSSB, "chf_complex"),
                        Port("d0_tab", "d0_tab", 36, "u32"),
                        Port("d1_tab", "d1_tab", 9, "u32"),
                        Port("param", "param", 2, "u32"),
                        Port("ctrl", "ctrl", 4, "u32")],
                outputs=[Port("det_flag", "det_flag", N_ID1, "u32"),
                         Port("seq_sign32", "seq_sign32", N_ID1, "u32"),
                         Port("sss_conv", "sss_conv_out", NSSB, "chf_complex")])
        return self._sss

    # --- operations -------------------------------------------------------
    def fft_windows(self, stream, starts) -> np.ndarray:
        """FFT the given windows of the stream, on the real kernel.

        `starts` need not be evenly spaced — the kernel walks a (win0, step)
        grid, so an arbitrary list is served by calling it once per run of a
        uniform grid. Returns natural-order spectra, shape (len(starts), 1024).
        """
        out = np.zeros((len(starts), NFFT), dtype=np.complex128)
        for i in range(0, len(starts), NWIN_MAX):
            chunk = starts[i:i + NWIN_MAX]
            # Encode an arbitrary list as a grid where possible; otherwise one
            # call per window. Uniform grids are the common case (the scan).
            step = chunk[1] - chunk[0] if len(chunk) > 1 else 0
            uniform = all(chunk[j] - chunk[j - 1] == step
                          for j in range(1, len(chunk)))
            if uniform:
                o = self.fft.compute({
                    "stream": stream,
                    "ctrl": np.array([len(chunk), step, chunk[0], 0],
                                     dtype=np.uint32)})
                self.phases += self.fft.phases
                fd = np.asarray(o["fd"]).reshape(NWIN_MAX, NFFT)[:len(chunk)]
            else:
                fd = np.zeros((len(chunk), NFFT), dtype=np.complex128)
                for j, w in enumerate(chunk):
                    o = self.fft.compute({
                        "stream": stream,
                        "ctrl": np.array([1, 0, w, 0], dtype=np.uint32)})
                    self.phases += self.fft.phases
                    fd[j] = np.asarray(o["fd"]).reshape(NWIN_MAX, NFFT)[0]
            out[i:i + len(chunk)] = fd[:, BR]        # undo the FFT's bit-reversal
        return out

    def correlate(self, fd_nat, refs) -> np.ndarray:
        """corr[w, h, lag], on the real kernels. fd_nat is natural-order."""
        nwin = fd_nat.shape[0]
        out = np.zeros((nwin, N_ID2, NFFT), dtype=np.complex128)
        for i in range(0, nwin, NWIN_MAX):
            chunk = fd_nat[i:i + NWIN_MAX]
            buf = np.zeros((NWIN_MAX, NFFT), dtype=np.complex128)
            buf[:len(chunk)] = chunk
            o = self.corr.compute({
                "fd_in": buf.ravel(),
                "ref": refs.ravel(),
                "ctrl": np.array([len(chunk), 0, 0, 0], dtype=np.uint32)})
            self.phases += self.corr.phases
            c = np.asarray(o["corr"]).reshape(NWIN_MAX, N_ID2, NFFT)
            out[i:i + len(chunk)] = c[:len(chunk)][:, :, BR]   # un-bit-reverse
        return out


# ===========================================================================
# The pipeline
# ===========================================================================
@dataclass
class Detection:
    id2: int
    peak_index: int          # absolute sample index of the PSS correlation peak
    peak: float
    margin: float            # peak / best peak of the other two hypotheses
    profile: np.ndarray      # (3, stream_len) — max over windows, for the figure
    id1: int = -1
    n_id: int = -1
    sss_hits: tuple = ()          # firmware chain: ZF-equalised SSS
    sss_hits_raw: tuple = ()      # same detector, unequalised — isolates the ZF cost
    id1_raw: int = -1


def pss_search(k: Kernels, stim: Stimulus, stream, win_starts, own) -> Detection:
    """Stage 3: correlate against the three N_id_2 hypotheses on the real kernel.

    THE LAG IS CYCLIC, AND THAT IS A TRAP.
    --------------------------------------
    The correlation a window produces is CIRCULAR over its 1024 points, so a lag
    d and a lag d-1024 are the same hypothesis: a symbol starting 18 samples
    BEFORE window W shows up at lag 1006, not at lag -18. Folding that onto the
    sample axis as W+1006 invents a peak a whole symbol away from the cell, with
    exactly the same height as the real one — and the argmax then picks between
    the true peak and its alias by luck. (It did, and the first version of this
    file reported a confident, wrong answer 1463 samples off.)

    The fix is to make the windows TILE the sample axis: window w is responsible
    for candidate symbol starts in [W_w, W_w + step) and looks at those lags
    only. Every candidate is then owned by exactly one window, no lag is ever
    counted twice, and no alias can be constructed. It also bounds the
    misalignment: the owned lags are at most `step` from the window start, so the
    window always holds at least (1024 + CP - step)/1024 of the symbol.
    """
    fd = k.fft_windows(stream, win_starts)                 # real FFT
    corr = k.correlate(fd, stim.pss_ref_spectra())         # real chEst + real IFFT
    mag = np.abs(corr) ** 2                                # host: magnitude

    # THE METRIC IS PEAK-TO-AVERAGE, NOT ABSOLUTE POWER — and it has to be.
    # ------------------------------------------------------------------
    # These vectors inject the SSB about 19 dB BELOW the TM3.3 data that shares
    # the band (measured: PSS REs rms 876, in-band data REs rms 7510). The
    # correlation has only sqrt(127) = 21 dB of processing gain over random data,
    # so the true PSS peak barely clears the correlation FLOOR of an ordinary data
    # symbol: measured, 4.7e4 against 1.3e5. An argmax over absolute |corr|^2
    # across the stream therefore lands on a data symbol — confidently, and
    # wrongly. (It did.)
    #
    # Normalising each window/hypothesis profile by its own mean power fixes it,
    # and is what a real searcher does anyway: the absolute correlation depends on
    # an unknown AGC gain, so only the peak-to-average ratio is meaningful. This
    # is a constant-false-alarm-rate (CFAR) detector. A data symbol's profile is
    # noise-like, so its max/mean over 1024 lags is only ~ln(1024) ~ 7; the PSS
    # symbol's peak is coherent and lands ~130. Clean separation.
    #
    # This is a property of the SIGNAL, not a trick: it is why the metric is
    # dimensionless everywhere below.
    noise = mag.mean(axis=-1, keepdims=True)               # per (window, hypothesis)
    metric = mag / np.maximum(noise, 1e-12)

    L = len(stim.clean)
    prof = np.zeros((N_ID2, L))
    for w, W in enumerate(win_starts):
        lo, hi = own[w]                                    # this window's slice
        d = np.arange(lo - W, hi - W)                      # lags it may claim
        idx = W + d
        keep = (idx >= 0) & (idx < L) & (d >= 0) & (d < NFFT)
        for h in range(N_ID2):
            prof[h, idx[keep]] = metric[w, h][d[keep]]

    flat = int(np.argmax(prof))
    h_hat, p_hat = flat // L, flat % L
    peak = prof[h_hat, p_hat]
    others = [prof[h].max() for h in range(N_ID2) if h != h_hat]
    margin = peak / max(max(others), 1e-30)
    return Detection(id2=h_hat, peak_index=p_hat, peak=float(peak),
                     margin=float(margin), profile=prof)


def _ssb_window(fd_sym, bins):
    """The 256-RE SSB analysis window out of a full 1024-bin spectrum."""
    return fd_sym[bins]


def sss_search(k: Kernels, stim: Stimulus, stream, det: Detection,
               backoff: int = CP_BACKOFF, with_measurements=False):
    """Stages 4-5: equalise the SSB on the real kernels, then sweep all 336 N_id_1.

    Follows DFE_func_rx.c exactly: the channel is estimated on PSS, inverted, and
    the resulting weights are applied to the SSS symbol — cross-symbol, not
    self-equalisation.

    THE ANALYSIS WINDOW MUST SIT INSIDE THE CYCLIC PREFIX.
    -----------------------------------------------------
    Do NOT start the FFT at the detected peak. The peak estimates where the
    symbol BODY begins, and the estimate has a couple of samples of jitter, so
    half the time the window starts LATE — and a late window is not a small
    error, it is a different kind of error:

        early window  -> still entirely inside this symbol (that is what the CP
                         is FOR). It only adds a linear phase ramp across the
                         subcarriers, and chEst folds that ramp into H, so the
                         ZF weights cancel it exactly. Harmless.
        late window   -> straddles the NEXT symbol. Inter-symbol interference.
                         Nothing downstream can undo it.

    The CP gives 72 samples of margin on ONE side only. Measured, holding the
    detection fixed and sliding only this window (vector id1=200 id2=0, whose
    peak lands +11 late):

        window residual   +19    +11     +3     -5    -13    -21    -37    -53
        SSS(ZF) accepts   53,200  --    200    200    200    200    200    200

    i.e. every window inside the CP recovers the true N_id_1, and the two past
    the symbol boundary lose it (+19 accepts a SPURIOUS second id as well —
    ambiguity, not just a miss). This one-sided failure is why the two toi=-40
    vectors used to fail the whole search while three others passed.

    So back the window off into the CP. 16 samples is ~CP/4: comfortably clear
    of the boundary, and far enough from the previous symbol to tolerate delay
    spread. This is what a real receiver does, and it is not a fudge — it is the
    reason the cyclic prefix exists.
    """
    pss_start = max(0, det.peak_index - backoff)
    sss_start = pss_start + 2 * SYMLEN
    if sss_start + NFFT > STREAM_MAX:
        return det                              # detected peak is nonsense

    fd = k.fft_windows(stream, [pss_start, sss_start])     # real FFT
    bins = stim.bins_for_ssb_window()
    pss_in = _ssb_window(fd[0], bins)
    sss_in = _ssb_window(fd[1], bins)

    # ONE gain for both symbols — they are one capture through one AGC. Scaling
    # them independently would put a random relative scale between the channel
    # estimate and the symbol it equalises, and the detector's threshold is not
    # scale-free. (The sc128 FFT leaves the REs small; cfixed16 wants headroom.)
    rms = max(np.sqrt(np.mean(np.abs(pss_in) ** 2)),
              np.sqrt(np.mean(np.abs(sss_in) ** 2)))
    peakv = max(np.abs(pss_in).max(), np.abs(sss_in).max())
    g = min(8000.0 / max(rms, 1e-9), 28000.0 / max(peakv, 1e-9))

    def fit(v):
        return np.round(v.real * g) + 1j * np.round(v.imag * g)

    pss_ref = nr_ssb_pilot_cfloat16(det.id2)
    # sss_ref only feeds the SECOND chEst (and the msmts); sss_eq — the thing the
    # detector needs — depends only on pss_in, pss_ref and sss_in. So a placeholder
    # is fine on the first pass, and we re-run with the truth once N_id_1 is known.
    placeholder = np.ones(NSSB, dtype=np.complex128)

    o = k.chest.compute({"pss_in": fit(pss_in), "pss_ref": pss_ref,
                         "sss_in": fit(sss_in), "sss_ref": placeholder,
                         "atan2_coeff": ATAN2_COEFF})
    k.phases += k.chest.phases
    sss_eq = o["sss_eq"]                        # real: chEst -> rcpv -> vmult

    d0, d1 = pack_u16(pack_sss_d0()), pack_u16(pack_sss_d1())
    param = np.array([f2b(THRE_SSS), f2b(SCALING_SSS)], dtype=np.uint32)

    def detect(px):
        o_ = k.sss.compute({"sss_eq": px, "d0_tab": d0, "d1_tab": d1,
                            "param": param,
                            "ctrl": np.array([det.id2, 0, 0, 0], dtype=np.uint32)})
        k.phases += k.sss.phases
        return tuple(int(h) for h in
                     np.flatnonzero(np.asarray(o_["det_flag"], dtype=np.uint32)))

    # (a) the firmware's chain: equalise the SSS with the PSS-derived 1/H weights.
    det.sss_hits = detect(sss_eq)

    # (b) the SAME real detector, on the UNEQUALISED SSS. A control, not an
    # alternative product: it isolates what the equaliser is actually worth, and
    # the answer has a sign on both sides.
    #
    #   the equaliser EARNS its place: the analysis window is deliberately inside
    #   the CP (see above), which puts a linear phase ramp on every subcarrier.
    #   The ramp is common to the PSS and the SSS symbol, so chEst absorbs it into
    #   H and the ZF weights cancel it exactly. The unequalised detector has no
    #   such luck — it correlates against a real +/-1 sequence, and a ramp of a few
    #   turns across the 127 REs annihilates a coherent correlation.
    #
    #   the equaliser COSTS: rcpv is a ZERO-FORCING inverse (1/H), and H comes
    #   from a PSS that sits ~19 dB below the data sharing its band. At low SNR
    #   the small-|H| bins blow up and the equaliser amplifies the noise it is
    #   meant to clean. The library ships no MMSE reciprocal, so there is no
    #   better weight available.
    #
    # Reporting both is the only honest way to show where the chain's floor is.
    raw = sss_in / (np.abs(sss_in[OFS:OFS + NRE]).mean() + 1e-12)
    det.sss_hits_raw = detect(raw)

    det.id1 = int(det.sss_hits[0]) if len(det.sss_hits) == 1 else -1
    det.n_id = 3 * det.id1 + det.id2 if det.id1 >= 0 else -1
    det.id1_raw = (int(det.sss_hits_raw[0])
                   if len(det.sss_hits_raw) == 1 else -1)

    if with_measurements and det.id1 >= 0:
        o2 = k.chest.compute({
            "pss_in": fit(pss_in), "pss_ref": pss_ref,
            "sss_in": fit(sss_in),
            "sss_ref": nr_ssb_sss_ref_cfloat16(det.id1, det.id2),
            "atan2_coeff": ATAN2_COEFF})
        k.phases += k.chest.phases
        det.msmt = np.asarray(o2["msmt"], dtype=np.uint32).astype(np.int32) / 65536.0
        det.est = np.asarray(o2["est"], dtype=np.uint32).astype(np.int32)
    return det


def scan_windows(stim: Stimulus, step=256):
    """Window grid for the full blind scan, plus what each window is responsible for.

    Returns (starts, own) where own[w] = [lo, hi) is the slice of candidate PSS
    symbol starts that window w — and only window w — may claim. The slices tile
    the searchable part of the stream, which is what kills the cyclic-lag alias
    (see pss_search).
    """
    L = len(stim.clean)
    last = max(0, L - NFFT)                    # a full window must still fit
    starts = list(range(0, last + 1, step)) or [0]
    own = []
    for i, W in enumerate(starts):
        hi = starts[i + 1] if i + 1 < len(starts) else last + 1
        own.append((W, hi))
    return starts, own


# ===========================================================================
# Figures
# ===========================================================================

# ── figure provenance ────────────────────────────────────────────────────────
# A figure gets lifted out of a document and pasted into a slide, where it no
# longer has the text that qualified it.  So the two things a reader could get
# WRONG from these plots are printed ON them:
#
#   1. NXP's library ships NO cell-search kernel.  The shipping firmware does cell
#      TRACKING — DFE_func_rx.c reads N_id_1 / N_id_2 straight from host config.
#      The search below is built HERE, out of the firmware's tracking primitives
#      (the cross-correlation theorem turns it into fft -> chEst -> ifft, all of
#      which are shipped assembly).  It runs on the real kernels; it is not an
#      NXP feature.
#   2. The centre frequency is NOT searched.  re_offset comes from the vector's
#      metadata, i.e. from ground truth.  Timing and N_id are searched blindly.
_PROV_PARAS = [
    "Cell SEARCH — built here on the firmware's TRACKING primitives. NXP ships no PSS "
    "detector; the firmware reads $N_{ID}$ from host config.",
    "Timing and $N_{ID}$ are searched blindly; the centre frequency is NOT searched.",
]


def _provenance(fig):
    """Hang the note BELOW the figure and let savefig(bbox_inches="tight") pull it
    into frame.  Call it AFTER tight_layout().

    Two layouts that do not work, recorded so nobody retries them: anchored to the
    AXES the text runs off the right edge (axes coords are not figure coords), and
    reserving a band with subplots_adjust leaves a dead gap under the x-label.
    Cropping to the artists is tight at any figure size — and these figures come in
    three.
    """
    import textwrap

    ncols = max(70, int(fig.get_size_inches()[0] * 15.5))   # ~15.5 chars/inch at 7pt
    lines = [ln for para in _PROV_PARAS for ln in textwrap.wrap(para, ncols)]
    fig.text(0.008, -0.015, "\n".join(lines), fontsize=7, color="#6b6b6b",
             va="top", ha="left", linespacing=1.5)


def save_figures(stim, det, sweep=None, terr=None, tag="", backoff=CP_BACKOFF):
    """tag: appended to every filename.

    WHY THIS EXISTS. The filenames used to be fixed, so ANY non-default run wrote
    over the tracked figures — and one did. `--backoff 0` (the §8 experiment, where
    the analysis window sits on the peak and the unequalised detector still works)
    silently overwrote cell_search_pdet_vs_snr.png, which the tutorial embeds
    directly beneath its DEFAULT sweep table. The result was a table saying "SSS
    unequalised: 0/10 at every SNR" with a plot underneath showing that same curve
    climbing to 10/10. A tag makes the experiment part of the filename, so a
    comparison run cannot impersonate the main one.
    """
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    os.makedirs(FIGDIR, exist_ok=True)
    sfx = f"_{tag}" if tag else ""
    t = stim.t
    made = []

    # --- 1. the PSS correlation profile --------------------------------
    fig, ax = plt.subplots(figsize=(11, 4.2))
    colors = ["#4C78A8", "#F58518", "#54A24B"]
    for h in range(N_ID2):
        ax.plot(det.profile[h], lw=1.0, color=colors[h], alpha=0.9,
                label=f"$N_{{ID}}^{{(2)}}$ = {h}"
                      + ("  (true)" if h == t.id2 else ""))
    ax.axvline(det.peak_index, color="#B00020", ls="--", lw=1.2,
               label=f"detected peak @ {det.peak_index}")
    ax.plot([det.peak_index], [det.peak], "v", color="#B00020", ms=9)
    ax.set_xlabel("sample index in the IQ stream")
    ax.set_ylabel("peak-to-average metric  " + r"$|\mathrm{corr}|^2 / \overline{|\mathrm{corr}|^2}$")
    ax.set_title(f"PSS correlation profile — {t.name[:52]}…\n"
                 f"real kernels: fftDIF1024 → chEst → ifftDIF1024   "
                 f"(argmax: $N_{{ID}}^{{(2)}}$={det.id2}, margin {det.margin:.1f}×)")
    ax.legend(loc="upper right", fontsize=9)
    ax.margins(x=0.01)
    fig.tight_layout()
    _provenance(fig)
    p = os.path.join(FIGDIR, f"cell_search_pss_profile{sfx}.png")
    fig.savefig(p, dpi=130, bbox_inches="tight")
    plt.close(fig)
    made.append(p)

    # --- 2. detection probability vs SNR -------------------------------
    if sweep:
        snrs = [r["snr"] for r in sweep]
        fig, ax = plt.subplots(figsize=(7.6, 4.6))
        for key, lab, c, mk, ls in (
                ("p_pss", "PSS  ($N_{ID}^{(2)}$ + timing)", "#4C78A8", "o", "-"),
                ("p_sss", "SSS  ($N_{ID}^{(1)}$), ZF-equalised", "#F58518", "s", "-"),
                ("p_raw", "SSS  ($N_{ID}^{(1)}$), unequalised", "#54A24B", "^", "--"),
                ("p_nid", "full cell ID $N_{ID}$ (firmware chain)", "#B00020", "D", "-")):
            if key in sweep[0]:
                ax.plot(snrs, [r[key] for r in sweep], marker=mk, color=c,
                        label=lab, lw=1.8, ls=ls)
        n = sweep[0]["trials"]
        ax.set_xlabel("SNR (dB, full-band, time domain)")
        ax.set_ylabel(f"detection probability ({n} trials/point)")
        ax.set_ylim(-0.05, 1.05)
        ax.grid(alpha=0.3)
        # The window placement goes IN THE TITLE. Two sweeps that differ only in
        # --backoff produce completely different SSS curves (with the window on the
        # peak the unequalised detector works; inside the CP it collapses to 0/10),
        # and an untitled figure cannot be told apart from the other one once it
        # leaves the document. That is not hypothetical — it already happened.
        win = (f"analysis window {backoff} samples into the CP"
               if backoff else "analysis window ON THE PEAK (--backoff 0)")
        ax.set_title("Cell-search detection probability vs SNR\n"
                     f"true cell: $N_{{ID}}$={stim.t.n_id} "
                     f"($N^{{(1)}}$={stim.t.id1}, $N^{{(2)}}$={stim.t.id2})   ·   {win}")
        ax.legend(fontsize=9)
        # Do not draw a fake floor: annotate the points that never failed.
        for r in sweep:
            if r["p_nid"] == 1.0:
                ax.annotate(f"0/{r['trials']}\nfailures", (r["snr"], 1.0),
                            textcoords="offset points", xytext=(0, -30),
                            ha="center", fontsize=7, color="#666")
        fig.tight_layout()
        _provenance(fig)
        p = os.path.join(FIGDIR, f"cell_search_pdet_vs_snr{sfx}.png")
        fig.savefig(p, dpi=130, bbox_inches="tight")
        plt.close(fig)
        made.append(p)

    # --- 3. timing-error histogram -------------------------------------
    if terr is not None and len(terr):
        fig, ax = plt.subplots(figsize=(7.6, 4.2))
        lo, hi = int(np.min(terr)), int(np.max(terr))
        bins = np.arange(lo - 0.5, hi + 1.5, 1.0) if hi - lo < 60 else 40
        ax.hist(terr, bins=bins, color="#4C78A8", edgecolor="white")
        ax.axvline(0, color="#B00020", ls="--", lw=1.2, label="zero error")
        ax.set_xlabel("timing error (samples)   detected peak − true PSS start")
        ax.set_ylabel("trials")
        ax.set_title("PSS timing-estimate error\n"
                     f"{len(terr)} trials, mean {np.mean(terr):+.2f}, "
                     f"std {np.std(terr):.2f} samples")
        ax.legend(fontsize=9)
        fig.tight_layout()
        _provenance(fig)
        p = os.path.join(FIGDIR, f"cell_search_timing_error{sfx}.png")
        fig.savefig(p, dpi=130, bbox_inches="tight")
        plt.close(fig)
        made.append(p)

    return made


# ===========================================================================
# main
# ===========================================================================
def run_once(k, stim, snr_db, timing_offset, win_starts, own, rng,
             backoff=CP_BACKOFF, with_measurements=False):
    stream = stim.realise(snr_db=snr_db, timing_offset=timing_offset, rng=rng)
    det = pss_search(k, stim, stream, win_starts, own)
    det = sss_search(k, stim, stream, det, backoff=backoff,
                     with_measurements=with_measurements)
    return det


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="5G NR cell search (PSS + SSS) on the real VSPA kernels.")
    ap.add_argument("--vector", default=DEFAULT_VECTOR,
                    help="NXP FR1 SSB vector (must carry a labelled SSB)")
    ap.add_argument("--snr", type=float, default=None,
                    help="run one detection at this SNR (dB). Default: no noise.")
    ap.add_argument("--sweep", action="store_true",
                    help="detection probability vs SNR")
    ap.add_argument("--snr-list", default="-5,0,5,10,15,20",
                    help="SNR points for --sweep (dB)")
    ap.add_argument("--trials", type=int, default=10,
                    help="trials per SNR point")
    ap.add_argument("--timing-offset", type=int, default=0,
                    help="shift the stream by K samples")
    ap.add_argument("--symbols", type=int, default=5,
                    help="OFDM symbols in the stream")
    ap.add_argument("--step", type=int, default=256,
                    help="window step of the blind scan (samples)")
    ap.add_argument("--backoff", type=int, default=CP_BACKOFF,
                    help="start the SSB analysis window this many samples EARLY, "
                         "i.e. inside the cyclic prefix (default %(default)s). "
                         "Set 0 to place it on the detected peak and watch the "
                         "late-window ISI failure of sss_search's docstring.")
    ap.add_argument("--figure-tag", default="", metavar="NAME",
                    help="append NAME to the figure filenames. Use it for any run "
                         "that is not the default sweep — otherwise it overwrites "
                         "the tracked figures the tutorial embeds.")
    ap.add_argument("--seed", type=int, default=20260714)
    ap.add_argument("--no-figures", action="store_true")
    a = ap.parse_args(argv)

    t0 = time.time()
    truth = parse_vector(a.vector)
    stim = Stimulus(truth, n_symbols=a.symbols)
    k = Kernels()
    rng = np.random.default_rng(a.seed)

    print("=" * 78)
    print("5G NR CELL SEARCH — real NXP burst, real VSPA kernels")
    print("=" * 78)
    print(f"  IQ SOURCE      REAL NXP FR1 VECTOR (not synthesised)")
    print(f"                 {truth.name}")
    print(f"                 {truth.bw_mhz} MHz / {truth.scs_khz} kHz, "
          f"{stim.n_sc} used SC, fs = {stim.fs_hz/1e6:.2f} MHz")
    print(f"                 grid -> IQ stream: {NFFT}-pt IFFT + {CP}-sample CP, "
          f"{a.symbols} symbols = {len(stim.clean)} samples")
    print(f"                 the vector carries injected CFO {truth.cfo_hz} Hz "
          f"and timing offset {truth.toi}")
    print(f"  GROUND TRUTH   N_id_1 = {truth.id1}, N_id_2 = {truth.id2}"
          f"  ->  N_id = 3*{truth.id1}+{truth.id2} = {truth.n_id}")
    print(f"                 PSS in grid symbol {truth.pss_sym}, "
          f"SSS in {truth.sss_sym}, SSB at RE {truth.re_offset}")
    print(f"                 PSS symbol body starts at stream sample "
          f"{stim.pss_start}")
    print(f"  REAL KERNELS   fftDIF1024_sc128 -> chEst -> ifftDIF1024   (PSS search)")
    print(f"                 chEst -> csp_chf_rcpv_asm -> chp_chf_csp_vmult_asm "
          f"(equalise)")
    print(f"                 sssdet_targeted_hpgemv_rc_v1 x 336 + sssConvert (SSS)")
    print(f"  HOST (python)  OFDM modulation of the stimulus, AWGN, the "
          f"bit-reversal reindex,")
    print(f"                 |corr|^2 and the argmax. No detector, no correlator.")

    # ---- the blind scan, once: where is the cell? ----------------------
    win, own = scan_windows(stim, step=a.step)
    print(f"\n--- BLIND SCAN over the whole stream "
          f"({len(win)} windows, step {a.step}, {NFFT} lags each) ---")
    det = run_once(k, stim, a.snr, a.timing_offset, win, own, rng,
                   backoff=a.backoff, with_measurements=True)

    expect = stim.pss_start + a.timing_offset
    resid = det.peak_index - expect
    # NXP's toi is quoted at a 61.44 MHz reference rate; this stream runs at
    # NFFT*SCS. Measured across 4 distinct toi values and both numerologies, the
    # residual tracks -toi * fs/61.44MHz to within ~3 samples (the peak is
    # integer-quantised, the injected offset is not).
    pred = -truth.toi * stim.fs_hz / 61.44e6
    print(f"  PSS  N_id_2 = {det.id2}   (true {truth.id2})   "
          f"{'OK' if det.id2 == truth.id2 else 'WRONG'}")
    print(f"       peak at sample {det.peak_index}, margin over the other two "
          f"hypotheses {det.margin:.1f}x")
    print(f"       symbol-grid prediction {expect} "
          f"(= body start {stim.pss_start} + offset {a.timing_offset}); "
          f"residual {resid:+d} samples")
    print(f"       -- residual is the vector's own injected timing offset: "
          f"toi={truth.toi} at 61.44 MHz -> {pred:+.1f} samples "
          f"at {stim.fs_hz/1e6:.2f} MHz")
    print(f"  SSS  blind sweep over all 336 N_id_1 on the real detector")
    print(f"       accepted N_id_1 = {list(det.sss_hits)}   (true {truth.id1})")
    print(f"  CELL N_id = {det.n_id}   (true {truth.n_id})   "
          f"{'OK' if det.n_id == truth.n_id else 'WRONG'}")
    if hasattr(det, "msmt"):
        m = det.msmt
        print(f"  GAP-12 (real chEst/rcpv/vmult/sss_msmts on the detected SSB):")
        print(f"       rssi {m[0]:+.2f} dB   rsrp {m[1]:+.2f} dB   "
              f"rsrq {m[2]:+.2f} dB   sinr {m[3]:+.2f} dB")
        print(f"       tim_off_est = {det.est[0]}   fineCFO_rc = {det.est[1]}")

    ok_headline = (det.n_id == truth.n_id)

    # The timing anchor for the sweep's error metric. It MUST come from a
    # noise-free run: anchoring on a noisy detection would measure the estimator
    # against itself and quietly report a smaller error than it earns.
    if a.snr is None:
        p0 = det.peak_index - a.timing_offset
    else:
        clean = stim.realise(timing_offset=a.timing_offset)
        p0 = (pss_search(k, stim, clean, win, own).peak_index - a.timing_offset)

    # ---- the sweep -----------------------------------------------------
    sweep, terr_all = None, None
    if a.sweep:
        snrs = [float(s) for s in a.snr_list.split(",")]
        # One analysis window per trial, locked to the symbol grid: the blind
        # multi-window scan above already characterises the SEARCH RANGE; the
        # sweep characterises SENSITIVITY, and re-running a 19-window scan per
        # trial would cost ~30 s each on the ISS. The detector still searches all
        # 1024 lags and all 3 hypotheses on every trial.
        wstart = [max(0, stim.pss_start - CP)]
        wown = [(wstart[0], wstart[0] + NFFT)]     # one window: all 1024 lags
        print(f"\n--- SWEEP: {len(snrs)} SNR points x {a.trials} trials, "
              f"1 window/trial ({NFFT} lags, 3 hypotheses) ---")
        print(f"    SNR is full-band, time-domain: the PSS occupies only "
              f"{NRE}/{stim.n_sc} subcarriers,")
        print(f"    so its in-band SNR is ~"
              f"{10*np.log10(stim.n_sc/NRE):.1f} dB below the number quoted.\n")
        hdr = (f"    {'SNR':>6}  {'PSS':>7}  {'SSS(ZF)':>8}  {'SSS(raw)':>9}  "
               f"{'N_id':>7}   {'timing err (samples)':>24}   {'N_id':>13}")
        print(hdr)
        print("    " + "-" * (len(hdr) - 4))
        sweep, terr_all = [], []
        for snr in snrs:
            n_pss = n_sss = n_raw = n_nid = 0
            errs = []
            for _ in range(a.trials):
                d = run_once(k, stim, snr, a.timing_offset, wstart, wown, rng,
                             backoff=a.backoff)
                e = d.peak_index - (p0 + a.timing_offset)
                pss_ok = (d.id2 == truth.id2) and abs(e) <= 4
                if pss_ok:
                    n_pss += 1
                    errs.append(e)
                if d.id1 == truth.id1:
                    n_sss += 1
                if d.id1_raw == truth.id1:
                    n_raw += 1
                if d.n_id == truth.n_id:
                    n_nid += 1
            n = a.trials
            me = f"mean {np.mean(errs):+6.2f}  std {np.std(errs):5.2f}" if errs \
                 else "        (no PSS lock)"
            row = {"snr": snr, "trials": n,
                   "p_pss": n_pss / n, "p_sss": n_sss / n, "p_raw": n_raw / n,
                   "p_nid": n_nid / n,
                   "mean_err": float(np.mean(errs)) if errs else float("nan"),
                   "std_err": float(np.std(errs)) if errs else float("nan"),
                   "n_err": len(errs)}
            sweep.append(row)
            terr_all.extend(errs)
            print(f"    {snr:>6.1f}  {n_pss:>3}/{n:<3}  {n_sss:>4}/{n:<3}  "
                  f"{n_raw:>5}/{n:<3}  {n_nid:>3}/{n:<3}   {me:>24}   "
                  f"{'0/%d failures' % n if n_nid == n else '%d/%d failures' % (n-n_nid, n):>13}")
        terr_all = np.array(terr_all)
        print("\n    SSS(ZF)  = the firmware chain: chEst -> rcpv(1/H) -> vmult, then detect")
        print("    SSS(raw) = the SAME real detector on the UNEQUALISED SSS — a control.")
        print("    The equaliser EARNS its place (it cancels the phase ramp of the")
        print("    CP-backed-off analysis window, which the raw detector cannot) and it")
        print("    COSTS (rcpv is a zero-forcing 1/H estimated from a PSS ~19 dB below")
        print("    the data in its band, so at low SNR the small-|H| bins amplify noise).")
        print("    The library ships no MMSE reciprocal, so there is no better weight.")

    # ---- figures --------------------------------------------------------
    figs = []
    if not a.no_figures:
        figs = save_figures(stim, det, sweep=sweep,
                            terr=terr_all if terr_all is not None else None,
                            tag=a.figure_tag, backoff=a.backoff)

    # ---- verdict --------------------------------------------------------
    dt = time.time() - t0
    print("\n" + "=" * 78)
    for f in figs:
        print(f"  figure: {os.path.relpath(f, ROOT)}")
    print(f"  {k.phases:,} VSPA phases simulated, {dt:.0f} s wall clock")
    if ok_headline:
        print(f"PASS — cell found in a real NXP burst: N_id = {det.n_id} "
              f"(N_id_1 = {det.id1}, N_id_2 = {det.id2}), "
              f"PSS peak @ sample {det.peak_index}")
    else:
        print(f"FAIL — detected N_id = {det.n_id}, expected {truth.n_id}")
    print("=" * 78)
    return 0 if ok_headline else 1


if __name__ == "__main__":
    sys.exit(main())
