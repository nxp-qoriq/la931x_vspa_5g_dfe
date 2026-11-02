#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""A/B-compare two LA9310 DFE builds on the same FR1 test vector.

Pushes the same frequency-domain symbols through both firmware images on the
native in-process ISS (framework/dfe) and compares the TX pipeline output
(phcom -> RE pack -> iFFT-1024, read back from _sym_queue_tx) two ways:

  * bit-exact  — how many of the 1024 complex samples are identical
  * EVM        — error-vector magnitude of B vs A, in dB (per symbol + total),
                 for when a simplification legitimately reorders arithmetic

Both images are resolved by SYMBOL NAME, so the comparison survives the code
relayout a rebuild causes.

Usage:
  python3 cosim/scripts/dfe_ab_compare.py \
      --old cosim/nxp/fr1_fr2_test_tool/MEvspa_images_LS.1T2R_25M_30K_61_61_TDDFDD_LA93/geul-vspa0.eld \
      --new firmware/eld/dfe-la9310-1t2r-25m30k.eld \
      --vector cosim/nxp/fr1_fr2_test_tool/test_vectors/TM3.3_10MHz_15kHz_FDD.bin \
      --scs 15 --bw 10 --symbols 4
"""
from __future__ import annotations

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from dfe.session import process_grid          # noqa: E402
from fr1fr2_vectors import load_fd_grid       # noqa: E402


def words_to_complex(z):
    """process_symbol() already returns natural-order, SM16-decoded complex
    samples, so this is now an identity kept for callers/tests."""
    return np.asarray(z)


def evm_db(ref: np.ndarray, got: np.ndarray) -> float:
    """EVM of `got` against `ref`, in dB. -inf when the two are identical."""
    err = np.abs(got - ref) ** 2
    sig = np.abs(ref) ** 2
    if sig.sum() == 0:
        return float("nan")
    if err.sum() == 0:
        return float("-inf")
    return 10 * np.log10(err.sum() / sig.sum())


def fmt_evm(x: float) -> str:
    return "exact" if x == float("-inf") else f"{x:.2f}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--old", required=True, help="reference (old) .eld")
    ap.add_argument("--new", required=True, help="candidate (simplified) .eld")
    ap.add_argument("--vector", required=True, help="fr1fr2 fd .bin")
    ap.add_argument("--scs", type=int, default=15)
    ap.add_argument("--bw", type=int, default=10)
    ap.add_argument("--symbols", type=int, default=4)
    ap.add_argument("--nsc", type=int, default=624)
    ap.add_argument("--dump", help="directory to write per-build .bin outputs")
    a = ap.parse_args()

    grid = load_fd_grid(a.vector, a.scs, a.bw)
    print(f"vector : {os.path.basename(a.vector)} "
          f"({grid.n_sym} sym x {grid.sym_len} stored / {grid.n_sc} used REs, "
          f"{grid.duration_ms} ms)")
    print(f"old    : {a.old}")
    print(f"new    : {a.new}")
    print(f"driving {a.symbols} symbol(s) through both builds, NSC={a.nsc}\n")

    old = process_grid(a.old, grid, a.symbols, nsc=a.nsc)
    new = process_grid(a.new, grid, a.symbols, nsc=a.nsc)

    total_exact = total_n = 0
    all_ref, all_got = [], []
    print(f"{'sym':>3}  {'bit-exact':>12}  {'EVM (dB)':>9}  {'first diff':>10}")
    print("-" * 42)
    for i, (o, n) in enumerate(zip(old, new)):
        exact = int(np.sum(np.asarray(o) == np.asarray(n)))
        diff = np.nonzero(np.asarray(o) != np.asarray(n))[0]
        first = int(diff[0]) if len(diff) else None
        ref, got = words_to_complex(o), words_to_complex(n)
        all_ref.append(ref); all_got.append(got)
        e = evm_db(ref, got)
        total_exact += exact; total_n += len(o)
        print(f"{i:>3}  {exact:>6}/{len(o):<5}  {fmt_evm(e):>9}  "
              f"{'-' if first is None else first:>10}")

    ref = np.concatenate(all_ref); got = np.concatenate(all_got)
    print("-" * 42)
    print(f"total bit-exact : {total_exact}/{total_n} "
          f"({100.0 * total_exact / total_n:.2f}%)")
    print(f"total EVM       : {fmt_evm(evm_db(ref, got))} dB")
    peak = np.abs(got - ref).max() if len(ref) else 0
    print(f"max |error|     : {peak:.0f} LSB "
          f"(ref peak |x| = {np.abs(ref).max():.0f})")

    if a.dump:
        os.makedirs(a.dump, exist_ok=True)
        for tag, outs in (("old", old), ("new", new)):
            path = os.path.join(a.dump, f"tx_out_{tag}.bin")
            flat = np.concatenate([words_to_complex(o) for o in outs])
            iq = np.empty(2 * len(flat), dtype="<i2")
            iq[0::2] = flat.real.astype("<i2")
            iq[1::2] = flat.imag.astype("<i2")
            iq.tofile(path)
            print(f"wrote {path}")

    verdict = "IDENTICAL" if total_exact == total_n else "DIFFERS"
    print(f"\nverdict: {verdict}")
    return 0 if total_exact == total_n else 1


if __name__ == "__main__":
    raise SystemExit(main())
