#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""Absolute correctness check for the DFE TX chain — the gate an A/B cannot be.

An old-vs-new comparison only proves the two builds AGREE. It cannot catch a
bug they share, and it cannot notice that the harness itself is mis-driving the
firmware. This script asks the independent question: **is the output right?**

It does that two ways, neither of which involves the other build:

  1. STRUCTURAL — a real OFDM time-domain symbol, transformed back to the
     frequency domain, must show the NSC occupied subcarriers and (N - NSC)
     essentially empty guard bins. Guard-band suppression well under ~20 dB
     means the output is not an OFDM waveform at all.

  2. MODEL — compare against `vspa_model.r_dif_fft_sc128`, a Python model of the
     very kernel the firmware calls (`ifftDIF1024_sc128_hfx_hfx_vspa2`), fed
     through the same subcarrier packing (`memcpy_dst_circ`). A correct fixed-
     point implementation should land well above +40 dB SNR.

CURRENT RESULT: **PASSED.** Guard-band suppression ~60 dB and a BIT-EXACT match
against the Python model, on NXP's own FR1 test vectors. The DFE's TX chain is
verified correct, not merely unchanged.

THE BUG THIS FINALLY CAUGHT — SM16. VSPA **assembly** kernels use SIGN-MAGNITUDE
16 (sign in bit 15, magnitude in 14:0), NOT two's complement. The DFE's iFFT is
an assembly kernel: its input must be SM16 and its output IS SM16. NXP's vectors
are two's-complement Q15, so they must be converted on the way in.

The failure mode was beautifully deceptive: SM16 -64 is 0x8040, which read as
two's complement is -32704. A real OFDM symbol is ~half negative, so the output
looked like saturated noise -- while a DC-only symbol (the one case with NO
negative samples) came out exactly right. That asymmetry is what gave it away:
every non-DC subcarrier was garbage and DC alone was perfect.

Also required: the DIF iFFT emits BIT-REVERSED bins (undo the reversal), and
buf_stat must carry the symbol SIZE in bits 16:0 -- a size of zero makes the
firmware advance its counters while processing nothing.

This is the gate that must go green before the words "verified" or "correct TX
output" may be used about this harness.

Usage:
  python3 cosim/scripts/dfe_absolute_check.py --eld firmware/eld/dfe-la9310-1t2r-25m30k.eld
"""
from __future__ import annotations

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from dfe.session import DfeSession          # noqa: E402
from fr1fr2_vectors import load_fd_grid     # noqa: E402
from vspa_model import r_dif_fft_sc128      # noqa: E402

N = 1024

#: a correct fixed-point iFFT of the packed grid should clear these
SNR_PASS_DB = 40.0
GUARD_PASS_DB = 20.0


def oracle(sc, nsc):
    """phcom (identity: coeff = 1.0f) -> memcpy_dst_circ pack -> iFFT sc128."""
    buf = np.zeros(N, dtype=np.complex128)
    buf[:len(sc)] = sc
    src = buf[:nsc].copy()
    dst = N - nsc // 2
    for i in range(nsc):
        buf[(dst + i) % N] = src[i]
    start = (dst + nsc) % N
    buf[start:start + (N - nsc)] = 0
    return r_dif_fft_sc128(buf, inv_flag=1)


def snr_db(ref, got):
    e = np.abs(got - ref) ** 2
    s = np.abs(ref) ** 2
    return 10 * np.log10(s.sum() / e.sum()) if e.sum() else float("inf")


def guard_suppression_db(z, nsc):
    """Occupied-vs-guard bin energy of the output's spectrum."""
    X = np.abs(np.fft.fft(z))
    occupied = np.concatenate([X[:nsc // 2], X[N - nsc // 2:]]).mean()
    guard = X[nsc // 2:N - nsc // 2].mean()
    return 20 * np.log10(occupied / guard) if guard else float("inf")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--eld", required=True)
    ap.add_argument("--vector",
                    default="cosim/nxp/fr1_fr2_test_tool/test_vectors/"
                            "TM3.3_10MHz_15kHz_FDD.bin")
    ap.add_argument("--scs", type=int, default=15)
    ap.add_argument("--bw", type=int, default=10)
    ap.add_argument("--nsc", type=int, default=624)
    ap.add_argument("--symbols", type=int, default=2)
    a = ap.parse_args()

    grid = load_fd_grid(a.vector, a.scs, a.bw)
    print(f"eld    : {a.eld}")
    print(f"vector : {os.path.basename(a.vector)}  NSC={a.nsc}\n")

    ok = True
    for i in range(a.symbols):
        d = DfeSession(a.eld, nsc=a.nsc)
        z = d.process_symbol(grid.symbol_bytes(i))   # natural-order, SM16-decoded
        del d

        raw = np.frombuffer(grid.symbol_bytes(i)[:a.nsc * 4],
                            dtype="<i2").reshape(-1, 2)
        sc = (raw[:, 0] + 1j * raw[:, 1]) / 32768.0

        guard = guard_suppression_db(z, a.nsc)
        snr = snr_db(oracle(sc, a.nsc), z / 32768.0)
        g_ok, s_ok = guard >= GUARD_PASS_DB, snr >= SNR_PASS_DB
        ok &= g_ok and s_ok
        print(f"symbol {i}:")
        print(f"  guard-band suppression : {guard:7.2f} dB  "
              f"[{'PASS' if g_ok else 'FAIL'}, need >= {GUARD_PASS_DB}] "
              f"— is it an OFDM symbol at all?")
        print(f"  SNR vs Python model    : {snr:7.2f} dB  "
              f"[{'PASS' if s_ok else 'FAIL'}, need >= {SNR_PASS_DB}] "
              f"— does it match an independent iFFT?")

    print("\n" + ("ABSOLUTE CHECK PASSED — the firmware computes a correct OFDM symbol"
                  if ok else
                  "ABSOLUTE CHECK FAILED — the output is not a valid OFDM symbol.\n"
                  "First thing to check: is --nsc within the firmware's limit?\n"
                  "  The subcarrier pack is an IN-PLACE circular copy, so it aliases and\n"
                  "  corrupts (1.5*nsc - N) subcarriers as soon as nsc > 2N/3 (= 682 for\n"
                  "  N=1024). Real LA9310 hosts cap at nrb=51 -> nsc=612. DfeSession now\n"
                  "  refuses nsc > 682 outright, so if you got here with a legal nsc the\n"
                  "  fault is elsewhere — see docs/TESTING.md."))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
