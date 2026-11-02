# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2025 NXP (original MATLAB), Python port 2025
"""
VSPA trigonometric models: r_atan, r_atan2.

Ported from NXP r_atan.m and r_atan2.m (vspa-lib/atan/matlab/).
"""

import numpy as np
from .arith import r_convert, r_smad, r_single
from .rcp import r_rcp


def _atan_coeffs(num_coeff: int, method: str = 'poly_fit',
                 coeff_prec: str = 'single', norm: bool = True) -> np.ndarray:
    """Derive polynomial coefficients matching MATLAB poly_fit or poly method."""
    coeffs_are_normalized = False
    if method == 'poly_fit':
        atan_coeff_hex = {
            3:  [0x3CD83074, 0xBDBE785D, 0x3EA2512E],
            4:  [0xBC54B5E1, 0x3D422AF3, 0xBDD21235, 0x3EA2DCD7],
            5:  [0x3BE387DB, 0xBCE30D0E, 0x3D6C7EAC, 0xBDD78226, 0x3EA2F49D],
            6:  [0xBB7FD1DF, 0x3C8CCC40, 0xBD195A56, 0x3D7CED30, 0xBDD8E161, 0x3EA2F8AC],
            7:  [0x3B14A927, 0xBC3422F3, 0x3CD263BE, 0xBD2D4665, 0x3D813894, 0xBDD93499, 0x3EA2F95E],
            8:  [0xBAB0EA77, 0x3BEAC77B, 0xBC941EB9, 0x3CFD0B06, 0xBD35A2E5, 0x3D820F0A, 0xBDD94772, 0x3EA2F97D],
            9:  [0x3A564FF0, 0xBB9AD7E4, 0x3C533BE3, 0xBCBE44BB, 0x3D094635, 0xBD38BD2F, 0x3D824B97, 0xBDD94B92, 0x3EA2F982],
            10: [0xBA039CC6, 0x3B4DEE50, 0xBC178FAF, 0x3C9101F0, 0xBCD717D7, 0x3D0DFDC2, 0xBD39CA08, 0x3D825BC1, 0xBDD94C74, 0x3EA2F983],
        }
        if num_coeff not in atan_coeff_hex:
            raise ValueError(f'Unsupported num_coeff for ATAN poly_fit: {num_coeff}')
        coeffs = np.array(atan_coeff_hex[num_coeff], dtype=np.uint32).view(np.float32).astype(np.float64)
        coeffs_are_normalized = True
    elif method == 'poly':
        idx = np.arange(num_coeff - 1, -1, -1)
        coeffs = ((-1.0) ** idx) / (2.0 * idx + 1.0)
    else:
        raise ValueError(f"Unknown method: {method}")

    if norm and not coeffs_are_normalized:
        coeffs = coeffs / np.pi

    return np.array([r_convert(c, coeff_prec) for c in coeffs])


def r_atan(x, num_coeff: int = 4, method: str = 'poly_fit',
           inp_prec: str = 'single', coeff_prec: str = 'single',
           out_prec: str = 'single', norm: bool = True) -> np.ndarray:
    """
    Polynomial atan(x) approximation normalized to PI.
    Matches r_atan.m VSPA behavioral model.
    """
    x = np.asarray(x, dtype=np.float64).flatten()
    tan_v = r_convert(x, inp_prec)

    coeff_v = _atan_coeffs(num_coeff, method, coeff_prec, norm)

    X  = tan_v
    X2 = r_smad(X, X, 0.0)
    V  = r_smad(X2, coeff_v[0], coeff_v[1])
    for ci in coeff_v[2:]:
        V = r_smad(V, X2, ci)
    V = r_smad(V, X, 0.0)

    return r_convert(V, out_prec)


def r_atan2(inp_complex: np.ndarray,
            inp_prec: str = 'half_fixed',
            coeff_prec: str = 'single',
            num_coeff: int = 4,
            out_prec: str = 'half_fixed',
            method: str = 'poly_fit',
            norm: bool = True) -> np.ndarray:
    """
    VSPA behavioral model for full-circle phase extraction.

    Computes: phase = atan2(imag, real) / pi  (normalized, default)

    Matches the VSPA kernel atan2_x64_chf_hf_asm (and other variants)
    with the same precision steps as the NXP MATLAB r_atan2.m model.

    Args:
        inp_complex : Complex input array (any numeric type, converted internally)
        inp_prec    : Input precision  ('half_fixed', 'half', 'single', 'double')
        coeff_prec  : Coefficient precision (always 'single' in NXP kernels)
        num_coeff   : Number of polynomial coefficients (3..10, default 4)
        out_prec    : Output precision ('half_fixed', 'half', 'single', 'double')
        method      : Polynomial method ('poly_fit' default, or 'poly')
        norm        : If True, output normalized to PI (default)

    Returns:
        1-D float64 array of phase values in output precision
    """
    inp = np.asarray(inp_complex, dtype=np.complex128).flatten()

    re = r_convert(inp.real, inp_prec)
    im = r_convert(inp.imag, inp_prec)

    re_rcp = r_rcp(re)
    im_rcp = r_rcp(im)

    tan_v     = r_smad(im, re_rcp, np.zeros_like(im))
    tan_rcp_v = r_smad(re, im_rcp, np.zeros_like(re))

    tan_rcp_diff = r_smad(-np.abs(tan_rcp_v), r_single(1.0), r_single(1.0))
    mask_inv_x = r_single(np.where(tan_rcp_diff >= 0.0, 1.0, 0.0)).astype(np.float64)

    tan_p = r_smad(-tan_v,     mask_inv_x, tan_v)
    tan_p = r_smad(-tan_rcp_v, mask_inv_x, tan_p)

    atan_p = r_atan(tan_p, num_coeff=num_coeff, method=method,
                    inp_prec='single', coeff_prec=coeff_prec,
                    out_prec='single', norm=norm)

    sign_tan = r_single(np.where(tan_v < 0.0, -1.0, 1.0)).astype(np.float64)
    add_half  = r_smad(sign_tan, mask_inv_x, 0.0)

    mask_re   = r_single(np.where(re < 0.0, 0.0, 1.0)).astype(np.float64)
    sign_im   = np.where(im < 0.0, -1.0, 1.0).astype(np.float64)
    with np.errstate(divide='ignore', invalid='ignore'):
        im_inv = 1.0 / im
    sign_im[im_inv == np.inf] = 1.0
    sign_im[im_inv == -np.inf] = -1.0
    sign_im   = r_single(sign_im).astype(np.float64)
    add_full  = r_smad(-mask_re, sign_im, sign_im)

    add_total = r_smad(add_half, r_single(0.5), add_full)
    phase     = r_smad(atan_p, r_single(1.0), add_total)

    if not norm:
        phase = r_smad(phase, r_single(np.pi), 0.0)

    if out_prec == 'half_fixed':
        if not norm:
            raise ValueError("Without normalization half_fixed output will saturate!")
        HF16_MAX = 1.0 - 2.0**-15
        phase = np.where((-1.0 <= phase) & (phase <= -HF16_MAX), -HF16_MAX, phase)
        phase = np.where((HF16_MAX <= phase) & (phase <= 1.0),   HF16_MAX,  phase)

    return r_convert(phase, out_prec).reshape(inp.shape)
