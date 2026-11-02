# SPDX-License-Identifier: BSD-3-Clause
"""Fixed-point model of the vspa-lib time-domain QEC kernel (``qec_opt_asm``).

The kernel is ``submodules/la931x_vspa_common/vspa-lib/qec/src/qec_opt.sx``, the
"2nd round optimization" body (-DVSPA2). With ``pad0 = 0`` it is memoryless and
per-sample:

    out_I = f1 * I            + dcoff_I
    out_Q = f2 * I + f4 * Q   + dcoff_Q

Samples are half-fixed (SM16, sign-magnitude, 15 fractional bits); the AU stores
the result by TRUNCATING the magnitude, which is what ``_float_to_sm16`` does.
Coefficients and DC terms are float32.

TWO DOMAINS, ONE FACTOR OF 32768
    The co-simulation carries samples as SM16 *counts* (full scale = 32768), the
    kernel's coefficients live in the *fractional* domain (full scale = 1.0).
    f1/f2/f4 are ratios, so they are the same number in both. The DC terms are
    not: a DC of ``d`` counts is ``d / 32768`` here. ``qec_params_words`` takes
    them in the kernel's domain; ``dc_counts`` below is the conversion.
"""
from __future__ import annotations

import struct

import numpy as np

from .io import _float_to_sm16, _sm16_to_float

Q = 32768.0

#: float32 word index of each field of ``qec_params_opt_t`` (qec/inc/qec_opt.h).
#: The struct is one 128-byte vector line: 32 float32, of which 12 are used.
PARAM_INDEX = {
    "f1": 0, "f4": 1, "pad0": 2, "f2": 3, "dcoff_I": 4, "dcoff_Q": 5,
    "f1_backup": 6, "f4_backup": 7, "pad1": 8, "f2_backup": 9,
    "dcoff_I_backup": 10, "dcoff_Q_backup": 11,
}
N_PARAM_WORDS = 32


def imbalance_to_params(eps: float, phi: float) -> tuple[float, float, float]:
    """(f1, f4, f2) that invert a receiver I/Q imbalance (eps, phi).

    Imbalance model — I is the reference branch, Q carries both errors:

        I_imb = I
        Q_imb = (1 + eps) * (Q * cos(phi) + I * sin(phi))

    Substituting into the kernel's transfer function gives back (I, Q) exactly.
    """
    return (1.0,
            1.0 / ((1.0 + eps) * np.cos(phi)),
            -np.tan(phi))


def apply_imbalance(x: np.ndarray, eps: float, phi: float) -> np.ndarray:
    """The channel-side impairment that :func:`imbalance_to_params` inverts."""
    x = np.asarray(x)
    i, q = np.real(x), np.imag(x)
    return i + 1j * ((1.0 + eps) * (q * np.cos(phi) + i * np.sin(phi)))


def qec_params_words(f1: float, f4: float, f2: float,
                     dcoff_I: float = 0.0, dcoff_Q: float = 0.0) -> np.ndarray:
    """The 32 float32 words of ``qec_params_opt_t``, as uint32 bit patterns.

    This is what the host writes to the kernel's ``qec_params`` port (enc="u32").
    ``pad0`` stays 0 — a non-zero pad0 turns the kernel stateful.
    """
    p = np.zeros(N_PARAM_WORDS, dtype=np.float32)
    for base in ("", "_backup"):
        p[PARAM_INDEX["f1" + base]] = f1
        p[PARAM_INDEX["f4" + base]] = f4
        p[PARAM_INDEX["f2" + base]] = f2
        p[PARAM_INDEX["dcoff_I" + base]] = dcoff_I
        p[PARAM_INDEX["dcoff_Q" + base]] = dcoff_Q
    return p.view(np.uint32)


def dc_counts(d: float) -> float:
    """A DC offset of ``d`` SM16 counts, in the kernel's fractional domain."""
    return float(struct.unpack("<f", struct.pack("<f", d / Q))[0])


def r_qec_td(x_counts: np.ndarray, f1: float, f4: float, f2: float,
             dcoff_I: float = 0.0, dcoff_Q: float = 0.0) -> np.ndarray:
    """Bit-exact model of ``qec_opt_asm``, in SM16 counts in and out.

    ``x_counts`` is complex with integer real/imag parts (SM16 counts) — i.e.
    exactly what the kernel sees after the host's ``sm16_complex`` encoding.
    """
    x = np.asarray(x_counts, dtype=np.complex128).ravel()
    i = np.real(x) / Q
    q = np.imag(x) / Q
    out_i = f1 * i + dcoff_I
    out_q = f2 * i + f4 * q + dcoff_Q
    # Store as half-fixed (magnitude truncation), then back to counts.
    out_i = _sm16_to_float(_float_to_sm16(out_i)) * Q
    out_q = _sm16_to_float(_float_to_sm16(out_q)) * Q
    return out_i + 1j * out_q
