#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""Wide A/B matrix: our stripped DFE build vs NXP's prebuilt image of the SAME config.

Widens the tier-3 differential test (scripts/dfe_ab_compare.py), which today runs
ONE vector, 3 symbols, at a NON-NATIVE bandwidth. This sweeps the NR test models
(TM1.1/TM2/TM2a/TM3.1/TM3.1a/TM3.2/TM3.3), FDD and TDD, across every bandwidth the
1024-point iFFT can carry, including the build's NATIVE 25 MHz / 30 kHz (NSC=612).

Two things this adds beyond dfe_ab_compare.py:

  * --offset / per-case symbol offset. The TDD vectors are ZERO for the first ~52
    symbols (the DL slots start later). An A/B on those symbols compares 1024
    zeros against 1024 zeros and reports "100% bit-exact" — a VACUOUS pass. Every
    row here records the input energy and is marked TRIVIAL if the stimulus is
    silent, so a green cell can never be mistaken for coverage it does not have.

  * a matrix summary across vectors instead of a single verdict.

The per-symbol drive is identical to dfe.session.process_grid (fresh firmware boot
per symbol); this file only adds the offset, which process_grid does not expose.

  python3 cosim/scripts/dfe_ab_matrix.py --symbols 3            # sweep
  python3 cosim/scripts/dfe_ab_matrix.py --only native --symbols 14   # full slot, native
"""
from __future__ import annotations

import argparse
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from dfe.session import DfeSession        # noqa: E402
from fr1fr2_vectors import load_fd_grid   # noqa: E402

OUR = "firmware/eld/dfe-la9310-1t2r-25m30k.eld"
NXP = ("cosim/nxp/fr1_fr2_test_tool/MEvspa_images_LS.1T2R_25M_30K_61_61_TDDFDD_LA93/"
       "geul-vspa0.eld")
V = "cosim/nxp/fr1_fr2_test_tool/test_vectors/"

# name, vector, scs, bw, nsc, offset, tag
# NSC = 12 * N_RB (3GPP 38.101-1 Table 5.3.2-1), see NRB_TABLE in fr1fr2_vectors.
# offset != 0 where the vector's leading symbols are silent (TDD DL starts at 52).
CASES = [
    # --- the build's NATIVE config: 25 MHz / 30 kHz, NSC=612 (cap at real LA9310 max) ---
    ("native TM3.3 25M/30k FDD", "TM3.3_25MHz_30kHz_FDD.bin", 30, 25, 612, 0, "native"),

    # --- NR test models, 20 MHz / 30 kHz (NSC=612) ---
    ("TM1.1  20M/30k FDD", "NR-FR1-TM1.1_20MHz_30kHz_FDD_fd.bin", 30, 20, 612, 0, "tm"),
    ("TM2    20M/30k FDD", "NR-FR1-TM2_20MHz_30kHz_FDD_fd.bin", 30, 20, 612, 0, "tm"),
    ("TM2a   20M/30k FDD", "NR-FR1-TM2a_20MHz_30kHz_FDD_fd.bin", 30, 20, 612, 0, "tm"),
    ("TM3.1  20M/30k FDD", "NR-FR1-TM3.1_20MHz_30kHz_FDD_fd.bin", 30, 20, 612, 0, "tm"),
    ("TM3.1a 20M/30k FDD", "NR-FR1-TM3.1a_20MHz_30kHz_FDD_fd.bin", 30, 20, 612, 0, "tm"),
    ("TM3.2  20M/30k FDD", "NR-FR1-TM3.2_20MHz_30kHz_FDD_fd.bin", 30, 20, 612, 0, "tm"),
    ("TM3.3  20M/30k FDD", "TM3.3_20MHz_30kHz_FDD.bin", 30, 20, 612, 0, "tm"),

    # --- NR test models, 10 MHz / 30 kHz (NSC=288) ---
    ("TM1.1  10M/30k FDD", "NR-FR1-TM1.1_10MHz_30kHz_FDD_fd.bin", 30, 10, 288, 0, "tm"),
    ("TM2a   10M/30k FDD", "NR-FR1-TM2a_10MHz_30kHz_FDD_fd.bin", 30, 10, 288, 0, "tm"),
    ("TM3.2  10M/30k FDD", "NR-FR1-TM3.2_10MHz_30kHz_FDD_fd.bin", 30, 10, 288, 0, "tm"),

    # --- other numerologies ---
    ("TM3.3  10M/15k FDD", "TM3.3_10MHz_15kHz_FDD.bin", 15, 10, 624, 0, "scs"),   # old baseline
    ("TM3.3  10M/15k TDD", "TM3.3_10MHz_15kHz_TDD.bin", 15, 10, 624, 0, "scs"),
    ("TM3.3  20M/60k FDD", "TM3.3_20MHz_60kHz_FDD.bin", 60, 20, 288, 0, "scs"),

    # --- SSB-bearing vector (PSS/SSS present) ---
    ("TM3.3 SSB 20M/30k", "NR-FR1-TM3.3_20MHz_30kHz_SSB_l_1_id1_335_id2_0_cfo_1000_"
     "toi_70_RE_offset_8_FDD_fd.bin", 30, 20, 612, 0, "ssb"),

    # --- TDD, driven at a LIVE symbol offset (first 52 symbols are silent) ---
    ("G-FR1-A1-2 10M/30k TDD", "G-FR1-A1-2_10MHz_30kHz_TDD.bin", 30, 10, 288, 52, "tdd"),
    ("G-FR1-A1-5 20M/30k TDD", "G-FR1-A1-5_20MHz_30kHz_TDD.bin", 30, 20, 612, 52, "tdd"),
    ("G-A1-5 TDD0606 20M/30k", "G-FR1-A1-5_20MHz_30kHz_TDD0606_0606_fd_640.bin",
     30, 20, 612, 8, "tdd"),
    ("UL 20M/30k TDD", "UL_20MHz_30kHz_TDD_3614_21022_20ms.bin", 30, 20, 612, 52, "tdd"),
]


def run(eld, grid, n, offset, nsc):
    """One fresh firmware boot per symbol — identical to dfe.session.process_grid,
    plus a symbol offset (which process_grid does not expose)."""
    return [DfeSession(eld, nsc=nsc).process_symbol(grid.symbol_bytes(offset + i))
            for i in range(n)]


def evm_db(ref, got):
    err = float((np.abs(got - ref) ** 2).sum())
    sig = float((np.abs(ref) ** 2).sum())
    if sig == 0:
        return float("nan")
    return float("-inf") if err == 0 else 10 * np.log10(err / sig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--symbols", type=int, default=3)
    ap.add_argument("--only", help="substring filter on case name / tag")
    ap.add_argument("--old", default=NXP)
    ap.add_argument("--new", default=OUR)
    a = ap.parse_args()

    cases = [c for c in CASES
             if not a.only or a.only.lower() in c[0].lower() or a.only == c[6]]

    # NXP's fetch script does not ship every vector these cases name (there is no
    # 25 MHz vector in the set at all). Skip what is absent, and say so — a matrix
    # that silently shrinks is worse than one that is short.
    missing = [c for c in cases if not os.path.exists(V + c[1])]
    cases = [c for c in cases if os.path.exists(V + c[1])]
    for name, vec, *_ in missing:
        print(f"  SKIP  {name:26} vector not fetched: {vec}", flush=True)
    if not cases:
        raise SystemExit("no vectors available — run ./cosim/scripts/fetch_fr1fr2.sh")

    t_all = time.time()
    rows = []
    for name, vec, scs, bw, nsc, off, tag in cases:
        t0 = time.time()
        grid = load_fd_grid(V + vec, scs, bw)
        n = min(a.symbols, grid.n_sym - off)
        stim = sum(float(np.abs(grid.symbol_iq(off + i)).sum()) for i in range(n))
        old = run(a.old, grid, n, off, nsc)
        new = run(a.new, grid, n, off, nsc)
        ex = sum(int(np.sum(np.asarray(o) == np.asarray(x))) for o, x in zip(old, new))
        tot = sum(len(o) for o in old)
        first = None
        for i, (o, x) in enumerate(zip(old, new)):
            d = np.nonzero(np.asarray(o) != np.asarray(x))[0]
            if len(d):
                first = (i, int(d[0]))
                break
        ref = np.concatenate(old)
        got = np.concatenate(new)
        e = evm_db(ref, got)
        rows.append(dict(name=name, nsc=nsc, off=off, n=n, ex=ex, tot=tot, evm=e,
                         first=first, peak=float(np.abs(ref).max()), stim=stim,
                         secs=time.time() - t0, tag=tag, vec=vec, scs=scs, bw=bw))
        r = rows[-1]
        print(f"[{len(rows)}/{len(cases)}] {name:26} nsc={nsc:<4} sym{off}..{off+n-1} "
              f"{ex}/{tot} exact  peak={r['peak']:.0f}  {r['secs']:.0f}s", flush=True)

    print("\n" + "=" * 108)
    print(f"{'vector / config':27} {'nsc':>4} {'sym':>8} {'bit-exact':>12} "
          f"{'EVM(dB)':>8} {'1st diff':>10} {'out peak':>9}  note")
    print("-" * 108)
    bad = triv = 0
    for r in rows:
        note = ""
        if r["stim"] == 0:
            note = "TRIVIAL (silent stimulus — proves nothing)"
            triv += 1
        elif r["peak"] == 0:
            note = "TRIVIAL (all-zero output)"
            triv += 1
        evm = "exact" if r["evm"] == float("-inf") else f"{r['evm']:.1f}"
        fd = "-" if r["first"] is None else f"s{r['first'][0]}/i{r['first'][1]}"
        if r["ex"] != r["tot"]:
            bad += 1
            note = "*** NOT BIT-EXACT ***"
        print(f"{r['name']:27} {r['nsc']:>4} {r['off']:>3}..{r['off']+r['n']-1:<3} "
              f"{r['ex']:>6}/{r['tot']:<5} {evm:>8} {fd:>10} {r['peak']:>9.0f}  {note}")
    print("-" * 108)
    ex = sum(r["ex"] for r in rows)
    tot = sum(r["tot"] for r in rows)
    print(f"{len(rows)} cases | {ex}/{tot} samples bit-exact "
          f"({100.0*ex/tot:.4f}%) | {bad} differing | {triv} trivial")
    print(f"wall clock: {time.time()-t_all:.0f}s")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
