# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""Drive the VSPA2 ISS **natively**, in-process, with no server at all.

This is the prototype of a Docker-free, Wine-free, socket-free backend. It loads
NXP's simulator model (`libvspa2_16au.so`) straight into the Python process with
ctypes and calls it directly — the same model DLL the ccsim2 bridge reaches
through two hops (`ccs.exe` under Wine → `ccssim2.exe` over CCSIO).

STATUS: **WORKS.** The VSPA2 ISS runs entirely in this Python process and
executes a real kernel to a BIT-EXACT result. `selftest()` loads vec_add with the
native ELF loader, boots it, runs it, and its output buffer matches
`kernels/vec_add/tests/vectors/ref.hex` 256/256 at ~13000 cycles. No server, no
socket, no Wine, no Docker.

The full recipe (each piece read out of DWARF or traced from a real runsim run):
  * init: initialize_interface_v2 with an 11-slot largs array — devtype in [0],
    the terminal/step/break host callbacks in [1]/[6]/[7], config in [8], a real
    (non-NULL) buspp scratch in [9]. Then set the `currentDevice` global.
  * create_device_v2(0, 157, little_endian).
  * load: for each PT_LOAD segment, `set_space` by flags (PF_X code → PRAM 0,
    PF_W data → DRAM 1) via generic9, then wmem the bytes. BYTE ORDER: code raw,
    **data byte-reversed per 32-bit word** (raw data → 0/256, word-reversed →
    256/256). `set_space` (generic9) is mandatory or every access is "undefined
    mem_space".
  * boot: wreg(CONTROL=1) host_go, wreg(EXECPC=entry).
  * run: call exec_phase_v3 repeatedly. Read results with rmem (DRAM words come
    back big-endian to compare against the golden hex).

Not done, and not needed for a correct result — only for a tidy session:
  * Semihosting (printf/exit). After computing the output the kernel calls printf,
    whose syscall goes unanswered, so it traps back to the reset vector and loops
    main. We detect completion by watching the output buffer; a NativeSession
    would instead service the syscalls (terminal callback largs[1] for writes,
    detect the exit) so a run prints PASS and halts on its own.

Next: a `NativeSession` implementing the `Ccsim2Session` interface, so cwtcl, the
BreakpointManager, the DAP and the MCP server sit on it unchanged — with real
breakpoints via the Query_break_mode callback (largs[7]) instead of swbreak
patching. Gate on the 9-kernel cwproj regression staying bit-exact.

KEY DISCOVERIES (all read out of DWARF or traced from a real runsim run):
  * `largs[9]` must be a real (zeroed) buffer — the model's bus-pointer scratch.
  * `currentDevice` (a global) must equal the device type; a server-less
    instance does not set it in create_device.
  * **`generic9(dev, core, space, ret)` selects the memory space** — 0 = PRAM
    (code), 1 = DRAM (data). runsim's ELF loader calls it per segment. Without
    it `curr_mem_space` stays -1 and every access is "undefined mem_space".

────────────────────────────────────────────────────────────────────────────────
HOW THIS WAS FOUND (so nobody has to redo it)

The model .so and `runsim` both ship **full DWARF**, and both are unstripped. So
none of this is guesswork — every signature below was read out of the binaries:

    gdb -batch -ex 'ptype ccssim_callback_rmem' libvspa2_16au.so
    gdb -batch -x trace.gdb --args runsim -d vspa2_16au <elf>   # + info args

`runsim` resolves the model through `dlopen`/`dlsym`, so the API is just C
symbols. The contract:

    int  ccssim_callback_initialize_interface_v2(TU64 *largs)
    int  ccssim_callback_create_device_v2(int DeviceNumber, int DeviceTypeIndex, int le)
    void ccssim_callback_elf_loader(int Dev, int DevType, int SelectedCore, char *path)
    u64  ccssim_callback_exec_phase_v3(int Dev, int DevType, int *CoreGoStatusList, int *RetVal)
    int  ccssim_callback_rmem(int Dev, int DevType, int SelectedCore, u32 addr,
                              unsigned char *val, int nbytes)          # and wmem, same shape
    int  ccssim_callback_rreg_v2(int Dev, int DevType, int SelectedCore, int idx,
                                 u32 *out, u32 count)
    u64  ccssim_callback_GetCoreCycle_v2(int Dev, int DevType, int SelectedCore)

`largs` is an 11-slot u64 array — an argc/argv-ish blob the model parses:

    [0]  DeviceTypeIndex          157 = vspa2_16au (read out of runsim at the call)
    [1]  void (*)(char *)         terminal sink; the model prints THROUGH the host
    [6]  int  (*)(int, int)       Query_step_mode  — "should I single-step?"
    [7]  int  (*)(int, int, u32)  Query_break_mode — "is there a breakpoint at this PC?"
    [8]  char *                   config string (pipeline model, verbosity)
    [10] char *                   accurate-config string (empty for the fast model)
    others: 0

**`Query_break_mode` is the interesting one.** The model asks the host, per
instruction, whether to halt at that PC. A native backend therefore gets REAL
breakpoints — no `swbreak` patching, no PRAM rewriting, no restore-on-clear. The
functional ISS "has no hardware breakpoints" only because nothing was there to
answer that callback; over CCSIO the hook is simply not exposed.

MEMORY SPACES: `rmem`/`wmem` take a **SelectedCore**, not a space id — the space
is implied by the flat address (PRAM and DRAM sit at different bases in the
model's map). This differs from the CCS API, where space is an explicit argument.
Passing a space id where the core goes yields `error: undefined mem_space`.
"""
from __future__ import annotations

import ctypes
import os
from pathlib import Path

# ── the contract, as read out of the binaries ─────────────────────────────────
DEVICE_TYPE_VSPA2_16AU = 157        # runsim passes this for `-d vspa2_16au`
LITTLE_ENDIAN = 1
CORE0 = 0

# Register indices, same numbering the ccsim2 bridge uses (traced in runsim).
REG_CONTROL = 2                     # CONTROL[0] = host_go
REG_STATUS = 4                      # bit 0 = VCPU DONE — LATCHED by `done`
REG_EXECPC = 3001
REG_SP = 3014                       # stack pointer, in HALFWORDS (Gr.spIndex)

# Semihosting. runsim arms the trap by OR-ing the SWBREAK bit into the LAST BYTE
# of the original 8-byte bundle at `___syscall`+8 — the rest of the bundle (often
# a live `rts`!) is preserved and still executes, which is why resuming after the
# halt needs no PC rewind. Replacing the bundle with a canned trap instead breaks
# stub returns.
SWBREAK_BIT = 0x10
SYSCALL_NAMES = {1: "open", 2: "close", 3: "read", 4: "write", 5: "lseek",
                 6: "unlink", 7: "rename", 8: "access", 9: "clock", 10: "time",
                 11: "hsst_cleanup", 12: "mkdir", 13: "rmdir", 14: "system"}
SYSCALL_WRITE = 4
SYSCALL_CLOCK = 9
SYSCALL_TIME = 10

# Memory-space ids for set_space (traced from runsim's per-segment generic9).
SPACE_PRAM = 0                      # code
SPACE_DRAM = 1                      # data
SPACE_IPPU_PRAM = 2                 # IPPU microcode bank (own bank at vaddr 0)
SPACE_IPPU_DRAM = 3                 # IPPU data bank — SEPARATE bank, rebased to 0

# Sections holding IPPU DATA. runsim routes these to memspace 3 and REBASES them
# to address 0 (traced: bitRev64's ippu_dram at vaddr 0x7de00 is written at
# 0x0, 0x2, 0x4 ... in space 3). Loading them into VCPU DRAM at their raw vaddr
# instead puts them one line PAST the end of that bank (0x7de00/128 = line 4028,
# and VCPU DRAM is exactly 4028 lines) — the IPPU then reads a garbage arg base
# and decodes a nonsense loop count. NOTE: the LA9310 DFE image is different —
# its `.idram` lives INSIDE the unified DMEM at 0x4000 and stays in space 1;
# there the IPPU is located via the DMEM PARTITION instead (init_dmem_layout).
# ONLY `ippu_dram` (the bitRev family's own bank). The LA9310 DFE's `.idram`
# is NOT this: runsim loads it to space 1, and the firmware's own init writes
# `tx_bitrev_progress_ippu` (0x4064) into space 1 — the VCPU array really does
# back that range. Routing .idram to space 3 makes the VCPU and the host read
# two different memories.
_IPPU_DRAM_SECTIONS = {"ippu_dram"}

# Sections holding IPPU microcode. Their segments are PF_X at vaddr 0 — same
# flags and address as VCPU code — so only the section name tells them apart
# (mirrors framework/ccsim2/elf_loader.py).
_IPPU_PRAM_SECTIONS = {"ippu_pram", ".itext", ".ipram"}

# The pipeline configuration runsim uses for the fast (functional) model.
FAST_CONFIG = (b" pipeline_model stall_m2 12 beat_delay_m2 2 stall_m3 44 "
               b"beat_delay_m3 8  VERBOSE_LEVEL 0 RUNSIM_VERBOSE_LEVEL 0 ")

_TERMINAL_CB = ctypes.CFUNCTYPE(None, ctypes.c_char_p)
_STEP_CB     = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int, ctypes.c_int)
_BREAK_CB    = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_uint)


def _word_reverse(data: bytes) -> bytes:
    """Byte-reverse each 32-bit word — the DRAM (space 1) load transform.

    MEASURED FACTS about the model's memory API:
      * space 0 / space 2 (code banks): wmem/rmem are IDENTITY at any size.
      * space 1 (DRAM): an N-byte access BYTE-REVERSES within the access, AND
        byte addressing is halfword-swapped (byte b lands at b^1).
    Because write and read apply the same swap, a read-back ALWAYS returns what
    you wrote — so read-back CANNOT tell you what the CORE sees. It is not an
    oracle. (A subagent concluded "1-byte rmem is an identity probe" from this
    and got it exactly backwards: raw 1-byte writes leave the core seeing a
    halfword-swapped image, and vec_add then computes 0/256.)

    THE ONLY ORACLE IS WHETHER A KERNEL COMPUTES THE RIGHT ANSWER:
        python3 scripts/native_regression.py
    word-reversed data in 4-byte accesses  -> vec_add 256/256   (this one)
    raw data in 1-byte accesses            -> vec_add   0/256
    raw data in 2-byte accesses            -> vec_add  13/256

    Tail bytes are handled explicitly below; a short tail used to be silently
    ZEROED (the loop skipped len % 4), and a misaligned p_vaddr used to mix
    conventions. Both are now errors or handled, rather than silent corruption.
    """
    n = len(data)
    out = bytearray(n)
    full = n - (n % 4)
    for i in range(0, full, 4):
        out[i:i + 4] = data[i:i + 4][::-1]
    if n % 4:                       # reverse the short tail within itself
        out[full:] = data[full:][::-1]
    return bytes(out)


def _default_model_path() -> Path:
    env = os.environ.get("VSPA_MODEL")
    if env:
        return Path(env)
    repo = Path(__file__).resolve().parents[2]
    return repo / "environment" / "ccssim2" / "bin" / "libvspa2_16au.so"


class NativeModel:
    """One VSPA2 device, running inside this Python process."""

    def __init__(self, model_path: str | Path | None = None,
                 device_type: int = DEVICE_TYPE_VSPA2_16AU,
                 dmem_partition_lines: int | None = None) -> None:
        self.path = Path(model_path or _default_model_path())
        if not self.path.exists():
            raise FileNotFoundError(
                f"VSPA model not found: {self.path} — run ./setup.sh")
        self.dev = 0
        self.devtype = device_type
        self.terminal: list[str] = []
        # Semihosted program output (printf, via the ___syscall trap) and the
        # planted trap's word PC — None until an ELF carrying the symbol loads.
        self.stdout: list[str] = []
        self.syscall_pc: int | None = None
        # Armed breakpoints: bundle word PC → the original 8-byte bundle.
        self._bp_saved: dict[int, bytes] = {}
        # ELF symbol table of the loaded image: name → byte address.
        self.symbols: dict[str, int] = {}
        # Feeds the Query_break_mode callback, which the model NEVER consults in
        # free-run (verified against runsim too). Real breakpoints are the
        # SWBREAK-bit traps below; this stays wired only in case a debug mode
        # surfaces.
        self.breakpoints: set[int] = set()
        self.step_mode = False

        # exec_phase OWNS these across calls: it clears go[] while the core
        # drains a pipeline stall. Re-creating them per run_phases() call would
        # re-assert go mid-stall and corrupt execution (the core's PC then
        # oscillates instead of advancing), so they live for the whole session.
        self._go = (ctypes.c_int * 8)()
        self._ret = (ctypes.c_int * 8)()

        self.lib = ctypes.CDLL(str(self.path), mode=ctypes.RTLD_GLOBAL)
        self._bind()

        # THE SETUP RUNSIM DOES, IN RUNSIM'S ORDER (traced call-for-call):
        #   setrestr -> initialize_interface -> noIdleMode / PRAM_SIZE /
        #   using_runsim -> create_device -> [ELF load] -> pcs_lognruns /
        #   pipeline_model_on -> exec
        # The ORDER is load-bearing: `pipeline_model_on` comes AFTER the image
        # is loaded (see post_load_config, called from load_elf). Skipping any
        # of this leaves the ISS in a subtly different machine mode.
        self._setrestr(1, b"", b"")
        self._init_interface()
        self.partition_bytes = 0
        for cmd in (b"noIdleMode", b"set PRAM_SIZE 65535", b"using_runsim"):
            self._g6(0, 0, cmd, None)

        rc = self._create_device(self.dev, self.devtype, LITTLE_ENDIAN)
        if rc != 0:
            raise RuntimeError(f"create_device_v2 failed: {rc}")

        # The DMEM partition goes AFTER create_device (which allocates the memory
        # arrays) and BEFORE load_elf (which routes segments on partition_bytes).
        # DO NOT use `set vcpu_dmem_size` / `set ippu_dmem_size` here: they set
        # DMEM_LINES = 128+128 = 256, i.e. a 32 KB total DMEM, and the DFE's upper
        # buffers then run off the end of the array.
        if dmem_partition_lines:
            self._set_dmem_partition(dmem_partition_lines)

    # ── ctypes plumbing ───────────────────────────────────────────────────────
    def _bind(self) -> None:
        L = self.lib
        self._init = L.ccssim_callback_initialize_interface_v2
        self._init.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
        self._init.restype = ctypes.c_int

        self._create_device = L.ccssim_callback_create_device_v2
        self._create_device.argtypes = [ctypes.c_int] * 3
        self._create_device.restype = ctypes.c_int

        self._elf_loader = L.ccssim_callback_elf_loader
        self._elf_loader.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                     ctypes.c_char_p]
        self._elf_loader.restype = None

        self._exec_phase = L.ccssim_callback_exec_phase_v3
        self._exec_phase.argtypes = [ctypes.c_int, ctypes.c_int,
                                     ctypes.POINTER(ctypes.c_int),
                                     ctypes.POINTER(ctypes.c_int)]
        self._exec_phase.restype = ctypes.c_ulong

        self._rmem = L.ccssim_callback_rmem
        self._rmem.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int,
                               ctypes.c_uint32, ctypes.POINTER(ctypes.c_ubyte),
                               ctypes.c_int]
        self._rmem.restype = ctypes.c_int

        self._wmem = L.ccssim_callback_wmem
        self._wmem.argtypes = self._rmem.argtypes
        self._wmem.restype = ctypes.c_int

        self._rreg = L.ccssim_callback_rreg_v2
        self._rreg.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int,
                               ctypes.c_int, ctypes.POINTER(ctypes.c_uint32),
                               ctypes.c_uint32]
        self._rreg.restype = ctypes.c_int

        self._cycles = L.ccssim_callback_GetCoreCycle_v2
        self._cycles.argtypes = [ctypes.c_int] * 3
        self._cycles.restype = ctypes.c_uint64

        self._wreg = L.ccssim_callback_wreg
        self._wreg.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int,
                               ctypes.c_int, ctypes.POINTER(ctypes.c_uint32)]
        self._wreg.restype = ctypes.c_int

        # generic6(dev, devtype, cmd, out) = the model's config/command channel.
        # runsim drives the whole simulator mode through it — WITHOUT these the
        # model runs in a different arithmetic/pipeline mode and results differ
        # (vec_add came out off-by-one; the IPPU read garbage loop counts).
        # generic5(dev, chain_pos, cmd, out) = the "strcmd" channel CodeWarrior
        # uses (ccs::strcmd). The LA9310 CAS init sends the DMEM partition
        # through it; the IPPU's DMA finds its arg base via that mapping.
        self._g5 = L.ccssim_callback_generic5
        self._g5.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p,
                             ctypes.c_char_p]
        self._g5.restype = None

        self._g6 = L.ccssim_callback_generic6
        self._g6.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p,
                             ctypes.c_char_p]
        self._g6.restype = None

        self._setrestr = L.ccssim_callback_setrestr
        self._setrestr.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p]
        self._setrestr.restype = ctypes.c_int

        # generic9(dev, core, space, ret) = select the current memory space.
        self._g9 = L.ccssim_callback_generic9
        self._g9.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p,
                             ctypes.c_void_p]
        self._g9.restype = None

    def _init_interface(self) -> None:
        # The callbacks must stay referenced for the model's whole lifetime, or
        # Python collects them and the model calls into freed memory.
        @_TERMINAL_CB
        def on_terminal(msg):
            self.terminal.append((msg or b"").decode("utf-8", "replace"))

        @_STEP_CB
        def on_query_step(dev, devtype):
            return 1 if self.step_mode else 0

        @_BREAK_CB
        def on_query_break(dev, devtype, pc):
            # THE breakpoint hook: the model asks, per instruction, whether to
            # stop here. Answering from a Python set means a breakpoint costs
            # nothing and needs no code patching.
            return 1 if pc in self.breakpoints else 0

        self._cb_terminal = on_terminal
        self._cb_step = on_query_step
        self._cb_break = on_query_break

        self._cfg = ctypes.create_string_buffer(FAST_CONFIG, 1024)
        self._acc = ctypes.create_string_buffer(b"", 2048)
        # Slot 9 is `buspp`, a bus-pointer scratch buffer the model fills; it
        # must be a real (zeroed) buffer, not NULL, or init leaves state unset.
        # Sized generously ON PURPOSE: the model writes bus-pointer state here
        # and the true size is unknown. At 32 slots it overran into adjacent
        # ctypes allocations and corrupted results sporadically (single-byte
        # damage in computed vectors).
        self._buspp = (ctypes.c_uint64 * 4096)()

        largs = (ctypes.c_uint64 * 11)()
        largs[0] = self.devtype
        largs[1] = ctypes.cast(on_terminal, ctypes.c_void_p).value
        largs[6] = ctypes.cast(on_query_step, ctypes.c_void_p).value
        largs[7] = ctypes.cast(on_query_break, ctypes.c_void_p).value
        largs[8] = ctypes.cast(self._cfg, ctypes.c_void_p).value
        largs[9] = ctypes.cast(self._buspp, ctypes.c_void_p).value
        largs[10] = ctypes.cast(self._acc, ctypes.c_void_p).value
        self._largs = largs

        rc = self._init(largs)
        if rc != 0:
            raise RuntimeError(f"initialize_interface_v2 failed: {rc}")
        # The model's device-type registry check keys on this global; create_
        # device does not set it for a directly-driven (server-less) instance.
        ctypes.c_int.in_dll(self.lib, "currentDevice").value = self.devtype

    # ── public surface ────────────────────────────────────────────────────────
    def set_space(self, space: int, core: int = CORE0) -> None:
        """Select the memory space (0 = PRAM/code, 1 = DRAM/data) for subsequent
        rmem/wmem. This is what `generic9` does — the model's ELF loader calls it
        per segment. Without it, `curr_mem_space` stays -1 and every memory
        access fails with "undefined mem_space"."""
        self._g9(self.dev, core, ctypes.c_void_p(space), None)

    def load_elf(self, path: str | Path, core: int = CORE0,
                 clear_bss: bool = True) -> int:
        """Load a VSPA ELF the way runsim does — parse it and write each PT_LOAD
        segment ourselves. ``ccssim_callback_elf_loader`` is a near-empty stub;
        runsim ignores it.

        Each segment goes to the bank its flags select: PF_X (code) → PRAM
        (set_space 0), PF_W (data) → DRAM (set_space 1). Bytes are written raw at
        the segment's vaddr, so nothing is lost (a byte-granular replay of a
        runsim trace silently dropped the high byte of every 2-byte write, which
        corrupted every non-round input value). Returns the entry point (words).
        """
        from elftools.elf.elffile import ELFFile
        from elftools.elf.constants import P_FLAGS

        path = Path(path)
        with path.open("rb") as f:
            elf = ELFFile(f)
            entry = elf.header.e_entry
            # Segments holding IPPU microcode are PF_X at vaddr 0, just like
            # VCPU code — classify by section membership (keyed on p_offset,
            # which is unique per segment).
            def _segs_for(names):
                return {
                    seg.header.p_offset
                    for seg in elf.iter_segments()
                    if seg.header.p_type == "PT_LOAD"
                    for sec in elf.iter_sections()
                    if sec["sh_size"] and sec.name in names
                    and sec["sh_type"] != "SHT_NOBITS"
                    and seg.header.p_offset <= sec["sh_offset"]
                    < seg.header.p_offset + max(seg.header.p_filesz, 1)
                }
            ippu_offsets = _segs_for(_IPPU_PRAM_SECTIONS)
            ippu_dram_offsets = _segs_for(_IPPU_DRAM_SECTIONS)
            for seg in elf.iter_segments():
                h = seg.header
                if h["p_type"] != "PT_LOAD":
                    continue
                is_code = bool(h["p_flags"] & P_FLAGS.PF_X)
                rebase = 0
                if h.p_offset in ippu_offsets:
                    space = SPACE_IPPU_PRAM
                elif h.p_offset in ippu_dram_offsets:
                    space = SPACE_IPPU_DRAM
                    # bitRev-family: its own bank, linker-addressed from 0x7de00
                    # but the model indexes it from 0.
                    rebase = h["p_vaddr"]
                elif is_code:
                    space = SPACE_PRAM
                elif (self.partition_bytes
                      and h["p_vaddr"] >= self.partition_bytes):
                    # PARTITIONED DMEM (LA9310): the VCPU and IPPU arrays are
                    # each `partition_bytes` long and BOTH indexed from 0, so a
                    # DMEM address at/above the boundary is IPPU-array offset
                    # (vaddr - partition_bytes). The VCPU's own loads/stores are
                    # routed there by the model at runtime — this makes the host
                    # image agree with what the core and the IPPU actually see.
                    space = SPACE_IPPU_DRAM
                    rebase = self.partition_bytes
                else:
                    space = SPACE_DRAM
                self.set_space(space, core)
                data = seg.data()                       # filesz bytes
                if clear_bss and h["p_memsz"] > h["p_filesz"]:
                    data = data + b"\x00" * (h["p_memsz"] - h["p_filesz"])
                # BYTE ORDER: code banks take raw bytes (wmem is identity
                # there); DRAM needs each 32-bit word byte-reversed and written
                # in 4-byte accesses. See _word_reverse — read-back is NOT an
                # oracle for this, only whether kernels compute correctly is.
                if space in (SPACE_DRAM, SPACE_IPPU_DRAM):
                    if space == SPACE_DRAM and h["p_vaddr"] % 4:
                        raise RuntimeError(
                            f"data segment vaddr 0x{h['p_vaddr']:x} is not "
                            f"4-byte aligned; the DRAM path writes 4-byte "
                            f"accesses (see _word_reverse)")
                    if len(data) % 4:       # pad so every access is a full word
                        data = data + b"\x00" * (4 - len(data) % 4)
                    data = _word_reverse(data)
                    unit = 4
                else:
                    unit = 1
                self._write_span(h["p_vaddr"] - rebase, data, core, unit=unit)
            self._read_symbols(elf)
            self._plant_syscall_trap(core)
        self.post_load_config()
        return entry

    # ── ELF symbols: how a host-side runner finds a kernel's I/O buffers ──────
    def _read_symbols(self, elf) -> None:
        self.symbols = {}
        symtab = elf.get_section_by_name(".symtab")
        if symtab is None:
            return
        for sym in symtab.iter_symbols():
            if sym.name:
                self.symbols[sym.name] = sym["st_value"]

    def symbol(self, name: str) -> int:
        """Byte address of ``name`` in the loaded image.

        This is what lets a host write a kernel's input buffer and read its
        output buffer directly, instead of the kernel fopen()-ing vector files:
        the native ISS services only write/clock/time syscalls, so open/read
        return -1 and any file-based harness fails (see service_syscall).

        The VSPA C compiler prefixes C identifiers with an underscore (`main` is
        `_main` in .symtab), so a C name is accepted as written.
        """
        for key in (name, f"_{name}"):
            if key in self.symbols:
                return self.symbols[key]
        objs = sorted(n for n in self.symbols if not n.startswith("."))
        raise KeyError(f"symbol {name!r} (nor {'_' + name!r}) in the loaded "
                       f"image. {len(objs)} symbols; e.g. {objs[:8]}")

    def _plant_syscall_trap(self, core: int) -> None:
        """Arm semihosting the way runsim does: OR the SWBREAK bit into the
        bundle at `___syscall`+8, so exec_phase raises ret[0] when the kernel
        makes a syscall. Only the last byte changes — the bundle's own
        instructions (often the stub's own `rts`) keep executing."""
        self.syscall_pc = None
        for name in ("___syscall", "__fake_syscall"):
            if name in self.symbols:
                addr = self.symbols[name] + 8
                bundle = bytearray(self._read_bundle(addr, core))
                bundle[7] |= SWBREAK_BIT
                self._write_bundle(addr, bytes(bundle), core)
                self.syscall_pc = addr // 4
                return

    def _read_bundle(self, addr: int, core: int) -> bytes:
        """The raw 8 bytes at a PRAM address. Read BYTE-WISE: rmem/wmem
        byte-reverse within the access size, and only a 1-byte access is an
        identity on code (which is also how load_elf writes it)."""
        self.set_space(SPACE_PRAM, core)
        return bytes(self.read_mem(addr + i, 1, core)[0] for i in range(8))

    def _write_bundle(self, addr: int, data: bytes, core: int) -> None:
        self.set_space(SPACE_PRAM, core)
        self._write_span(addr, data, core, unit=1)

    # VSPA object layout (DWARF): _vspa[] stride 0x25ca0
    _OFF_DMEM_LINES = 0x25B38       # total lines (VCPU + IPPU); vspa2_16au: 5052
    _OFF_VDMEM_LINES = 0x25B40      # THE MASTER — sim_cycle recomputes the
                                    # boundary from this EVERY cycle
    _OFF_LINE_BDRY = 0x23C54        # darb_state.rubyDramLineBdry (derived)

    def _set_dmem_partition(self, vcpu_lines: int) -> None:
        """Split DMEM into a VCPU half and an IPPU half at `vcpu_lines` x 128 B.

        THE DETAILS ARE ALL LOAD-BEARING:

        * The command channel is **generic6**, and it is the ONLY callback that
          reaches the model's command parser (generic5 and
          simulator_interpretter never do). Its 2nd argument is `2 * coreIdx`,
          NOT the device type — pass 0. With devtype (157) the model computes
          157>>1 = 78 >= nrVspaCores and silently DROPS the string.
        * Use the **TWO-argument** form. The one-arg form only writes the derived
          boundary, which `sim_cycle` then CLOBBERS every cycle by recomputing it
          from VDMEM_LINES. The two-arg form writes VDMEM_LINES/IDMEM_LINES and
          REALLOCATES both MemArrays — it is the only durable one.
        * Preserve TOTAL DMEM. `set vcpu_dmem_size 128` + `set ippu_dmem_size
          128` gives DMEM_LINES = 256 (32 KB total) and the DFE's upper buffers
          then run off the end of the array. Read the device's real total and
          give the remainder to the IPPU.
        * Must run AFTER create_device (which allocates the arrays) and BEFORE
          load_elf (which routes segments on self.partition_bytes).

        Verified: the model replies "Command executed successfully", the boundary
        lands at byte 0x4000, and the firmware's own store to VCPU address 0x4064
        then appears in the IPPU array at offset 0x64.
        """
        vspa = ctypes.addressof(ctypes.c_char.in_dll(self.lib, "_vspa"))
        total = ctypes.c_uint32.from_address(vspa + self._OFF_DMEM_LINES).value
        if not 0 < vcpu_lines < total:
            raise ValueError(f"vcpu_lines {vcpu_lines} outside DMEM ({total} lines)")
        out = ctypes.create_string_buffer(4096)
        self._g6(0, 0,
                 b"set dmem_partition %d %d" % (vcpu_lines, total - vcpu_lines),
                 out)
        if out.value != b"Command executed successfully":
            raise RuntimeError(f"set dmem_partition rejected: {out.value!r}")
        self.partition_bytes = vcpu_lines * 128

    def dmem_geometry(self) -> dict:
        """(diagnostic) the model's live DMEM line counts and boundary."""
        vspa = ctypes.addressof(ctypes.c_char.in_dll(self.lib, "_vspa"))
        g = lambda o: ctypes.c_uint32.from_address(vspa + o).value
        return {"DMEM_LINES": g(self._OFF_DMEM_LINES),
                "VDMEM_LINES": g(self._OFF_VDMEM_LINES),
                "line_bdry": g(self._OFF_LINE_BDRY)}

    def init_dmem_layout(self, vcpu_lines: int = 128, ippu_lines: int = 128,
                         partition_line: int = 128) -> None:
        """The LA9310 CAS DMEM-init (LA9310_CAS_Init.tcl), in 128-byte lines.

        ⚠ DOES NOT CURRENTLY TAKE EFFECT IN-PROCESS. The model accepts these
        strings but the runtime partition does not move: the VCPU's stores above
        0x4000 still land in VSPA::dmem, not VSPA::ippuDmem (verified — the DFE
        firmware's own `tx_bitrev_progress_ippu` at 0x4064 reads back from space
        1, never space 3). Tried and rejected: generic5, generic6, the
        simulator_interpretter callback, and the init config string.

        It is NOT required for the IPPU fix — bitRev64 is 64/64 bit-exact with
        or without it, because its linker script gives IPPU data its OWN bank
        (`ippu_dram` -> space 3), which matches the model's two-array design.

        It matters only for the LA9310 DFE, whose UNIFIED DMEM expects the IPPU
        to see the upper half of the VCPU's memory. Until the partition can be
        driven, that firmware's IPPU bit-reverse cannot complete in-process:
        the VCPU writes the IPPU's argument block (which its own source says
        "must be on IPPU DMEM due to Errata") into the VCPU array, while the
        IPPU reads its own. See docs/DFE_FR1FR2_SIM.md.
        """
        # generic6 is the model's COMMAND channel (the same one runsim uses for
        # "noIdleMode" / "set PRAM_SIZE 65535"). generic5 is not — it silently
        # ignores these, which is why the partition never took effect.
        out = ctypes.create_string_buffer(4096)
        for name, val in (("vcpu_dmem_size", vcpu_lines),
                          ("ippu_dmem_size", ippu_lines),
                          ("dmem_partition", partition_line)):
            self._g6(0, self.devtype, f"set {name} {val}".encode(), out)

    def post_load_config(self) -> None:
        """The two generic6 commands runsim issues AFTER loading the image."""
        for cmd in (b"pcs_lognruns", b"pipeline_model_on"):
            self._g6(0, self.devtype, cmd, None)

    def _write_span(self, addr: int, data: bytes, core: int,
                    unit: int = 1) -> None:
        """Write a byte span via wmem, in ``unit``-byte accesses.

        THE ACCESS SIZE IS PART OF THE SEMANTICS, not just a performance knob:
        a 4-byte wmem is a 32-bit word write and the model BYTE-REVERSES it,
        whereas 1- and 2-byte writes go in raw. Writing data as 4-byte chunks
        therefore silently swaps the two halfwords of every word. runsim itself
        writes code 1 byte at a time and data in 2-byte halfwords — mirror that
        exactly (verified: with unit=1/2 our image is byte-identical to
        runsim's for all three banks).
        """
        buf = (ctypes.c_ubyte * len(data)).from_buffer_copy(data)
        off, n = 0, len(data)
        while off < n:
            chunk = unit if (n - off) >= unit and (addr + off) % unit == 0 else 1
            sub = (ctypes.c_ubyte * chunk).from_buffer(buf, off)
            rc = self._wmem(self.dev, self.devtype, core, addr + off, sub, chunk)
            if rc != 0:
                raise RuntimeError(f"wmem(0x{addr + off:x}, {chunk}) failed: rc={rc}")
            off += chunk

    def read_mem(self, addr: int, nbytes: int, core: int = CORE0) -> bytes:
        buf = (ctypes.c_ubyte * nbytes)()
        rc = self._rmem(self.dev, self.devtype, core, addr, buf, nbytes)
        if rc != 0:
            raise RuntimeError(f"rmem(0x{addr:x}, {nbytes}) failed: rc={rc} "
                               f"{''.join(self.terminal[-1:])}")
        return bytes(buf)

    def read_halfwords(self, addr: int, count: int, core: int = CORE0) -> list[int]:
        """Read ``count`` 16-bit halfwords — the natural unit of VSPA data memory.

        rmem/wmem BYTE-REVERSE WITHIN THE ACCESS SIZE (1 byte raw, 2 bytes
        swapped, 4 bytes fully reversed), so a 2-byte access already undoes the
        DMEM halfword order and the result decodes little-endian.
        """
        return [int.from_bytes(self.read_mem(addr + 2 * i, 2, core), "big")
                for i in range(count)]

    def write_halfwords(self, addr: int, values: list[int],
                        core: int = CORE0) -> None:
        buf = b"".join((v & 0xFFFF).to_bytes(2, "big") for v in values)
        self._write_span(addr, buf, core, unit=2)

    def read_dram_words(self, addr: int, count: int, core: int = CORE0) -> list[int]:
        """Read ``count`` 32-bit words from DRAM. A 32-bit value occupies two
        halfwords, LOW half first (little-endian halfword order)."""
        self.set_space(SPACE_DRAM, core)
        hw = self.read_halfwords(addr, 2 * count, core)
        return [hw[2 * i] | (hw[2 * i + 1] << 16) for i in range(count)]

    def write_mem(self, addr: int, data: bytes, core: int = CORE0) -> None:
        buf = (ctypes.c_ubyte * len(data))(*data)
        rc = self._wmem(self.dev, self.devtype, core, addr, buf, len(data))
        if rc != 0:
            raise RuntimeError(f"wmem(0x{addr:x}, {len(data)}B) failed: rc={rc}")

    def read_reg(self, index: int, count: int = 1, core: int = CORE0) -> list[int]:
        out = (ctypes.c_uint32 * count)()
        rc = self._rreg(self.dev, self.devtype, core, index, out, count)
        if rc != 0:
            raise RuntimeError(f"rreg_v2(idx={index}) failed: rc={rc}")
        return list(out)

    def write_reg(self, index: int, value: int, core: int = CORE0) -> None:
        val = ctypes.c_uint32(value)
        self._wreg(self.dev, self.devtype, core, index, ctypes.byref(val))

    def boot(self, entry_word: int = 0, core: int = CORE0) -> None:
        """Arm the core to run: host_go + entry PC. Mirrors what runsim writes
        between loading the image and the first exec_phase (traced)."""
        self.write_reg(REG_CONTROL, 1, core)       # CONTROL[0] = host_go
        self.write_reg(REG_EXECPC, entry_word, core)
        self._go[0] = 1                            # armed once; exec_phase owns it

    def cycles(self, core: int = CORE0) -> int:
        return int(self._cycles(self.dev, self.devtype, core))

    def run_phases(self, max_phases: int = 5_000_000) -> tuple[int, int]:
        """Execute up to ``max_phases`` simulator phases. Returns (phases, ret).

        CRITICAL: ``go[0]`` is set ONCE and then left alone — ``exec_phase``
        owns it. The pipeline model stalls (config: stall_m2 12, stall_m3 44), and
        during a stall ``exec_phase`` clears ``go[0]`` to mean "the core is not
        advancing this phase, it is draining the pipeline". Re-asserting go=1 each
        phase FORCES the core through its own stalls and corrupts execution — the
        PC then oscillates instead of advancing. With go left to exec_phase, the
        kernel runs and computes correct results (vec_add's output buffer fills
        with the expected 0x6000 Q15 values).
        """
        go, ret = self._go, self._ret
        for i in range(max_phases):
            self._exec_phase(self.dev, self.devtype, go, ret)
            if ret[0] != 0:
                return i + 1, ret[0]
        return max_phases, ret[0]

    # ── breakpoints (SWBREAK-bit traps, same mechanism as the syscall) ────────
    def set_breakpoint(self, word_pc: int, core: int = CORE0) -> int:
        """Arm a breakpoint on the bundle containing ``word_pc``. The bundle's
        own instructions still execute and the core halts right AFTER it, so a
        resume needs no PC rewind — just keep exec-ing."""
        bundle_pc = word_pc & ~1                    # bundles are 2 words
        if bundle_pc not in self._bp_saved:
            orig = self._read_bundle(bundle_pc * 4, core)
            armed = bytearray(orig)
            armed[7] |= SWBREAK_BIT
            self._write_bundle(bundle_pc * 4, bytes(armed), core)
            self._bp_saved[bundle_pc] = orig
        return bundle_pc

    def clear_breakpoint(self, word_pc: int, core: int = CORE0) -> None:
        orig = self._bp_saved.pop(word_pc & ~1, None)
        if orig is not None:
            self._write_bundle((word_pc & ~1) * 4, orig, core)

    def clear_all_breakpoints(self, core: int = CORE0) -> None:
        for pc in list(self._bp_saved):
            self.clear_breakpoint(pc, core)

    # ── semihosting ──────────────────────────────────────────────────────────
    def is_done(self, core: int = CORE0) -> bool:
        """True once the program executed `done` (reg 4 bit 0). LATCHED — so it
        must be cleared before a re-boot, or the next run reads it as finished
        before it has run a single phase (see reset_core)."""
        return bool(self.read_reg(REG_STATUS, 1, core)[0] & 1)

    def _read_dram_u32(self, addr: int, core: int) -> int:
        return int.from_bytes(self.read_mem(addr, 4, core), "big")

    def service_syscall(self, core: int = CORE0) -> str:
        """Service the pending syscall (ret[0] != 0 and execpc == syscall_pc).

        Only `write` is real — it is what carries printf. open/read/close and the
        rest return -1: THE NATIVE ISS HAS NO FILE I/O. A kernel harness that
        fopen()s its input vectors gets a NULL FILE* here; host-side runners must
        write the buffers into DRAM directly (see symbol()).
        """
        sp2 = self.read_reg(REG_SP, 1, core)[0] * 2      # sp is in HALFWORDS
        self.set_space(SPACE_DRAM, core)
        stype = self._read_dram_u32(sp2 - 4, core)
        retval_ptr = self._read_dram_u32(sp2 - 8, core)
        a1 = self._read_dram_u32(sp2 - 12, core)
        a2 = self._read_dram_u32(sp2 - 16, core)
        a3 = self._read_dram_u32(sp2 - 20, core)

        if stype == SYSCALL_WRITE:                       # write(fd=a1, buf=a2, n=a3)
            text = bytes(self.read_mem(a2 * 2 + 2 * i, 1, core)[0]
                         for i in range(a3)).decode("ascii", "replace")
            self.stdout.append(text)
            rv = a3
            desc = f"write(fd={a1}, n={a3}): {text!r}"
        elif stype in (SYSCALL_CLOCK, SYSCALL_TIME):
            # 0, NOT the cycle count: NXP harnesses calibrate timing loops from
            # clock() deltas and feed them to `set.loop`, which faults the ISS
            # above 65536. Neither CodeWarrior nor the bridge ever answers this
            # syscall, so 0 is what every golden vector was made against.
            rv = 0
            desc = f"{SYSCALL_NAMES[stype]}() = 0 (unanswered-compatible)"
        else:
            rv = 0xFFFFFFFF                              # -1: fail softly
            desc = f"{SYSCALL_NAMES.get(stype, stype)}(...) unhandled, rv=-1"

        # The return value goes back as two big-endian halfwords at retval_ptr (a
        # HALFWORD address) — byte-for-byte what runsim's do_syscall writes, and
        # the only thing it writes: the frame is left alone, the caller pops it.
        self.write_mem(retval_ptr * 2, (rv & 0xFFFF).to_bytes(2, "big"), core)
        self.write_mem(retval_ptr * 2 + 2,
                       (rv >> 16 & 0xFFFF).to_bytes(2, "big"), core)
        return desc

    def run_to_exit(self, max_phases: int = 20_000_000, core: int = CORE0,
                    done_poll: int = 64) -> tuple[str, int]:
        """Run the booted program to completion, servicing semihosted syscalls.

        Returns (reason, phases). 'done' = clean exit (printf output is in
        self.stdout); 'trap' = a ret[0] halt that is NOT the syscall trap (e.g. a
        breakpoint — the pc is left there for the caller); 'limit' = ran out.

        DONE (reg 4 bit 0) is latched, so polling every ``done_poll`` phases
        cannot miss it.
        """
        go, ret = self._go, self._ret
        for i in range(1, max_phases + 1):
            self._exec_phase(self.dev, self.devtype, go, ret)
            if ret[0] != 0:
                if self.read_reg(REG_EXECPC, 1, core)[0] == self.syscall_pc:
                    self.service_syscall(core)
                    ret[0] = 0
                    continue
                # `done` ALSO raises ret[0] (and latches reg 4 bit 0) — so a
                # halt is only a real trap if the program has not finished.
                return ("done" if self.is_done(core) else "trap"), i
            if i % done_poll == 0 and self.is_done(core):
                return "done", i
        return "limit", max_phases

    # ── running many programs in ONE process ─────────────────────────────────
    # The model keeps per-device state that a re-boot does not clear. The bridge
    # never had to care because it restarts ccssim2 for every testcase; in-process
    # we must heal it, or the 2nd..15th boot in a session misbehaves.
    _RUBY_HALT_CODE = 0x11FEC   # halt code, re-reported EVERY phase once latched
    _RUBY_HALT_FLAG = 0x120B7   # its companion flag
    _RUBY_JSR_DEPTH = 67904     # RUBY_STATE.jsr_depth
    _RU_JSR_DEPTH = 20548       # RU_STATE.jsr_depth

    def reset_core(self, core: int = CORE0) -> None:
        """Make the device fit to boot another program. Call between runs.

        Three things leak across a boot:
          * the DONE latch (reg 4 bit 0) — else the next run "finishes" instantly;
          * the model's halt latch + a core_status of {started, !idle, !busy},
            a dead state from which start_vspa() never relaunches the core (this
            is the bridge's famous wedged core);
          * jsr_depth — every swbreak halt inside a jsr'd routine (i.e. every
            printf) leaks one nesting level, because its `rts` never runs. After
            ~15 the ISS faults with "TOO MANY NESTED JSRS".
        """
        self.write_reg(REG_STATUS, 0, core)
        try:
            ruby = (ctypes.c_void_p * 13).in_dll(self.lib, "_ruby_state")
            cs = (ctypes.c_ubyte * 52).in_dll(self.lib, "core_status")
        except ValueError:                      # symbol absent: nothing to heal
            return
        p = ruby[self.dev]
        if p:
            ctypes.c_int.from_address(p + self._RUBY_HALT_CODE).value = 0
            ctypes.c_ubyte.from_address(p + self._RUBY_HALT_FLAG).value = 0
        for sym, off in (("_ru_state", self._RU_JSR_DEPTH),
                         ("_ruby_state", self._RUBY_JSR_DEPTH)):
            try:
                arr = (ctypes.c_void_p * 13).in_dll(self.lib, sym)
            except ValueError:
                continue
            if arr[self.dev]:
                ctypes.c_uint32.from_address(arr[self.dev] + off).value = 0
        # Heal ONLY the dead state {started, !idle, !busy}. A busy core is
        # re-drivable in place; flipping it to idle would make start_vspa()
        # relaunch from PMEM 0 and re-run crt0 over live DRAM.
        base = self.dev * 4
        if cs[base] and not cs[base + 1] and not cs[base + 2]:
            cs[base + 1] = 1
            cs[base + 3] = 0


def selftest() -> int:
    """Run vec_add end-to-end and check the output is bit-exact — no server."""
    import sys
    from pathlib import Path as _P

    repo = _P(__file__).resolve().parents[2]
    elf = repo / "build" / "tests" / "test_vec_add_vspa2_16au.elf"
    ref_hex = repo / "kernels" / "vec_add" / "tests" / "vectors" / "ref.hex"
    if not elf.exists() or not ref_hex.exists():
        print("selftest needs a built vec_add — run: "
              "python3 scripts/run_test.py --kernel vec_add --au 16")
        return 1

    ref = [int(l.strip().rstrip(","), 16) for l in ref_hex.read_text().splitlines()
           if l.strip().startswith("0x")]

    m = NativeModel()
    print(f"model      : {m.path.name}  (device type {m.devtype} = vspa2_16au)")
    entry = m.load_elf(elf)                 # native loader: set_space per segment
    m.boot(entry)                           # host_go + execpc
    print(f"loaded     : {elf.name}, entry={entry}")

    # Run until the output buffer matches the golden vectors. vec_add writes its
    # result to _z_buf (0x1400, DRAM); it is bit-exact after ~9k phases.
    best = 0
    for i in range(1, 12000):
        m.run_phases(1)                     # one phase; go is owned by exec_phase
        if i % 500 == 0:
            got = m.read_dram_words(0x1400, len(ref))
            n = sum(1 for a, b in zip(ref, got) if a == b)
            best = max(best, n)
            if n == len(ref):
                print(f"result     : z_buf {n}/{len(ref)} BIT-EXACT vs ref.hex "
                      f"at phase {i}, {m.cycles()} cycles")
                print("status     : the VSPA ISS ran a real kernel to a bit-exact "
                      "result, in-process — no server, no Wine, no Docker")
                return 0
    print(f"result     : best {best}/{len(ref)} — did not fully converge")
    return 1


if __name__ == "__main__":
    raise SystemExit(selftest())
