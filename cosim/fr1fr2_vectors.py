# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""Adapter for NXP fr1_fr2_test_tool waveform vectors.

The FR1/FR2 test tool ships test vectors as raw binary files of interleaved
little-endian int16 I,Q pairs, Q15-scaled (see utils/conv_bin2csv.py in
nxp-qoriq/fr1_fr2_test_tool).  Two flavors exist:

- frequency-domain grids (``*_fd.bin`` and the TDD injection vectors):
  symbol-major RE grids, one full 10 ms or 20 ms radio frame, each OFDM
  symbol stored as ``sym_len`` complex samples where ``sym_len`` is the
  number of used subcarriers, zero-padded up to a round buffer size for
  some bandwidths (e.g. 612 used SCs stored as 640 at 20 MHz / 30 kHz).

- time-domain waveforms (``*_timedomain_*sps.bin`` etc.): a flat sample
  stream at the rate named in the file.

This module parses the fd grids into per-symbol slices ready for DMEM
injection (raw bytes are already in VSPA DMEM wire format — no re-encoding)
and into complex numpy arrays for analysis.

Fetch the vectors with ``scripts/fetch_fr1fr2.sh`` (untracked,
``nxp/fr1_fr2_test_tool/test_vectors/``).
"""

from __future__ import annotations

import os
from dataclasses import dataclass

import numpy as np

#: 3GPP 38.101-1 Table 5.3.2-1 — max transmission bandwidth N_RB for FR1,
#: keyed by (channel_bw_mhz, scs_khz).  Only LA9310-relevant entries.
NRB_TABLE = {
    (10, 15): 52,
    (10, 30): 24,
    (10, 60): 11,
    (20, 15): 106,
    (20, 30): 51,
    (20, 60): 24,
    (25, 15): 133,
    (25, 30): 65,
    (25, 60): 31,
}

SYMBOLS_PER_SLOT = 14
BYTES_PER_SAMPLE = 4  # int16 I + int16 Q


def load_iq_int16(path):
    """Load a raw vector file as an Nx2 int16 array of (I, Q) rows."""
    samples = np.fromfile(path, dtype="<i2")
    return samples.reshape(-1, 2)


def iq_to_complex(iq_int16):
    """Q15 int16 (I, Q) rows -> complex128 in [-1, 1)."""
    scale = float(2**15)
    return iq_int16[:, 0] / scale + 1j * iq_int16[:, 1] / scale


@dataclass
class FdGrid:
    """A frequency-domain test vector: per-OFDM-symbol RE grid."""

    path: str
    scs_khz: int
    bw_mhz: int
    n_sc: int        # used subcarriers (12 * N_RB)
    sym_len: int     # stored samples per symbol (>= n_sc, zero-padded)
    n_sym: int       # total OFDM symbols in the file
    duration_ms: int  # 10 or 20
    data: np.ndarray  # (n_sym, sym_len, 2) int16

    @property
    def n_slots(self):
        return self.n_sym // SYMBOLS_PER_SLOT

    def symbol_bytes(self, sym_idx):
        """Raw little-endian int16 I/Q bytes of one symbol — DMEM wire format."""
        return self.data[sym_idx].tobytes()

    def symbol_iq(self, sym_idx):
        """One symbol as complex128 Q15-scaled values (sym_len samples)."""
        return iq_to_complex(self.data[sym_idx])

    def slot_bytes(self, slot_idx):
        """Raw bytes of one full slot (14 symbols)."""
        s = slot_idx * SYMBOLS_PER_SLOT
        return self.data[s:s + SYMBOLS_PER_SLOT].tobytes()


def _numerology(scs_khz):
    mu = {15: 0, 30: 1, 60: 2, 120: 3}.get(scs_khz)
    if mu is None:
        raise ValueError(f"unsupported SCS {scs_khz} kHz")
    return mu


def load_fd_grid(path, scs_khz, bw_mhz):
    """Parse an fd vector file into an FdGrid.

    The stored per-symbol length is inferred from the file size: the file
    holds an integer number of radio frames (10 ms each, tried up to 20 ms)
    of ``14 * 10 * 2^mu`` symbols per frame.
    """
    iq = load_iq_int16(path)
    total = len(iq)
    mu = _numerology(scs_khz)
    sym_per_10ms = SYMBOLS_PER_SLOT * 10 * (1 << mu)

    n_rb = NRB_TABLE.get((bw_mhz, scs_khz))
    if n_rb is None:
        raise ValueError(f"no N_RB entry for {bw_mhz} MHz / {scs_khz} kHz")
    n_sc = 12 * n_rb

    # Longest duration first: it yields the smallest per-symbol storage that
    # still holds all used SCs (a 20 ms grid also divides as 10 ms with a
    # doubled, implausible symbol size).
    for duration_ms in (40, 20, 10):
        n_sym = sym_per_10ms * duration_ms // 10
        if total % n_sym == 0 and total // n_sym >= n_sc:
            sym_len = total // n_sym
            break
    else:
        raise ValueError(
            f"{os.path.basename(path)}: {total} samples do not divide into "
            f"10/20/40 ms of {scs_khz} kHz symbols with >= {n_sc} REs/symbol"
        )

    data = iq.reshape(n_sym, sym_len, 2)
    return FdGrid(
        path=path, scs_khz=scs_khz, bw_mhz=bw_mhz, n_sc=n_sc,
        sym_len=sym_len, n_sym=n_sym, duration_ms=duration_ms, data=data,
    )


def load_td_stream(path):
    """Load a time-domain vector as complex128 (Q15-scaled)."""
    return iq_to_complex(load_iq_int16(path))
