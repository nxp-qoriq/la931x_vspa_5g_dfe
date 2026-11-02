# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2025 NXP (original MATLAB), Python port 2025
"""
VSPA hardware logarithm (r_log).

Ported from NXP r_log.m + r_log2.m (vspa-lib/{logarithm,common}/matlab/).
Computes  y = fact * log2(x)  using:

  1. Hardware log2 instruction modelled by an  M=5,  Q=16  LUT  with
     y_log entries and m_log slopes (mirroring r_log2.m exactly).  The
     mantissa is read as a Q+1-bit fixed-point fraction; idx = top M
     bits, rem = next Q-M bits.
  2. The LUT result is fed through `float2fix` (round to a 32-bit
     Q17.15 fixed-point) and reinterpreted as int32 / 2**15 — this is
     what r_log.m does between the log2 step and the smad chain, and
     is what loses 1 bit of precision relative to the LUT's Q16 work.
  3. Two `r_smad` operations evaluate  fact * (int_part + frac_part).

Bit-exactness against `log_asm.sx` is **only** guaranteed for inputs
whose mantissa lands exactly at a LUT knot — i.e. x = 2^k * (1 + j/2**M)
for j in [0, 2**M).  In that regime the slope term `m_log[idx] * rem`
vanishes (rem == 0), so any residual difference between hardware and
the MATLAB simulation in the slope quantization is removed.  For
arbitrary inputs the cwproj testplan only validates within
LOG_ERR_MAX = 1e-3, so the asm and oracle drift by a few Q15 ULPs in
the slope direction and bit-exact comparison is not achievable without
reverse-engineering the actual hardware LUT.

Public API:
    r_log(x, fact)              -> float32 array, same shape as x
    LOG_FACT_VALUES             -> dict of name -> float32 factor
"""

import numpy as np

from .arith import r_smad, r_single

# LUT design parameters (match r_log2.m).
_LOG_M = 5
_LOG_Q = 16

# Factor table mirroring the LOG_FACT_T enum in
# submodules/la931x_vspa_common/vspa-lib/logarithm/include/log.h
LOG_FACT_VALUES = {
    'LOG2x1':   np.float32(1.0),                              # 0x3F800000
    'LOG10x10': np.float32(10.0 * np.log10(2.0)),             # 0x4040A8C1
    'LOG10x20': np.float32(20.0 * np.log10(2.0)),             # 0x40C0A8C1
}


def _build_log_table(M=_LOG_M, Q=_LOG_Q):
    """Build the hardware log2 LUT (y_log) and slope table (m_log).

    Mirrors r_log2.m.  The midpoint correction `e/2` plus the
    +0.42e-4 bias on entries 1..N is from the MATLAB reference; do not
    "simplify" it — those constants are part of the spec.
    """
    x1 = 1.0 + np.arange(2**M) * 2.0**(-M)              # 1, 1+1/32, ..., 2-1/32
    mid = x1 + 2.0**(-M - 1)                            # midpoints
    knots_x = np.append(x1, 2.0)
    knots_y = np.append(np.log2(x1), 1.0)
    interp = np.interp(mid, knots_x, knots_y)
    e = np.log2(mid) - interp
    e[0] = 0.0
    e[1:] = e[1:] + 0.42e-4
    y = np.log2(x1) + e / 2.0
    y_log = np.round(y * 2.0**Q).astype(np.int64)
    diff_y = np.append(np.diff(y), 1.0 - y[-1])
    m_log = np.round(diff_y * 2.0**Q).astype(np.int64)
    return y_log, m_log


_Y_LOG, _M_LOG = _build_log_table()


def r_log2(x, M=_LOG_M, Q=_LOG_Q):
    """Hardware log2 simulation (returns log2(x) in Q-bit fractional float).

    Mirrors r_log2.m line-by-line.  Input is treated as VSPA single
    precision (mantissa truncated).  Returns float64.
    """
    x = np.asarray(r_single(x), dtype=np.float64)
    exp_x = np.floor(np.log2(x))
    man_x = x / (2.0 ** exp_x)                          # in [1, 2)
    x_rnd = np.floor((man_x - 1.0) * 2.0**(Q + 1)) / 2.0**(Q + 1)
    idx = np.floor(x_rnd * 2.0**M).astype(np.int64)
    idx = np.clip(idx, 0, 2**M - 1)
    rem = np.floor(np.mod(x_rnd * 2.0**Q, 2.0**(Q - M))).astype(np.int64)
    num = (1 << (Q - M)) * _Y_LOG[idx] + _M_LOG[idx] * rem
    frac_y = num.astype(np.float64) / 2.0**(2 * Q - M)
    frac_y = np.floor(frac_y * 2.0**Q) / 2.0**Q
    return exp_x + frac_y


def _float2fix_q15(flt):
    """Real -> 32-bit Q17.15 fixed-point bits, matching the *hardware*
    `float2fix` instruction (truncation toward -infinity on the frac).

    Note: r_log.m documents `round(frac/2^-15)` for its float2fix.m, but
    the actual VSPA hardware float2fix instruction truncates the
    fractional part (floor for positive integer-part values).  Empirical
    matching against the cwproj golden vectors confirms the asm uses
    truncation — round-to-nearest produces a 1-Q15-ULP drift on every
    input whose log2 fractional part is just under a Q15 boundary.

    The integer part is a signed 17-bit value; combined with a 15-bit
    truncated fractional part to form a uint32.  Used inside r_log to
    drop from the LUT's Q16 representation down to the asm's Q15
    hardware format before splitting into int/frac for r_smad.
    """
    flt = np.asarray(r_single(flt), dtype=np.float64)
    int_part = np.floor(flt).astype(np.int64)
    frac = np.floor((flt - int_part) * 2**15).astype(np.int64)
    fixed = (int_part * (1 << 15) + frac) & 0xFFFFFFFF
    return fixed.astype(np.uint32)


def r_log(x, fact):
    """VSPA-precision logarithm  y = fact * log2(x).

    Args:
        x    : real input array (any shape).  Must be > 0.
        fact : scalar float32 factor (use a value from LOG_FACT_VALUES,
               or any float).

    Returns:
        float32 ndarray, same shape as x.
    """
    x_arr = np.asarray(x, dtype=np.float64)
    shape = x_arr.shape
    flat = x_arr.ravel()

    log2_v = r_log2(flat)

    # float2fix → reinterpret as int32 → /2^15 (matches r_log.m).
    fixed = _float2fix_q15(log2_v)
    sint  = np.frombuffer(fixed.astype('<u4').tobytes(),
                          dtype='<i4').astype(np.float64)
    log2_q15 = sint / 2.0**15

    log2_int = np.floor(log2_q15)
    log2_frac = log2_q15 - log2_int

    # smad chain — bit-exact float32 truncating MAC pair.
    out = r_smad(np.float64(fact), log2_int, 0.0)
    out = r_smad(np.float64(fact), log2_frac, out)

    return out.astype(np.float32).reshape(shape)
