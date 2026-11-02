# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""test_dfe_ab.py — negative control for the DFE A/B harness.

Guards against the failure mode that makes an A/B worthless: a comparison
that reports IDENTICAL because both sides produce nothing (or the same
thing regardless of input). Different input symbols MUST give different
output; the two builds on the same symbol are then compared for real.

Needs: scripts/fetch_fr1fr2.sh + a built DFE_ref_la9310 eld.

Original: the A/B harness must DETECT a difference when one exists.
Feed the two builds different symbols and confirm non-identical output."""
import sys, os
_COSIM = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, _COSIM); sys.path.insert(0, os.path.join(_COSIM, "scripts"))
import numpy as np
from dfe.session import process_grid
from fr1fr2_vectors import load_fd_grid
from dfe_ab_compare import words_to_complex, evm_db

OLD = "nxp/fr1_fr2_test_tool/MEvspa_images_LS.1T2R_25M_30K_61_61_TDDFDD_LA93/geul-vspa0.eld"
NEW = "firmware/eld/dfe-la9310-1t2r-25m30k.eld"
g = load_fd_grid("nxp/fr1_fr2_test_tool/test_vectors/TM3.3_10MHz_15kHz_FDD.bin", 15, 10)

# same eld, DIFFERENT symbols -> must differ (proves outputs are input-sensitive)
a = process_grid(OLD, g, 1)[0]
class Shift:                      # grid wrapper serving symbol 7 as index 0
    def symbol_bytes(self, i): return g.symbol_bytes(i + 7)
b = process_grid(OLD, Shift(), 1)[0]
exact = sum(1 for x, y in zip(a, b) if x == y)
print(f"same eld, different symbol: {exact}/1024 bit-exact, "
      f"EVM={evm_db(words_to_complex(a), words_to_complex(b)):.2f} dB")
assert exact < 1024, "HARNESS BROKEN: different inputs gave identical output"

# different eld, same symbol -> the real A/B (expect identical)
c = process_grid(NEW, g, 1)[0]
exact2 = sum(1 for x, y in zip(a, c) if x == y)
print(f"old vs new eld, same symbol: {exact2}/1024 bit-exact")

# and old eld vs new eld on the SHIFTED symbol too, as a second data point
d = process_grid(NEW, Shift(), 1)[0]
exact3 = sum(1 for x, y in zip(b, d) if x == y)
print(f"old vs new eld, symbol 7   : {exact3}/1024 bit-exact")
print("\nnegative control PASSED: harness detects real differences")
