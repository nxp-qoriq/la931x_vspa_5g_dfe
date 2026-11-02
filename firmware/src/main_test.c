/*
 * main_test.c — DFE TX/RX pipeline testbench (no DMA, no host)
 *
 * Purpose: show the TX "in-place concatenation" that maps Nsc packed
 * frequency-domain subcarriers into the IFFT input buffer, run the IFFT,
 * then recover the subcarriers with the forward FFT — a loopback whose RX
 * bit-reversal runs on REAL NXP IPPU microcode.
 *
 * Config (BANDWIDTH_25M_30K_61_61 / PROJ_1T1R_1CORE_LS_9310):
 *   N   = POINTS_FFT           = 1024   FFT/IFFT size
 *   NSC = 61                            active subcarriers (see below)
 *   LCP = NUM_SAMPLES_LCP      = 80     long cyclic-prefix length  (320/4)
 *   SCP = NUM_SAMPLES_SCP      = 72     short cyclic-prefix length (288/4)
 *
 * Memory note: VCPU DRAM = 16 KB.  Two large buffers (TX + RX FFT) would
 * not fit simultaneously, so we share one N-point working buffer for both
 * paths — this is fine because they run sequentially.
 *
 * Build:   make test                → eld/dfe-la9310-…-test.eld  (IPPU RX bitrev)
 *          make test SW_BITREV=1    → software-permutation A/B reference
 * Verify:  scripts/verify_dfe_loopback.py           (verdict)
 *          scripts/verify_dfe_loopback.py --stages  (every stage vs the oracle)
 */

#include "pair_config.h"   /* N, NSC, CP sizes, BYTE2UNIT                  */
#include "diffft.h"        /* ifftDIF1024_sc128_hfx_hfx_vspa2, fftDIF1024  */
#include "mem.h"           /* kept for the lib's other users; see STEP 1 note */
#include "ippu.h"          /* ippu_enable / ippu_dmem_base / ippu_is_done   */
#include <stdint.h>

/* ── shorthand constants ───────────────────────────────────────────────── */
#define N    POINTS_FFT           /* 1024 */

/*
 * Active subcarriers under test.  NOT MAX_NUM_SUBCARRIERS (624 for this
 * config): that is the BUFFER-CAPACITY constant.  The runtime count comes from
 * the host via GET_NUM_SC and is 61 in the profile the rest of this repo
 * simulates — kernels/compute_dfe_tx.c and the BER co-simulation both use 61.
 * Before this was unified, the loopback and the co-simulation were quietly
 * exercising two different configurations.
 *
 * The in-place concatenation below (packed subcarriers -> DFT-bin order in the
 * SAME buffer) is only overlap-free while NSC <= 2N/3: past that, the wrap half
 * of the copy overwrites source samples that have not been read yet, and no
 * library call can make that overlap safe.  Enforced, not assumed:
 */
#define NSC  61
#if NSC > (2 * (N)) / 3
#error "in-place subcarrier concat self-clobbers for NSC > 2N/3 - use a second buffer"
#endif
#define LCP  NUM_SAMPLES_LCP      /* 80 */

/*
 * "4" = sizeof(cfixed16_t) in 8-bit bytes (2 × fixed16_t halves × 2 bytes each).
 * NXP convention throughout the codebase: use *4 instead of *sizeof(cfixed16_t)
 * because fsvspacc's sizeof() returns MAU counts (16-bit units on VSPA2).
 */
#define SC_BYTES(n)   ((n) * 4)

/* ── buffers ───────────────────────────────────────────────────────────── */

/*
 * Shared TX/RX working buffer — in the real DFE app these are separate
 * (tx_sym_buf and FFTin respectively) but here we reuse one buffer because:
 *   1. TX and RX are sequential in this test (no concurrent DMA).
 *   2. VCPU DRAM is only 16 KB; two N-point buffers would exhaust it.
 *
 * Non-static so the host driver (scripts/verify_dfe_loopback.py) can resolve
 * them from the symbol table and dump each pipeline stage.
 *
 * aligned(64) is LOAD-BEARING: the FFT kernels and the lib's memcpy_dst_circ
 * use hardware circular addressing (set.range) over this buffer, which silently
 * writes to WRONG OFFSETS when the base is not vector-aligned.  The app aligns
 * every circular buffer with __align_vec (global_var.c); an unaligned work_buf
 * is one of the two defects that made this loopback put the tone in the wrong
 * bins.
 */
cfixed16_t work_buf[N] __attribute__((aligned(64)));

/* Recovered subcarriers after the full TX→RX round trip */
cfixed16_t rx_sc_out[NSC] __attribute__((aligned(64)));

/* ── result flags (simulator reads these at halt) ──────────────────────── */
volatile int test_result  = -1;   /* -1=not run, 0=FAIL, 1=PASS */
volatile int dominant_sc  = -1;   /* subcarrier index with peak energy after RX */
volatile unsigned int dominant_e  = 0;  /* its energy (SM16-decoded magnitudes)  */
volatile unsigned int guard_max_e = 0;  /* peak guard-band energy after RX FFT   */

/* ── host-driven stage handshake ───────────────────────────────────────── */
/*
 * The loopback publishes a stage number after every pipeline step.  When the
 * host driver sets dbg_enable it additionally pauses until the driver
 * acknowledges (dump-and-continue co-simulation).  With dbg_enable left at 0
 * (a plain run) the markers are free-running and cost nothing.
 */
volatile unsigned int test_nsc  __attribute__((used)) = NSC;  /* read by the host driver */
volatile unsigned int test_ippu __attribute__((used)) = 0;    /* 1 = IPPU bitrev build */
volatile unsigned int dbg_enable = 0;
volatile unsigned int dbg_stage  = 0;
volatile unsigned int dbg_go     = 0;

#define STAGE(n) do {                                        \
        dbg_stage = (n);                                     \
        if (dbg_enable) { while (!dbg_go) {} dbg_go = 0; }   \
    } while (0)

/* ── bit-reversal (DIF IFFT/FFT emit samples in bit-reversed order) ─────── */
/*
 * TX uses the software permutation; RX uses REAL NXP IPPU microcode.  That
 * split is not laziness — it is what the two kernels actually do:
 *
 *   RX: bitRev1024sEbyE (vspa-lib, submodules/.../bitRev/asm/bitRev1024sEbyE.sx)
 *       performs bit-reversal AND an N/2 shift — the "shifting" in its
 *       description.  That shift is exactly what an OFDM RX wants: it DC-centres
 *       the spectrum, so the active subcarriers become one CONTIGUOUS run
 *       starting at bin N/2 - NSC/2, and extraction collapses from two
 *       wrap-around copies into a single copy (see rx_process).
 *       Measured: the sc[0] tone lands at bin 482 = 512 - 61/2. ✓
 *
 *   TX: needs a PURE permutation — bit-reversal plus an N/2 shift would emit a
 *       time-domain symbol cyclically rotated by N/2, i.e. a different signal
 *       (its spectrum picks up a (-1)^k factor, flipping the sign of every odd
 *       subcarrier).  vspa-lib has no pure 1024-point bitrev IPPU procedure
 *       (only *sEbyE at 1024), and the app's own TX procedure
 *       (bitRev1024_reverseIn_linearOut_vspa2_b*) is fused with CP insertion and
 *       circular-DMA addressing: driven with the arg layout its .inc documents,
 *       it stalls at IPPU exec_pc 14 in the ISS because it needs the full TX DMA
 *       context this loopback does not have.  So TX keeps sw_bitrev.
 *
 *       CAUTION for anyone tempted to "just use the IPPU here too": with the
 *       single tone at sc[0] the TX stage compares bit-exact EVEN WITH the
 *       shift, because that tone (even bin 994) is invariant under a 512-sample
 *       rotation.  The test would pass for the wrong reason.
 *
 * Both bitrevs are checked stage-by-stage against the Python oracle by
 * scripts/verify_dfe_loopback.py --stages.
 */
#ifndef USE_IPPU_BITREV
#define USE_IPPU_BITREV 1
#endif

static unsigned int bitrev10(unsigned int x)
{
    unsigned int r = 0, i;
    for (i = 0; i < 10; i++) { r = (r << 1) | (x & 1); x >>= 1; }
    return r;
}

static void sw_bitrev(cfixed16_t *buf)
{
    unsigned int i, j;
    cfixed16_t tmp;
    for (i = 0; i < (unsigned int)N; i++) {
        j = bitrev10(i);
        if (j > i) { tmp = buf[i]; buf[i] = buf[j]; buf[j] = tmp; }
    }
}

#if USE_IPPU_BITREV

extern void bitRev1024sEbyE(void);

/* IPPU destination (the procedure cannot work in place). */
static cfixed16_t bitrev_out[N] __attribute__((aligned(64)));

/*
 * Run bitRev1024sEbyE on the IPPU: dst = bitrev(src) rotated by N/2.
 *
 * ARG PLACEMENT — the subtle part.  The IPPU fetches its arguments from ITS OWN
 * DMEM window, relative to ippu_dmem_base().  Rather than hard-coding a memory
 * map, ask the running machine where that window is and write there, passing
 * arg base 0.  DMEM is one flat address space from the VCPU's side, and the
 * LA9310 DMEM partition redirects the store to the IPPU's bank.
 *
 * Why not the usual `ippu_arg_base(IPPU_OFFSET((uint32_t)ippu_args))` idiom?
 * It only holds when the linker places `ippu_args` exactly at the base the
 * MODEL reports.  The stock kernel LCFs do; the DFE app's LCF splits DMEM per
 * LA9310 silicon (16 KB VCPU + 16 KB IPPU, .idram at MAU 0x2000), and against a
 * model whose partition has not been configured the same way, IPPU_OFFSET()
 * yields a NEGATIVE base — the microcode's `ld as0,+1` then reads garbage and
 * the output is SILENTLY ALL-ZERO.  No fault, no hang, just zeros.  Asking
 * ippu_dmem_base() tracks whatever model is running.
 */
static void ippu_bitrev_shift(cfixed16_t *dst, cfixed16_t *src)
{
    volatile uint32_t *ia = (volatile uint32_t *)(uintptr_t)ippu_dmem_base();

    while (ippu_is_busy()) {}

    ia[0] = (uint32_t)((void *)dst);   /* arg0: linear output      */
    ia[1] = (uint32_t)((void *)src);   /* arg1: bit-reversed input */

    ippu_arg_base(0);
    ippu_enable(bitRev1024sEbyE, IPPU_PEND_NONE | IPPU_MODE_16BIT);
    __asm volatile("fnop .asmvol");   /* IPPU status is not readable in the */
    __asm volatile("fnop .asmvol");   /* next few instruction slots         */
    __asm volatile("fnop .asmvol");

    while (!ippu_is_done()) {}
}

/* RX reorder, in place: bit-reversal + N/2 shift, on the IPPU. */
static void rx_bitrev(cfixed16_t *buf)
{
    unsigned int k;
    uint32_t *s = (uint32_t *)(void *)buf;
    uint32_t *d = (uint32_t *)(void *)bitrev_out;

    ippu_bitrev_shift(bitrev_out, buf);
    for (k = 0; k < (unsigned int)N; k++) s[k] = d[k];
}

#define RX_SPECTRUM_IS_DC_CENTRED  1

#else  /* software reference build: plain permutation, no shift */

static void rx_bitrev(cfixed16_t *buf) { sw_bitrev(buf); }

#define RX_SPECTRUM_IS_DC_CENTRED  0

#endif

/* ── TX path ───────────────────────────────────────────────────────────── */
/*
 * tx_process()
 *
 * Input:   work_buf[0..NSC-1]  — packed frequency-domain subcarriers:
 *            [0 .. NSC/2-1]     = negative-frequency subcarriers
 *            [NSC/2 .. NSC-1]   = positive-frequency subcarriers (DC at NSC/2)
 *
 * Output:  work_buf[0..N-1]    — time-domain OFDM symbol (natural sample order)
 *          (cyclic prefix not added; see note below)
 */
static void tx_process(void)
{
    /* ------------------------------------------------------------------
     * STEP 1: IN-PLACE CONCATENATION
     * ---------------------------------------------------------------
     * work_buf[0..NSC-1] holds packed subcarriers (neg freqs first, then pos
     * freqs).  The IFFT wants standard DFT bin order:
     *
     *   bins 0 .. NSC-NSC/2-1       = pos freqs  (sc[NSC/2 .. NSC-1])
     *   bins NSC-NSC/2 .. N-NSC/2-1 = guard      (zeroed in step 2)
     *   bins N-NSC/2 .. N-1         = neg freqs  (sc[0 .. NSC/2-1])
     *
     * Plain scalar loops, same pattern as the bit-exact dfe_tx kernel.  Copy
     * order matters: neg freqs -> high bins FIRST (the regions are disjoint for
     * NSC <= 2N/3, enforced by the #error above), then pos freqs -> low bins
     * (that source range is no longer needed).
     *
     * History — why not the lib's memcpy_dst_circ, which is what the real app
     * uses?  Two defects hid behind that call:
     *   (1) work_buf was not vector-aligned, so the lib's circular VECTOR writes
     *       landed at wrong offsets (fixed by aligned(64) above);
     *   (2) NSC was MAX_NUM_SUBCARRIERS > 2N/3, so even a correct in-place
     *       circular copy self-clobbers.
     * Both are fixed now, so the library call would very likely work again — but
     * the scalar version is what has actually been verified bit-exact against
     * the oracle, stage by stage.  Restoring the idiomatic circ copy is listed
     * as future work (docs/GAPS_AND_FUTURE_WORK.md).
     */
    {
        unsigned int k;
        uint32_t *b = (uint32_t *)(void *)work_buf;
        for (k = 0; k < NSC/2; k++)
            b[N - NSC/2 + k] = b[k];      /* neg freqs → bins N-NSC/2 .. N-1 */
        for (k = 0; k < NSC - NSC/2; k++)
            b[k] = b[NSC/2 + k];          /* pos freqs → bins 0 .. NSC-NSC/2-1 */
    }
    STAGE(2);

    /* ------------------------------------------------------------------
     * STEP 2: ZERO THE GUARD BAND — bins NSC-NSC/2 .. N-NSC/2-1
     */
    {
        unsigned int k;
        uint32_t *b = (uint32_t *)(void *)work_buf;
        for (k = NSC - NSC/2; k < N - NSC/2; k++) b[k] = 0;
    }
    STAGE(3);

    /* ------------------------------------------------------------------
     * STEP 3: IN-PLACE IFFT
     * ---------------------------------------------------------------
     * DIF IFFT: natural-order frequency input → bit-reversed time output.
     * All three pointer arguments equal work_buf (in-place operation).
     * sc128 scaling: output is divided by 128 to prevent fixed-point saturation.
     */
    ifftDIF1024_sc128_hfx_hfx_vspa2(
        work_buf, work_buf, work_buf,
        BYTE2UNIT(SC_BYTES(N))    /* 2048: buffer size in 16-bit half-words */
    );
    STAGE(4);

    /* ------------------------------------------------------------------
     * STEP 4: BIT-REVERSAL — software, deliberately (see the note above)
     */
    sw_bitrev(work_buf);
    STAGE(5);

    /* work_buf[0..N-1] is now the time-domain OFDM symbol in natural order.
     *
     * Cyclic prefix note: in the real app the IPPU copies work_buf[N-CP..N-1]
     * to the front of the output stream.  We skip CP here because the loopback
     * RX test only needs the N-sample symbol body.  See start_ippu_bitrev_tx()
     * in DFE_func_tx.c for the full CP-insertion flow. */
}

/* ── RX path ───────────────────────────────────────────────────────────── */
/*
 * rx_process()
 *
 * Input:   work_buf[0..N-1]  — time-domain OFDM symbol body (CP already removed)
 * Output:  rx_sc_out[0..NSC-1] — recovered subcarriers, same order as TX input
 */
static void rx_process(void)
{
    /* STEP 1: FORWARD FFT (in-place) — time → frequency, output bit-reversed */
    fftDIF1024_sc128_hfx_hfx_vspa2(
        work_buf, work_buf, work_buf,
        BYTE2UNIT(SC_BYTES(N))
    );
    STAGE(6);

    /* STEP 2: BIT-REVERSAL — real IPPU microcode (or sw in the A/B build) */
    rx_bitrev(work_buf);
    STAGE(7);

    /* ------------------------------------------------------------------
     * STEP 3: SUBCARRIER EXTRACTION (inverse of the TX concatenation)
     * ---------------------------------------------------------------
     * With the IPPU RX bitrev the spectrum is DC-centred (bitrev + N/2 shift),
     * so the NSC active subcarriers are ONE CONTIGUOUS run starting at
     * N/2 - NSC/2, already in the packed order the TX consumed (sc[0] = first
     * neg freq … sc[NSC-1] = last pos freq).  Collapsing the two wrap-around
     * copies into this single copy is the whole point of the sEbyE kernel — and
     * it is what start_ippu_bitRev_reverseIn_linearOut_CopyDestToSrc_vspa2()
     * does in the real app.
     *
     * The software build produces the un-shifted spectrum, so it still needs the
     * two wrap-around copies.
     */
    {
        unsigned int k;
        uint32_t *dst = (uint32_t *)(void *)rx_sc_out;
        uint32_t *b   = (uint32_t *)(void *)work_buf;
#if RX_SPECTRUM_IS_DC_CENTRED
        for (k = 0; k < (unsigned int)NSC; k++)
            dst[k] = b[N/2 - NSC/2 + k];
#else
        for (k = 0; k < NSC/2; k++)       dst[k]         = b[N - NSC/2 + k];
        for (k = 0; k < NSC - NSC/2; k++) dst[NSC/2 + k] = b[k];
#endif
    }
    STAGE(8);
}

/* ── main ──────────────────────────────────────────────────────────────── */

void main(void)
{
    int i;

    /* Volatile stores keep the symbols alive for the host driver (the compiler
     * discards an unreferenced global even with attribute used). */
    test_nsc  = NSC;
    test_ippu = USE_IPPU_BITREV;

    /* Load test vector: single tone at sc[0] (a negative-frequency subcarrier).
     * 0x2000 = 0.25 in Q15/SM16 (identical bit patterns for positive values —
     * the FFT kernels use SM16).  Written through a HALFWORD VIEW: assigning an
     * int literal to a __fx16 member converts NUMERICALLY (8192 > 1.0 saturates
     * to 0x7FFF), it does not set the bit pattern. */
    {
        short *raw = (short *)(void *)work_buf;
        for (i = 0; i < 2 * NSC; i++) raw[i] = 0;
        raw[0] = 0x2000;   /* sc[0].real */
    }
    /* work_buf[NSC..N-1] will be overwritten by the in-place concatenation */
    STAGE(1);

    tx_process();   /* packed subcarriers → time-domain symbol */
    rx_process();   /* time-domain symbol → recovered subcarriers */

    /* ------------------------------------------------------------------
     * Verify 1: the dominant subcarrier, by energy.
     *
     * sc128 round-trip scaling (IFFT then FFT, each divides by 128):
     *   energy scale ≈ (N / 128²)² = (1024/16384)² = (1/16)² = 1/256
     * Input amplitude 0x2000 = 8192 → expected output amplitude ~512.
     *
     * THE BUG THIS REPLACES.  The old verdict read the samples as
     *     int re = rx_sc_out[i].real;
     * — and `.real` is a __fx16.  That assignment is a NUMERIC conversion, not a
     * bit copy: every |value| < 1.0 truncates to 0.  So `e` was always 0, max_e
     * stayed 0, dominant_sc kept its initial value of 0, and
     * `test_result = (dominant_sc == 0)` reported PASS — unconditionally, even
     * for a pipeline producing pure garbage.  The test asserted nothing.
     *
     * The FFT kernels produce SM16 (sign-magnitude): magnitude = bits & 0x7FFF.
     * Read the raw bits through a halfword view.
     */
    dominant_sc = -1;
    {
        unsigned int max_e = 0;
        short *raw = (short *)(void *)rx_sc_out;
        for (i = 0; i < NSC; i++) {
            unsigned int mr = (unsigned int)raw[2*i]     & 0x7FFFu;
            unsigned int mi = (unsigned int)raw[2*i + 1] & 0x7FFFu;
            unsigned int e = mr * mr + mi * mi;
            if (e > max_e) { max_e = e; dominant_sc = i; }
        }
        dominant_e = max_e;
    }

    /* ------------------------------------------------------------------
     * Verify 2: the guard band (the DFT bins outside the active run, still in
     * work_buf after the RX bit-reversal) must hold only rounding noise.
     *
     * Without this, "the tone is at sc[0]" is a weak claim: it says nothing
     * about where the ENERGY went.  A pipeline that smears the tone across the
     * spectrum can still have its peak in the right bin.
     *
     * Threshold: the measured noise floor of the sc128 IFFT→FFT round trip is
     * max |guard|² = 16 (±4 LSB residuals; tone 0x2000 at sc[0]).  256 is 16×
     * that floor and still ~900× below the recovered tone energy (236196), so it
     * separates spectral leakage from rounding noise with room on both sides.
     */
    #define GUARD_E_THRESH 256u
    {
        unsigned int max_g = 0;
        short *raw = (short *)(void *)work_buf;
        for (i = 0; i < N; i++) {
#if RX_SPECTRUM_IS_DC_CENTRED
            /* active run is contiguous: [N/2-NSC/2, N/2-NSC/2+NSC) */
            if (i >= N/2 - NSC/2 && i < N/2 - NSC/2 + NSC) continue;
#else
            /* active bins wrap: [0, NSC-NSC/2) and [N-NSC/2, N) */
            if (i < NSC - NSC/2 || i >= N - NSC/2) continue;
#endif
            {
                unsigned int mr = (unsigned int)raw[2*i]     & 0x7FFFu;
                unsigned int mi = (unsigned int)raw[2*i + 1] & 0x7FFFu;
                unsigned int e = mr * mr + mi * mi;
                if (e > max_g) max_g = e;
            }
        }
        guard_max_e = max_g;
    }

    /* All three must hold.  dominant_e > 0 is what makes an all-zero pipeline
     * FAIL instead of silently passing. */
    test_result = (dominant_sc == 0 &&
                   dominant_e  > 0 &&
                   guard_max_e < GUARD_E_THRESH) ? 1 : 0;

    STAGE(9);
    while (1) {}   /* halt — simulator: inspect test_result / dominant_sc /
                    * dominant_e / guard_max_e */
}
