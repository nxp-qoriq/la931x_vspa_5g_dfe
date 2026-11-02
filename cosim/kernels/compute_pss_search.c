/* SPDX-License-Identifier: BSD-3-Clause
 * compute_pss_search.c — PSS / N_id_2 search, on the real chEst kernel.
 *
 * Gap 11, PSS half.
 *
 * THERE IS NO PSS DETECTOR IN THE LIBRARY. Say it plainly, because the obvious
 * assumption is that there must be:
 *
 *   - a full symbol dump of all 27 members of lib_DFE_Ref_vspa2.elb has ZERO
 *     hits for pss / xcorr / corr / peak / search;
 *   - ssdet.h declares pssdet_param_T, pssdet_peaks_T and a "pssdet_xcorr_fft_
 *     asm.sx" — all of them in COMMENTS only. Nothing implements them and no
 *     caller references them;
 *   - `sssdet_asm_rc.eln` is an archive MEMBER name, not a symbol; it exports
 *     _fineCFO_rc. There is no blind detector hiding in it;
 *   - the submodule's freq_domain_corr is frequency-domain CORRECTION (a complex
 *     gain and a phase ramp, x*g*exp(-j2*pi*f*(ki+k)/2^32)), not CORRELATION.
 *     It has no conjugate, no reduction and no peak search. It cannot search.
 *
 * The reason is architectural: the shipping firmware does cell TRACKING, not
 * cell SEARCH. DFE_func_rx.c reads N_id_2 from the host's config word and DMAs
 * in a precomputed pssRef256[id2]; the PSS symbol is only ever used to estimate
 * the channel against a reference that is already known.
 *
 * WHAT THIS FILE DOES INSTEAD
 * ---------------------------
 * It keeps the correlation on the real kernel. chEst(y, h, ref, N) computes the
 * elementwise conjugate product h[k] = y[k] * conj(ref[k]) — which, for a ±1
 * BPSK reference, is exactly the per-subcarrier term of a PSS correlation. So
 * scoring the three N_id_2 hypotheses is three real chEst calls:
 *
 *     for id2 in 0,1,2:  chEst(pss_in, h[id2], pss_ref[id2], 8)
 *                        score[id2] = | sum over the 127 PSS REs of h[id2][k] |^2
 *
 * and the argmax over three numbers picks N_id_2.
 *
 * The reduction (a sum of 127 complex values) and the argmax (three compares)
 * are done here on the VCPU, in C. They have no library kernel — pow_sum_asm
 * accumulates |x|^2, which is the WRONG metric: with |ref| = 1 it is identical
 * for all three hypotheses and discriminates nothing. The discriminant is the
 * COHERENT sum, |sum h|^2. A 127-term scalar sum is not a reimplemented
 * detector; the correlation itself is entirely the shipped assembly.
 *
 * SCOPE, HONESTLY STATED
 * ----------------------
 * A coherent sum is the right metric on a clean, aligned symbol — which is what
 * this harness is tested on. On the NXP FR1 vectors, which carry deliberately
 * injected CFO (300..1000 Hz) and timing offset, the residual phase ramp across
 * subcarriers destroys a coherent sum, and a DIFFERENTIAL correlation is needed
 * instead. That belongs to the host, is a Python model, and is labelled as one:
 * it is the one step of this whole feature with no kernel behind it.
 *
 * The 127 PSS REs sit at buffer index 64..190 of the 256-point window, because
 * the window starts 8 REs before SSB RE 0 and PSS starts 56 REs into the SSB:
 * NR_SSB_KRE(8) + NR_PSS_KRE(56) = 64 = SLOTTED_PSS_SSS_OFST. Three independent
 * sources agree on that number, which is reassuring for a magic constant.
 */
#include <stdint.h>

/* --- the real routine from lib/lib_DFE_Ref_vspa2.elb -------------------- */
extern void chEst(void const *pIn, void *pChEst, void const *pPilot,
                  unsigned int N);

#define NFFT        256
#define NLINES      (NFFT / 32)   /* 8 DMEM lines */
#define PSS_SSS_OFS 64            /* SLOTTED_PSS_SSS_OFST */
#define N_PSS_RE    127
#define N_ID2       3

/* ---- host-written inputs ---------------------------------------------- */
static uint32_t pss_in[NFFT]            __attribute__((aligned(128))); /* cfixed16 */
static uint32_t pss_ref[N_ID2][NFFT]    __attribute__((aligned(128))); /* cfloat16 */

/* ---- outputs the host reads ------------------------------------------- */
static uint32_t h_out[N_ID2][NFFT]      __attribute__((aligned(128))); /* cfixed16 */
static uint32_t score[N_ID2]            __attribute__((aligned(128))); /* float bits */
static uint32_t best[4]                 __attribute__((aligned(128)));

/* best[0] = argmax N_id_2
 * best[1] = score[argmax], float bits
 * best[2] = runner-up score, float bits  (the margin is the confidence)
 * best[3] = 0
 */

static uint32_t f2b(float f) { union { uint32_t u; float f; } c; c.f = f; return c.u; }

/* Sign-magnitude half-fixed -> int. Bit 15 is a SIGN BIT, not the top bit of a
 * two's-complement number: 0x8003 is -3, not -32765. Getting this wrong is
 * silent and produces plausible garbage. */
static int32_t sm16(uint32_t h)
{
    int32_t mag = (int32_t)(h & 0x7FFFu);
    return (h & 0x8000u) ? -mag : mag;
}

int main(void)
{
    uint32_t id2;
    int      k;
    uint32_t bi = 0;
    float    b0 = -1.0f, b1 = -1.0f;

    for (id2 = 0; id2 < N_ID2; id2++) {
        int32_t sr = 0, si = 0;    /* |sum| <= 127 * 32767 — fits in int32 */
        float   fr, fi, s;

        /* REAL kernel: h = pss_in * conj(pss_ref[id2]), per subcarrier. */
        chEst(pss_in, h_out[id2], pss_ref[id2], NLINES);

        /* Coherent sum over the 127 PSS REs. h_out is cfixed16: the real part
         * is the LOW halfword of the word, the imag part the high one. */
        for (k = PSS_SSS_OFS; k < PSS_SSS_OFS + N_PSS_RE; k++) {
            uint32_t w = h_out[id2][k];
            sr += sm16(w & 0xFFFFu);
            si += sm16((w >> 16) & 0xFFFFu);
        }

        fr = (float)sr;
        fi = (float)si;
        s  = fr * fr + fi * fi;
        score[id2] = f2b(s);

        if (s > b0) { b1 = b0; b0 = s; bi = id2; }
        else if (s > b1) { b1 = s; }
    }

    best[0] = bi;
    best[1] = f2b(b0);
    best[2] = f2b(b1);
    best[3] = 0;

    return 0;
}
