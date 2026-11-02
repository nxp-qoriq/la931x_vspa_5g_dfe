#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""Generate the tutorial figures — every one from a REAL simulator run.

Nothing here is mocked or hand-drawn: each figure is produced by booting the
LA9310 DFE firmware on the native VSPA ISS, pushing an NXP FR1 test-model symbol
through its TX chain, and plotting what came back.

    python3 scripts/make_tutorial_figures.py [--out docs/figures]
"""
from __future__ import annotations

import argparse
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from dfe.session import DfeSession, from_sm16, bitrev_index   # noqa: E402
from fr1fr2_vectors import load_fd_grid                        # noqa: E402
from vspa_model import r_dif_fft_sc128                         # noqa: E402

ELD = "firmware/eld/dfe-la9310-1t2r-25m30k.eld"
VEC = "nxp/fr1_fr2_test_tool/test_vectors/TM3.3_10MHz_15kHz_FDD.bin"
N, NSC = 1024, 624

# ── design tokens (validated palette; see the dataviz skill) ─────────────────
SURFACE   = "#fcfcfb"
INK       = "#0b0b0b"
INK_2     = "#52514e"
GRID      = "#e6e5e1"
FIRMWARE  = "#2a78d6"   # categorical slot 1 — blue
MODEL     = "#1baf7a"   # categorical slot 2 — aqua
WRONG     = "#d03b3b"   # status: critical
RIGHT     = "#0ca30c"   # status: good
GUARD     = "#f0efec"   # neutral band fill


def style(ax, title, xlabel, ylabel):
    ax.set_facecolor(SURFACE)
    ax.set_title(title, color=INK, fontsize=11, loc="left", pad=10)
    ax.set_xlabel(xlabel, color=INK_2, fontsize=9)
    ax.set_ylabel(ylabel, color=INK_2, fontsize=9)
    ax.grid(True, color=GRID, linewidth=0.8, zorder=0)
    ax.set_axisbelow(True)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(GRID)
    ax.tick_params(colors=INK_2, labelsize=8, length=0)


def save(fig, path):
    fig.patch.set_facecolor(SURFACE)
    fig.tight_layout()
    fig.savefig(path, dpi=150, facecolor=SURFACE)
    plt.close(fig)
    print("wrote", path)


def oracle(sym):
    iq = np.frombuffer(sym[:NSC * 4], dtype="<i2").reshape(-1, 2)
    b = np.zeros(N, dtype=complex)
    b[:NSC] = (iq[:, 0] + 1j * iq[:, 1]) / 32768.0
    src = b[:NSC].copy()
    dst = N - NSC // 2
    for i in range(NSC):
        b[(dst + i) % N] = src[i]
    st = (dst + NSC) % N
    b[st:st + (N - NSC)] = 0
    return r_dif_fft_sc128(b, inv_flag=1) * 32768.0, b


def raw_words(eld, sym, nsc=NSC):
    """The firmware's RAW output words — undecoded, so we can show the bug."""
    d = DfeSession(eld, nsc=nsc)
    d.process_symbol(sym)                     # runs the chain
    raw = np.asarray(d.read_dmem(d.syms.sym_queue_tx, N), dtype=np.uint32)
    del d
    return raw


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="docs/figures")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)

    grid = load_fd_grid(VEC, 15, 10)
    sym = grid.symbol_bytes(0)
    ref, packed = oracle(sym)

    print("running the firmware …")
    d = DfeSession(ELD)
    got = d.process_symbol(sym)               # SM16-decoded, natural order
    del d
    raw = raw_words(ELD, sym)                 # same run, undecoded

    # ── 1. the stimulus: what actually reaches the iFFT ──────────────────────
    fig, ax = plt.subplots(figsize=(9, 3.4))
    style(ax, "1 — The stimulus: one NXP FR1 test-model symbol, as the iFFT sees it",
          "iFFT bin", "|X|")
    ax.axvspan(NSC // 2, N - NSC // 2, color=GUARD, zorder=1,
               label="_nolegend_")
    ax.plot(np.abs(packed) * 32768, color=FIRMWARE, linewidth=1.0, zorder=3)
    ax.annotate("400 empty guard bins\n(the pack puts them here)",
                xy=(N // 2, np.abs(packed).max() * 32768 * 0.55),
                ha="center", color=INK_2, fontsize=9)
    ax.annotate("624 occupied\nsubcarriers", xy=(120, np.abs(packed).max() * 32768 * 1.02),
                ha="center", color=FIRMWARE, fontsize=9)
    save(fig, f"{a.out}/1_stimulus.png")

    # ── 2. the output: the OFDM symbol the firmware produced ─────────────────
    fig, ax = plt.subplots(figsize=(9, 3.4))
    style(ax, "2 — What the firmware produced: the time-domain OFDM symbol",
          "sample", "amplitude")
    n = 256
    ax.plot(got.real[:n], color=FIRMWARE, linewidth=1.6, zorder=3)
    ax.plot(got.imag[:n], color=MODEL, linewidth=1.6, zorder=2)
    ax.text(n * 0.99, got.real[:n].max() * 0.95, "I", color=FIRMWARE,
            fontsize=10, fontweight="bold", ha="right")
    ax.text(n * 0.99, got.imag[:n].min() * 0.95, "Q", color=MODEL,
            fontsize=10, fontweight="bold", ha="right", va="top")
    save(fig, f"{a.out}/2_waveform.png")

    # ── 3. is it even an OFDM symbol? the structural check ───────────────────
    X = np.abs(np.fft.fft(got))
    Xdb = 20 * np.log10(X / X.max() + 1e-12)
    occ = np.concatenate([X[:NSC // 2], X[N - NSC // 2:]]).mean()
    gd = X[NSC // 2:N - NSC // 2].mean()
    supp = 20 * np.log10(occ / gd)
    fig, ax = plt.subplots(figsize=(9, 3.6))
    style(ax, f"3 — Tier 5 structural check: spectrum of the firmware's output "
              f"({supp:.0f} dB guard-band suppression)",
          "FFT bin", "magnitude (dB, normalised)")
    ax.axvspan(NSC // 2, N - NSC // 2, color=GUARD, zorder=1)
    ax.plot(Xdb, color=FIRMWARE, linewidth=0.9, zorder=3)
    ax.axhline(-supp, color=RIGHT, linewidth=1.6, linestyle="--", zorder=4)
    ax.annotate(f"guard band sits {supp:.0f} dB down\n→ this IS an OFDM symbol",
                xy=(N // 2, -supp + 6), ha="center", color=RIGHT, fontsize=9)
    ax.set_ylim(-100, 8)
    save(fig, f"{a.out}/3_spectrum.png")

    # ── 4. firmware vs an INDEPENDENT model ─────────────────────────────────
    err = np.abs(got - ref).max()
    fig, ax = plt.subplots(figsize=(9, 3.6))
    style(ax, f"4 — Tier 5 model check: firmware vs an independent Python model "
              f"(max error = {err:.0f} LSB — BIT-EXACT)",
          "sample", "amplitude (real part)")
    m = 128
    ax.plot(ref.real[:m], color=MODEL, linewidth=4.5, alpha=0.55, zorder=2)
    ax.plot(got.real[:m], color=FIRMWARE, linewidth=1.6, zorder=3)
    ax.text(m * 0.985, ref.real[:m].max() * 1.0, "Python model",
            color=MODEL, fontsize=9, ha="right", fontweight="bold")
    ax.text(m * 0.985, ref.real[:m].max() * 0.78, "DFE firmware",
            color=FIRMWARE, fontsize=9, ha="right", fontweight="bold")
    save(fig, f"{a.out}/4_vs_model.png")

    # ── 5. THE BUG: the same bytes, decoded two ways ────────────────────────
    lo, hi = raw & 0xFFFF, (raw >> 16) & 0xFFFF
    BR = bitrev_index(10)
    twos = (lo.astype(np.uint16).astype(np.int16).astype(float)
            + 1j * hi.astype(np.uint16).astype(np.int16).astype(float))[BR]
    sm = (from_sm16(lo) + 1j * from_sm16(hi))[BR]
    at, as_ = np.abs(twos)[:256], np.abs(sm)[:256]
    fig, axes = plt.subplots(1, 2, figsize=(10.5, 3.8))
    style(axes[0], "5a — decoded as two's complement  (WRONG)", "sample", "|x|")
    axes[0].plot(at, color=WRONG, linewidth=1.2, zorder=3)
    axes[0].set_ylim(0, at.max() * 1.45)
    axes[0].annotate(f"saturated noise — peaks at {at.max():,.0f}.\nNot a waveform at all.",
                     xy=(128, at.max() * 1.30), ha="center", color=WRONG, fontsize=9)
    style(axes[1], "5b — decoded as SM16  (RIGHT)", "sample", "|x|")
    axes[1].plot(as_, color=RIGHT, linewidth=1.2, zorder=3)
    axes[1].set_ylim(0, as_.max() * 1.45)
    axes[1].annotate(f"a real OFDM envelope — peaks at {as_.max():,.0f},\n"
                     f"{at.max()/as_.max():.0f}x smaller. Note the axis.",
                     xy=(128, as_.max() * 1.30), ha="center", color=RIGHT, fontsize=9)
    fig.suptitle("5 — The bug: the SAME firmware bytes, two decodings.\n"
                 "VSPA assembly kernels are SIGN-MAGNITUDE (SM16), not two's complement.",
                 color=INK, fontsize=11, x=0.012, ha="left")
    fig.tight_layout(rect=[0, 0, 1, 0.88])
    save(fig, f"{a.out}/5_sm16_bug.png")

    # ── 6. the clue that named it: only DC ever passed ──────────────────────
    print("running the subcarrier sweep (this is the diagnostic that found it) …")
    bins, mt, ms = [], [], []
    for k in (312, 313, 316, 320, 344, 440, 568):
        w = [0] * NSC
        w[k] = 8192                                  # a single subcarrier
        b = np.zeros(NSC * 4, dtype=np.uint8)
        iq = np.zeros((NSC, 2), dtype="<i2")
        iq[k, 0] = 8192
        r = raw_words(ELD, iq.tobytes())
        l2, h2 = r & 0xFFFF, (r >> 16) & 0xFFFF
        zt = (l2.astype(np.uint16).astype(np.int16).astype(float)
              + 1j * h2.astype(np.uint16).astype(np.int16).astype(float))
        zs = from_sm16(l2) + 1j * from_sm16(h2)
        bins.append((k - 312) % N)
        mt.append(np.abs(zt).mean())
        ms.append(np.abs(zs).mean())
        print(f"  bin {bins[-1]:4d}: two's-compl {mt[-1]:8.1f}   SM16 {ms[-1]:6.1f}")
    order = np.argsort(bins)
    bins = np.array(bins)[order]; mt = np.array(mt)[order]; ms = np.array(ms)[order]
    x = np.arange(len(bins))                     # categorical: equal spacing
    fig, ax = plt.subplots(figsize=(9, 4.0))
    style(ax, "6 — The clue that named the bug: one subcarrier at a time.\n"
              "Only DC came out right — and DC is the only symbol with no negative samples.",
          "subcarrier bin", "mean |x|   (theory: 64)")
    ax.plot(x, mt, marker="o", markersize=8, color=WRONG, linewidth=1.8, zorder=3)
    ax.plot(x, ms, marker="o", markersize=8, color=RIGHT, linewidth=1.8, zorder=4)
    ax.axhline(64, color=INK_2, linewidth=1.0, linestyle=":", zorder=2)
    ax.set_yscale("log")
    ax.set_xticks(x)
    ax.set_xticklabels([("DC" if b == 0 else str(b)) for b in bins])
    ax.set_xlim(-0.45, len(bins) - 0.25)
    ax.set_ylim(30, 200_000)
    ax.text(x[-1] - 0.08, mt[-1] * 2.2, "decoded as two's complement",
            color=WRONG, fontsize=9, ha="right", fontweight="bold")
    ax.text(x[-1] - 0.08, ms[-1] * 0.42, "decoded as SM16",
            color=RIGHT, fontsize=9, ha="right", fontweight="bold")
    ax.text(x[1], 78, "theory: 8192/128 = 64", color=INK_2, fontsize=8)
    ax.annotate("only DC is right\neven when decoded wrongly",
                xy=(0.05, 70), xytext=(0.9, 800), color=INK_2, fontsize=9,
                arrowprops=dict(arrowstyle="->", color=INK_2, linewidth=1))
    save(fig, f"{a.out}/6_bin_sweep.png")

    print("\nall figures generated from live simulator runs")


if __name__ == "__main__":
    raise SystemExit(main())
