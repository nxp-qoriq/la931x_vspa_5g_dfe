/* SPDX-License-Identifier: BSD-3-Clause
 * compute_dfe_tx.c — the DFE transmit chain, as one callable kernel.
 *
 * Subcarriers in, time-domain OFDM symbol out:
 *   reorder into DFT-bin order -> iFFT-1024 (sc128) -> bit-reversal -> windowing
 *
 * The iFFT is the real one: ifftDIF1024_sc128_hfx_hfx_vspa2, linked from
 * lib/lib_DFE_Ref_vspa2.elb — the same library the firmware itself links.
 *
 * HOW DATA GETS IN AND OUT
 *   It does not. There is no file I/O and no stdout dump: the host writes
 *   input_sc_packed and reads work_buf_out directly out of DMEM, by symbol
 *   (sim/dfe/kernel_runner.py). The native ISS cannot service open/read anyway.
 *
 * Config: BANDWIDTH_25M_30K_61_61 / PROJ_1T1R_1CORE_LS_9310 (LA9310).
 */
#include <stdint.h>
#include "dfe_types.h"        /* cfixed16_t */

extern void ifftDIF1024_sc128_hfx_hfx_vspa2(
    cfixed16_t const *pIn, cfixed16_t *pOut,
    cfixed16_t const *pBuff, unsigned int cbuffSize);

#define N               1024
#define NSC             61
#define CBUFF_HALFWORDS (N * 2)          /* DFE_Ref's BYTE2UNIT(N*4) */
#define WINDOW_W        16

/* Host-written: uint32 = Im_sm16 << 16 | Re_sm16 (SM16, not two's complement). */
static uint32_t input_sc_packed[NSC];

/* pIn = pBuff = work_buf_in, pOut = work_buf_out — the sc128 entry sets up
 * circular addressing from pBuff and writes the output through a3. */
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

/* txWindowing_w16_vecaligned in single-shot mode (hist = 0): a raised-cosine
 * fade-in over the first W samples and fade-out over the last W, which tapers
 * the symbol edges and keeps the CP boundary from splattering out of band. */
static const float win_f32[WINDOW_W] = {
    0.0000000000f, 0.0096073598f, 0.0380602337f, 0.0842651938f,
    0.1464466094f, 0.2222148835f, 0.3086582838f, 0.4024548390f,
    0.5000000000f, 0.5975451610f, 0.6913417162f, 0.7777851165f,
    0.8535533906f, 0.9157348062f, 0.9619397663f, 0.9903926402f,
};

static void apply_tx_window(cfixed16_t *buf)
{
    unsigned int k;
    for (k = 0; k < WINDOW_W; k++) {
        float w = win_f32[k];
        buf[k].real = (float)buf[k].real * w;
        buf[k].imag = (float)buf[k].imag * w;
    }
    for (k = 0; k < WINDOW_W; k++) {
        float w = 1.0f - win_f32[k];
        unsigned int idx = (unsigned int)N - WINDOW_W + k;
        buf[idx].real = (float)buf[idx].real * w;
        buf[idx].imag = (float)buf[idx].imag * w;
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
     * Neg freqs move to the high bins first, so neither copy overlaps its own
     * source. */
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

    /* STEP 4 — raised-cosine edge windowing */
    apply_tx_window(work_buf_out);

    return 0;                    /* `done`: the host reads work_buf_out */
}
