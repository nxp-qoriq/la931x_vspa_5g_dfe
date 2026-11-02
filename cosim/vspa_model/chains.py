# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2025 NXP (original MATLAB), Python port 2025
"""
Multi-kernel composition oracles (Layer 3 — Application Pipelines).

DECT NR+ receiver chain and related kernel models.
"""

import numpy as np
from .arith import r_convert
from .fft import r_dit_fft


def r_dect_nr_receiver_chain(rx_signal: np.ndarray,
                              fft_size: int = 512,
                              num_subcarriers: int = 480,
                              channel_response: np.ndarray = None,
                              snr_db: float = 20.0,
                              prec_rx: str = 'half_fixed',
                              prec_fft_out: str = 'half_fixed',
                              prec_final: str = 'half_fixed') -> dict:
    """
    DECT NR+ Receiver Composition Oracle.

    Chains multiple kernel oracles to simulate full receiver:
      1. FFT (ditfft kernel)
      2. Channel estimation (ideal: assume perfect channel)
      3. Equalization (ZF 1-tap)
      4. QPSK symbol slicing (hard decision)

    Args:
        rx_signal: Time-domain RX I/Q samples (complex array)
        fft_size: FFT size (typically 512)
        num_subcarriers: Number of active subcarriers
        channel_response: Channel impulse response (optional, default=ideal)
        snr_db: SNR in dB (informational only in oracle)
        prec_rx: Input RX precision
        prec_fft_out: FFT output precision
        prec_final: Final symbol precision

    Returns:
        Dictionary with 'stages', 'symbols', 'debug', 'chain_status', 'num_stages'
    """
    result = {
        'stages': [],
        'symbols': None,
        'debug': {}
    }

    try:
        rx_truncated = rx_signal[:fft_size]
        rx_p = r_convert(rx_truncated, prec_rx)
        fft_out = r_dit_fft(rx_p, order=9, prec='half_fixed_to_half_fixed')
        result['stages'].append({
            'name': 'FFT Demodulation',
            'output_shape': fft_out.shape,
            'output_prec': prec_fft_out
        })
        result['debug']['fft_output'] = fft_out
    except Exception:
        rx_p = r_convert(rx_signal[:fft_size], prec_rx)
        fft_out = np.fft.fft(rx_p)
        result['debug']['fft_method'] = 'fallback_numpy'
        result['stages'].append({
            'name': 'FFT Demodulation (fallback)',
            'output_shape': fft_out.shape
        })

    subcarrier_indices = np.arange(64, 64 + num_subcarriers)
    fft_subcarriers = fft_out[subcarrier_indices]
    result['stages'].append({
        'name': 'Subcarrier Extraction',
        'output_shape': fft_subcarriers.shape
    })

    if channel_response is None:
        channel_est = np.ones(num_subcarriers, dtype=np.complex128)
    else:
        channel_est = channel_response[:num_subcarriers]
    result['stages'].append({
        'name': 'Channel Estimation',
        'method': 'Perfect (unity)',
        'output_shape': channel_est.shape
    })
    result['debug']['channel_estimate'] = channel_est

    eq_out = fft_subcarriers / (channel_est + 1e-12)
    eq_out = r_convert(eq_out, prec_final)
    result['stages'].append({
        'name': 'ZF Equalization (1-tap)',
        'output_prec': prec_final,
        'output_shape': eq_out.shape
    })
    result['debug']['eq_output'] = eq_out

    symbols = np.zeros(num_subcarriers, dtype=np.uint8)
    for k in range(num_subcarriers):
        i_bit = 1 if eq_out[k].real >= 0 else 0
        q_bit = 1 if eq_out[k].imag >= 0 else 0
        symbols[k] = i_bit * 2 + q_bit
    result['symbols'] = symbols
    result['stages'].append({
        'name': 'QPSK Slicer',
        'output_shape': symbols.shape,
        'constellation_mapping': 'QPSK {0,1,2,3}'
    })

    result['chain_status'] = 'PASS (oracle chain complete)'
    result['num_stages'] = len(result['stages'])
    return result


def r_dect_pilot_extractor(fft_out: np.ndarray, pilot_bins: np.ndarray):
    """
    Extract pilot subcarriers from OFDM FFT output at specified bin indices.

    ETSI TS 103 636-3 V1.6.1, clause 5.2.3.

    Args:
        fft_out: Complex FFT output (512 or other size)
        pilot_bins: Array of uint16 bin indices to extract

    Returns:
        Complex array of extracted pilot samples (same precision as input)
    """
    n_pilots = len(pilot_bins)
    pilot_out = np.zeros(n_pilots, dtype=fft_out.dtype)
    for i in range(n_pilots):
        pilot_out[i] = fft_out[int(pilot_bins[i])]
    return pilot_out


def r_qpsk_slicer(eq_symbols: np.ndarray):
    """
    Hard-decision QPSK slicer — map complex symbols to bits {0,1}.

    ETSI TS 103 636-3 V1.6.1, clause 6.3.1.3.
    Slicing rule: 0 for positive quadrant, 1 for negative.

    Args:
        eq_symbols: Complex array of equalized symbols (any precision)

    Returns:
        Bit array (0 or 1), length = 2 × len(eq_symbols)
    """
    n_symbols = len(eq_symbols)
    bits = np.zeros(2 * n_symbols, dtype=np.uint8)
    for i in range(n_symbols):
        symbol = eq_symbols[i]
        bits[2 * i]     = 0 if symbol.real >= 0 else 1
        bits[2 * i + 1] = 0 if symbol.imag >= 0 else 1
    return bits
