#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""Bring-up + verification probe for the DFE **RX** chain (the untested half).

Everything verified before this script was TX (freq -> iFFT -> time). RX is the
mirror: time-domain antenna samples -> FFT-1024 -> IPPU bit-reverse + subcarrier
extract -> `_sym_queue_rx`. Nothing tested it, so a strip-down could have broken
it silently.

HOW RX IS DRIVEN (the recipe -- all of it reverse-engineered from
DFE_ref_la9310/src/DFE_func_rx.c + build/DFE_func_rx.s1, which gives the exact
`_dfe_ctrl` byte offsets):

  The pipeline stage that matters starts at the FFT-input CIRCULAR BUFFER
  `_FFTin` (0x4080, FFT_CIRC_SIZE = 3840 B = 960 complex samples). On hardware
  the AXIQ DMA + QEC + mixer + 2x-decimator fill it. The host can play producer
  directly, exactly as `DfeSession.process_symbol` fakes a completed fetch-DMA
  on TX -- `run_fft()` reads NOTHING but DMEM state:

    valid = dfe_ctrl.rx_circ_total_produced_size - rx_circ_total_released_size
    (get_valid_size_for_fft_in(); returns 0 -- i.e. STALLS -- if valid > 3840)

  So one RX symbol = write [CP][1024 samples] into the ring in <=960 B blocks,
  bumping `rx_circ_total_produced_size` as you go, and let the firmware consume:

    state A (FFTout_valid == 0)            needs valid >= 2048 + CP_size
        -> memcpy_src_circ  first 512 samples (CP skipped)  -> sym_queue_rx[0:512]
        -> FFTout_valid = 1ST_HALF_READY
    state B (FFTout_valid == 1ST_HALF_READY) needs valid >= 2048
        -> memcpy_src_circ  next  512 samples -> sym_queue_rx[512:1024]
        -> fftDIF1024_sc128_hfx_hfx_vspa2 IN PLACE on sym_queue_rx
        -> IPPU fft_bitRev1024 (reverseIn/linearOut + copy-dest-to-src):
           un-reverses the DIF output and extracts NSC subcarriers back into
           sym_queue_rx[0:NSC]
    PHY_DFE_interf_rx_check_dma_start() then sets FFTout_valid |= 0x80000000,
    phase-compensates (phcom_coeff_rx is all 1.0f -> identity) and DMAs the
    symbol out. The ISS *does* raise the DMA-completion event, so one iteration
    later PHY_DFE_interf_rx_check_dma_finish() -> rx_sym_ready_processing()
    increments `num_rx_sym_to_hiphy` and clears FFTout_valid again. The counter
    is therefore the reliable completion signal (VALID is transient and a coarse
    poll misses it); `_sym_queue_rx` still holds the result -- the DMA only
    reads it.

  ARMING (must be done once after boot, before the first injection):
    * `rx_bitrev_progress_ippu` lives in .ibss and is NEVER initialised to
      BITREV_NOT_STARTED. It boots as 0 == BITREV_DONE, so the first
      check_dma_start() fires a PHANTOM symbol: FFTout_valid |= VALID,
      rx_sym_idx_per_sframe -> 1, and FAST_FLAG_RX_SYM_DMA_INP latched.
      With FFTout_valid stuck at VALID, run_fft() can never start.
      => clear FFTout_valid, and set rx_sym_idx_per_sframe yourself.
    * align released:=produced, read_offset_fft_in := 0, rx_timing_offset := 0
      (run_fft adds rx_timing_offset*4 to the CP on the first symbol of a slot).

  ENCODING: SM16 in and SM16 out, exactly as TX. The FFT input in _sym_queue_rx
  is NATURAL time order (the DIF FFT takes natural in / bit-reversed out; the
  bit-reversal on RX is done AFTERWARDS, by the IPPU, on the OUTPUT). Verified
  empirically below: feeding bit-reversed time samples gives garbage.

Usage:
  python3 cosim/scripts/dfe_rx_probe.py --eld firmware/eld/dfe-la9310-1t2r-25m30k.eld
  python3 cosim/scripts/dfe_rx_probe.py --eld E --nsc 780 --test tone|model|roundtrip|all
"""
from __future__ import annotations

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from dfe.session import DfeSession, from_sm16, to_sm16   # noqa: E402
from vspa_model import r_dif_fft_sc128                   # noqa: E402

N = 1024
FFT_CIRC_SIZE = 3840          # bytes; == sizeof(_FFTin), 0xF00 in the asm
RX_BLOCK_SIZE = 960           # bytes; the granularity the 2xdown stage produces
CP_LONG, CP_SHORT = 352, 288  # bytes (88 / 72 samples) -- get_cp_size(), 30 kHz
NUM_SYM_PER_CP_PERIOD = 14

BIT_FFT_OUT_VALID = 0x80000000
BIT_FFT_OUT_1ST = 0x08000000
BIT_FFT_OUT_2ND = 0x04000000

#: `_dfe_ctrl` byte offsets for the RX state machine. Taken from the compiler's
#: own asm (build/DFE_func_rx.s1 addresses `&_dfe_ctrl+N` in HALFWORDS -> byte
#: offset = 2N), so they are exact for this build and not a hand-count.
RX_CTRL = {
    "rx_circ_total_released_size": 96,    # hw+48
    "rx_timing_offset_total": 104,        # hw+52
    "FFTout_valid": 112,                  # hw+56
    "flag_AtoB": 128,
    "num_rx_sym_to_hiphy": 276,           # hw+138
    "rx_circ_total_produced_size": 376,   # hw+188
    "rx_timing_offset": 404,              # hw+202
    "rx_sym_idx_ready_for_host": 460,     # hw+230
    "rx_win_start_sym_idx": 580,
    "rx_sym_idx_per_sframe": 652,         # hw+326
    "rx_sym_dest_size": 656,              # hw+328
    "rx_release_size_backup": 660,        # hw+330
    "rx_FFTin_circ_wr_offset": 664,
    "read_offset_fft_in": 668,            # hw+334
}


def cp_bytes(sym_idx: int) -> int:
    return CP_LONG if (sym_idx % NUM_SYM_PER_CP_PERIOD) == 0 else CP_SHORT


class RxSession(DfeSession):
    """DfeSession + the RX drive path. Adds no state to the base class."""

    def _sym(self, name: str) -> int:
        from elftools.elf.elffile import ELFFile
        if not hasattr(self, "_symcache"):
            with open(self.eld, "rb") as f:
                st = ELFFile(f).get_section_by_name(".symtab")
                self._symcache = {s.name: s["st_value"] for s in st.iter_symbols()}
        return self._symcache[name]

    def rx_u32(self, field: str) -> int:
        return self.s.read_words(self.syms.dfe_ctrl + RX_CTRL[field], 1)[0]

    def rx_set(self, field: str, value: int) -> None:
        self.s.write_words(self.syms.dfe_ctrl + RX_CTRL[field],
                           [value & 0xFFFFFFFF])

    # ── arming ────────────────────────────────────────────────────────────────
    def rx_arm(self, sym_idx: int = 0) -> None:
        """Undo the phantom first symbol and put the RX state machine at rest."""
        prog = self._sym("_rx_bitrev_progress_ippu")
        self.write_dmem(prog, [1])                     # BITREV_NOT_STARTED
        self.rx_set("FFTout_valid", 0)
        self.rx_set("rx_timing_offset", 0)
        self.rx_set("rx_timing_offset_total", 0)
        self.rx_set("read_offset_fft_in", 0)
        self.rx_set("rx_sym_idx_per_sframe", sym_idx)
        self.rx_set("rx_circ_total_released_size",
                    self.rx_u32("rx_circ_total_produced_size"))
        self._wr = 0        # host-side ring write offset (bytes)

    # ── one RX symbol ────────────────────────────────────────────────────────
    def rx_symbol(self, td, *, sym_idx: int = 0, cp_extra: int = 0,
                  max_phases: int = 600_000, chunk: int = 2000):
        """Push ``td`` (1024 complex time-domain samples, firmware fixed-point
        scale) through FFT + bit-reverse + SC extract. Returns the raw 1024-word
        `_sym_queue_rx` decoded as complex (only [:NSC] are subcarriers).

        ``cp_extra``: extra CP SAMPLES to prepend. run_fft() lands its 1024-point
        window at read_offset + CP_size, so this shifts the window; the NXP
        prebuilt images need cp_extra=2 (see the report), our build needs 0."""
        td = np.asarray(td)
        assert td.size == N
        i = np.rint(td.real).astype(np.int32)
        q = np.rint(td.imag).astype(np.int32)
        words = (to_sm16(i) | (to_sm16(q) << 16)).tolist()

        cp = cp_bytes(sym_idx) + 4 * cp_extra
        stream = [0] * (cp // 4) + words            # CP content is SKIPPED by
        pad = (-len(stream)) % (RX_BLOCK_SIZE // 4)  # run_fft -> zeros are fine
        stream += [0] * pad

        fftin = self._sym("_FFTin")
        nrx0 = self.rx_u32("num_rx_sym_to_hiphy")
        pos, ran = 0, 0
        while ran < max_phases:
            # producer: fill the ring, never letting valid exceed FFT_CIRC_SIZE
            # (get_valid_size_for_fft_in() returns 0 -- a hard stall -- if it does)
            while pos < len(stream):
                prod = self.rx_u32("rx_circ_total_produced_size")
                valid = prod - self.rx_u32("rx_circ_total_released_size")
                if FFT_CIRC_SIZE - valid < RX_BLOCK_SIZE:
                    break
                blk = stream[pos:pos + RX_BLOCK_SIZE // 4]
                self.write_dmem(fftin + self._wr, blk)
                self._wr = (self._wr + RX_BLOCK_SIZE) % FFT_CIRC_SIZE
                self.rx_set("rx_circ_total_produced_size", prod + RX_BLOCK_SIZE)
                pos += RX_BLOCK_SIZE // 4
            self.s.run_phases(chunk)
            ran += chunk
            # VALID is transient (rx_sym_ready_processing clears it once the
            # symbol-out DMA completes); the counter is the durable signal.
            if (self.rx_u32("num_rx_sym_to_hiphy") != nrx0
                    or self.rx_u32("FFTout_valid") & BIT_FFT_OUT_VALID):
                break
        else:
            raise RuntimeError(
                f"RX symbol never completed: FFTout_valid="
                f"{self.rx_u32('FFTout_valid'):#x} pushed={pos*4}/{len(stream)*4}B "
                f"valid={self.rx_u32('rx_circ_total_produced_size') - self.rx_u32('rx_circ_total_released_size')}")

        raw = np.asarray(self.read_dmem(self._sym("_sym_queue_rx"), N),
                         dtype=np.uint32)
        return from_sm16(raw & 0xFFFF) + 1j * from_sm16((raw >> 16) & 0xFFFF)


# ── oracles ──────────────────────────────────────────────────────────────────
def rx_oracle(td, nsc):
    """Python model of the RX chain: forward DIF FFT (sc128) then the IPPU's
    bit-reverse + NSC extract. The extract is the exact inverse of the TX pack
    (`memcpy_dst_circ`): SC i  <->  FFT bin (i - nsc//2) mod N -- CONFIRMED
    empirically by the tone test.

    vspa_model works in Q15-NORMALISED units (|x| < 1.0), the firmware in raw
    integers; scale in and out, exactly as scripts/dfe_absolute_check.py does."""
    x = np.asarray(td, dtype=np.complex128) / 32768.0
    X = r_dif_fft_sc128(x, inv_flag=0) * 32768.0
    return np.array([X[(i - nsc // 2) % N] for i in range(nsc)])


def snr_db(ref, got):
    e = np.abs(np.asarray(got) - np.asarray(ref)) ** 2
    s = np.abs(np.asarray(ref)) ** 2
    return 10 * np.log10(s.sum() / e.sum()) if e.sum() else float("inf")


def evm_db(ref, got):
    ref, got = np.asarray(ref), np.asarray(got)
    a = (got @ ref.conj()) / (ref @ ref.conj())     # best complex scale
    return snr_db(ref * a, got), a


# ── tests ────────────────────────────────────────────────────────────────────
def test_tone(eld, nsc, k, amp=8000.0, cp_extra=0):
    """A pure complex tone must come back as ONE occupied subcarrier."""
    n = np.arange(N)
    td = amp * np.exp(2j * np.pi * k * n / N)
    d = RxSession(eld, nsc=nsc)
    d.rx_arm(0)
    out = d.rx_symbol(td, cp_extra=cp_extra)
    del d

    sc = out[:nsc]
    peak = int(np.argmax(np.abs(sc)))
    mag = np.abs(sc)
    others = np.delete(mag, peak)
    sidelobe_db = 20 * np.log10(mag[peak] / max(others.max(), 1e-9))
    expect = (k + nsc // 2) % N          # inverse of SC i -> bin (i - nsc/2)
    print(f"  tone bin k={k:4d}: peak SC={peak:4d} (expect {expect:4d})  "
          f"|peak|={mag[peak]:8.1f}  worst other={others.max():7.1f}  "
          f"suppression={sidelobe_db:5.1f} dB")
    return peak == expect and sidelobe_db > 20, out


def test_model(eld, nsc, seed=1, cp_extra=0):
    """Random time-domain signal vs the Python FFT model (r_dif_fft_sc128)."""
    rng = np.random.default_rng(seed)
    td = np.rint(rng.normal(0, 3000, N) + 1j * rng.normal(0, 3000, N))
    d = RxSession(eld, nsc=nsc)
    d.rx_arm(0)
    out = d.rx_symbol(td, cp_extra=cp_extra)
    del d
    ref = rx_oracle(td, nsc)
    got = out[:nsc]
    s = snr_db(ref, got)
    exact = int(np.sum(np.rint(ref.real) == got.real) +
                np.sum(np.rint(ref.imag) == got.imag))
    print(f"  random TD vs r_dif_fft_sc128: SNR = {s:6.2f} dB   "
          f"bit-exact halves {exact}/{2*nsc}   "
          f"|ref|={np.abs(ref).mean():7.1f} |got|={np.abs(got).mean():7.1f}")
    return s, ref, got


#: TX's in-place subcarrier pack (DFE_func_tx.c:554, memcpy_dst_circ with
#: dst = src + POINTS_FFT - NSC/2 inside the SAME buffer) reads and writes the
#: same array. It only stays correct while the write region [N-NSC/2, N) does
#: not reach back into the source region [0, NSC) -- i.e. while NSC <= 2N/3.
#: Above that the copy destroys source subcarriers before reading them. Not an
#: RX issue; see the report. Round-trip tests must stay below it to isolate RX.
TX_PACK_NSC_LIMIT = 2 * N // 3          # 682


def test_roundtrip(eld, nsc, vector=None, sym=0, cp_extra=0):
    """TX (freq -> time) then RX (time -> freq): recover the subcarriers."""
    if nsc > TX_PACK_NSC_LIMIT:
        print(f"  !! NSC={nsc} > {TX_PACK_NSC_LIMIT}: the TX in-place pack "
              f"self-clobbers SC[{N - nsc//2}:{nsc}] before reading them "
              f"(DFE_func_tx.c:554). The round trip measures that TX bug, not RX.")
    if vector:
        from fr1fr2_vectors import load_fd_grid
        grid = load_fd_grid(vector, nsc=nsc)
        sym_bytes = grid.symbol_bytes(sym)
        sc_in = np.frombuffer(sym_bytes[:nsc * 4], dtype="<i2").reshape(-1, 2)
        sc_in = sc_in[:, 0].astype(float) + 1j * sc_in[:, 1].astype(float)
    else:
        rng = np.random.default_rng(7)
        qam = (rng.choice([-1, 1], nsc) + 1j * rng.choice([-1, 1], nsc)) * 8000
        sc_in = qam
        i = np.rint(sc_in.real).astype(np.int16)
        q = np.rint(sc_in.imag).astype(np.int16)
        sym_bytes = np.stack([i, q], 1).astype("<i2").tobytes()

    tx = DfeSession(eld, nsc=nsc)
    z = tx.process_symbol(sym_bytes)          # 1024 time-domain, natural order
    del tx

    rx = RxSession(eld, nsc=nsc)
    rx.rx_arm(0)
    out = rx.rx_symbol(z, cp_extra=cp_extra)
    del rx
    got = out[:nsc]

    s, a = evm_db(sc_in, got)
    print(f"  TX->RX round trip ({'vector' if vector else 'QPSK'}): "
          f"recovery SNR = {s:6.2f} dB   scale = {abs(a):.4f} "
          f"(expect ~1/16 = 0.0625)  phase = {np.angle(a):+.4f} rad")
    return s, sc_in, got


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--eld", default="firmware/eld/dfe-la9310-1t2r-25m30k.eld")
    ap.add_argument("--nsc", type=int, default=780)
    ap.add_argument("--vector")
    ap.add_argument("--cp-extra", type=int, default=0, dest="cp_extra",
                    help="extra CP samples before the FFT window; 0 for our "
                         "build, 2 for the NXP prebuilt images")
    ap.add_argument("--test", default="all",
                    choices=["tone", "model", "roundtrip", "bitrev", "all"])
    a = ap.parse_args()

    ok = True
    if a.test in ("tone", "all"):
        print("[1] TONE -- a single complex exponential -> a single subcarrier")
        for k in (0, 37, -100):
            good, _ = test_tone(a.eld, a.nsc, k, cp_extra=a.cp_extra)
            ok &= good

    if a.test in ("bitrev", "all"):
        print("[2] BIT-REVERSAL -- is the FFT input natural or bit-reversed?")
        from dfe.session import bitrev_index
        rng = np.random.default_rng(3)
        td = np.rint(rng.normal(0, 3000, N) + 1j * rng.normal(0, 3000, N))
        ref = rx_oracle(td, a.nsc)
        for name, inp in (("natural", td), ("bit-reversed", td[bitrev_index(10)])):
            d = RxSession(a.eld, nsc=a.nsc); d.rx_arm(0)
            got = d.rx_symbol(inp, cp_extra=a.cp_extra)[:a.nsc]; del d
            print(f"  input {name:12s}: SNR vs model(natural) = "
                  f"{snr_db(ref, got):7.2f} dB")

    if a.test in ("model", "all"):
        print("[3] MODEL -- vs vspa_model.r_dif_fft_sc128(inv_flag=0)")
        s, _, _ = test_model(a.eld, a.nsc, cp_extra=a.cp_extra)
        ok &= s > 40

    if a.test in ("roundtrip", "all"):
        print("[4] ROUND TRIP -- DfeSession TX output fed back into RX")
        rt_nsc = min(a.nsc, TX_PACK_NSC_LIMIT) if a.vector is None else a.nsc
        if rt_nsc != a.nsc:
            print(f"  (running at NSC={rt_nsc}, not {a.nsc}: above "
                  f"{TX_PACK_NSC_LIMIT} the TX pack self-clobbers -- see report)")
        s, _, _ = test_roundtrip(a.eld, rt_nsc, a.vector, cp_extra=a.cp_extra)
        ok &= s > 20

    print("\nRESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
