# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2025 NXP (original MATLAB), Python port 2025
"""
Custom VSPA vector primitive oracles.

These are new DSP primitives not present in NXP vspa-lib.
"""

import numpy as np
from .arith import r_convert


def r_vector_add(x1: np.ndarray, x2: np.ndarray,
                 prec_in: str = 'half_fixed',
                 prec_out: str = 'half_fixed') -> np.ndarray:
    """
    Vector addition: z = x1 + x2.

    Args:
        x1, x2: Complex or real input vectors
        prec_in: Input precision ('half_fixed', 'single', etc.)
        prec_out: Output precision

    Returns:
        Sum vector in output precision
    """
    x1 = np.asarray(x1, dtype=np.complex128)
    x2 = np.asarray(x2, dtype=np.complex128)
    x1_p = r_convert(x1, prec_in)
    x2_p = r_convert(x2, prec_in)
    return r_convert(x1_p + x2_p, prec_out)


def r_vector_scale(x: np.ndarray, scale: float,
                   prec_in: str = 'half_fixed',
                   prec_scale: str = 'single',
                   prec_out: str = 'half_fixed') -> np.ndarray:
    """
    Vector scalar multiply: z = x * scale.

    Args:
        x: Complex or real input vector
        scale: Scalar (real) multiplier
        prec_in: Input precision
        prec_scale: Scalar precision
        prec_out: Output precision

    Returns:
        Scaled vector in output precision
    """
    x = np.asarray(x, dtype=np.complex128)
    scale_p = r_convert(scale, prec_scale)
    x_p = r_convert(x, prec_in)
    z = x_p * float(scale_p if isinstance(scale_p, (int, float)) else scale_p.real)
    return r_convert(z, prec_out)


def r_vector_rotate(x: np.ndarray, angle_rad: float,
                    prec_in: str = 'half_fixed',
                    prec_angle: str = 'single',
                    prec_out: str = 'half_fixed') -> np.ndarray:
    """
    Vector phase rotation: z = x * exp(j*angle).

    Simulates NCO + mixer composition.

    Args:
        x: Complex input vector
        angle_rad: Rotation angle in radians
        prec_in: Input precision
        prec_angle: Angle precision
        prec_out: Output precision

    Returns:
        Rotated vector in output precision
    """
    x = np.asarray(x, dtype=np.complex128)
    x_p = r_convert(x, prec_in)
    angle_p = r_convert(angle_rad, prec_angle)
    rot = np.exp(1j * float(angle_p if isinstance(angle_p, (int, float)) else angle_p.real))
    return r_convert(x_p * rot, prec_out)


def r_vector_multiply_conj(x1: np.ndarray, x2: np.ndarray,
                           prec_in: str = 'half_fixed',
                           prec_out: str = 'half_fixed') -> np.ndarray:
    """y[n] = x1[n] · conj(x2[n]) — element-wise, with output saturation."""
    x1 = np.asarray(x1, dtype=np.complex128)
    x2 = np.asarray(x2, dtype=np.complex128)
    x1_p = r_convert(x1, prec_in)
    x2_p = r_convert(x2, prec_in)
    return r_convert(x1_p * np.conj(x2_p), prec_out)


def r_vector_dot_complex(x1: np.ndarray, x2: np.ndarray,
                         prec_in: str = 'half_fixed',
                         prec_out: str = 'half_fixed') -> complex:
    """scalar = Σ x1[n] · conj(x2[n]) — complex dot product, saturated."""
    x1 = np.asarray(x1, dtype=np.complex128)
    x2 = np.asarray(x2, dtype=np.complex128)
    x1_p = r_convert(x1, prec_in)
    x2_p = r_convert(x2, prec_in)
    acc  = np.sum(x1_p * np.conj(x2_p))
    return complex(r_convert(acc, prec_out))


def r_vector_abs_square(x: np.ndarray,
                        prec_in: str = 'half_fixed',
                        prec_out: str = 'half_fixed') -> np.ndarray:
    """y[n] = |x[n]|² = Re² + Im² — real output, saturated to prec_out."""
    x = np.asarray(x, dtype=np.complex128)
    x_p = r_convert(x, prec_in)
    return r_convert(x_p.real * x_p.real + x_p.imag * x_p.imag, prec_out)


def r_vector_circular_shift(x: np.ndarray, shift: int,
                            prec_in: str = 'half_fixed',
                            prec_out: str = 'half_fixed') -> np.ndarray:
    """y = roll(x, shift) — circular permutation, no arithmetic."""
    x = np.asarray(x, dtype=np.complex128)
    x_p = r_convert(x, prec_in)
    return r_convert(np.roll(x_p, int(shift)), prec_out)


def r_complex_to_magnitude(x: np.ndarray,
                           prec_in: str = 'half_fixed',
                           prec_out: str = 'half_fixed') -> np.ndarray:
    """y[n] = |x[n]| ≈ sqrt(Re² + Im²) — real output, clipped to [0, 1]."""
    x = np.asarray(x, dtype=np.complex128)
    x_p = r_convert(x, prec_in)
    mag = np.abs(x_p)
    return r_convert(np.minimum(mag, 1.0 - 2.0**-15), prec_out)
