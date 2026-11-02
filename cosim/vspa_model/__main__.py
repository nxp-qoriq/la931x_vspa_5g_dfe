#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""
Self-test / demo for the vspa_model package.

Run with:  python3 -m vspa_model
"""

import os
import sys
import numpy as np

from .arith import r_smac
from .filter import r_firFilter
from .trig import r_atan2
from .io import read_ctrl, read_complex_binary, read_binary


def main():
    # --- r_smac unit test ---
    print("r_smac unit test")
    print("-" * 40)
    A = np.array([[0.1, 0.2], [0.3, 0.4], [0.5, 0.6]], dtype=np.float64)
    B = np.array([[1.0, 1.0], [1.0, 1.0], [1.0, 1.0]], dtype=np.float64)
    C = np.array([0.0, 0.0])
    result = r_smac(A, B, C)
    acc0 = np.float32(0.0)
    for v in [0.1, 0.3, 0.5]:
        acc0 = np.float32(np.float32(v) * np.float32(1.0) + acc0)
    acc1 = np.float32(0.0)
    for v in [0.2, 0.4, 0.6]:
        acc1 = np.float32(np.float32(v) * np.float32(1.0) + acc1)
    print(f"  col0: {result[0]:.10f} == {float(acc0):.10f} : {'PASS' if abs(result[0]-float(acc0))<1e-15 else 'FAIL'}")
    print(f"  col1: {result[1]:.10f} == {float(acc1):.10f} : {'PASS' if abs(result[1]-float(acc1))<1e-15 else 'FAIL'}")

    # --- r_firFilter smoke test ---
    print("\nr_firFilter smoke test (half_fixed)")
    print("-" * 40)
    np.random.seed(42)
    K_fir = 8
    h_fir = np.random.uniform(-0.5, 0.5, K_fir)
    x_fir = 0.5 * (np.random.uniform(-1, 1, 64) + 1j * np.random.uniform(-1, 1, 64))
    yr_fir = r_firFilter(h_fir, x_fir, 'half_fixed')
    print(f"  Output length: {len(yr_fir)} (expected {len(x_fir)-K_fir+1})")
    ym_fir = np.convolve(x_fir, h_fir, mode='valid')
    qnr = 10 * np.log10(np.mean(np.abs(yr_fir - ym_fir)**2) / np.mean(np.abs(ym_fir)**2))
    print(f"  QNR vs double conv: {qnr:.1f} dB (expect < -40 dB for Q15)")
    print(f"  Max output magnitude: {np.max(np.abs(yr_fir)):.4f} (should be < 1.0)")
    ok = qnr < -40 and np.max(np.abs(yr_fir)) < 1.0
    print(f"  Status: {'PASS' if ok else 'WARN'}")

    # --- ATAN2 verification against NXP golden references ---
    VECTOR_ROOT = os.path.join(
        os.path.dirname(__file__), '..', '..', '..',
        'submodules', 'la931x_vspa_common',
        'vspa-lib', 'atan', 'atan_cwproj', 'vector'
    )

    print("\nATAN2 Python behavioral model — verification against NXP golden references")
    print("=" * 72)

    NUM_COEFF_LIST = [3,4,5,6,7,8,9,10, 3,4,5,6,7,8,9,10,
                      3,4,5,6,7,8,9,10, 3,4,5,6,7,8,9,10,
                      3,4,5,6,7,8,9,10, 3,4,5,6,7,8,9,10,
                      3,4,5,6,7,8,9,10]

    errors = []
    for tc_idx in range(56):
        tc = f"TC{tc_idx:03d}"
        tc_in  = os.path.join(VECTOR_ROOT, 'in',  tc)
        tc_ref = os.path.join(VECTOR_ROOT, 'ref', tc)

        if not os.path.exists(os.path.join(tc_in, 'ctrl.bin')):
            continue

        n, inp_prec, out_prec = read_ctrl(os.path.join(tc_in, 'ctrl.bin'))
        inp = read_complex_binary(os.path.join(tc_in, 'input.bin'), inp_prec)
        ref = read_binary(os.path.join(tc_ref, 'atan_ref.bin'), out_prec)

        num_coeff = NUM_COEFF_LIST[tc_idx]
        phase = r_atan2(inp, inp_prec=inp_prec, out_prec=out_prec,
                        num_coeff=num_coeff)

        max_err = np.max(np.abs(phase - ref[:len(phase)]))
        errors.append(max_err)
        status = "EXACT" if max_err == 0.0 else ("OK" if max_err < 1e-4 else "WARN")
        print(f"  {tc} | nCoeff={num_coeff:2d} {inp_prec:10s}→{out_prec:10s} | "
              f"max_err={max_err:.2e} | {status}")

    if errors:
        print(f"\nMax error across all TCs: {max(errors):.2e}")
        print("(Should be 0.00e+00 — model is bit-exact with hardware LUT)")


if __name__ == '__main__':
    main()
