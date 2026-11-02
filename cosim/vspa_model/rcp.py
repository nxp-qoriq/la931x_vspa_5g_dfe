# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2025 NXP (original MATLAB), Python port 2025
"""
VSPA hardware reciprocal (r_rcp).

Ported from NXP r_rcp.m (vspa-lib/common/matlab/vspa/).
Hardware uses a 7-bit index piecewise-linear LUT with 18-bit entries
and 20-bit output mantissa (M=7, Q=18, Qout=20 — the 'lo' mode).
"""

import numpy as np

_RCP_M, _RCP_Q, _RCP_QOUT = 7, 18, 20


def _build_rcp_lut(M=7, Q=18):
    """Build the hardware reciprocal LUT (y_rcp) and slope table (m_rcp)."""
    x1 = np.arange(1.0, 2.0, 2.0**(-M))
    mid = x1 + 2.0**(-M - 1)
    y_ideal = 2.0 / x1
    e = 2.0 / mid - np.interp(mid, np.append(x1, 2.0), np.append(y_ideal, 1.0))
    y = y_ideal + e / 2.0
    y_rcp = np.round((y - 1.0) * 2**Q).astype(np.int64)
    m_rcp = np.round(np.diff(np.append(y, 1.0)) * 2**Q).astype(np.int64)
    return y_rcp, m_rcp


_RCP_Y, _RCP_M_SLP = _build_rcp_lut(_RCP_M, _RCP_Q)


def r_rcp(x):
    """
    Bit-exact VSPA hardware reciprocal (lo-precision mode, M=7, Q=18, Qout=20).
    Ported from NXP r_rcp.m — matches hardware LUT output exactly.
    """
    x = np.asarray(x, dtype=np.float64).flatten()
    M, Q, Qout = _RCP_M, _RCP_Q, _RCP_QOUT

    SP_MAX = np.float64(np.finfo(np.float32).max)
    SP_MIN = np.float64(np.finfo(np.float32).tiny)
    i_zero  = (x == 0.0)
    i_nan   = np.isnan(x)
    i_inf   = np.abs(x) > (1.0 / SP_MIN)
    i_sub   = (np.abs(x) < SP_MIN) & ~i_zero
    i_neg   = (x < 0.0)

    ax = np.abs(x).copy()
    ax[i_zero | i_nan | i_inf | i_sub] = 1.0

    man_x, exp_x = np.frexp(ax)
    man_x = man_x * 2.0
    exp_x = exp_x - 1

    x_rnd = np.floor((man_x - 1.0) * 2.0**(Q + 1)) / 2.0**(Q + 1)
    idx  = np.floor(x_rnd * 2**M).astype(np.int64)
    frac = np.floor(x_rnd * 2**Q).astype(np.int64) % 2**(Q - M)

    man_y_raw = (_RCP_Y[idx] * 2**(Q - M) + _RCP_M_SLP[idx] * frac) / 2.0**(2 * Q - M)
    man_y = 1.0 + np.floor(man_y_raw * 2**Qout) / 2.0**Qout

    y = man_y * np.ldexp(1.0, -(exp_x.astype(int) + 1))

    y[i_zero]  = 2.0**128
    y[y > SP_MAX] = 2.0**128
    y[i_nan]   = 0.0
    y[i_inf]   = 0.0
    y[i_sub]   = 2.0**128
    y[i_neg]   = -y[i_neg]

    return y
