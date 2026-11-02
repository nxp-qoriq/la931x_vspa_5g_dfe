/* SPDX-License-Identifier: BSD-3-Clause
 * compute_ssb_tx.c — the DFE transmit chain, widened to a full carrier.
 *
 * Subcarriers in, time-domain OFDM symbol out:
 *   reorder into DFT-bin order -> iFFT-1024 (sc128) -> bit-reversal
 *
 * This is compute_dfe_tx.c with exactly two deliberate differences, and it calls
 * the SAME real library iFFT (ifftDIF1024_sc128_hfx_hfx_vspa2, out of
 * lib/lib_DFE_Ref_vspa2.elb). No signal processing is reimplemented here.
 *
 * WHY A SECOND TX HARNESS AT ALL
 *   compute_dfe_tx.c hard-codes NSC = 61 — the narrowband 61-subcarrier DFE test
 *   config. An SSB is 240 contiguous REs wide and simply does not fit in it. The
 *   carrier here is NSC = 624 REs (the LA9310 "9310" build's NSC for 10 MHz /
 *   15 kHz, and the width of NXP's own FR1 fd test vectors), which holds the SSB
 *   with room around it. Everything else — the reorder, the iFFT call, the
 *   bit-reversal — is character-for-character the 61-subcarrier chain.
 *
 * WHY NO EDGE WINDOWING
 *   compute_dfe_tx.c ends with apply_tx_window(): a 16-sample raised-cosine
 *   fade-in and fade-out. That is plain C, not a library kernel, and it is an
 *   OVERLAP-ADD device: the fade-out of symbol n is meant to be summed onto the
 *   fade-in of symbol n+1 *inside the cyclic prefix*. Applied to an isolated
 *   symbol with no overlap-add, it tears the last 16 samples of the useful part
 *   of the symbol and puts a hard floor of a few percent under the EVM — a floor
 *   that has nothing to do with the channel, the estimator or the equaliser,
 *   which are what this co-simulation measures. So it is left out, and its
 *   absence is stated rather than hidden.
 */
#include <stdint.h>
#include "dfe_types.h"        /* cfixed16_t */

extern void ifftDIF1024_sc128_hfx_hfx_vspa2(
    cfixed16_t const *pIn, cfixed16_t *pOut,
    cfixed16_t const *pBuff, unsigned int cbuffSize);

#define N               1024
#define NSC             624              /* carrier REs (must be even) */
#define CBUFF_HALFWORDS (N * 2)          /* DFE_Ref's BYTE2UNIT(N*4) */

/* Host-written: uint32 = Im_sm16 << 16 | Re_sm16 (SM16, not two's complement).
 * RE index j = 0..NSC-1, low j = most negative frequency. */
static uint32_t input_sc_packed[NSC];

static cfixed16_t work_buf_in[N]  __attribute__((aligned(128)));
static cfixed16_t work_buf_out[N] __attribute__((aligned(128)));

static unsigned int bitrev10(unsigned int x)
{
    unsigned int r = 0, i;
    for (i = 0; i < 10; i++) { r = (r << 1) | (x & 1); x >>= 1; }
    return r;
}

/* The DIF iFFT emits BIT-REVERSED bins; on silicon the IPPU undoes this. */
static void sw_bitrev(cfixed16_t *buf)
{
    unsigned int i, j;
    cfixed16_t tmp;
    for (i = 0; i < (unsigned int)N; i++) {
        j = bitrev10(i);
        if (j > i) { tmp = buf[i]; buf[i] = buf[j]; buf[j] = tmp; }
    }
}

int main(void)
{
    int k;

    /* Copy through uint32* to preserve the raw SM16 bit patterns — a direct
     * __fx16 assignment would numerically convert them. */
    {
        uint32_t *dst = (uint32_t *)(void *)work_buf_in;
        for (k = 0; k < NSC; k++) dst[k] = input_sc_packed[k];
        for (k = NSC; k < N;  k++) dst[k] = 0;
    }

    /* STEP 1 — subcarriers into iFFT-ready DFT-bin order.
     *   in:  [0 .. NSC/2-1] neg freqs, [NSC/2 .. NSC-1] pos freqs
     *   out: [0 ..] pos freqs, guard zeros, [N-NSC/2 ..] neg freqs
     * i.e. RE j lands on DFT bin (j - NSC/2) mod N. Neg freqs move to the high
     * bins first, so neither copy overlaps its own source. */
    for (k = 0; k < NSC/2; k++)
        work_buf_in[N - NSC/2 + k] = work_buf_in[k];
    for (k = 0; k < NSC - NSC/2; k++)
        work_buf_in[k] = work_buf_in[NSC/2 + k];
    {
        uint32_t *g = (uint32_t *)(void *)work_buf_in;
        for (k = NSC - NSC/2; k < N - NSC/2; k++) g[k] = 0;
    }

    /* STEP 2 — iFFT (pIn = pBuff = work_buf_in, pOut = work_buf_out) */
    ifftDIF1024_sc128_hfx_hfx_vspa2(work_buf_in, work_buf_out,
                                    work_buf_in, CBUFF_HALFWORDS);

    /* STEP 3 — un-reverse the bins into natural time order */
    sw_bitrev(work_buf_out);

    return 0;                    /* `done`: the host reads work_buf_out */
}
