# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 NXP
"""3GPP TS 38.211 NR reference sequences for 5G SSB (PSS / SSS).

Provides:
  nr_x0_sequence()          — 127-bit SSS x0 m-sequence (int8, 0/1)
  nr_x1_sequence()          — 127-bit SSS x1 m-sequence (int8, 0/1)
  nr_pss_sequence(nid2)     — 127-element ±1 PSS BPSK symbols
  nr_sss_sequence(nid1,nid2)— 127-element ±1 SSS Gold code
  nr_ssb_pilot_cfloat16()   — 256-element PSS pilot on fp16 grid
  pack_sss_d0()             — 72 uint16 D0 reference matrix for sss_det
  pack_sss_d1()             — 18 uint16 D1 reference matrix for sss_det
"""
import numpy as np

NR_SSB_NFFT = 256
NR_PSS_NRE  = 127
NR_SSS_NRE  = 127
NR_SSB_KRE  = 8     # (256-240)/2: SSB RE 0 sits 8 bins into the 256-pt window
NR_PSS_KRE  = 56    # PSS/SSS offset FROM SSB RE 0 — not from the window start.
                    # The index inside the window is NR_SSB_KRE + NR_PSS_KRE = 64
                    # (= SLOTTED_PSS_SSS_OFST, defined below).


# ---------------------------------------------------------------------------
# Internal m-sequence generators
# ---------------------------------------------------------------------------

def _gen_x0() -> np.ndarray:
    # x0(i+7) = (x0(i+4) + x0(i)) mod 2,  init = [1,0,0,0,0,0,0]
    state = [1, 0, 0, 0, 0, 0, 0]
    x = np.empty(127, dtype=np.int8)
    for i in range(127):
        x[i] = state[0]
        state = state[1:] + [(state[4] ^ state[0]) & 1]
    return x


def _gen_x1() -> np.ndarray:
    # x1(i+7) = (x1(i+1) + x1(i)) mod 2,  init = [1,0,0,0,0,0,0]
    state = [1, 0, 0, 0, 0, 0, 0]
    x = np.empty(127, dtype=np.int8)
    for i in range(127):
        x[i] = state[0]
        state = state[1:] + [(state[1] ^ state[0]) & 1]
    return x


def _gen_pss_base() -> np.ndarray:
    # Same poly as x0, init = [0,1,1,0,1,1,1]  (3GPP 38.211 §7.4.2.2)
    state = [0, 1, 1, 0, 1, 1, 1]
    x = np.empty(127, dtype=np.int8)
    for i in range(127):
        x[i] = state[0]
        state = state[1:] + [(state[4] ^ state[0]) & 1]
    return x


# Module-level constants (computed once)
_X0_SEQ   = _gen_x0()
_X1_SEQ   = _gen_x1()
_PSS_BASE = _gen_pss_base()


# ---------------------------------------------------------------------------
# Public sequence generators
# ---------------------------------------------------------------------------

def nr_x0_sequence() -> np.ndarray:
    """127-element SSS x0 m-sequence (int8, values 0 or 1)."""
    return _X0_SEQ.copy()


def nr_x1_sequence() -> np.ndarray:
    """127-element SSS x1 m-sequence (int8, values 0 or 1)."""
    return _X1_SEQ.copy()


def nr_pss_sequence(nid2: int) -> np.ndarray:
    """127 real ±1 PSS BPSK symbols for cell ID2 ∈ {0,1,2} (3GPP 38.211 §7.4.2.2)."""
    x = _PSS_BASE
    return np.array([1 - 2 * int(x[(n + 43 * nid2) % 127]) for n in range(127)],
                    dtype=np.float64)


def nr_sss_sequence(nid1: int, nid2: int) -> np.ndarray:
    """127 real ±1 SSS Gold code (3GPP 38.211 §7.4.2.3).

    m0 = 15*floor(nid1/112) + 5*nid2
    m1 = nid1 % 112
    d_sss(n) = (1-2*x0((n+m0)%127)) * (1-2*x1((n+m1)%127))
    """
    x0, x1 = _X0_SEQ, _X1_SEQ
    m0 = 15 * (nid1 // 112) + 5 * nid2
    m1 = nid1 % 112
    return np.array(
        [(1 - 2*int(x0[(n+m0) % 127])) * (1 - 2*int(x1[(n+m1) % 127]))
         for n in range(127)],
        dtype=np.float64)


SLOTTED_PSS_SSS_OFST = 64   # NR_SSB_KRE(8) + NR_PSS_KRE(56) — see below


def nr_ssb_pilot_cfloat16(nid2: int, n_fft: int = NR_SSB_NFFT) -> np.ndarray:
    """n_fft-element PSS reference on the fp16 grid — the pPilot input to chEst().

    This is the host-side stand-in for celltrack_extbuf->pssRef256[nid2], which
    the firmware DMAs in and never generates on-chip (there is no PSS generator
    in the library).

    LAYOUT — and this is not a guess, it is what the hardware says.  The SSS half
    of the same chain builds its reference with the real sssConvert() kernel, and
    reading that kernel's actual output back out of DMEM gives:

        out[0:63]    = 1+0j        (64 of them)
        out[64:190]  = +/-1.0      (the 127 SSS chips)
        out[191:255] = 1+0j        (65 of them)

    So the reference is +/-1 on the sequence REs and 1+0j — NOT zero — everywhere
    else, and the PSS reference must match that convention or the two channel
    estimates are not comparable (sss_msmts and fineCFO_rc compare them directly).

    Two things follow, both of which were wrong in the first version of this
    function:

    1. OFFSET IS 64, NOT 56.  NR_PSS_KRE=56 is measured from SSB RE 0, but chEst's
       256-point window starts NR_SSB_KRE=8 REs *before* SSB RE 0.  8+56 = 64 =
       SLOTTED_PSS_SSS_OFST, which is exactly the offset the firmware itself adds
       when it hands the SSS detector its candidate REs.

    2. PADDING IS 1+0j, NOT 0.  The very next kernel in the chain is rcpv, which
       computes 1/H.  A zero pilot gives H=0 on 129 of the 256 bins and the
       equaliser weights blow up to inf/NaN.  The 1+0j padding is what keeps the
       reciprocal finite.
    """
    pss = nr_pss_sequence(nid2)
    pilot = np.ones(n_fft, dtype=np.complex128)
    k0 = SLOTTED_PSS_SSS_OFST
    pilot[k0 : k0 + NR_PSS_NRE] = pss
    return (np.float16(pilot.real).astype(np.float64)
            + 1j * np.float16(pilot.imag).astype(np.float64))


def nr_ssb_sss_ref_cfloat16(nid1: int, nid2: int,
                            n_fft: int = NR_SSB_NFFT) -> np.ndarray:
    """The SSS reference, in the same layout — a model of sssConvert()'s output.

    Verified bit-exact against the real sssConvert() kernel (see
    cosim/tests/test_cell_search.py).  Use the kernel where you can; this exists
    so the PSS and SSS references can be built the same way in host-side models.
    """
    sss = nr_sss_sequence(nid1, nid2)
    ref = np.ones(n_fft, dtype=np.complex128)
    k0 = SLOTTED_PSS_SSS_OFST
    ref[k0 : k0 + NR_SSS_NRE] = sss
    return (np.float16(ref.real).astype(np.float64)
            + 1j * np.float16(ref.imag).astype(np.float64))


# ---------------------------------------------------------------------------
# PBCH DMRS (3GPP TS 38.211 §7.4.1.4) — the pilot the SSB co-simulation
# estimates the channel from.  Unlike the PSS/SSS, this one is QPSK, so it is
# the sequence that tells chEst's conj() apart: h = y*conj(d), not conj(y)*d.
# ---------------------------------------------------------------------------

NR_SSB_NSYM      = 4     # PSS, PBCH, SSS+PBCH, PBCH
NR_SSB_NRE       = 240   # 20 RB
NR_PBCH_DMRS_NRE = 144   # 60 + 24 + 60 over SSB symbols 1, 2, 3
NR_PBCH_DATA_NRE = 432   # 180 + 72 + 180  -> 864 QPSK bits


def _gold_sequence(length: int, c_init: int) -> np.ndarray:
    """Length-31 Gold sequence c(n) of TS 38.211 §5.2.1 (Nc = 1600)."""
    nc = 1600
    n = length + nc
    x1 = np.zeros(n + 31, dtype=np.int8)
    x2 = np.zeros(n + 31, dtype=np.int8)
    x1[0] = 1
    for i in range(31):
        x2[i] = (c_init >> i) & 1
    for i in range(n):
        x1[i + 31] = (x1[i + 3] ^ x1[i]) & 1
        x2[i + 31] = (x2[i + 3] ^ x2[i + 2] ^ x2[i + 1] ^ x2[i]) & 1
    return (x1[nc:nc + length] ^ x2[nc:nc + length]).astype(np.int8)


def nr_pbch_dmrs(cell_id: int, i_ssb: int = 0) -> np.ndarray:
    """144 unit-power QPSK PBCH DMRS symbols (TS 38.211 §7.4.1.4.1).

    c_init = 2^11 (i_ssb+1) (floor(N_ID/4)+1) + 2^6 (i_ssb+1) + (N_ID mod 4)
    r(m)   = [(1-2c(2m)) + j(1-2c(2m+1))] / sqrt(2)

    The sequence runs over the DMRS REs in (symbol, subcarrier) order — SSB
    symbol 1, then 2, then 3, each in increasing k.  See nr_ssb_re_map().
    """
    c_init = ((1 << 11) * (i_ssb + 1) * ((cell_id >> 2) + 1)
              + (1 << 6) * (i_ssb + 1)
              + (cell_id & 3))
    c = _gold_sequence(2 * NR_PBCH_DMRS_NRE, c_init)
    m = np.arange(NR_PBCH_DMRS_NRE)
    return ((1 - 2 * c[2 * m]) + 1j * (1 - 2 * c[2 * m + 1])) / np.sqrt(2)


# RE-type codes used by nr_ssb_re_map()
RE_EMPTY, RE_PSS, RE_SSS, RE_PBCH, RE_DMRS = 0, 1, 2, 3, 4


def nr_ssb_re_map(cell_id: int) -> np.ndarray:
    """(4, 240) int8 map of what each SSB resource element carries.

    TS 38.211 Table 7.4.3.1-1, with SSB symbol l and subcarrier k relative to
    SSB RE 0:

      l=0  PSS  on k = 56..182;  k = 0..55, 183..239 are set to zero
      l=1  PBCH on k = 0..239
      l=2  SSS  on k = 56..182;  PBCH on k = 0..47 and 192..239;
                                 k = 48..55, 183..191 are set to zero
      l=3  PBCH on k = 0..239

    DMRS sits on every 4th PBCH RE, at k mod 4 == v with v = N_ID_cell mod 4.
    """
    m = np.full((NR_SSB_NSYM, NR_SSB_NRE), RE_EMPTY, dtype=np.int8)
    v = cell_id & 3

    m[0, 56:183] = RE_PSS
    m[2, 56:183] = RE_SSS

    pbch_k = {
        1: np.arange(240),
        2: np.concatenate([np.arange(0, 48), np.arange(192, 240)]),
        3: np.arange(240),
    }
    for l, ks in pbch_k.items():
        m[l, ks] = RE_PBCH
        m[l, ks[ks % 4 == v]] = RE_DMRS
    return m


# ---------------------------------------------------------------------------
# D0 / D1 packing for sss_det
# ---------------------------------------------------------------------------

def _pack_bits_lsb(bits: np.ndarray, n_words: int) -> np.ndarray:
    """Pack a bit array (values 0/1) LSB-first into n_words of uint16."""
    out = np.zeros(n_words, dtype=np.uint16)
    for i, b in enumerate(bits[:n_words * 16]):
        if b:
            out[i // 16] |= np.uint16(1 << (i % 16))
    return out


def pack_sss_d0() -> np.ndarray:
    """SSS D0 reference table: 72 uint16 (9 blocks × 8 uint16).

    Layout: block(id2, id1_grp) at uint16 offset (id2*3 + id1_grp)*8,
    where id1_grp = floor(id1/112) ∈ {0,1,2} and id2 ∈ {0,1,2}.

    Each block holds 128 bits of x0 cyclically shifted by m0=15*id1_grp+5*id2,
    packed LSB-first.  The 4QAM ld.qam instruction decodes bit-pairs (2n, 2n+1)
    as complex ±0.125 values: bit 0 → −0.125, bit 1 → +0.125.
    """
    x0 = _X0_SEQ
    d0 = np.zeros(72, dtype=np.uint16)
    for id2 in range(3):
        for id1_grp in range(3):
            m0 = 15 * id1_grp + 5 * id2
            shifted = np.zeros(128, dtype=np.uint8)
            for n in range(127):
                shifted[n] = x0[(n + m0) % 127]
            # shifted[127] = 0 (padding)
            block_off = (id2 * 3 + id1_grp) * 8
            d0[block_off : block_off + 8] = _pack_bits_lsb(shifted, 8)
    return d0


def pack_sss_d1() -> np.ndarray:
    """SSS D1 reference table: 18 uint16.

    Contains x1[0..126] concatenated twice (254 bits) plus 34 zero padding bits
    = 288 bits total, packed LSB-first into 18 uint16 words.

    The assembly reads only the first 256 bits (4×U4 bytes) and accesses them
    cyclically at byte offset m1 = id1%112, so any m1 ≤ 111 maps correctly to
    x1[(n+m1)%127] for n = 0..126 within the 288-bit window.
    """
    x1 = _X1_SEQ
    bits = np.zeros(288, dtype=np.uint8)
    bits[:127]    = x1.astype(np.uint8)
    bits[127:254] = x1.astype(np.uint8)
    return _pack_bits_lsb(bits, 18)
