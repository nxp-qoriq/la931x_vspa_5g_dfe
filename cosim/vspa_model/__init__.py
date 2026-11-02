# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2025 NXP (original MATLAB), Python port 2025
"""
Python behavioral model of NXP VSPA kernels.

All public symbols are re-exported here so existing code using
  from vspa_model import r_smad, r_half, ...
continues to work without any changes.

Sub-module layout:
  arith   — r_single, r_half, r_half_flt, r_convert, r_smad, r_smac
  rcp     — r_rcp
  trig    — r_atan, r_atan2
  nco     — r_nco
  fft     — _bit_rev_order, r_dit_fft, r_dif_fft
  filter  — r_decimator_2x, r_decimator, r_interpolator_4x, r_interpolator_2x, r_firFilter
  matrix  — r_mat_bmult
  io      — _sm16_to_float, _float_to_sm16, read_binary, read_complex_binary, read_ctrl, PREC_IDX
  vector  — r_vector_add, r_vector_scale, r_vector_rotate
  chains  — r_dect_nr_receiver_chain, r_dect_pilot_extractor, r_qpsk_slicer
"""

from .arith import (
    r_single,
    r_half,
    r_half_flt,
    r_convert,
    r_smad,
    r_smac,
)

from .rcp import r_rcp

from .log import r_log, r_log2, LOG_FACT_VALUES

from .qam import r_qam_mod, QAM_MODES

from .trig import (
    r_atan,
    r_atan2,
)

from .nco import r_nco

from .fft import (
    _bit_rev_order,
    r_dit_fft,
    r_dif_fft,
    r_dif_fft_sc128,
    r_dft,
)

from .filter import (
    r_decimator_2x,
    r_decimator,
    r_interpolator_4x,
    r_interpolator_2x,
    r_firFilter,
    r_firFilter64,
    r_tx_windowing,
)

from .matrix import r_mat_bmult

from .io import (
    _sm16_to_float,
    _float_to_sm16,
    read_binary,
    read_complex_binary,
    read_ctrl,
    PREC_IDX,
)

from .vector import (
    r_vector_add,
    r_vector_scale,
    r_vector_rotate,
    r_vector_multiply_conj,
    r_vector_dot_complex,
    r_vector_abs_square,
    r_vector_circular_shift,
    r_complex_to_magnitude,
)

from .chains import (
    r_dect_nr_receiver_chain,
    r_dect_pilot_extractor,
    r_qpsk_slicer,
)

from . import nr5g

__all__ = [
    # arith
    'r_single', 'r_half', 'r_half_flt', 'r_convert', 'r_smad', 'r_smac',
    # rcp
    'r_rcp',
    # log
    'r_log', 'r_log2', 'LOG_FACT_VALUES',
    # qam
    'r_qam_mod', 'QAM_MODES',
    # trig
    'r_atan', 'r_atan2',
    # nco
    'r_nco',
    # fft
    '_bit_rev_order', 'r_dit_fft', 'r_dif_fft', 'r_dif_fft_sc128', 'r_dft',
    # filter
    'r_decimator_2x', 'r_decimator',
    'r_interpolator_4x', 'r_interpolator_2x',
    'r_firFilter', 'r_firFilter64', 'r_tx_windowing',
    # matrix
    'r_mat_bmult',
    # io
    '_sm16_to_float', '_float_to_sm16',
    'read_binary', 'read_complex_binary', 'read_ctrl', 'PREC_IDX',
    # vector
    'r_vector_add', 'r_vector_scale', 'r_vector_rotate',
    'r_vector_multiply_conj', 'r_vector_dot_complex',
    'r_vector_abs_square', 'r_vector_circular_shift',
    'r_complex_to_magnitude',
    # chains
    'r_dect_nr_receiver_chain', 'r_dect_pilot_extractor', 'r_qpsk_slicer',
    # nr5g (3GPP TS 38.211 NR sequences)
    'nr5g',
]
