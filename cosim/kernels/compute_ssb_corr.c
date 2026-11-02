/* SPDX-License-Identifier: BSD-3-Clause
 * compute_ssb_corr.c — stage 2 of the cell-search correlator: the conjugate
 * multiply and the inverse transform, on real library kernels.
 *
 * Gap 11, PSS half. Read compute_ssb_fft.c first — it explains why the
 * bit-reversal between the two transforms lives on the host.
 *
 * THE LIBRARY HAS NO PSS DETECTOR. This is not an oversight in the search, it
 * is a fact about the product: a full symbol dump of all 27 members of
 * lib_DFE_Ref_vspa2.elb has zero hits for pss / xcorr / corr / peak / search.
 * ssdet.h declares pssdet_param_T and names a "pssdet_xcorr_fft_asm.sx" — in
 * COMMENTS only; nothing implements them. The shipping firmware does cell
 * TRACKING, not cell SEARCH: DFE_func_rx.c reads N_id_2 out of a host config
 * word and DMAs in a precomputed pssRef256[id2].
 *
 * So the correlator has to be BUILT. It does not have to be INVENTED, because
 * the cross-correlation theorem turns it into three transforms that all exist:
 *
 *     corr[d] = sum_n rx[W+n] . conj(pss_td[n-d])
 *             = IDFT( DFT(rx_window) . conj(PSS_spectrum) ) [d]
 *
 *   DFT(rx_window)   fftDIF1024_sc128_hfx_hfx_vspa2   (compute_ssb_fft.c)
 *   . conj(REF)      chEst                            <- here
 *   IDFT(...)        ifftDIF1024_sc128_hfx_hfx_vspa2  <- here
 *
 * chEst is the conjugate multiplier. Its own header calls it a least-squares
 * channel estimator and talks about 802.11ac VHT-LTF, but what the machine code
 * does is `set.Smode S0conj...` + `cmad`: an elementwise h[k] = x[k].conj(p[k]).
 * That IS the per-subcarrier term of a matched filter, and using it as one is
 * not a repurposing trick — it is the same arithmetic under a different name.
 *
 * WHY THIS BEATS CORRELATING IN THE TIME DOMAIN
 * --------------------------------------------
 * One pass gives the correlation at ALL 1024 lags simultaneously, so the peak
 * search over timing costs nothing extra. A time-domain correlator would need
 * 1024 dot products of 1024 terms each, and there is no kernel for that either.
 *
 * THE REFERENCE IS ZERO OFF THE PSS SUBCARRIERS — that is what makes this a
 * matched filter rather than a channel estimate. (chEst's *pilot* in the
 * gap-12 chain pads with 1+0j instead, because rcpv then computes 1/H and a
 * zero would blow up. Same kernel, two different references, two different
 * jobs. Do not copy one into the other.)
 *
 * WHAT IS NOT ON A KERNEL: |corr|^2 and the argmax. Both are done on the host.
 * pow_sum_asm accumulates |x|^2 over a buffer — a reduction, not the per-lag
 * magnitude profile — and the peak search would need xtrm, whose index is known
 * to be wrong when N exceeds the batch size. A magnitude and an argmax are
 * bookkeeping, not signal processing; the correlation itself is entirely the
 * shipped assembly.
 */
#include <stdint.h>

/* --- real routines from lib/lib_DFE_Ref_vspa2.elb ----------------------- */
extern void chEst(void const *pIn, void *pChEst, void const *pPilot,
                  unsigned int N);
extern void ifftDIF1024_sc128_hfx_hfx_vspa2(void const *pIn, void *pOut,
                                            void const *pBuff,
                                            unsigned int cbuffSize);

#define NFFT     1024
#define NLINES   (NFFT / 32)   /* chEst's N is in DMEM LINES of 32 complex
                                * elements, not samples: 1024/32 = 32. */
#define NFFT_HW  (2 * NFFT)    /* circular buffer size in half-words = 2048 */
#define N_ID2    3
#define NWIN_MAX 24

/* ---- host-written inputs ---------------------------------------------- */
/* Natural-order spectra of the windows (the host has undone the FFT's
 * bit-reversal). cfixed16. */
static uint32_t fd_in[NWIN_MAX][NFFT] __attribute__((aligned(128)));

/* The three PSS reference spectra, one per N_id_2 candidate: the 127 BPSK chips
 * on their SSB subcarriers and ZERO everywhere else. cfloat16. */
static uint32_t ref[N_ID2][NFFT]      __attribute__((aligned(128)));

static uint32_t ctrl[4]               __attribute__((aligned(128)));
/* ctrl[0] = n_win */

/* ---- output the host reads -------------------------------------------- */
/* corr_out[w][h] = the lag profile of window w against hypothesis h, complex,
 * in BIT-REVERSED order (the inverse transform is DIF too). The host un-permutes
 * it, takes |.|^2 and finds the peak. cfixed16. */
static uint32_t corr_out[NWIN_MAX][N_ID2][NFFT] __attribute__((aligned(128)));

int main(void)
{
    uint32_t n_win = ctrl[0];
    uint32_t w, h;

    if (n_win > NWIN_MAX)
        n_win = NWIN_MAX;

    for (w = 0; w < n_win; w++) {
        for (h = 0; h < N_ID2; h++) {
            /* REAL kernel: P[k] = X[k] * conj(REF_h[k]), written straight into
             * the output slot so there is no copy. */
            chEst(fd_in[w], corr_out[w][h], ref[h], NLINES);

            /* REAL kernel: in-place inverse transform of that product. The
             * circular buffer is the slot itself — the transform never steps
             * outside its own 1024 points, so base = slot, size = 2048 hw. */
            ifftDIF1024_sc128_hfx_hfx_vspa2(corr_out[w][h], corr_out[w][h],
                                            corr_out[w][h], NFFT_HW);
        }
    }

    return 0;
}
