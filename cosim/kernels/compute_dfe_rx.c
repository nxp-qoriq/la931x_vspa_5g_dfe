/* SPDX-License-Identifier: BSD-3-Clause
 * compute_dfe_rx.c — the DFE receive chain, as one callable kernel.
 *
 * Time-domain OFDM symbol in, subcarriers out:
 *   FFT-1024 (sc128, in place) -> bit-reversal -> subcarrier extraction
 *
 * The exact inverse of compute_dfe_tx.c, and its FFT is the real one
 * (fftDIF1024_sc128_hfx_hfx_vspa2, from lib/lib_DFE_Ref_vspa2.elb).
 *
 * The host writes input_td and reads rx_out directly out of DMEM, by symbol —
 * see the note in compute_dfe_tx.c.
 */
#include <stdint.h>

extern void fftDIF1024_sc128_hfx_hfx_vspa2(
    void const *pIn, void *pOut, void const *pBuff, unsigned int cbuffSize);

#define N               1024
#define NSC             61
#define CBUFF_HALFWORDS (N * 2)

/* Host-written: uint32 = Im_sm16 << 16 | Re_sm16. */
static uint32_t input_td[N] __attribute__((aligned(128)));
static uint32_t rx_out[NSC] __attribute__((aligned(128)));
static uint32_t work_buf[N] __attribute__((aligned(128)));

static unsigned int bitrev10(unsigned int x)
{
    unsigned int r = 0, i;
    for (i = 0; i < 10; i++) { r = (r << 1) | (x & 1); x >>= 1; }
    return r;
}

static void sw_bitrev(uint32_t *buf)
{
    unsigned int i, j, tmp;
    for (i = 0; i < (unsigned int)N; i++) {
        j = bitrev10(i);
        if (j > i) { tmp = buf[i]; buf[i] = buf[j]; buf[j] = tmp; }
    }
}

int main(void)
{
    int k;

    for (k = 0; k < N; k++)
        work_buf[k] = input_td[k];

    /* STEP 1 — in-place forward FFT (sc128: pIn = pOut = pBuff).
     * Note the asymmetry with TX: the FFT wants NATURAL time order in, and it
     * is the OUTPUT that comes back bit-reversed. */
    fftDIF1024_sc128_hfx_hfx_vspa2(work_buf, work_buf, work_buf, CBUFF_HALFWORDS);

    /* STEP 2 — un-reverse into natural DFT-bin order */
    sw_bitrev(work_buf);

    /* STEP 3 — subcarrier extraction (the inverse of the TX reorder) */
    for (k = 0; k < NSC/2; k++)
        rx_out[k] = work_buf[N - NSC/2 + k];             /* neg freqs */
    for (k = 0; k < NSC - NSC/2; k++)
        rx_out[NSC/2 + k] = work_buf[k];                 /* pos freqs */

    return 0;                    /* `done`: the host reads rx_out */
}
