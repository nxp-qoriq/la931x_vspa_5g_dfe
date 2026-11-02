# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2025 NXP (original MATLAB), Python port 2025
"""
VSPA NCO (Numerically Controlled Oscillator) behavioral model.

Ported from NXP r_nco.m (vspa-lib/common/matlab/vspa/).
Implements exp(-2πj * f * k) using an 8-octant LUT with M=8, Q=17.
The 32-bit phase accumulator and integer LUT arithmetic exactly match
the VSPA hardware NCO.
"""

import numpy as np

_NCO_M, _NCO_Q = 8, 17


def _build_nco_tables():
    """Build the 8-octant NCO tables (y_cos, y_sin, m_cos, m_sin)."""
    M, Q = _NCO_M, _NCO_Q
    n_pts = 2**M + 1
    x1 = np.arange(n_pts, dtype=np.float64) / 2**M
    cos_v = np.cos(np.pi * x1 / 4)
    sin_v = np.sin(np.pi * x1 / 4)
    y_cos = np.round(cos_v / 2.0**(-Q)).astype(np.int64)
    y_sin = np.round(sin_v / 2.0**(-Q)).astype(np.int64)
    cos_ext = np.append(cos_v, np.sqrt(2) / 2.0)
    sin_ext = np.append(sin_v, np.sqrt(2) / 2.0)
    m_cos = np.round(np.diff(cos_ext) / 2.0**(-Q)).astype(np.int64)
    m_sin = np.round(np.diff(sin_ext) / 2.0**(-Q)).astype(np.int64)
    return y_cos, y_sin, m_cos, m_sin


_NCO_Y_COS, _NCO_Y_SIN, _NCO_M_COS, _NCO_M_SIN = _build_nco_tables()


def r_nco(f, L, phase_count=0):
    """
    VSPA NCO behavioral model.

    Synthesizes L complex samples  exp(-2πj * f * (phaseCount + k)) for k=0..L-1
    using the hardware 8-octant LUT with M=8, Q=17.

    Args:
        f          : scalar normalized frequency (cycles/sample).  Mapped to [0, 1).
        L          : number of samples to synthesize (int).
        phase_count: initial 32-bit phase register value (integer, default 0).

    Returns:
        Complex128 ndarray of shape (L,) [scalar f] or (L, N_f) [array f].
    """
    M, Q = _NCO_M, _NCO_Q

    f_v = np.atleast_1d(np.asarray(f, dtype=np.float64)).ravel()
    f_mapped = f_v % 1.0
    N_f = len(f_v)

    delta_q = np.floor(f_mapped * 2**32).astype(np.int64)

    pc = np.atleast_1d(np.asarray(phase_count, dtype=np.int64)).ravel()
    if len(pc) == 1:
        pc = np.tile(pc, N_f)
    mask32 = np.int64(0xFFFFFFFF)
    dq_lo = delta_q & np.int64(0x0000FFFF)
    dq_hi = delta_q & np.int64(0xFFFF0000)
    pc_lo = pc       & np.int64(0x0000FFFF)
    pc_hi = pc       & np.int64(0xFFFF0000)
    del_phase = (dq_lo * pc_hi + dq_hi * pc_lo + dq_lo * pc_lo) & mask32

    k = np.arange(L, dtype=np.int64)
    raw = (del_phase[:, None].astype(np.float64)
           + delta_q[:, None].astype(np.float64) * k[None, :])
    x = (raw / float(2**32)) % 1.0

    val = np.round(x * float(2**(Q + 3))).astype(np.int64)
    oct_idx   = (val >> Q) & np.int64(7)
    phase_lsb = val & np.int64((1 << Q) - 1)
    phase_refl = np.int64(1 << Q) - phase_lsb

    def _lut(y_lut, m_lut, ph):
        idx  = ph >> (Q - M)
        frac = ph & np.int64((1 << (Q - M)) - 1)
        return (np.int64(2**(Q - M)) * y_lut[idx] + m_lut[idx] * frac) / float(2**(2*Q - M))

    cos_d = _lut(_NCO_Y_COS, _NCO_M_COS, phase_lsb)
    sin_d = _lut(_NCO_Y_SIN, _NCO_M_SIN, phase_lsb)
    cos_r = _lut(_NCO_Y_COS, _NCO_M_COS, phase_refl)
    sin_r = _lut(_NCO_Y_SIN, _NCO_M_SIN, phase_refl)

    o = oct_idx
    i_arr = np.select(
        [o == 0, o == 1, o == 2, o == 3, o == 4, o == 5, o == 6, o == 7],
        [ cos_d,  sin_r, -sin_d, -cos_r, -cos_d, -sin_r,  sin_d,  cos_r]
    )
    q_arr = np.select(
        [o == 0, o == 1, o == 2, o == 3, o == 4, o == 5, o == 6, o == 7],
        [ sin_d,  cos_r,  cos_d,  sin_r, -sin_d, -cos_r, -cos_d, -sin_r]
    )

    y = i_arr - 1j * q_arr

    if N_f == 1:
        return y[0]
    return y.T
