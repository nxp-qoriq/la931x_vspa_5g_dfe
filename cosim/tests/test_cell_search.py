# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""test_cell_search.py — the gap-11 / gap-12 regression.

Drives three kernel ELFs on the in-process ISS and checks each against a Python
model of the same operation:

  compute_ssb_chest   gap 12 — chEst -> rcpv -> vmult -> chEst -> sss_msmts
  compute_sss_det     gap 11 — sssdet_targeted_hpgemv_rc_v1, swept over all 336
                               N_id_1, plus sssConvert
  compute_pss_search  gap 11 — N_id_2 search built on the real chEst kernel

Every routine named above is the REAL assembly from firmware/lib/lib_DFE_Ref_vspa2.elb.
The one thing with no kernel behind it is PSS *detection* under CFO/timing offset —
see the note at the bottom of compute_pss_search.c and section 4 here.

THE TRAP THIS FILE IS BUILT TO CATCH
------------------------------------
The kernel's buffers are .bss globals, and crt0 zeroes .bss *after* the image is
loaded. An input written before the breakpoint on main() is silently wiped, and
the kernel then computes on zeros and returns a clean, plausible, WRONG answer.
So a passing number is not enough: every check below is paired with a control
that PROVES the output tracks the input. Section 5 is nothing but such controls.

Run:
  LD_LIBRARY_PATH=$PWD/environment/ccssim2/bin python3 cosim/tests/test_cell_search.py
"""
import os
import sys

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "cosim"))

from dfe.kernel_runner import NativeKernelRunner, Port      # noqa: E402
from vspa_model.nr5g import (                                # noqa: E402
    nr_pss_sequence, nr_sss_sequence, pack_sss_d0, pack_sss_d1,
    nr_ssb_pilot_cfloat16, nr_ssb_sss_ref_cfloat16,
)

BUILD = os.path.join(ROOT, "cosim/kernels/build")
N     = 256          # NR_SSB_NFFT
OFS   = 64           # SLOTTED_PSS_SSS_OFST
NRE   = 127          # PSS / SSS resource elements
Q15   = 32768.0

# ATAN2_COEFF_BUFF, ATAN2_NUM_COEFF=4, each coeff duplicated for S2mode=S2i1r1i1r1.
# Straight out of vspa-lib/atan/src/atan_coeff.txt — the app DMAs this in.
ATAN2_COEFF = np.array([0xBC54B5E1, 0xBC54B5E1, 0x3D422AF3, 0x3D422AF3,
                        0xBDD21235, 0xBDD21235, 0x3EA2DCD7, 0x3EA2DCD7],
                       dtype=np.uint32)

_fails = []


def check(name, ok, detail=""):
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}" + (f"  {detail}" if detail else ""))
    if not ok:
        _fails.append(name)


# ---------------------------------------------------------------------------
# Python models of the three library kernels
# ---------------------------------------------------------------------------
def fp16(x):
    """Quantise to IEEE half — what a cfloat16_t buffer can actually hold."""
    x = np.asarray(x, dtype=np.complex128)
    return (np.float16(x.real).astype(np.float64)
            + 1j * np.float16(x.imag).astype(np.float64))


def cfix(x):
    """Quantise a complex value into cfixed16's integer grid (toward zero)."""
    x = np.asarray(x, dtype=np.complex128)
    return np.trunc(x.real) + 1j * np.trunc(x.imag)


def _rz16(v):
    """Narrow float64 -> fp16 ROUNDING TOWARD ZERO.

    Not a detail, and not a guess. The AU truncates toward zero everywhere: the
    cfixed16 path does it (chEst: 889.5 -> 889, -641.5 -> -641) and so does the
    fp16 narrowing here. Modelling the fp16 output with numpy's default
    round-to-nearest disagrees with the kernel on ~30% of samples by 1 ULP
    (2^-11); rounding toward zero is bit-exact on all 256. Same rule, both
    formats — which is what you would expect of one piece of hardware.
    """
    v = np.asarray(v, dtype=np.float64)
    f = np.float16(v)
    over = np.abs(f.astype(np.float64)) > np.abs(v)          # rounded away from 0
    stepped = np.nextafter(f, np.float16(0.0)).astype(np.float64)
    return np.where(over, stepped, f.astype(np.float64))


def fp16_rz(x):
    """Complex fp16 with the AU's toward-zero narrowing."""
    x = np.asarray(x, dtype=np.complex128)
    return _rz16(x.real) + 1j * _rz16(x.imag)


def m_chest(y_int, pilot):
    """chEst: h = y * conj(pilot), elementwise.

    y is cfixed16 (sign-magnitude half-fixed, held here as its integer value);
    pilot is cfloat16; h is cfixed16 again. The output truncates the MAGNITUDE
    toward zero — which is the natural rounding for a sign-magnitude format, and
    is what the kernel measurably does (889.5 -> 889, -641.5 -> -641).
    """
    p = fp16(pilot)
    prod = np.asarray(y_int, dtype=np.complex128) * np.conj(p)
    re = np.clip(np.trunc(prod.real), -32767, 32767)
    im = np.clip(np.trunc(prod.imag), -32767, 32767)
    return re + 1j * im


def m_rcpv(h_int):
    """rcpv: equaliser weights = 1/H, where H is read as a Q15 fraction.

    The cfixed16 integer h is the Q15 value h/32768, so the reciprocal is
    32768/h, returned in cfloat32. (The AU uses a reciprocal approximation, so
    this is accurate to ~3e-6 relative, not bit-exact.)
    """
    h = np.asarray(h_int, dtype=np.complex128)
    return Q15 / h


def m_vmult(x_int, w):
    """vmult: y = (x/32768) * w — cfixed16 times cfloat32, result cfloat16.

    The narrowing to fp16 rounds TOWARD ZERO (see _rz16). With that, the model is
    bit-exact against the kernel; with round-to-nearest it is off by 1 ULP on a
    third of the samples.
    """
    x = np.asarray(x_int, dtype=np.complex128) / Q15
    return fp16_rz(x * np.asarray(w, dtype=np.complex128))


# --- host-side packing helpers ---------------------------------------------
def pack_u16(a):
    """uint16[] -> uint32[] words, LOW halfword first (the DMEM convention:
    read_dram_words assembles a word as hw[0] | hw[1]<<16)."""
    a = np.asarray(a, dtype=np.uint16)
    return a[0::2].astype(np.uint32) | (a[1::2].astype(np.uint32) << 16)


def f2b(x):
    return int(np.float32(x).view(np.uint32))


def b2f(w):
    return np.asarray(w, dtype=np.uint32).view(np.float32)


def unpack_seq(words):
    """pSeq: 128 sign-magnitude fixed16 packed 2-per-word -> signed values."""
    w = np.asarray(words, dtype=np.uint32)
    hw = np.empty(2 * len(w), dtype=np.uint32)
    hw[0::2] = w & 0xFFFF
    hw[1::2] = (w >> 16) & 0xFFFF
    mag = (hw & 0x7FFF).astype(np.float64)
    return np.where(hw & 0x8000, -mag, mag)


# ---------------------------------------------------------------------------
# Runners
# ---------------------------------------------------------------------------
def chest_runner():
    return NativeKernelRunner(
        os.path.join(BUILD, "compute_ssb_chest_vspa2_16au.eld"),
        inputs=[Port("pss_in",      "pss_in",      N, "sm16_complex"),
                Port("pss_ref",     "pss_ref",     N, "chf_complex"),
                Port("sss_in",      "sss_in",      N, "sm16_complex"),
                Port("sss_ref",     "sss_ref",     N, "chf_complex"),
                Port("atan2_coeff", "atan2_coeff", 8, "u32")],
        outputs=[Port("pss_csi", "pss_csi", N, "sm16_complex"),
                 Port("pss_eqw", "pss_eqw", N, "csp_complex"),
                 Port("sss_eq",  "sss_eq",  N, "chf_complex"),
                 Port("sss_csi", "sss_csi", N, "sm16_complex"),
                 Port("msmt",    "msmt",    4, "u32"),
                 Port("est",     "est",     4, "u32")])


def sssdet_runner():
    return NativeKernelRunner(
        os.path.join(BUILD, "compute_sss_det_vspa2_16au.eld"),
        inputs=[Port("sss_eq", "sss_eq", N,  "chf_complex"),
                Port("d0_tab", "d0_tab", 36, "u32"),
                Port("d1_tab", "d1_tab", 9,  "u32"),
                Port("param",  "param",  2,  "u32"),
                Port("ctrl",   "ctrl",   4,  "u32")],
        outputs=[Port("det_flag",   "det_flag",     336, "u32"),
                 Port("score",      "score",        336, "u32"),
                 Port("seq_sign32", "seq_sign32",   336, "u32"),
                 Port("seq_out",    "seq_out",       64, "u32"),
                 Port("sss_conv",   "sss_conv_out",  N,  "chf_complex")])


def pss_runner():
    return NativeKernelRunner(
        os.path.join(BUILD, "compute_pss_search_vspa2_16au.eld"),
        inputs=[Port("pss_in",  "pss_in",  N,     "sm16_complex"),
                Port("pss_ref", "pss_ref", 3 * N, "chf_complex")],
        outputs=[Port("h_out", "h_out", 3 * N, "sm16_complex"),
                 Port("score", "score", 3,     "u32"),
                 Port("best",  "best",  4,     "u32")])


def sm_lsb(a, b):
    """Max |diff| between two cfixed16 arrays, in LSB (1 LSB = 1 integer step)."""
    return float(np.abs(np.asarray(a) - np.asarray(b)).max())


# ===========================================================================
def main():
    rng = np.random.default_rng(20260714)

    # A concrete cell, and a channel that is not flat, not symmetric, and not
    # unity — so nothing can pass by accident.
    ID1, ID2 = 200, 1
    pss = nr_pss_sequence(ID2)
    sss = nr_sss_sequence(ID1, ID2)

    k = np.arange(N)
    H = (0.8 * np.exp(-2j * np.pi * 3.0 * k / N)
         + 0.35 * np.exp(-2j * np.pi * 11.0 * k / N + 0.7j))

    pss_ref = nr_ssb_pilot_cfloat16(ID2)          # +/-1 on 64..190, 1+0j elsewhere
    sss_ref = nr_ssb_sss_ref_cfloat16(ID1, ID2)

    # RX symbols: the reference through the channel, scaled into cfixed16 range.
    AMP = 6000.0
    pss_rx = cfix(AMP * H * pss_ref)
    sss_rx = cfix(AMP * H * sss_ref)

    # ---------------------------------------------------------------- 1
    print("\n1. chEst / rcpv / vmult  — the gap-12 chain (compute_ssb_chest)")
    r = chest_runner()
    out = r.compute({"pss_in": pss_rx, "pss_ref": pss_ref,
                     "sss_in": sss_rx, "sss_ref": sss_ref,
                     "atan2_coeff": ATAN2_COEFF})

    want_pss_csi = m_chest(pss_rx, pss_ref)
    d = sm_lsb(out["pss_csi"], want_pss_csi)
    check("chEst(PSS)  h = y*conj(ref)", d == 0, f"max|diff| = {d:.0f} LSB")

    want_sss_csi = m_chest(sss_rx, sss_ref)
    d = sm_lsb(out["sss_csi"], want_sss_csi)
    check("chEst(SSS)  h = y*conj(ref)", d == 0, f"max|diff| = {d:.0f} LSB")

    want_eqw = m_rcpv(want_pss_csi)
    rel = float((np.abs(out["pss_eqw"] - want_eqw) / np.abs(want_eqw)).max())
    check("rcpv        w = 32768/H", rel < 1e-4, f"max rel err = {rel:.2e}")

    want_eq = m_vmult(sss_rx, out["pss_eqw"])
    d = float(np.abs(out["sss_eq"] - want_eq).max())
    check("vmult       eq = (x/32768)*w", d == 0.0, f"max|diff| = {d:g} (fp16 exact)")

    # The point of the whole chain: the equalised SSS should be the SSS again.
    eq = out["sss_eq"][OFS:OFS + NRE]
    evm = float(np.abs(eq - sss).max())
    check("ZF equalisation recovers SSS", evm < 0.05,
          f"max|eq - sss| = {evm:.4f}  (mean |eq| = {np.abs(eq).mean():.4f})")

    msmt = np.asarray(out["msmt"], dtype=np.uint32).astype(np.int32) / 65536.0
    print(f"       sss_msmts: rssi={msmt[0]:8.3f} dB  rsrp={msmt[1]:8.3f} dB  "
          f"rsrq={msmt[2]:8.3f} dB  sinr={msmt[3]:8.3f} dB")
    check("sss_msmts returns finite dB", bool(np.all(np.abs(msmt) < 1e4)))
    est = np.asarray(out["est"], dtype=np.uint32).astype(np.int32)
    print(f"       tim_off_est = {est[0]}   fineCFO_rc = {est[1]}")
    print(f"       phases = {r.phases}")

    # ---------------------------------------------------------------- 1b
    # fineCFO_rc returns 0 above, and 0 is the RIGHT answer there (PSS and SSS
    # see the same channel, so there is no CFO between them). But "returns 0" is
    # precisely what a kernel computing on a zeroed .bss would also do. So sweep
    # the input and prove the output is a COMPUTED zero, not a default one.
    print("\n1b. fineCFO_rc / tim_off_est_sp_asm_rc — linearity, not just a value")

    def est_for(H_pss, H_sss):
        o = r.compute({"pss_in": cfix(AMP * H_pss * pss_ref), "pss_ref": pss_ref,
                       "sss_in": cfix(AMP * H_sss * sss_ref), "sss_ref": sss_ref,
                       "atan2_coeff": ATAN2_COEFF})
        e = np.asarray(o["est"], dtype=np.uint32).astype(np.int32)
        return int(e[0]), int(e[1])

    H0 = 0.9 * np.ones(N, dtype=np.complex128)

    # A common phase rotation between the two symbols IS a CFO.
    phis = np.deg2rad([10.0, 30.0, 60.0, -45.0])
    cfos = [est_for(H0, H0 * np.exp(1j * p))[1] for p in phis]
    slopes = np.array(cfos) / phis
    lin = float(slopes.std() / abs(slopes.mean()))
    print("       phase(SSS)-phase(PSS): "
          + ", ".join(f"{np.rad2deg(p):+.0f}deg -> {c:+d}" for p, c in zip(phis, cfos)))
    check("fineCFO_rc is linear in the PSS->SSS phase", lin < 1e-3,
          f"slope = {slopes.mean():.4g} per rad, spread = {lin:.1e} (zero CFO -> 0)")

    # A linear phase ramp across subcarriers IS a timing offset.
    tois = [1, 2, -3]
    tos = [est_for(H0, H0 * np.exp(-2j * np.pi * t * k / N))[0] for t in tois]
    exact = all(t_o == -256 * t for t_o, t in zip(tos, tois))
    print("       timing offset: "
          + ", ".join(f"{t:+d} smp -> {o:+d}" for t, o in zip(tois, tos)))
    check("tim_off_est is exactly -256 * (timing offset)", exact,
          "exact integer scale; 0 offset -> 0")

    # ---------------------------------------------------------------- 2
    print("\n2. SSS blind N_id_1 search over all 336 IDs (compute_sss_det)")
    rs = sssdet_runner()
    grid = np.ones(N, dtype=np.complex128)
    grid[OFS:OFS + NRE] = sss                    # perfectly equalised SSS symbol
    d0, d1 = pack_u16(pack_sss_d0()), pack_u16(pack_sss_d1())
    param = np.array([f2b(0.05), f2b(0.0310058593750)], dtype=np.uint32)

    o = rs.compute({"sss_eq": grid, "d0_tab": d0, "d1_tab": d1, "param": param,
                    "ctrl": np.array([ID2, ID1, 20, 0], dtype=np.uint32)})

    det = np.asarray(o["det_flag"], dtype=np.uint32)
    hits = np.flatnonzero(det)
    check("detects the true N_id_1, and only it",
          list(hits) == [ID1], f"accepted id1 = {list(hits)} (true = {ID1})")

    sc = b2f(o["score"])
    order = np.argsort(sc)[::-1]
    margin = sc[order[0]] / max(sc[order[1]], 1e-12)
    check("argmax of the recovered metric = true N_id_1", int(order[0]) == ID1,
          f"peak={sc[order[0]]:.4f} @id1={order[0]}, "
          f"runner-up={sc[order[1]]:.4f} @id1={order[1]}, margin={margin:.1f}x")

    # The sharpest check available: the detector EMITS the SSS sequence for every
    # id1 it is asked about. All 336 must match 3GPP, or the D0/D1 bit tables,
    # the ld.qam decode, or the kernel are wrong.
    sign32 = np.asarray(o["seq_sign32"], dtype=np.uint32)
    bad = []
    for i in range(336):
        want = 0
        s = nr_sss_sequence(i, ID2)
        for n in range(32):
            if s[n] < 0:
                want |= (1 << n)
        if int(sign32[i]) != want:
            bad.append(i)
    check("all 336 emitted SSS sequences match 3GPP 38.211 §7.4.2.3",
          not bad, f"{336 - len(bad)}/336 exact")

    seq = unpack_seq(o["seq_out"])
    mags = np.unique(np.abs(seq[:NRE]))
    signs_ok = np.array_equal(np.sign(seq[:NRE]), sss)
    check("pSeq @id1=200: 127 chips exact, +1 zero pad",
          signs_ok and len(mags) == 1 and seq[NRE] == 0,
          f"|chip| = {mags[0]:.0f} (= Q15 {mags[0]/Q15:g} = 1/64), pad = {seq[NRE]:.0f}")

    conv = o["sss_conv"]
    want_conv = nr_ssb_sss_ref_cfloat16(ID1, ID2)
    d = float(np.abs(conv - want_conv).max())
    check("sssConvert = +/-1 on REs 64..190, 1+0j padding", d == 0.0,
          f"max|diff| = {d:g}  (64 * 1/64 = 1.0 exactly)")
    print(f"       phases = {rs.phases}")

    # ---------------------------------------------------------------- 3
    print("\n3. PSS N_id_2 search on the real chEst kernel (compute_pss_search)")
    rp = pss_runner()
    refs = np.concatenate([nr_ssb_pilot_cfloat16(i) for i in range(3)])
    for true_id2 in (0, 1, 2):
        p = nr_pss_sequence(true_id2)
        ref_t = nr_ssb_pilot_cfloat16(true_id2)
        rx = cfix(AMP * H * ref_t)
        # Equalise with the true channel so the coherent sum is meaningful (this
        # is the "clean input" case; see section 4 for the real-vector case).
        rx_eq = cfix(AMP * ref_t)

        op = rp.compute({"pss_in": rx_eq, "pss_ref": refs})
        s = b2f(op["score"])
        best = int(np.asarray(op["best"], dtype=np.uint32)[0])
        marg = s[best] / max(np.sort(s)[-2], 1e-9)
        check(f"N_id_2={true_id2}: argmax correct", best == true_id2,
              f"scores = {['%.3e' % v for v in s]}, margin = {marg:.1f}x")

        # chEst inside the PSS harness must still be bit-exact.
        h = np.asarray(op["h_out"]).reshape(3, N)
        d = sm_lsb(h[true_id2], m_chest(rx_eq, ref_t))
        check(f"N_id_2={true_id2}: chEst bit-exact", d == 0, f"max|diff| = {d:.0f} LSB")
    print(f"       phases = {rp.phases}")

    # ---------------------------------------------------------------- 4
    print("\n4. Where the coherent metric stops working (the honest limit)")
    # The NXP FR1 vectors inject CFO and timing offset. A timing offset is a
    # linear phase ramp across subcarriers, and it destroys a coherent sum.
    ref_t = nr_ssb_pilot_cfloat16(1)
    ramp = np.exp(-2j * np.pi * 40.0 * np.arange(N) / N)   # toi ~ 40 samples
    rx_toi = cfix(AMP * ramp * ref_t)
    op = rp.compute({"pss_in": rx_toi, "pss_ref": refs})
    s = b2f(op["score"])
    best = int(np.asarray(op["best"], dtype=np.uint32)[0])
    print(f"       with a 40-sample timing offset: scores = "
          f"{['%.3e' % v for v in s]} -> argmax = {best} (true = 1)")
    print("       -> coherent correlation is NOT usable on the raw FR1 vectors;")
    print("          a differential metric is required, and it has NO library")
    print("          kernel. That step is a documented Python model.")

    # ---------------------------------------------------------------- 5
    print("\n5. Controls — the output must TRACK the input (crt0 .bss trap)")
    # If inputs were being zeroed by crt0, every one of these would return the
    # same thing, and every check above could still 'pass' by looking plausible.
    o2 = rs.compute({"sss_eq": grid, "d0_tab": d0, "d1_tab": d1, "param": param,
                     "ctrl": np.array([ID2, 7, 0, 0], dtype=np.uint32)})
    seq7 = unpack_seq(o2["seq_out"])
    check("different id1 -> different SSS sequence",
          not np.array_equal(np.sign(seq7[:NRE]), sss)
          and np.array_equal(np.sign(seq7[:NRE]), nr_sss_sequence(7, ID2)),
          "id1=7 emits its own sequence, not id1=200's")

    # A different cell's SSS on the wire must move the detection.
    ID1B = 42
    gridB = np.ones(N, dtype=np.complex128)
    gridB[OFS:OFS + NRE] = nr_sss_sequence(ID1B, ID2)
    o3 = rs.compute({"sss_eq": gridB, "d0_tab": d0, "d1_tab": d1, "param": param,
                     "ctrl": np.array([ID2, ID1B, 0, 0], dtype=np.uint32)})
    hitsB = list(np.flatnonzero(np.asarray(o3["det_flag"], dtype=np.uint32)))
    check("different SSS on the wire -> detection moves", hitsB == [ID1B],
          f"accepted id1 = {hitsB} (true = {ID1B})")

    # Zeroed D0/D1 must make the detector fail — this is exactly the silent
    # failure mode the .bss trap produces, so we assert it is DETECTABLE.
    o4 = rs.compute({"sss_eq": grid,
                     "d0_tab": np.zeros(36, dtype=np.uint32),
                     "d1_tab": np.zeros(9, dtype=np.uint32),
                     "param": param,
                     "ctrl": np.array([ID2, ID1, 0, 0], dtype=np.uint32)})
    hits0 = list(np.flatnonzero(np.asarray(o4["det_flag"], dtype=np.uint32)))
    check("zeroed D0/D1 -> no detection (the trap, made visible)",
          hits0 == [], f"accepted id1 = {hits0} (a zeroed table detects nothing)")

    # And the chEst chain must move when the channel moves.
    H2 = 0.5 * np.exp(-2j * np.pi * 9.0 * k / N)
    pss_rx2 = cfix(AMP * H2 * pss_ref)
    out2 = r.compute({"pss_in": pss_rx2, "pss_ref": pss_ref,
                      "sss_in": sss_rx, "sss_ref": sss_ref,
                      "atan2_coeff": ATAN2_COEFF})
    moved = float(np.abs(out2["pss_csi"] - out["pss_csi"]).max())
    check("different channel -> different chEst output", moved > 100,
          f"max|H1 - H2| = {moved:.0f} LSB")

    # ------------------------------------------------------------------
    print("\n" + "=" * 66)
    if _fails:
        print(f"FAIL — {len(_fails)} check(s): {_fails}")
        return 1
    print("PASS — all checks green")
    return 0


if __name__ == "__main__":
    sys.exit(main())
