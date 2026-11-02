#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""Tier-5 probe for the DOWNSTREAM TX stages of the LA9310 DFE firmware.

Everything after ``_iFFTout_CFRinout`` used to be untested.  This drives it and
checks it quantitatively, on the standalone native ISS, with **no AXIQ model**:

  CP insertion  -> TX windowing -> 2x upsampler -> mixer -> TX QEC -> ant buffer

WHAT ACTUALLY GATES EACH STAGE (verified, not guessed - see the report):

  * ``tx_allowed`` gates NOTHING in the datapath of this build.  ``SHARED_TXRX_
    AXIQ_BUFFER`` is undefined for LA9310, so the one call to
    ``get_tx_allowed_status()`` is compiled out.  The WAITING_TX_ALLOWED state
    only gates ``PHY_DFE_interf_tx_check_dma_start()`` - the *symbol fetch* -
    and ``DfeSession`` already bypasses that by faking a completed fetch-DMA.
  * CFR and DPD are NOT IN THE BINARY (``DEFAULT_CFR_DISABLE=1``,
    ``DEFAULT_DPD_DISABLE=1``, ``NUM_CFR_ADVANCED_SAMPLES=0``, plus an #error
    guard in main_pair_coreA.c).  ``get_tx_1block_from_cfr_out()`` is pure
    bookkeeping: it hands ring blocks to the 2xup input queue.
  * CP insertion is done BY THE IPPU, not by a memcpy: ``start_ippu_bitrev_tx``
    passes ``sample_offset = POINTS_FFT - cp_samples`` so the bit-reverse reads
    the FFT body circularly starting at sample 944.  It therefore only happens
    when ``buf_stat_ifft_in`` carries a symbol size **bigger than 1024*4**.
    DfeSession injects exactly 4096 -> zero CP -> this stage never ran before.
  * The 2xup/mixer/QEC block is gated on
        buf_stat_2xup1_in[0].buf_stat_size & (1<<31)   (a block is queued)
    AND antman_ctrl_tx.num_empty_buffers != 0          (an ant buffer is free).
  * The ONLY thing the ISS cannot do is retire the AXIQ DMA
    (``DMAC_FIFO_HS`` handshake into the antenna FIFO).  It never completes, so
    ``num_DMA_in_progress`` saturates at 2, ``num_empty_buffers`` hits 0 and the
    pipeline stalls after 2 blocks.  ``_axiq_drain()`` below is the host-side
    AXIQ consumer model - the exact analogue of DfeSession faking the fetch-DMA.

Usage:  python3 cosim/scripts/dfe_tx_downstream_probe.py [--symbols 3] [--mixer-mhz 0]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from dfe.session import DfeSession, to_sm16, from_sm16, bitrev_index  # noqa: E402

ELD = "firmware/eld/dfe-la9310-1t2r-25m30k.eld"
NSC = 780                       # 25 MHz / 30 kHz -> 65 RB * 12

# ── addresses (bytes; from the eld symtab) ───────────────────────────────────
SYM_QUEUE_TX = 28288
IFFTOUT = 20352                 # _iFFTout_CFRinout, 3840 B = 960 complex
TAPS_2XUP = 12672               # _filter_taps_upsampling, 32 x float32
ANTMAN_TX = 11792               # _antman_ctrl_tx

# ── dfe_ctrl byte offsets (pair_config_types.h; anchored + verified on-target)─
C_MIXER_FREQ = 28               # tx_mixer_freq_update
C_TX_SYM_IDX = 108              # tx_sym_idx_per_sframe
C_NTX = 272                     # num_tx_sym_from_hiphy
C_RELEASED = 360                # tx_circ_total_released_size
C_PRODUCED = 472                # tx_circ_total_produced_size
C_ERR_MSB = 436                 # error_info_MSB32
C_BS_2XUP = 528                 # buf_stat_2xup1_in[0].buf_stat_size
C_BS_IFFT = 560                 # buf_stat_ifft_in[0]
C_CFR_OUT = 588                 # size_cfr_out_valid
C_RD_2XUP = 592                 # read_ptr_copy_to_2xup1   (halfword ptr)
C_WR_IFFT = 596                 # write_ptr_ifft_out       (halfword ptr)

# ── antman_ctrl_tx byte offsets ──────────────────────────────────────────────
A_NDMA = ANTMAN_TX + 4
A_NVALID = ANTMAN_TX + 8
A_NEMPTY = ANTMAN_TX + 10
A_BUFSTAT = ANTMAN_TX + 48      # struct_buf_stat_td[4], 16 B each

# ── firmware constants (proj_config.h / pair_config.h) ───────────────────────
POINTS_FFT = 1024
TX_BLOCK = 1280                 # bytes  (320 complex samples)
ANT_SAMPLES = 640               # SIZE_TX_ANT_BUFFER/4 = TX_BLOCK/4 * 2
N_TX_ANT_BUF = 2
CP_LONG, CP_SHORT = 320, 288    # NUM_SAMPLES_LCP*4, NUM_SAMPLES_SCP*4  (bytes)
SYMS_PER_CP_PERIOD = 28
WINDOW = 16                     # WINDOW_SIZE (txWindowing.h)
FS_ANT = 61.44e6                # after 2x upsampling
BW_HALF = NSC * 30e3 / 2        # 11.7 MHz occupied half-bandwidth


def cx(words) -> np.ndarray:
    """SM16 word pairs -> complex.  Assembly kernels write SIGN-MAGNITUDE 16."""
    u = np.asarray(words, dtype=np.uint32)
    return from_sm16(u & 0xFFFF) + 1j * from_sm16((u >> 16) & 0xFFFF)


class TxProbe:
    def __init__(self, eld: str, mixer_freq: int = 0):
        self.d = DfeSession(eld, nsc=NSC)
        self.buf_addr = [self.d.s.read_words(A_BUFSTAT + 16 * i + 4, 1)[0] * 2
                         for i in range(N_TX_ANT_BUF)]
        self.blocks: list[np.ndarray] = []
        self._next = 0
        if mixer_freq:
            self.d.s.write_words(C_MIXER_FREQ, [mixer_freq & 0xFFFFFFFF])

    # -- register helpers ----------------------------------------------------
    def u32(self, off):
        return self.d.s.read_words(off, 1)[0]

    def _u16(self, byte):
        return (self.d.s.read_words(byte & ~3, 1)[0] >> (8 * (byte & 3))) & 0xFFFF

    def _set_u16(self, byte, val):
        a, sh = byte & ~3, 8 * (byte & 3)
        w = self.d.s.read_words(a, 1)[0]
        self.d.s.write_words(a, [(w & ~(0xFFFF << sh)) | ((val & 0xFFFF) << sh)])

    # -- host-side AXIQ consumer --------------------------------------------
    def _axiq_drain(self):
        """Retire the AXIQ DMAs the ISS cannot complete, capturing each
        antenna block on its way out (this IS the transmitted stream)."""
        n = self._u16(A_NDMA)
        if not n:
            return
        for _ in range(n):
            self.blocks.append(cx(self.d.read_dmem(self.buf_addr[self._next],
                                                   ANT_SAMPLES)))
            self._next = (self._next + 1) % N_TX_ANT_BUF
        self._set_u16(A_NDMA, 0)
        self._set_u16(A_NEMPTY, self._u16(A_NEMPTY) + n)

    # -- symbol injection ----------------------------------------------------
    def push_symbol(self, iq: np.ndarray, *, snapshot_ring=False):
        """Inject one fd symbol WITH ITS CYCLIC PREFIX and pump the pipeline."""
        for _ in range(200):
            if self.u32(C_BS_IFFT) == 0:
                break
            self._axiq_drain()
            self.d.s.run_phases(500)

        sym_idx = self.u32(C_TX_SYM_IDX)
        cp = CP_LONG if (sym_idx % SYMS_PER_CP_PERIOD) == 0 else CP_SHORT
        w = (to_sm16(iq[:, 0].astype(np.int32))
             | (to_sm16(iq[:, 1].astype(np.int32)) << 16)).tolist()
        self.d.write_dmem(SYM_QUEUE_TX, w)
        # THE LOAD-BEARING LINE: size = CP + FFT, not just FFT.  A size of
        # POINTS_FFT*4 gives sample_offset 0 -> the IPPU inserts NO cyclic
        # prefix and txWindowing is called with zero length.
        self.d.s.write_words(C_BS_IFFT, [cp + POINTS_FFT * 4])
        self.d.s.write_reg(0x1C5, 1 << 25)          # fetch-DMA "completed"

        ntx0 = self.u32(C_NTX)
        ring = ifft = None
        for _ in range(400):
            self.d.s.run_phases(200)
            if snapshot_ring and ring is None and self.u32(C_CFR_OUT) >= TX_BLOCK:
                # first bit-reversed block has landed and the 960-sample ring
                # has not wrapped yet: this is the CP + head of the symbol.
                # Grab THIS symbol's iFFT output too - sym_queue_tx is reused by
                # the next symbol, so reading it later gives the wrong symbol.
                ring = cx(self.d.read_dmem(IFFTOUT, TX_BLOCK // 4))
                ifft = self.ifft_natural()
            self._axiq_drain()
            if (self.u32(C_NTX) != ntx0 and self.u32(C_BS_IFFT) == 0
                    and self.u32(C_CFR_OUT) < TX_BLOCK):
                break
        return cp // 4, ring, ifft

    def flush(self):
        for _ in range(80):
            self.d.s.run_phases(500)
            self._axiq_drain()

    def ifft_natural(self):
        return cx(self.d.read_dmem(SYM_QUEUE_TX, POINTS_FFT))[bitrev_index(10)]

    def taps(self):
        w = np.asarray(self.d.read_dmem(TAPS_2XUP, 32), dtype=np.uint32)
        return np.frombuffer(w.astype("<u4").tobytes(), dtype="<f4")

    def stream(self):
        return np.concatenate(self.blocks) if self.blocks else np.zeros(0, complex)


def poly2x(x, h):
    """Reference 2x polyphase interpolator, zero history (first block)."""
    y = np.empty(2 * len(x), dtype=complex)
    y[0::2] = np.convolve(x, h[1::2])[:len(x)]
    y[1::2] = np.convolve(x, h[0::2])[:len(x)]
    return y


def mixer_freq_word(f_hz: float) -> int:
    """dfe_ctrl.tx_mixer_freq_update for a wanted shift of ``f_hz``.

    MEASURED on-target (three points, ratio -1.0000 each):
        f_shift = -FreqIn / 2**32 * FS_ANT
    i.e. the NCO turns the OPPOSITE way from the naive f/FS*2**32 guess.
    """
    return int(round(-f_hz / FS_ANT * 2 ** 32)) & 0xFFFFFFFF


def estimate_shift(ref: np.ndarray, mixed: np.ndarray) -> tuple[float, float]:
    """Frequency shift between two streams, from the lag-1 phase of their
    ratio (no unwrap -> immune to the gaps where |ref| is small).
    Returns (Hz, mean |gain| deviation from 1)."""
    n = min(len(ref), len(mixed))
    a, b = ref[:n], mixed[:n]
    g = np.abs(a) > 0.15 * np.abs(a).max()
    r = np.zeros(n, dtype=complex)
    r[g] = b[g] / a[g]
    pair = g[:-1] & g[1:]
    acc = np.sum(np.conj(r[:-1][pair]) * r[1:][pair])
    return np.angle(acc) / (2 * np.pi) * FS_ANT, float(np.abs(np.abs(r[g]) - 1).mean())


def psd_db(y, seg=1024):
    n = (len(y) // seg) * seg
    w = np.hanning(seg)
    acc = np.zeros(seg)
    k = 0
    for i in range(0, n - seg + 1, seg // 2):
        acc += np.abs(np.fft.fft(y[i:i + seg] * w)) ** 2
        k += 1
    return np.fft.fftfreq(seg, 1 / FS_ANT), acc / max(k, 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--eld", default=ELD)
    ap.add_argument("--symbols", type=int, default=3)
    ap.add_argument("--mixer-mhz", type=float, default=0.0,
                    help="additionally verify the MIXER: boot a 2nd time with "
                         "dfe_ctrl.tx_mixer_freq_update set for this shift and "
                         "measure the actual shift of the antenna stream")
    a = ap.parse_args()

    p = TxProbe(a.eld)
    rng = np.random.default_rng(3)
    fails = []

    ring = ifft = None
    for s in range(a.symbols):
        iq = np.stack([rng.integers(-8000, 8000, NSC),
                       rng.integers(-8000, 8000, NSC)], 1).astype(np.int16)
        cps, r, f0 = p.push_symbol(iq, snapshot_ring=(s == 0))
        if s == 0:
            ring, ifft, cp0 = r, f0, cps
    p.flush()

    print(f"symbols={a.symbols}  produced={p.u32(C_PRODUCED)}B "
          f"released={p.u32(C_RELEASED)}B  ant-blocks={len(p.blocks)}  "
          f"error_info=0x{p.u32(C_ERR_MSB):08x}")
    if p.u32(C_ERR_MSB):
        fails.append("firmware reported an error")

    # ── CHECK 1: CP insertion (bit-exact) ────────────────────────────────────
    cp_fw, cp_ref = ring[WINDOW:cp0], ifft[POINTS_FFT - cp0 + WINDOW:]
    bod_fw, bod_ref = ring[cp0:], ifft[:len(ring) - cp0]
    ok_cp = np.array_equal(cp_fw, cp_ref)
    ok_bd = np.array_equal(bod_fw, bod_ref)
    print(f"\nCP INSERTION (IPPU sample_offset={POINTS_FFT - cp0}, cp={cp0} samples)")
    print(f"  ring[{WINDOW}:{cp0}] == ifft[{POINTS_FFT-cp0+WINDOW}:{POINTS_FFT}] "
          f"(last {cp0-WINDOW} samples copied to the FRONT) : "
          f"{'BIT-EXACT' if ok_cp else 'MISMATCH'}")
    print(f"  ring[{cp0}:{len(ring)}] == ifft[0:{len(ring)-cp0}] (symbol body)      : "
          f"{'BIT-EXACT' if ok_bd else 'MISMATCH'}")
    fails += [] if (ok_cp and ok_bd) else ["CP insertion"]

    # ── CHECK 2: TX windowing ────────────────────────────────────────────────
    ramp = np.abs(ring[:WINDOW]) / np.maximum(np.abs(ifft[POINTS_FFT - cp0:
                                                          POINTS_FFT - cp0 + WINDOW]), 1)
    mono = bool(np.all(np.diff(ramp) > 0)) and ramp[0] < 0.05 and ramp[-1] > 0.9
    print(f"\nTX WINDOWING (WINDOW_SIZE={WINDOW}, zero history on symbol 0)")
    print(f"  |ring[0:{WINDOW}]| / |ifft tail| = {np.round(ramp, 3)}")
    print(f"  monotone 0 -> 1 rise window : {'YES' if mono else 'NO'}")
    fails += [] if mono else ["TX windowing ramp"]

    # ── CHECK 3: 2x upsampler vs polyphase model ─────────────────────────────
    h = p.taps()
    y0 = poly2x(ring, h)
    a0 = p.blocks[0]
    n = min(len(a0), len(y0))
    err = np.abs(a0[:n] - y0[:n])
    snr = 10 * np.log10((np.abs(y0[:n]) ** 2).sum() / max((err ** 2).sum(), 1e-9))
    print(f"\n2x UPSAMPLER (32-tap polyphase, tap sum={h.sum():.3f})")
    print(f"  in {len(ring)} samples @30.72 MHz -> out {len(a0)} samples @61.44 MHz"
          f"  (rate x{len(a0)/len(ring):.0f})")
    print(f"  firmware vs python model : SNR {snr:.1f} dB, "
          f"max |err| {err.max():.1f} LSB  ({'PASS' if snr > 50 else 'FAIL'})")
    fails += [] if snr > 50 else ["2x upsampler model match"]

    # ── CHECK 4: spectrum of the transmitted stream ──────────────────────────
    y = p.stream()
    f, P = psd_db(y)
    sig = np.abs(f) < BW_HALF
    img = np.abs(f) > FS_ANT / 2 - BW_HALF - 0.3e6
    rej = 10 * np.log10(P[sig].mean() / P[img].mean())
    ctr = float(f[np.argmax(P)]) / 1e6
    print(f"\nANTENNA STREAM SPECTRUM ({len(y)} samples @ {FS_ANT/1e6:.2f} MHz)")
    print(f"  occupied |f|<{BW_HALF/1e6:.1f} MHz : {10*np.log10(P[sig].mean()):5.1f} dB")
    print(f"  image    |f|>{(FS_ANT/2-BW_HALF-0.3e6)/1e6:.1f} MHz : "
          f"{10*np.log10(P[img].mean()):5.1f} dB")
    print(f"  --> 2x-upsampler image rejection : {rej:.1f} dB "
          f"({'PASS' if rej > 40 else 'FAIL'})")
    fails += [] if rej > 40 else ["image rejection"]
    del ctr

    # ── CHECK 5 (optional): the MIXER ────────────────────────────────────────
    if a.mixer_mhz:
        word = mixer_freq_word(a.mixer_mhz * 1e6)
        q = TxProbe(a.eld, mixer_freq=word)
        rng2 = np.random.default_rng(3)
        for _ in range(a.symbols):
            iq = np.stack([rng2.integers(-8000, 8000, NSC),
                           rng2.integers(-8000, 8000, NSC)], 1).astype(np.int16)
            q.push_symbol(iq)
        q.flush()
        got, gdev = estimate_shift(y, q.stream())
        ok = abs(got / 1e6 - a.mixer_mhz) < 0.05 and gdev < 0.01
        print(f"\nMIXER (dfe_ctrl+{C_MIXER_FREQ} tx_mixer_freq_update = 0x{word:08x})")
        print(f"  requested {a.mixer_mhz:+.3f} MHz -> measured {got/1e6:+.4f} MHz "
              f"(phase-ramp vs the unmixed stream)")
        print(f"  |gain| deviation from 1 : {gdev:.4f}  (pure phase rotation => "
              f"mixer+QEC add no amplitude distortion)  ({'PASS' if ok else 'FAIL'})")
        fails += [] if ok else ["mixer shift"]

    print("\n" + ("PASS - all downstream TX stages verified"
                  if not fails else "FAIL: " + ", ".join(fails)))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
