# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""Determinism + liveness test for framework/dfe DfeSession on the NXP prebuilt LA9310 DFE eld (needs scripts/fetch_fr1fr2.sh).
"""
import sys, time, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from dfe.session import process_grid
from fr1fr2_vectors import load_fd_grid

ELD = "nxp/fr1_fr2_test_tool/MEvspa_images_LS.1T2R_25M_30K_61_61_TDDFDD_LA93/geul-vspa0.eld"
g = load_fd_grid("nxp/fr1_fr2_test_tool/test_vectors/TM3.3_10MHz_15kHz_FDD.bin", 15, 10)

t0 = time.time()
outs1 = process_grid(ELD, g, 5)
t1 = time.time()
print(f"run1: 5 symbols in {t1-t0:.1f}s")
for i, o in enumerate(outs1):
    nz = sum(1 for w in o if w)
    print(f"  sym{i}: {o[0]:#010x} {o[1]:#010x} nonzero={nz}/1024")

outs2 = process_grid(ELD, g, 5)
same = all(a == b for a, b in zip(outs1, outs2))
print("determinism:", "BIT-EXACT" if same else "DIVERGES")
assert same
# distinct symbols must give distinct outputs (sanity)
assert outs1[0] != outs1[1], "sym0 == sym1 ?!"
print("OK")
