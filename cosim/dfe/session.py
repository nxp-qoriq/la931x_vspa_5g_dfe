# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""Drive the LA9310 DFE VSPA firmware on the native in-process ISS.

Runs the REAL DFE application (`geul-vspa0.eld`, single-core "9310" build of
DFE_Ref) on `framework/native`, replicating what the host/CW-CAS environment
does on hardware, so frequency-domain test vectors from NXP's
fr1_fr2_test_tool can be pushed through the firmware's own TX pipeline
(phcom -> subcarrier packing -> iFFT-1024) and the produced samples read back.

Drive protocol (reverse-engineered from DFE_ref_project/9310/source_shared and
the DFE_Ref User Guide §4.2.4/§4.3; see docs/TESTING.md):

1.  BOOT HANDSHAKE (`boot.s1 _boot`): firmware posts ready 0xF1000000 on its
    out-mailbox, then spins reading the in-mailbox until a message with
    MSB[31:24] == 0x70 arrives; it ACKs 0xF0700000. The host side of the
    mailbox is IP regs 416/417 (HOST_OUT_0), the ACK mirrors at 420 (HOST_IN).

2.  MODE GATE (`DFE_init.s1` L18): after `_init_common` the core spins until
    IP reg 0x4F (LD_RF_TB_REAL_7 == the DFE_MODE_CONFIG MSB on LA9310) reads
    nonzero. Mode LSB goes to 0x50. NSC lives in IP reg 0x4E (defaults to 780
    for the 25M_30K build if the host leaves it 0 — write it first).

3.  SYMBOL INJECTION (`DFE_func_tx.s1 _tx_sym_ready_processing`): the per-
    symbol worker is normally invoked by `_PHY_DFE_interf_tx_check_dma_finish`
    once the fetch-DMA completed. Its inputs are pure DMEM/IP state, so the
    host can emulate a completed fetch directly, with no AXIQ/DMA model:
      - write the fd symbol (NSC x 4B, int16 I,Q LE) into `_sym_queue_tx`
      - clear `_buf_stat_ifft_in[0]` (dfe_ctrl+560; bit31 clear = present)
      - ensure `[_tx_sym_dma_flag_addr]` != 0x7FFF8000 (DMA-done sentinel)
      - set IP reg 0x1C5 bit25 ("TX symbol DMA in flight")
    The main loop then runs phcom + `_memcpy_dst_circ` packing + `_ifftDIF1024
    _sc128_hfx_hfx_vspa2` IN PLACE in `_sym_queue_tx`, increments
    `_num_tx_sym_from_hiphy` (dfe_ctrl+272) and advances the CP-period symbol
    index. Completion is detected by the counter increment.

All buffers/fields are resolved BY SYMBOL NAME from the eld (both the NXP
prebuilt images and local rebuilds carry full symtabs), so the same driver
works for any build of the app; `_dfe_ctrl` struct offsets are fixed by
`global_var.s1` and shared between builds.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from native.session import NativeSession  # noqa: E402

# ── host-side register indices (CCSR offset / 4 == mvip IP index) ────────────
HOST_OUT_MSB, HOST_OUT_LSB = 416, 417   # HOST_OUT_0: host -> VSPA mailbox
HOST_IN_MSB, HOST_IN_LSB = 420, 421     # HOST_IN_0 : VSPA -> host mailbox
REG_NSC = 0x4E                          # number of subcarriers per symbol
REG_MODE_HI = 0x4F                      # DFE_MODE_CONFIG MSB (nonzero unblocks)
REG_MODE_LO = 0x50                      # DFE_MODE_CONFIG LSB
REG_DMA_FLAGS = 0x1C5                   # bit25 = TX symbol fetch in flight

BOOT_READY = 0xF1000000
BOOT_GO = 0x70000000
BOOT_ACK = 0xF0700000

#: byte offsets into the host-visible `_dfe_ctrl` struct (global_var.s1)
DFE_CTRL = {
    "kernels_disable": 16, "tx_ppln_state": 18,
    "tx_sym_idx_per_sframe": 108, "FFTout_valid": 112,
    "ifft_in_buf_wr_idx": 116,
    "num_tx_sym_from_hiphy": 272, "num_rx_sym_to_hiphy": 276,
    "tx_sym_buf_base": 292, "tx_num_sym_in_buf": 296, "tx_sym_buff_size": 300,
    "error_info_LSB32": 432, "error_info_MSB32": 436,
    "rx_sym_idx_ready_for_host": 460, "tx_sym_idx_request_for_host": 464,
    "tx_sym_dma_flag_addr": 524, "buf_stat_ifft_in": 560,
    "msg_received_status": 568, "num_tx_symbols_counter": 604,
}

DMA_DONE_SENTINEL = 0x7FFF8000

# ── SM16: THE ENCODING THAT DECIDES EVERYTHING ───────────────────────────────
# VSPA **assembly** kernels use SIGN-MAGNITUDE 16 (sign in bit 15, magnitude in
# bits 14:0), NOT two's complement. The DFE's iFFT is an assembly kernel, so its
# input must be SM16 and its output IS SM16.
#
# Get this wrong and the failure is spectacular but subtle: SM16 -64 is 0x8040,
# which read as two's complement is -32704. A real symbol is ~half negative, so
# the output looks like saturated noise -- while a DC-only symbol (the one case
# with no negative samples) comes out perfect. That asymmetry is what finally
# gave the bug away.
#
# NXP's FR1 test vectors are two's-complement Q15, so they must be CONVERTED on
# the way in.


def to_sm16(v):
    import numpy as np
    v = np.asarray(v, dtype=np.int32)
    return (np.where(v < 0, 0x8000 | (-v), v).astype(np.uint32) & 0xFFFF)


def from_sm16(u):
    import numpy as np
    u = np.asarray(u, dtype=np.uint32)
    mag = (u & 0x7FFF).astype(np.float64)
    return np.where(u & 0x8000, -mag, mag)


def bitrev_index(n_bits: int = 10):
    """The DIF iFFT emits BIT-REVERSED order; this undoes it."""
    import numpy as np
    n = 1 << n_bits
    return np.array([int(format(i, f"0{n_bits}b")[::-1], 2) for i in range(n)])

#: LA9310 DMEM partition: everything at/above this lives in the IPPU's OWN
#: memory array ("rudmem", space 3), not the VCPU's. The model has NO partition
#: redirect on the debug read/write path -- a host that reads _sym_queue_tx
#: (0x6e80) through space 1 is reading a DIFFERENT, unused array.
DMEM_PARTITION = 0x4000
IFFT_N = 1024  # points; output is IFFT_N complex samples, in place


@dataclass
class DfeSymbols:
    """Resolved data-symbol addresses of one DFE eld."""
    dfe_ctrl: int
    sym_queue_tx: int
    ifftout_cfrinout: int
    ant_buf: int


def resolve_symbols(eld: str | Path) -> DfeSymbols:
    from elftools.elf.elffile import ELFFile
    with open(eld, "rb") as f:
        st = ELFFile(f).get_section_by_name(".symtab")
        if st is None:
            raise ValueError(f"{eld}: no symtab — cannot resolve DFE buffers")
        names = {s.name: s["st_value"] for s in st.iter_symbols()}
    def need(n):
        if n not in names:
            raise KeyError(f"{eld}: symbol {n} not found")
        return names[n]
    return DfeSymbols(
        dfe_ctrl=need("_dfe_ctrl"),
        sym_queue_tx=need("_sym_queue_tx"),
        ifftout_cfrinout=need("_iFFTout_CFRinout"),
        ant_buf=need("_ant_buf"),
    )


class DfeError(RuntimeError):
    pass


class DfeSession:
    """One booted, mode-configured DFE firmware instance on the native ISS."""

    def __init__(self, eld: str | Path, *, nsc: int = 624,
                 mode_hi: int = 0x0E003000, mode_lo: int = 0x0) -> None:
        self.eld = str(eld)
        self.nsc = nsc
        self.syms = resolve_symbols(eld)
        # THE LA9310 CAS DMEM PARTITION (128 lines x 128 B = 0x4000). Without it
        # the VCPU and the IPPU are backed by two different memory arrays, the
        # VCPU writes the IPPU's argument block into one while the IPPU reads the
        # other, and the TX bit-reverse starts but never completes.
        from native.model import NativeModel
        self.s = NativeSession.__new__(NativeSession)
        self.s.m = NativeModel(dmem_partition_lines=128)
        self.s.elf = None
        self.s.entry = 0
        self.s.load_elf(eld)
        self.s.boot()
        self._boot_handshake()
        self._configure(nsc, mode_hi, mode_lo)

    # ── plumbing ──────────────────────────────────────────────────────────────
    def _run_until(self, pred, *, chunk=2000, max_phases=400_000, what=""):
        ran = 0
        while ran < max_phases:
            self.s.run_phases(chunk)
            ran += chunk
            if pred():
                return ran
        raise DfeError(f"timeout waiting for {what} "
                       f"(pc={self.s.read_pc():#x}, {ran} phases)")

    @staticmethod
    def _space(addr: int) -> int:
        """Which memory array actually backs this DMEM address (see
        DMEM_PARTITION). dfe_ctrl and friends are below the boundary (VCPU
        DRAM); the symbol queues and FFT buffers are above it (IPPU DRAM)."""
        from native.model import SPACE_DRAM, SPACE_IPPU_DRAM
        return SPACE_IPPU_DRAM if addr >= DMEM_PARTITION else SPACE_DRAM

    @staticmethod
    def _addr(addr: int) -> int:
        """Both DMEM arrays are indexed FROM 0, so an address at/above the
        partition is IPPU-array offset (addr - DMEM_PARTITION)."""
        return addr - DMEM_PARTITION if addr >= DMEM_PARTITION else addr

    def read_dmem(self, addr: int, count: int) -> list[int]:
        return self.s.read_words(self._addr(addr), count, space=self._space(addr))

    def write_dmem(self, addr: int, words: list[int]) -> None:
        self.s.write_words(self._addr(addr), words, space=self._space(addr))

    def ctrl_u32(self, field: str) -> int:
        off = DFE_CTRL[field]
        return self.s.read_words(self.syms.dfe_ctrl + (off & ~3), 1)[0] \
            if off % 4 == 0 else self._ctrl_unaligned(off)

    def _ctrl_unaligned(self, off: int) -> int:
        w = self.s.read_words(self.syms.dfe_ctrl + (off & ~3), 1)[0]
        return (w >> (8 * (off & 3))) & 0xFFFF

    def ctrl_write_u32(self, field: str, value: int) -> None:
        off = DFE_CTRL[field]
        assert off % 4 == 0, field
        self.s.write_words(self.syms.dfe_ctrl + off, [value])

    # ── bring-up ──────────────────────────────────────────────────────────────
    def _boot_handshake(self) -> None:
        self._run_until(lambda: self.s.read_reg(HOST_IN_MSB) == BOOT_READY,
                        what="firmware ready msg 0xF1000000")
        self.s.write_reg(HOST_OUT_MSB, BOOT_GO)
        self.s.write_reg(HOST_OUT_LSB, 0)
        self._run_until(lambda: self.s.read_reg(HOST_IN_MSB) == BOOT_ACK,
                        what="boot ACK 0xF0700000")

    def _configure(self, nsc: int, mode_hi: int, mode_lo: int) -> None:
        self.s.write_reg(REG_NSC, nsc)
        self.s.write_reg(REG_MODE_LO, mode_lo)
        self.s.write_reg(REG_MODE_HI, mode_hi)
        # main loop starts iterating; ntx counter is live from here on
        self.s.run_phases(20_000)
        # neutralize the fetch-DMA completion sentinel once: point the flag
        # word at a scratch word that never equals the sentinel
        flag_ptr = self.ctrl_u32("tx_sym_dma_flag_addr")
        if flag_ptr:
            # ptr is a VSPA data address in halfwords; make the word it
            # targets != sentinel (byte addr = ptr*2, restricted to DMEM)
            byte = (flag_ptr * 2) & 0x7FFC
            if self.s.read_words(byte, 1)[0] == DMA_DONE_SENTINEL:
                self.s.write_words(byte, [0])

    # ── per-symbol drive ─────────────────────────────────────────────────────
    def process_symbol(self, sym_bytes: bytes, *,
                       max_phases: int = 400_000):
        """Push one frequency-domain symbol through the firmware TX pipeline.

        ``sym_bytes``: NSC complex REs as int16 I,Q little-endian, two's
        complement (NXP's fr1fr2 vector wire format).

        Returns the produced OFDM symbol as IFFT_N complex time-domain samples,
        in NATURAL order, at the firmware's own fixed-point scale.

        Three conversions are load-bearing, and each one silently corrupts the
        result if skipped:
          * input  : two's complement -> SM16 (the asm kernel's encoding)
          * output : SM16 -> signed
          * order  : the DIF iFFT emits BIT-REVERSED bins; undo the reversal
        """
        import numpy as np

        data = sym_bytes[:self.nsc * 4]
        if len(data) < self.nsc * 4:
            data = data + b"\x00" * (self.nsc * 4 - len(data))
        # THE NSC <= 2N/3 CONSTRAINT — a property of the TX subcarrier pack only.
        # The pack is an IN-PLACE circular copy (DFE_func_tx.c:554):
        #     memcpy_dst_circ(&src[N - nsc/2], src, nsc*4, src, N*4)
        # The destination starts at bin (N - nsc/2) while the source still occupies
        # [0, nsc), so they overlap once nsc > 2N/3 (= 682 for N=1024) and the copy
        # destroys (1.5*nsc - N) subcarriers BEFORE they are read.
        # Measured: 682 -> bit-exact | 684 -> 24 dB | 780 -> 4 dB.
        #
        # It never fires on silicon: the host caps LA9310 at nrb=51 -> 612 SCs
        # (check_dfe_cap_core_map.sh:528). 780 is only the firmware's fallback when
        # the host leaves NSC at 0 — an operating point the real system never picks.
        #
        # Refuse it here rather than return a corrupted symbol. NOTE this is a TX-PACK
        # limit, NOT a device limit: the RX chain is unaffected and is bit-exact at
        # nsc=780 (see scripts/dfe_rx_probe.py), so the check lives on the TX path,
        # not in __init__.
        nsc_max = 2 * IFFT_N // 3
        if self.nsc > nsc_max:
            raise ValueError(
                f"nsc={self.nsc} exceeds {nsc_max} (= 2*{IFFT_N}/3): the firmware's "
                f"in-place TX subcarrier pack would alias and corrupt "
                f"{int(1.5 * self.nsc) - IFFT_N} subcarriers. Real LA9310 hosts cap at "
                f"nrb=51 -> nsc=612. (RX is unaffected by this limit.)")

        iq = np.frombuffer(data, dtype="<i2").reshape(-1, 2).astype(np.int32)
        words = (to_sm16(iq[:, 0]) | (to_sm16(iq[:, 1]) << 16)).tolist()

        ntx0 = self.ctrl_u32("num_tx_sym_from_hiphy")
        self.write_dmem(self.syms.sym_queue_tx, words)
        # buf_stat: TIMEDOMAIN/FULL clear, symbol size in bits 16:0. A size of
        # ZERO makes the firmware advance its counters while processing nothing.
        self.ctrl_write_u32("buf_stat_ifft_in", IFFT_N * 4)
        self.s.write_reg(REG_DMA_FLAGS, 1 << 25)        # fetch-DMA "completed"
        self._run_until(
            lambda: self.ctrl_u32("num_tx_sym_from_hiphy") != ntx0,
            max_phases=max_phases, what=f"symbol {ntx0} processing")

        raw = np.asarray(self.read_dmem(self.syms.sym_queue_tx, IFFT_N),
                         dtype=np.uint32)
        z = from_sm16(raw & 0xFFFF) + 1j * from_sm16((raw >> 16) & 0xFFFF)
        return z[bitrev_index(10)]

    @property
    def cycles(self) -> int:
        return self.s.cycles()


def process_grid(eld: str | Path, grid, n_symbols: int, *,
                 nsc: int = 624) -> list[list[int]]:
    """Process ``n_symbols`` of an FdGrid through the DFE TX pipeline,
    one fresh firmware boot per symbol.

    A shared session can only process ~2 symbols before the pack stage
    blocks: the iFFT output ring's consumer (CFR/upsampler) is gated on
    tx_allowed timing that the standalone ISS does not generate. Rebooting
    per symbol keeps every symbol's pipeline state identical — which is
    also the cleanest semantics for old-vs-new A/B comparison. One boot +
    symbol costs ~50k phases (~1-2 s).
    """
    outs = []
    for i in range(n_symbols):
        d = DfeSession(eld, nsc=nsc)
        outs.append(d.process_symbol(grid.symbol_bytes(i)))
        del d
    return outs
