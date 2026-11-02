# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""A Ccsim2Session-shaped facade over the native in-process VSPA model.

The ccsim2 bridge (`framework/ccsim2/session.py`) reaches the simulator DLL
through two Wine hops. This class reaches the SAME model directly, in-process,
via ctypes (`framework/native/model.py`). Everything above the session — the
cwtcl engine, the DAP, the MCP server — talks only to this small method surface,
so a working NativeSession lets them run with no server, no Wine, no Docker.

STATUS: the compute path is proven bit-exact (see model.py and
`native_verify` below — vec_add loads, runs, and its output matches the golden
vectors 256/256). What is NOT wired yet, and why the method set below is the
"compute + inspect" subset rather than the full debugger surface:

  * Semihosting (printf/exit) is not serviced, so a kernel loops main after
    computing its result instead of halting on a clean exit. `run_to_output`
    therefore detects completion by watching the output buffer converge, not by
    a swbreak halt. The NXP cwproj harnesses end in __swbreak(), which the model
    only honours in a debug mode we have not activated (the Query_break_mode
    callback is never invoked in free-run) — wiring that is the next step for a
    full cwtcl regression.
  * So `run_to_swbreak` / breakpoints are deliberately absent here; add them once
    the debug mode (or a swbreak-decode hook) is understood.

BYTE ORDER (the subtlety that cost a day): the model stores code big-endian and
data little-endian. `NativeModel.load_elf` already handles the load side (code
raw, data word-reversed). For read/write of DRAM *words* through this session we
present them big-endian, matching the golden .hex vectors — see read_words.
"""
from __future__ import annotations

from pathlib import Path

from .model import (CORE0, REG_CONTROL, REG_EXECPC, SPACE_DRAM, SPACE_PRAM,
                    NativeModel)


class NativeSession:
    """Compute-and-inspect session over one native VSPA2 device."""

    def __init__(self, model_path: str | Path | None = None) -> None:
        self.m = NativeModel(model_path)
        self.elf: str | None = None
        self.entry = 0

    # ── lifecycle ─────────────────────────────────────────────────────────────
    def load_elf(self, path: str | Path) -> int:
        self.entry = self.m.load_elf(path)
        self.elf = str(path)
        return self.entry

    def boot(self, start_pc: int | None = None) -> None:
        """Arm the core: host_go + execpc. Defaults to the ELF entry."""
        self.m.boot(self.entry if start_pc is None else start_pc)

    # ── execution ─────────────────────────────────────────────────────────────
    def run_phases(self, n: int) -> None:
        self.m.run_phases(n)

    def run_to_output(self, addr: int, golden: list[int], *,
                      chunk: int = 500, max_phases: int = 200_000) -> tuple[int, int]:
        """Run until the ``len(golden)`` DRAM words at ``addr`` match ``golden``,
        or ``max_phases`` elapse. Returns (best_match_count, phases_run).

        This is the stand-in for a swbreak halt while semihosting is unwired: the
        kernel computes the result, and we stop as soon as the output buffer is
        the golden result. The result is bit-exact well before the kernel loops
        on its unanswered printf syscall.
        """
        best = 0
        ran = 0
        while ran < max_phases:
            self.m.run_phases(chunk)
            ran += chunk
            got = self.read_words(addr, len(golden))
            n = sum(1 for a, b in zip(golden, got) if a == b)
            best = max(best, n)
            if n == len(golden):
                return n, ran
        return best, ran

    # ── memory / registers ────────────────────────────────────────────────────
    def read_words(self, addr: int, count: int, space: int = SPACE_DRAM,
                   core: int = CORE0) -> list[int]:
        """Read ``count`` 32-bit words. Each is two halfwords, LOW half first;
        every halfword is stored big-endian (see NativeModel._halfword_swap)."""
        self.m.set_space(space, core)
        return [int.from_bytes(self.m.read_mem(addr + 4 * i, 4, core), "big")
                for i in range(count)]

    def write_words(self, addr: int, words: list[int], space: int = SPACE_DRAM,
                    core: int = CORE0) -> None:
        self.m.set_space(space, core)
        for i, w in enumerate(words):
            self.m.write_mem(addr + 4 * i, int(w).to_bytes(4, "big"), core)

    def read_halfwords(self, addr: int, count: int, space: int = SPACE_DRAM,
                       core: int = CORE0) -> list[int]:
        self.m.set_space(space, core)
        return self.m.read_halfwords(addr, count, core)

    def write_halfwords(self, addr: int, values: list[int],
                        space: int = SPACE_DRAM, core: int = CORE0) -> None:
        self.m.set_space(space, core)
        self.m.write_halfwords(addr, values, core)

    def read_reg(self, index: int, core: int = CORE0) -> int:
        return self.m.read_reg(index, 1, core)[0]

    def write_reg(self, index: int, value: int, core: int = CORE0) -> None:
        self.m.write_reg(index, value, core)

    def read_pc(self, core: int = CORE0) -> int:
        return self.m.read_reg(REG_EXECPC, 1, core)[0]

    def cycles(self, core: int = CORE0) -> int:
        return self.m.cycles(core)


def native_verify(elf: str | Path, out_addr: int, ref_hex: str | Path) -> tuple[int, int]:
    """Load, run and check one kernel natively. Returns (matched, total)."""
    ref = [int(l.strip().rstrip(","), 16)
           for l in Path(ref_hex).read_text().splitlines()
           if l.strip().startswith("0x")]
    s = NativeSession()
    s.load_elf(elf)
    s.boot()
    matched, _ = s.run_to_output(out_addr, ref)
    return matched, len(ref)


def _out_symbol_addr(elf: str | Path) -> int | None:
    """Best-effort output-buffer address from the ELF symbol table. Harness
    conventions vary (y_out / z_buf / y_buf / out), so this covers the common
    names in priority order; a kernel whose output symbol is not one of these
    needs an explicit --out-addr and is reported as skipped, not failed."""
    from elftools.elf.elffile import ELFFile
    with open(elf, "rb") as f:
        st = ELFFile(f).get_section_by_name(".symtab")
        if st is None:
            return None
        names = {s.name: s["st_value"] for s in st.iter_symbols()}
    for want in ("_y_out", "y_out", "_y_out_i16", "_z_buf", "z_buf",
                 "_y_buf", "y_buf", "_out", "out",
                 # FFT-family harnesses transform in place in a work buffer and
                 # compare against a compiled-in golden. Without these two names
                 # the regression SKIPPED both FFT kernels — which is exactly how
                 # a 1024-point FFT went unverified on this ISS for months.
                 "_WORK_BUF", "work_buf"):
        if want in names:
            return names[want]
    return None


if __name__ == "__main__":
    import argparse
    import sys
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("ref")
    ap.add_argument("--out-addr", type=lambda s: int(s, 0), default=None)
    a = ap.parse_args()
    out = a.out_addr if a.out_addr is not None else _out_symbol_addr(a.elf)
    if out is None:
        print(f"{Path(a.elf).stem}: SKIP (no known output symbol; pass --out-addr)")
        sys.exit(3)
    matched, total = native_verify(a.elf, out, a.ref)
    name = Path(a.elf).stem
    print(f"{name}: {matched}/{total} bit-exact "
          f"{'PASS' if matched == total else 'FAIL'}")
    sys.exit(0 if matched == total else 1)
