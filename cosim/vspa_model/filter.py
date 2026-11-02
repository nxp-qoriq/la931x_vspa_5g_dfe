# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2025 NXP (original MATLAB), Python port 2025
"""
VSPA filter models: decimator, interpolator, FIR.

  r_decimator_2x / r_decimator — bit-exact 2x/4x/8x decimation
  r_interpolator_4x / r_interpolator_2x — polyphase upsampling
  r_firFilter — FIR filter (fir33 kernel model, ported from r_firFilter.m)
"""

import numpy as np
from .arith import r_convert, r_smad, r_half, r_half_flt, r_smac


def r_decimator_2x(inp, flt, inp_hist=None,
                   input_prec='half_fixed', filter_prec='single', output_prec='half_fixed'):
    """Bit-exact 2x decimator model for the VSPA decimator kernels."""
    inp = np.asarray(inp, dtype=np.complex128).reshape(-1)
    flt = np.asarray(flt, dtype=np.float64).reshape(-1)

    if np.iscomplexobj(flt) and np.any(flt.imag != 0):
        raise ValueError("Decimator FIR filter must be real")
    if len(flt) % 2 != 0:
        raise ValueError("Decimator FIR filter length must be even")
    if len(inp) % 2 != 0:
        raise ValueError("Decimator input length must be even")

    if inp_hist is None:
        inp_hist = np.zeros(len(flt) - 1, dtype=np.complex128)
    inp_hist = np.asarray(inp_hist, dtype=np.complex128).reshape(-1)
    if inp_hist.shape != (len(flt) - 1,):
        raise ValueError("Decimator input history size is not valid")

    inp_v = r_convert(inp, input_prec)
    inp_hist_v = r_convert(inp_hist, input_prec)
    flt_v = r_convert(flt.real, filter_prec)[::-1]

    inp_v_p = np.concatenate([inp_hist_v, inp_v])
    inp_len = len(inp_v)
    acc_lanes = np.zeros(inp_len, dtype=np.float64)

    for iter_idx in range(len(flt_v) // 2):
        h0 = flt_v[2 * iter_idx]
        h1 = flt_v[2 * iter_idx + 1]
        aux = inp_v_p[2 * iter_idx: 2 * iter_idx + inp_len]

        s1 = np.empty(inp_len, dtype=np.complex128)
        s1[0::2] = aux[1::2].real + 1j * aux[0::2].real
        s1[1::2] = aux[1::2].imag + 1j * aux[0::2].imag
        acc_lanes = np.imag(r_smad(h0 + 1j * h1, s1, 1j * acc_lanes))

    out_v = acc_lanes[0::2] + 1j * acc_lanes[1::2]
    next_hist_v = inp_v_p[-(len(flt_v) - 1):]
    return r_convert(out_v, output_prec), next_hist_v


def r_decimator(inp, flt, factor, num_blocks=1,
                input_prec='half_fixed', filter_prec='single', output_prec='half_fixed', inp_hist=None):
    """Bit-exact block decimator model for factors 2x, 4x, and 8x."""
    if factor not in (2, 4, 8):
        raise ValueError(f"Decimation factor {factor} not supported")

    inp = np.asarray(inp, dtype=np.complex128).reshape(-1)
    flt = np.asarray(flt, dtype=np.float64).reshape(-1)
    stages = int(np.log2(factor))

    if num_blocks <= 0 or len(inp) % num_blocks != 0:
        raise ValueError("Decimator input length is not divisible by the number of blocks")

    if inp_hist is None:
        inp_hist = np.zeros((len(flt) - 1, stages), dtype=np.complex128)
    inp_hist = np.asarray(inp_hist, dtype=np.complex128)
    if inp_hist.shape != (len(flt) - 1, stages):
        raise ValueError("Decimator input history size is not valid")

    hist_state = inp_hist.copy()
    inp_blocks = inp.reshape(-1, num_blocks, order='F')
    out_blocks = []

    for block_idx in range(num_blocks):
        stage_out = inp_blocks[:, block_idx]
        for stage_idx in range(stages):
            stage_out, hist_state[:, stage_idx] = r_decimator_2x(
                stage_out,
                flt,
                inp_hist=hist_state[:, stage_idx],
                input_prec=input_prec,
                filter_prec=filter_prec,
                output_prec=output_prec,
            )
        out_blocks.append(stage_out)

    return np.concatenate(out_blocks), hist_state


def r_interpolator_4x(inp, flt, inp_hist=None,
                      input_prec='half_fixed', filter_prec='single', output_prec='half_fixed'):
    """Bit-exact 4x polyphase FIR interpolator model for X4_interp_tap* kernels.

    flt : flat array of shape (4 * num_tap,) storing the polyphase matrix in
          row-major order: H[j][k] = flt[j * num_tap + k] for j=0..3.
    inp : complex half_fixed input samples (length N).
    inp_hist : last (num_tap-1) complex samples from previous call; zeros if None.
    Returns (y_out, next_hist).
    """
    inp = np.asarray(inp, dtype=np.complex128).reshape(-1)
    flt = np.asarray(flt, dtype=np.float64).reshape(-1)

    NUM_INTERP = 4
    if len(flt) % NUM_INTERP != 0:
        raise ValueError("r_interpolator_4x: filter length must be divisible by 4")
    num_tap = len(flt) // NUM_INTERP
    hist_len = num_tap - 1

    if inp_hist is None:
        inp_hist = np.zeros(hist_len, dtype=np.complex128)
    inp_hist = np.asarray(inp_hist, dtype=np.complex128).reshape(-1)
    if len(inp_hist) != hist_len:
        raise ValueError(f"r_interpolator_4x: history must have {hist_len} samples, got {len(inp_hist)}")

    inp_q      = r_convert(inp,      input_prec)
    inp_hist_q = r_convert(inp_hist, input_prec)
    flt_q      = r_convert(flt,      filter_prec)

    x_ext = np.concatenate([inp_hist_q, inp_q])
    N = len(inp_q)
    y_out = np.empty(N * NUM_INTERP, dtype=np.complex128)

    for j in range(NUM_INTERP):
        h_phase = flt_q[j * num_tap: (j + 1) * num_tap]
        acc = np.zeros(N, dtype=np.complex128)
        for k in range(num_tap):
            x_k = x_ext[k: k + N]
            acc = r_smad(h_phase[k], x_k, acc)
        y_out[j::NUM_INTERP] = r_convert(acc, output_prec)

    next_hist = inp_q[-hist_len:] if hist_len > 0 else np.empty(0, dtype=np.complex128)
    return y_out, next_hist


def r_interpolator_2x(inp, flt, inp_hist=None,
                      input_prec='half_fixed', filter_prec='single', output_prec='half_fixed'):
    """Bit-exact 2x polyphase FIR interpolator model for X2_interp_tap* kernels.

    flt : flat array h[0..M-1] in natural order, length M (must be even).
    inp : complex half_fixed input samples (length N).
    inp_hist : last (M/2-1) complex samples from previous call; zeros if None.
    Returns (y_out, next_hist).
    """
    inp = np.asarray(inp, dtype=np.complex128).reshape(-1)
    flt = np.asarray(flt, dtype=np.float64).reshape(-1)

    if len(flt) % 2 != 0:
        raise ValueError("r_interpolator_2x: filter length must be even")
    num_tap_per_phase = len(flt) // 2
    hist_len = num_tap_per_phase - 1

    if inp_hist is None:
        inp_hist = np.zeros(hist_len, dtype=np.complex128)
    inp_hist = np.asarray(inp_hist, dtype=np.complex128).reshape(-1)
    if len(inp_hist) != hist_len:
        raise ValueError(f"r_interpolator_2x: history must have {hist_len} samples, got {len(inp_hist)}")

    inp_q      = r_convert(inp,      input_prec)
    inp_hist_q = r_convert(inp_hist, input_prec)
    flt_q      = r_convert(flt,      filter_prec)

    x_ext = np.concatenate([inp_hist_q, inp_q])
    N = len(inp_q)
    y_out = np.empty(N * 2, dtype=np.complex128)

    for p in range(2):
        h_phase = flt_q[p::2]
        acc = np.zeros(N, dtype=np.complex128)
        for k in range(num_tap_per_phase):
            start = hist_len - k
            x_k = x_ext[start: start + N]
            acc = r_smad(h_phase[k], x_k, acc)
        y_out[p::2] = r_convert(acc, output_prec)

    next_hist = inp_q[-hist_len:] if hist_len > 0 else np.empty(0, dtype=np.complex128)
    return y_out, next_hist


def r_tx_windowing(inp_out, hist, win):
    """W-OFDM TX windowing — overlap-add at symbol edges (txWindowing_w16_*).

    Models txWindowing_w16_vecaligned: blends the first W samples of the
    current symbol with the previous symbol's faded tail (fade-in at the CP)
    and saves the current symbol's faded tail into history for the next call
    (fade-out at the symbol end). The middle samples (between offsets W and
    -W) are passed through unchanged.

    Args:
        inp_out : complex ndarray, length N (>= 2W). Modified IN PLACE.
        hist    : complex ndarray, length W. Modified IN PLACE with the new
                  faded tail; pass zeros on the first call.
        win     : real ndarray, length W. Rising-edge window (e.g.
                  win[k] = sin²(π·k/(2W)) for raised-cosine edge).

    The fixed-point grid is half_fixed (SM16); inputs and outputs are
    quantised to that precision so the model matches the kernel.
    """
    inp_out = np.asarray(inp_out, dtype=np.complex128)
    hist = np.asarray(hist, dtype=np.complex128)
    win = np.asarray(win, dtype=np.float64).real

    W = len(win)
    if len(inp_out) < 2 * W:
        raise ValueError(f"r_tx_windowing: inp_out length {len(inp_out)} < 2*W={2*W}")
    if len(hist) != W:
        raise ValueError(f"r_tx_windowing: hist length {len(hist)} != W={W}")

    # Quantise everything to the precisions the kernel actually uses
    head_in = r_convert(inp_out[:W],  'half_fixed')
    tail_in = r_convert(inp_out[-W:], 'half_fixed')
    hist_in = r_convert(hist,         'half_fixed')
    win_q   = r_convert(win,          'half')              # window is float16

    # Fade-in: out[0..W-1] = win·head + hist  (single-precision MAC, then SM16)
    head_out = r_convert(win_q * head_in + hist_in, 'half_fixed')

    # Fade-out: new_hist = (1 - win) · tail
    new_hist = r_convert((1.0 - win_q) * tail_in, 'half_fixed')

    inp_out[:W]  = head_out
    inp_out[-W:] = tail_in    # echo quantised tail (kernel leaves it untouched)
    hist[:]      = new_hist
    return inp_out, hist


def r_firFilter64(h, x, inp_hist=None,
                  input_prec='half_fixed', filter_prec='single',
                  output_prec='half_fixed'):
    """
    Bit-exact FIR filter model for the VSPA fir64 (`fir_filter`) kernel.

    Mirrors NXP's fir_filter.sx pipeline (vspa-lib/fir64/src/fir_filter.sx):
      - taps        : float32 (single-precision); sx `set.prec single,...`
      - input/state : cfixed16 (half_fixed, complex SM16)
      - output      : cfixed16 (half_fixed)
      - MAC chain   : truncating float32 (`r_smad`)

    The kernel keeps a (NUM_FIR_TAPS-1)-sample history; for the first call
    history is BSS-zeroed.  This oracle accepts an explicit `inp_hist` so
    callers can match the asm's read pattern exactly.

    Algorithm (per output sample n):
        y[n] = sum_{k=0..K-1} h[k] * x_extended[n + (K-1) - k]
             = h convolved with the (history || input) stream, decimated to
               same-rate (length N_x) FIR with taps in natural order h[0..K-1].

    Args:
        h        : (K,) real filter coefficients (will be converted to single)
        x        : (N_x,) complex input data (cfixed16 grid)
        inp_hist : (K-1,) complex prior samples, or None for zeros
        input_prec / filter_prec / output_prec : VSPA precision strings

    Returns:
        (y_out, next_hist) where y_out has shape (N_x,) at output_prec
        and next_hist has shape (K-1,) at input_prec.
    """
    h = np.asarray(h, dtype=np.float64).reshape(-1)
    x = np.asarray(x, dtype=np.complex128).reshape(-1)
    K = len(h)
    N = len(x)
    hist_len = K - 1

    if inp_hist is None:
        inp_hist = np.zeros(hist_len, dtype=np.complex128)
    inp_hist = np.asarray(inp_hist, dtype=np.complex128).reshape(-1)
    if len(inp_hist) != hist_len:
        raise ValueError(
            f"r_firFilter64: inp_hist must have {hist_len} samples, got {len(inp_hist)}")

    h_q   = r_convert(h,        filter_prec)         # float32-quantised taps
    x_q   = r_convert(x,        input_prec)
    hist_q = r_convert(inp_hist, input_prec)

    x_ext = np.concatenate([hist_q, x_q])            # length N + K - 1

    acc = np.zeros(N, dtype=np.complex128)
    # Match asm MAC pairing order: the sx schedule starts with the oldest
    # data (h[K-1] * x[n-K+1]) and accumulates forward (h[0] * x[n] last).
    # For truncating-float32 MACs the summation order is bit-significant —
    # natural-order summation drifts ~1 LSB on dense random inputs.
    for k in range(K - 1, -1, -1):
        # tap h[k] multiplies x[n - k]; with x_ext starting at n=-(K-1),
        # the data slice for tap k is x_ext[(K-1)-k : (K-1)-k + N].
        start = (K - 1) - k
        acc = r_smad(h_q[k], x_ext[start: start + N], acc)

    # Quantise real and imag to output_prec independently, then reassemble
    # via .real/.imag assignment so negative-zero signbits survive (numpy's
    # `re + 1j * im` collapses (-0)*1j → (+0j), which would lose the sx
    # hardware's 0x8000 SM16 encoding for tiny-negative MAC results).
    re_q = r_convert(acc.real, output_prec)
    im_q = r_convert(acc.imag, output_prec)
    y_out = np.empty(N, dtype=np.complex128)
    y_out.real = re_q
    y_out.imag = im_q

    next_hist = x_q[-hist_len:] if hist_len > 0 else np.empty(0, dtype=np.complex128)
    return y_out, next_hist


def r_firFilter(h, x, prec='half_fixed'):
    """
    Bit-exact FIR filter model for the VSPA fir33 kernel.

    Args:
        h    : (K,) real filter coefficients in double precision
        x    : (N_x,) complex input data in double precision
        prec : 'half_fixed' (Q15) or 'half' (float16)

    Returns:
        yr : (N_x - K + 1,) complex array in output precision (as float64/complex128)
    """
    h = np.asarray(h, dtype=np.float64)
    x = np.asarray(x, dtype=np.complex128)
    K = len(h)
    N = len(x) - K + 1
    if N <= 0:
        raise ValueError(f"Input too short: len(x)={len(x)} < K={K}")

    s1 = np.zeros((K, N), dtype=np.complex128)
    for k in range(K):
        s1[k, :] = x[k:N + k]

    h_rev = h[K - 1::-1]
    s0 = np.tile(h_rev.reshape(-1, 1), (1, N))

    if prec == 'half':
        yr = r_half_flt(r_smac(r_half_flt(s0), r_half_flt(s1), np.zeros(N, dtype=np.complex128)))
    else:
        yr = r_half(r_smac(r_half(s0), r_half(s1), np.zeros(N, dtype=np.complex128)))

    return yr
