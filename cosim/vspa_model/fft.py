# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2025 NXP (original MATLAB), Python port 2025
"""
VSPA FFT behavioral models: r_dit_fft, r_dif_fft, r_dft.

Ported from NXP r_dit_fft.m, r_dif_fft.m, and r_dft.m
(vspa-lib/ditfft/, diffft/, dft/matlab/).
"""

import numpy as np
from .arith import r_smad, r_half, r_half_flt, r_single, r_convert
from .nco import r_nco


def r_dft(inp, inp_prec='half_fixed', out_prec='half'):
    """
    VSPA O(N^2) DFT model — direct port of NXP r_dft.m.

    Computes  Y[k] = sum_{n=0..N-1}  x[n] * exp(-2pi*j*n*k/N)
    using the VSPA NCO + r_smad chain.  Matches MATLAB r_dft to within
    ~1 ULP (the residual is intermediate accumulator drift in the long
    SMAD chain, exactly the class of issue noted in r_decimator_2x.m's
    "closer to bit-exact" comment).

    Used by the `mini_dft_*_asm` and `dft_*_asm` kernels (N <= 512).

    Args:
        inp       : 1-D complex input of length N.
        inp_prec  : input precision  ('half_fixed' | 'half' | 'single' | 'double')
        out_prec  : output precision (same options)

    Returns:
        complex128 ndarray of shape (N,), quantised to out_prec.
    """
    inp = np.asarray(inp, dtype=np.complex128).reshape(-1)
    N = len(inp)
    inp_v = r_convert(inp, inp_prec)

    nco_freq = round(2**31 / N)

    s0_v = np.zeros(N, dtype=np.complex128)
    for n in range(N):
        nco_k = 2 * n
        S0 = inp_v[n]                                       # scalar input sample
        S1 = r_nco(nco_freq * nco_k / 2**32, N, phase_count=0)
        s0_v = r_smad(S0, S1, s0_v)

    return r_convert(s0_v, out_prec)


def _bit_rev_order(N):
    """Return bit-reversal permutation array of length N (N must be power of 2)."""
    n_bits = int(round(np.log2(N)))
    return np.array([int(f'{i:0{n_bits}b}'[::-1], 2) for i in range(N)], dtype=np.int32)


def r_dit_fft(x, inv_flag=0, prec_type='half_fixed', scale_out=0):
    """
    VSPA DIT-FFT behavioral model.

    Args:
        x         : Complex input array of length N (power of 2).
        inv_flag  : 0 = forward FFT, 1 = inverse FFT.
        prec_type : Precision mode: 'half_fixed', 'single', or 'mixed'.
        scale_out : 1 = apply 1/2 per-stage scaling (half_fixed mode only).

    Returns:
        Complex128 ndarray of length N in the specified output precision.
    """
    x = np.asarray(x, dtype=np.complex128)
    N = len(x)
    n_stages = int(round(np.log2(N)))
    n_vrastages = 9
    n_scalestages = max(n_stages - n_vrastages, 0)

    scale_fact = 2 if (prec_type == 'half_fixed' and scale_out == 1) else 1

    br_idx = _bit_rev_order(N)
    y = x[br_idx].copy()

    n_samples_grp = 1
    n_groups = N // 2
    freq = 1.0 / N

    for stage_i in range(1, n_stages + 1):
        if not inv_flag:
            twf = r_nco(freq * n_groups, n_samples_grp, 0)
        else:
            twf = r_nco(-freq * n_groups, n_samples_grp, 0)

        Y = y.reshape(n_groups, 2, n_samples_grp)
        T1 = Y[:, 0, :].copy()
        T2 = Y[:, 1, :].copy()
        Y[:, 0, :] = r_smad(T2,  twf, T1)
        Y[:, 1, :] = r_smad(-T2, twf, T1)
        y = Y.reshape(N)

        if prec_type == 'half_fixed':
            y = r_half(y / scale_fact)
        elif prec_type == 'mixed':
            if stage_i > (n_stages - n_scalestages):
                y = r_half(y / scale_fact)
            else:
                y = r_single(y)
        else:
            y = r_single(y)

        n_samples_grp *= 2
        n_groups //= 2

    return y


def r_dif_fft(x, inv_flag=0, prec_type='half_fixed', scale_out=0):
    """
    VSPA DIF-FFT behavioral model.

    Args:
        x         : Complex input array of length N (power of 2).
        inv_flag  : 0 = forward FFT, 1 = inverse FFT.
        prec_type : Precision mode: 'half_fixed', 'half', or 'single'.
        scale_out : 1 = apply 1/2 per-stage scaling (half_fixed mode only).

    Returns:
        Complex128 ndarray of length N in the specified output precision.
    """
    x = np.asarray(x, dtype=np.complex128)
    N = len(x)
    n_stages = int(round(np.log2(N)))

    scale_fact = 2 if (prec_type == 'half_fixed' and scale_out == 1) else 1

    br_idx = _bit_rev_order(N)

    y = x.copy()
    n_samples_grp = N // 2
    n_groups = 1
    freq = 1.0 / N

    for stage_i in range(1, n_stages + 1):
        if inv_flag:
            twf = r_nco(-freq * n_groups, n_samples_grp, 0)
        else:
            twf = r_nco(freq * n_groups, n_samples_grp, 0)

        Y = y.reshape(n_groups, 2, n_samples_grp)
        T1 = Y[:, 0, :]
        T2 = Y[:, 1, :]
        inner = r_smad(T1, twf, np.zeros_like(T1))
        Y[:, 0, :] = r_smad(T2,  np.float64(1.0), T1)
        Y[:, 1, :] = r_smad(-T2, twf, inner)
        y = Y.reshape(N)

        if prec_type == 'half_fixed':
            y = r_half(y / scale_fact)
        elif prec_type == 'half':
            y = r_half_flt(y)
        else:
            y = r_single(y)

        n_samples_grp //= 2
        n_groups *= 2

    y_out = np.empty_like(y)
    y_out[br_idx] = y
    return y_out


def r_dif_fft_sc128(x, inv_flag=0):
    """
    DIF FFT/IFFT for fftDIF1024_sc128_hfx_hfx_vspa2 / ifftDIF1024_sc128_hfx_hfx_vspa2.

    The assembly (`fft_DIF_1024_hfx_vspa2.sx`) applies /2 per-stage scaling at
    stages 1-7 (total gain 1/128 = sc128) then turns scaling off at stage 8 via
    `set.creg 13,0`.  Round-trip (IFFT then FFT): A_out = A_in * N/128^2 = A_in/16.

    Args:
        x        : Complex input array of length 1024.
        inv_flag : 0 = forward FFT, 1 = inverse FFT.

    Returns:
        Complex128 ndarray of length 1024, half_fixed (Q15) precision, natural bin order.
    """
    x = np.asarray(x, dtype=np.complex128)
    N = len(x)
    assert N == 1024, "r_dif_fft_sc128 requires N=1024"
    n_stages = 10
    n_scaled_stages = 7   # stages 1..7 apply /2; matches `set.creg 13,0` at stage 8

    br_idx = _bit_rev_order(N)
    y = x.copy()
    n_samples_grp = N // 2
    n_groups = 1
    freq = 1.0 / N

    for stage_i in range(1, n_stages + 1):
        if inv_flag:
            twf = r_nco(-freq * n_groups, n_samples_grp, 0)
        else:
            twf = r_nco(freq * n_groups, n_samples_grp, 0)

        Y = y.reshape(n_groups, 2, n_samples_grp)
        T1, T2 = Y[:, 0, :], Y[:, 1, :]
        inner = r_smad(T1, twf, np.zeros_like(T1))
        Y[:, 0, :] = r_smad(T2, np.float64(1.0), T1)
        Y[:, 1, :] = r_smad(-T2, twf, inner)
        y = Y.reshape(N)

        y = r_half(y / 2.0) if stage_i <= n_scaled_stages else r_half(y)

        n_samples_grp //= 2
        n_groups *= 2

    y_out = np.empty_like(y)
    y_out[br_idx] = y
    return y_out
