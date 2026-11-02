# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2025 NXP (original MATLAB), Python port 2025
"""
VSPA batch matrix-vector multiply model.

Ported from NXP r_mat_bmult.m.
"""

import numpy as np
from .arith import r_convert, r_smad


def r_mat_bmult(vec, mat, vec_prec, mat_prec, out_prec, mat_interp=1):
    """
    Bit-exact batch matrix-vector multiply (port of r_mat_bmult.m).

    Args:
        vec        : (D1, D2) complex — batch of row vectors
        mat        : (D1//mat_interp, D3, D2) complex — batch of matrices
        vec_prec   : precision string for vec ('half_fixed','half','single','double')
        mat_prec   : precision string for mat
        out_prec   : precision string for output
        mat_interp : integer — replicate mat along D1 axis (default 1)

    Returns:
        out_v : (D1, D3) complex array in out_prec
    """
    vec = np.asarray(vec, dtype=np.complex128)
    mat = np.asarray(mat, dtype=np.complex128)

    if mat_interp > 1:
        mat = np.repeat(mat, mat_interp, axis=0)

    dim1 = vec.shape[0]
    dim2 = vec.shape[1]
    dim3 = mat.shape[1]

    # Apply r_convert directly — do NOT ravel() first (would reorder Fortran-contiguous arrays).
    vec_v = r_convert(vec, vec_prec)
    mat_v = r_convert(mat, mat_prec)

    out_v = np.zeros((dim1, dim3), dtype=np.complex128)
    for out_idx in range(dim3):
        for inp_idx in range(dim2):
            S0 = vec_v[:, inp_idx]
            S1 = mat_v[:, out_idx, inp_idx]
            V  = out_v[:, out_idx]
            out_v[:, out_idx] = r_smad(S0, S1, V)

    return r_convert(out_v, out_prec)
