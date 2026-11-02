# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2025 NXP (original MATLAB), Python port 2025
"""
VSPA QAM modulator (r_qamMod) — Python oracle.

Faithful Python port of NXP `r_qamMod.m`
(submodules/la931x_vspa_common/vspa-lib/qam/matlab/r_qamMod.m), validated
bit-exact against the cwproj golden vectors for all six constellations
(BPSK, QPSK, 16-QAM, 64-QAM, 256-QAM, 1024-QAM).

Algorithm (per MATLAB reference):

    bits  = reshape(bitStream, M, [])           # M = bits per symbol
    bI    = sum_k 2^k * bits(k,        :)       # k = 0 .. M/2 - 1
    bQ    = sum_k 2^k * bits(k+M/2,    :)
    sym   = mapTable(bI+1) + 1j * mapTable(bQ+1)
    y_bx  = r_half_flt(r_smad(sym, r_single(normalization), 0))

For BPSK (M=1) the MATLAB code splits to a single component (no Q) but
the asm still emits a `vspa_complex_float16` whose imag part is forced
to zero by `set.Smode S2zeros`. We mirror that by setting `bQ = 0` and
`mapTable(1)` accordingly so the imag oracle output is exactly +0.0
half-float (0x0000) — matching the cwproj BPSK golden vectors.

Bit ordering inside each uint32 word: LSB-first. cwproj golden BPSK
verifies this — input.hex word 0 = 0x57DE5B9B has LSB = 1, and
output_bpsk_expected.hex sample 0 = 0x00003C00 = (real=+1.0, imag=0).

Constellation tables (mapTable) are taken **verbatim** from the MATLAB
reference. They follow IEEE 802.11 Gray coding but the index ordering
inside each table is non-monotonic; do not "simplify" to a textbook
formula — the asm `ld.qam` instruction reads the same table indices.

Public API:
    r_qam_mod(bits_u32, mode)        -> uint32 ndarray of cfloat16 symbols
    QAM_MODES                        -> dict mode_name -> spec dict
"""

import numpy as np

from .arith import r_smad, r_half_flt, r_single


# IEEE 802.11 mapTable values (real and imag axes share the same table).
# Index = decimal value of the M/2-bit Gray-coded input. Verbatim from
# r_qamMod.m; do not reorder.
_MAP_TABLES = {
    'bpsk':   np.array([-1, 1], dtype=np.float64),
    'qpsk':   np.array([-1, 1], dtype=np.float64),
    '16qam':  np.array([-3, 3, -1, 1], dtype=np.float64),
    '64qam':  np.array([-7, 7, -1, 1, -5, 5, -3, 3], dtype=np.float64),
    '256qam': np.array([-15, 15, -1, 1, -9, 9, -7, 7,
                        -13, 13, -3, 3, -11, 11, -5, 5], dtype=np.float64),
    '1024qam': np.array([-31, 31, -1, 1, -17, 17, -15, 15, -25, 25,
                         -7, 7, -23, 23, -9, 9, -29, 29, -3, 3,
                         -19, 19, -13, 13, -27, 27, -5, 5, -21, 21,
                         -11, 11], dtype=np.float64),
}


# Normalization factors. Match QAM_NORMALIZATION_* hex constants in
# submodules/.../qam/include/qam.h verbatim (IEEE-754 float32 bit
# patterns). Using r_single() here pins the value to single precision
# so the smad chain matches the asm's float32 multiplier register.
def _f32_from_u32(u):
    return np.frombuffer(np.uint32(u).tobytes(), dtype='<f4')[0]


QAM_MODES = {
    'bpsk':    {'M': 1,  'norm_u32': 0x3F800000},   # 1
    'qpsk':    {'M': 2,  'norm_u32': 0x3F3504F3},   # 1/sqrt(2)
    '16qam':   {'M': 4,  'norm_u32': 0x3EA1E89B},   # 1/sqrt(10)
    '64qam':   {'M': 6,  'norm_u32': 0x3E1E01B2},   # 1/sqrt(42)
    '256qam':  {'M': 8,  'norm_u32': 0x3D9D130D},   # 1/sqrt(170)
    '1024qam': {'M': 10, 'norm_u32': 0x3D1CD80C},   # 1/sqrt(682)
}
for _m, _s in QAM_MODES.items():
    _s['norm_f32'] = _f32_from_u32(_s['norm_u32'])
    _s['map_table'] = _MAP_TABLES[_m]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _unpack_bits_lsb_first(words_u32, n_bits):
    """Unpack a uint32 array LSB-first into a flat int8 (0/1) array of
    length n_bits. Word k bit b → output index k*32 + b."""
    words = np.asarray(words_u32, dtype=np.uint32).ravel()
    needed_words = (n_bits + 31) // 32
    if words.size < needed_words:
        raise ValueError(
            f'need {needed_words} input words for {n_bits} bits, got {words.size}')
    bits = np.zeros(needed_words * 32, dtype=np.int8)
    for b in range(32):
        bits[b::32] = ((words >> np.uint32(b)) & np.uint32(1)).astype(np.int8)
    return bits[:n_bits]


def _pack_cfloat16(c):
    """Pack a complex array (numpy complex128, components already in
    half-float-truncated values) into uint32 with (imag<<16) | real."""
    re_f16 = np.asarray(c.real, dtype=np.float16)
    im_f16 = np.asarray(c.imag, dtype=np.float16)
    re_u16 = np.frombuffer(re_f16.tobytes(), dtype='<u2').astype(np.uint32)
    im_u16 = np.frombuffer(im_f16.tobytes(), dtype='<u2').astype(np.uint32)
    return (im_u16 << np.uint32(16)) | re_u16


# ---------------------------------------------------------------------------
# Public oracle
# ---------------------------------------------------------------------------

def r_qam_mod(bits_u32, mode):
    """VSPA-precision QAM modulator (mirrors r_qamMod.m).

    Args:
        bits_u32 : uint32 array (LSB-first packed bit stream). Must hold
                   at least N*M bits where N is the number of output
                   symbols and M = QAM_MODES[mode]['M'].
        mode     : one of {'bpsk','qpsk','16qam','64qam','256qam','1024qam'}.

    Returns:
        uint32 ndarray of length N (one cfloat16 symbol per element,
        packed as (imag_fp16 << 16) | real_fp16). N is determined as
        floor(total_bits / M).
    """
    if mode not in QAM_MODES:
        raise ValueError(f'unknown mode {mode!r}; expected one of '
                         f'{sorted(QAM_MODES)}')
    spec = QAM_MODES[mode]
    M = spec['M']
    map_tab = spec['map_table']
    norm = spec['norm_f32']

    words = np.asarray(bits_u32, dtype=np.uint32).ravel()
    total_bits = words.size * 32
    N = total_bits // M
    if N == 0:
        return np.empty(0, dtype=np.uint32)

    bits = _unpack_bits_lsb_first(words, N * M)
    # MATLAB: bits = reshape(bitStream, M, []) — column-major. The first
    # M consecutive bits form symbol 0 (column 0).
    grid = bits.reshape(N, M).T  # shape (M, N)

    if M > 1:
        bI = grid[:M // 2, :]
        bQ = grid[M // 2:, :]
        weights = (1 << np.arange(M // 2, dtype=np.int64))
        idxI = (weights @ bI.astype(np.int64))
        idxQ = (weights @ bQ.astype(np.int64))
        sym_re = map_tab[idxI]
        sym_im = map_tab[idxQ]
    else:
        idxI = grid[0, :].astype(np.int64)
        sym_re = map_tab[idxI]
        sym_im = np.zeros(N, dtype=np.float64)

    # r_smad(sym * norm + 0) — implemented per real/imag axis so the
    # truncating-float32 chain matches the asm's per-component multiply.
    norm_arr = np.full(N, norm, dtype=np.float64)
    out_re = r_smad(sym_re, r_single(norm_arr), np.zeros(N))
    out_im = r_smad(sym_im, r_single(norm_arr), np.zeros(N))

    out_re_h = r_half_flt(out_re)
    out_im_h = r_half_flt(out_im)

    return _pack_cfloat16(out_re_h + 1j * out_im_h)
