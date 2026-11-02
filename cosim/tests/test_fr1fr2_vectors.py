# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""
test_fr1fr2_vectors.py — sanity tests for framework/fr1fr2_vectors.py

Parses real vectors from nxp-qoriq/fr1_fr2_test_tool and checks the
inferred grid geometry against the 3GPP frame structure:

  1. 10 MHz / 30 kHz fd grid  — 280 symbols x 288 REs, 10 ms, no padding
  2. 20 MHz / 30 kHz fd grid  — 612 used REs stored padded to 640
  3. 10 MHz / 15 kHz fd grid  — 140 symbols x 640 stored / 624 used
                                (624 == NSC of the LA9310 DFE 9310 config)
  4. 20 ms TDD injection grid — 560 symbols
  5. byte-slicing round-trip  — symbol_bytes == raw file slice
  6. energy sanity            — used REs carry energy, pad region is quiet

Vectors must be fetched first:  scripts/fetch_fr1fr2.sh

Run:
  python3 tests/fr1fr2/test_fr1fr2_vectors.py
"""

import os
import sys

import numpy as np

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
sys.path.insert(0, os.path.join(ROOT, "cosim"))
from fr1fr2_vectors import load_fd_grid, load_iq_int16  # noqa: E402

VEC_DIR = os.path.join(ROOT, "nxp", "fr1_fr2_test_tool", "test_vectors")


def vec(name):
    path = os.path.join(VEC_DIR, name)
    if not os.path.exists(path):
        print(f"SKIP: {name} not fetched (run scripts/fetch_fr1fr2.sh)")
        sys.exit(0)
    return path


def main():
    # 1. 10 MHz / 30 kHz FDD test model: exact fit, no padding
    g10 = load_fd_grid(vec("NR-FR1-TM3.1_10MHz_30kHz_FDD_fd.bin"), 30, 10)
    assert (g10.n_sym, g10.sym_len, g10.n_sc, g10.duration_ms) == (280, 288, 288, 10), g10
    print(f"PASS 10MHz/30kHz: {g10.n_sym} sym x {g10.sym_len} REs, {g10.duration_ms} ms")

    # 2. 20 MHz / 30 kHz: 612 used, stored padded to 640
    g20 = load_fd_grid(vec("NR-FR1-TM1.1_20MHz_30kHz_FDD_fd.bin"), 30, 20)
    assert (g20.n_sym, g20.sym_len, g20.n_sc) == (280, 640, 612), g20
    print(f"PASS 20MHz/30kHz: {g20.n_sym} sym x {g20.sym_len} stored / {g20.n_sc} used")

    # 3. 10 MHz / 15 kHz: 624 used REs — matches DFE 9310 NSC=624
    g15 = load_fd_grid(
        vec("TM3.3_10MHz_15kHz_FDD.bin"), 15, 10)
    assert (g15.sym_len >= 624 and g15.n_sc == 624), g15
    assert g15.n_sym % 140 == 0, g15
    print(f"PASS 10MHz/15kHz: {g15.n_sym} sym x {g15.sym_len} stored / {g15.n_sc} used")

    # 4. TDD injection vector is a 20 ms grid
    gtdd = load_fd_grid(vec("G-FR1-A1-5_20MHz_30kHz_TDD.bin"), 30, 20)
    assert (gtdd.n_sym, gtdd.duration_ms) == (560, 20), gtdd
    print(f"PASS TDD 20MHz: {gtdd.n_sym} sym, {gtdd.duration_ms} ms")

    # 5. symbol_bytes round-trip against the raw file
    raw = open(g10.path, "rb").read()
    sym_size = g10.sym_len * 4
    for i in (0, 1, g10.n_sym - 1):
        assert g10.symbol_bytes(i) == raw[i * sym_size:(i + 1) * sym_size], i
    print("PASS byte round-trip")

    # 6. energy: TM1.1 is full allocation — every used RE region has energy;
    #    the 20 MHz pad region (612..640) stays far below the used region
    power = np.abs(np.stack([g20.symbol_iq(i) for i in range(28)]))
    used = power[:, :g20.n_sc].mean()
    pad = power[:, g20.n_sc:].mean()
    assert used > 1e-3, used
    assert pad < used / 10, (pad, used)
    print(f"PASS energy: used-RE mean |x|={used:.4f}, pad mean |x|={pad:.6f}")

    # geometry summary of every fetched fd vector (informative)
    print("\nAll fetched *_fd/TDD vectors:")
    for name in sorted(os.listdir(VEC_DIR)):
        n = len(load_iq_int16(os.path.join(VEC_DIR, name)))
        print(f"  {name}: {n} samples")

    print("\nALL PASS")


if __name__ == "__main__":
    main()
