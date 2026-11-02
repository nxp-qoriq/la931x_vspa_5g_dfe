#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""verify_dfe_loopback.py — run the src/main_test.c loopback on the native
in-process ISS and check its verdict.

Default mode: free-run to the final halt, read test_result / dominant_sc /
dominant_e / guard_max_e, exit 0 on PASS.

--stages mode: co-simulate stage by stage against the Python oracle
(vspa_model.fft.r_dif_fft_sc128).  main_test.c publishes a stage number after
every pipeline step and, when the driver sets dbg_enable, pauses until dbg_go is
acked.  At each pause the driver dumps the buffer, compares it with the oracle's
version of the same stage, and continues.

This is how the loopback's defects were localised — a wrong bin at stage 2 looks
identical to a wrong bin at stage 7 if you only ever inspect the final verdict.
Keep it: it is the fastest way to re-triage a regression.

Usage:
    python3 scripts/verify_dfe_loopback.py
    python3 scripts/verify_dfe_loopback.py --stages
    python3 scripts/verify_dfe_loopback.py --elf eld/…-test.eld --stages
"""
from __future__ import annotations

import argparse
import os
import sys

import numpy as np

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(_ROOT, "cosim"))

from native.session import NativeSession   # noqa: E402
from vspa_model.fft import r_dif_fft_sc128  # noqa: E402

ELF_DEFAULT = os.path.join(_ROOT, "firmware", "eld",
                           "dfe-la9310-1t2r-25m30k-test.eld")

N = 1024
TONE = 0x2000  # sc[0].real, 0.25 in Q15/SM16
# NSC and the bitrev flavour are read from the ELF (volatile test_nsc /
# test_ippu in main_test.c) so the oracle always matches the build.  Hardcoding
# them is exactly the class of bug this script exists to catch: the loopback and
# the co-simulation once ran with different NSC and nobody noticed.


# ── ELF symbols ───────────────────────────────────────────────────────────────
def symtab(elf: str) -> dict[str, int]:
    from elftools.elf.elffile import ELFFile
    with open(elf, "rb") as f:
        st = ELFFile(f).get_section_by_name(".symtab")
        return {s.name: s["st_value"] for s in st.iter_symbols()}


def sym(names: dict[str, int], want: str) -> int:
    for cand in (want, "_" + want):
        if cand in names:
            return names[cand]
    raise KeyError(f"symbol {want} not in ELF")


# ── SM16 <-> complex (memory word = (im16 << 16) | re16, sign-magnitude) ─────
def sm16_words_to_complex(words: list[int]) -> np.ndarray:
    def dec(h: int) -> float:
        mag = h & 0x7FFF
        return -mag / 32768.0 if h & 0x8000 else mag / 32768.0
    return np.array([complex(dec(w & 0xFFFF), dec((w >> 16) & 0xFFFF))
                     for w in words])


def bitrev_perm(n_bits: int) -> np.ndarray:
    n = 1 << n_bits
    r = np.zeros(n, dtype=int)
    for i in range(n):
        x, v = i, 0
        for _ in range(n_bits):
            v = (v << 1) | (x & 1)
            x >>= 1
        r[i] = v
    return r


# ── oracle replication of main_test.c ────────────────────────────────────────
def oracle_stages(nsc: int, ippu: bool) -> dict[int, np.ndarray]:
    """Stage number → expected work_buf / rx_sc_out contents (fractional floats)."""
    br = bitrev_perm(10)
    st: dict[int, np.ndarray] = {}

    buf = np.zeros(N, dtype=np.complex128)
    buf[0] = TONE / 32768.0
    st[1] = buf[:nsc].copy()                       # packed subcarriers only

    # concat — replicate main_test.c's loops LITERALLY (sequential, in-place) so
    # any overlap effect would show up identically in the oracle instead of being
    # masked by a "clean" vectorised model.
    dft = buf.copy()
    for k in range(nsc // 2):
        dft[N - nsc // 2 + k] = dft[k]             # neg freqs → high bins
    for k in range(nsc - nsc // 2):
        dft[k] = dft[nsc // 2 + k]                 # pos freqs → low bins
    st[2] = dft.copy()

    dft[nsc - nsc // 2:N - nsc // 2] = 0           # guard zero
    st[3] = dft.copy()

    td_nat = r_dif_fft_sc128(dft, inv_flag=1)      # natural order (post-bitrev)
    st[4] = td_nat[br]                             # raw kernel output = bit-reversed
    st[5] = td_nat.copy()

    fd_nat = r_dif_fft_sc128(td_nat, inv_flag=0)
    st[6] = fd_nat[br]

    if ippu:
        # bitRev1024sEbyE = bit-reversal + N/2 shift: the spectrum comes out
        # DC-centred, so the active subcarriers are one contiguous run.
        spec = np.roll(fd_nat, -(N // 2))
        st[7] = spec
        lo = N // 2 - nsc // 2
        st[8] = spec[lo:lo + nsc]
    else:
        st[7] = fd_nat.copy()
        st[8] = np.concatenate([fd_nat[N - nsc // 2:], fd_nat[:nsc - nsc // 2]])
    return st


STAGE_NAMES = {
    1: "input packed subcarriers", 2: "after concat", 3: "after guard zero",
    4: "after IFFT (raw, bit-reversed)", 5: "after TX bitrev (time domain)",
    6: "after FFT (raw, bit-reversed)", 7: "after RX bitrev (spectrum)",
    8: "after extraction (rx_sc_out)", 9: "verdict computed",
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf", default=ELF_DEFAULT)
    ap.add_argument("--stages", action="store_true",
                    help="pause at each pipeline stage and diff vs the oracle")
    ap.add_argument("--max-phases", type=int, default=3_000_000)
    a = ap.parse_args()

    if not os.path.exists(a.elf):
        print(f"ERROR: {a.elf} not built — run `make test`", file=sys.stderr)
        return 2

    names = symtab(a.elf)
    addr = {k: sym(names, k) for k in
            ("work_buf", "rx_sc_out", "test_result", "dominant_sc",
             "dominant_e", "guard_max_e", "dbg_enable", "dbg_stage", "dbg_go",
             "test_nsc", "test_ippu")}

    s = NativeSession()
    # The LA9310 DMEM partition MUST be applied before the load, or `.idram`
    # never reaches the IPPU's own bank and the microcode silently reads zeros.
    s.m.init_dmem_layout()
    s.load_elf(a.elf)
    s.boot()

    if a.stages:
        # crt0 zeroes .bss (where dbg_enable lives) AFTER boot, so a single early
        # write is silently clobbered.  Re-write it in small phase chunks until
        # main reaches STAGE(1); the input-load loop before STAGE(1) is thousands
        # of phases long, so one write always lands in time.
        s.write_words(addr["dbg_enable"], [1])
        while s.read_words(addr["dbg_stage"], 1)[0] == 0:
            s.run_phases(200)
            s.write_words(addr["dbg_enable"], [1])
        got_stage = s.read_words(addr["dbg_stage"], 1)[0]
        if got_stage > 1:
            print(f"ERROR: raced past stage 1 (at {got_stage}) — "
                  f"stage dumps unreliable, aborting", file=sys.stderr)
            return 2

    nsc = None
    oracle: dict[int, np.ndarray] = {}
    worst = 0.0
    seen = 0
    ran = 0
    CHUNK = 20_000
    while ran < a.max_phases:
        s.run_phases(CHUNK)
        ran += CHUNK
        stage = s.read_words(addr["dbg_stage"], 1)[0]
        if stage == seen:
            continue
        seen = stage
        if a.stages and nsc is None:
            nsc = s.read_words(addr["test_nsc"], 1)[0]
            if not 0 < nsc <= N:
                print(f"ERROR: implausible test_nsc={nsc}", file=sys.stderr)
                return 2
            ippu = s.read_words(addr["test_ippu"], 1)[0]
            print(f"NSC under test (from ELF): {nsc}   RX bitrev: "
                  + ("IPPU microcode (bitrev + N/2 shift)" if ippu
                     else "software loop"))
            oracle = oracle_stages(nsc, bool(ippu))
        if a.stages and stage in oracle:
            exp = oracle[stage]
            base = addr["rx_sc_out"] if stage == 8 else addr["work_buf"]
            got = sm16_words_to_complex(s.read_words(base, len(exp)))
            # stage 2: bins [nsc, N-nsc/2) are still uninitialised DRAM — compare
            # only the bins the concat defines (the guard is judged at stage 3).
            if stage == 2:
                mask = np.ones(N, dtype=bool)
                mask[nsc:N - nsc // 2] = False
                diff = np.abs(got[mask] - exp[mask])
            else:
                diff = np.abs(got - exp)
            lsb = float(diff.max()) * 32768.0
            if stage >= 4:
                worst = max(worst, lsb)            # FFT stages only
            tag = "OK " if lsb <= 2.0 else "BAD"
            print(f"stage {stage} ({STAGE_NAMES[stage]:34s}): "
                  f"max |diff| = {lsb:8.2f} LSB  {tag}")
            if lsb > 2.0:
                bad = int(np.argmax(diff))
                print(f"    first-worst index {bad}: got {got[bad]:.6f} "
                      f"exp {exp[bad]:.6f}")
                for label, v in (("got", got), ("exp", exp)):
                    hot = np.nonzero(np.abs(v) > 100 / 32768.0)[0][:8]
                    print(f"    {label} hot bins: "
                          + ", ".join(f"{i}:{v[i]*32768:.0f}" for i in hot))
        if stage < 9:
            if a.stages:
                s.write_words(addr["dbg_go"], [1])
            continue

        # stage 9: the verdict is written just after — let it settle, then read
        s.run_phases(CHUNK)
        res = s.read_words(addr["test_result"], 1)[0]
        dom = s.read_words(addr["dominant_sc"], 1)[0]
        dom = dom - (1 << 32) if dom >= (1 << 31) else dom
        dom_e = s.read_words(addr["dominant_e"], 1)[0]
        guard_e = s.read_words(addr["guard_max_e"], 1)[0]
        bitrev = "IPPU" if s.read_words(addr["test_ippu"], 1)[0] else "sw"
        print(f"bitrev={bitrev} test_result={res} dominant_sc={dom} "
              f"dominant_e={dom_e} guard_max_e={guard_e}"
              + (f"  (worst FFT-stage diff {worst:.2f} LSB)" if a.stages else ""))
        print("PASS" if res == 1 else "FAIL")
        return 0 if res == 1 else 1

    print(f"ERROR: stage {seen} after {ran} phases — never reached the verdict",
          file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
