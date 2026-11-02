# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""
test_ippu_native.py — the IPPU regression that pins the fix in place.

bitRev64 is REAL IPPU microcode (`vspa-lib/bitRev/asm/bitRev64.sx`, section
`ippu_pram`) with a golden reference, so it fails loudly if the IPPU is not
executing. Before the fix it produced garbage and flooded the log with

    IPPU ISS ERROR: ASA loop count must be 255 >= lcnt >= 0, not: 153056

TWO THINGS WERE WRONG, and both are needed:

1. `.idram` / `ippu_dram` must be loaded into MEMORY SPACE 3 (the IPPU's own
   "rudmem" array). `VSPA::dmemWrite` — which is what a space-1 write calls —
   writes the VCPU array unconditionally; there is NO partition redirect on the
   debug-write path, so IPPU data written to space 1 can NEVER reach the IPPU.
   Its static tables therefore stayed uninitialised and it decoded a garbage
   loop count out of them.

2. The LA9310 CAS DMEM partition (`set vcpu_dmem_size / ippu_dmem_size /
   dmem_partition`, 128 lines each) must be applied BEFORE the load, via the
   model's generic5 "strcmd" channel. 128 lines x 128 B = 0x4000 — exactly the
   VCPU/IPPU boundary of the unified LA9310 DMEM.

Consequence for hosts: any DMEM address >= 0x4000 must be read/written through
space 3. Reading it through space 1 returns a different, unused array.

Build the kernel first:
  make -C submodules/la931x_vspa_common/vspa-lib/bitRev/tests \
       VSPA_TOOL=$PWD/environment/cwvspa.vbeta_14_00_781_vspa.linux/VSPA_Tools

Run:
  python3 tests/fr1fr2/test_ippu_native.py
"""

import os
import sys

from elftools.elf.elffile import ELFFile

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
sys.path.insert(0, os.path.join(ROOT, "cosim"))
from native.session import NativeSession  # noqa: E402

BITREV = os.path.join(
    ROOT, "submodules", "la931x_vspa_common", "vspa-lib", "bitRev")
ELD = os.path.join(BITREV, "build", "tests", "test_bitRev_vspa2_16au.eld")
REF = os.path.join(BITREV, "tests", "vectors", "ref.hex")


def main():
    if not os.path.exists(ELD):
        print(f"SKIP: {ELD} not built (see module docstring)")
        return 0

    ref = [int(l.strip().rstrip(","), 16)
           for l in open(REF).read().splitlines()
           if l.strip().startswith("0x")]

    with open(ELD, "rb") as f:
        elf = ELFFile(f)
        secs = [s.name for s in elf.iter_sections() if s["sh_size"]]
        syms = {s.name: s["st_value"]
                for s in elf.get_section_by_name(".symtab").iter_symbols()
                if s.name}
    assert any("ippu" in s or "itext" in s for s in secs), \
        f"{ELD} carries no IPPU microcode — this test would prove nothing"
    out_addr = next(syms[n] for n in ("_out", "out", "_y_out") if n in syms)

    s = NativeSession()
    s.m.init_dmem_layout()          # (2) partition — BEFORE the load
    s.load_elf(ELD)                 # (1) ippu_dram -> space 3, inside load_elf
    s.boot()

    best = 0
    for _ in range(80):
        s.run_phases(500)
        got = s.read_words(out_addr, len(ref))
        best = max(best, sum(1 for a, b in zip(ref, got) if a == b))
        if best == len(ref):
            break

    errors = [t for t in s.m.terminal if "IPPU ISS ERROR" in t]
    print(f"bitRev64: {best}/{len(ref)} bit-exact, {len(errors)} IPPU errors")
    if errors:
        print("  e.g.", errors[0].strip()[:80])

    ok = best == len(ref) and not errors
    print("PASS — the IPPU executes correctly on the native ISS" if ok else
          "FAIL — the IPPU is not executing correctly")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
