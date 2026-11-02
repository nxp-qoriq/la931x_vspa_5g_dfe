/* SPDX-License-Identifier: BSD-3-Clause
 * compute_decim_2x_8tap.c — 2x 8-tap FIR decimator, as a kernel.
 *
 * The receive rate converter, and the inverse of compute_x2_interp.c: 2176
 * complex samples in, 1088 out. Linked from lib/lib_DFE_Ref_vspa2.elb
 * (decimator_2x_8_Taps_asm).
 *
 * As with the interpolator, N_IN must stay a multiple of 64 (2176 = 34 * 64): the
 * asm drops a partial trailing block, and the dropped samples come back as a clean
 * 0.0 rather than as garbage.
 *
 * The host writes input_td and reads output_td directly out of DMEM, by symbol.
 */
#include <stdint.h>

extern void decimator_2x_8_Taps_asm(void *pOut, void *pIn, float *pTaps,
                                    void *filtState, unsigned int n_samples);

#define N_IN         2176            /* 34 * 64 */
#define N_OUT        1088             /* N_IN / 2 */
#define NUM_TAPS     8
#define STATE_LEN    32               /* cfixed16_t samples, 128-byte aligned */

/* 2xdown_coeff (natural order, symmetric). */
static const uint32_t filter_taps_u32[NUM_TAPS] = {
    0xBE0263BE, 0xBDE8A4F0, 0x3EA45618, 0x3F5B3BFF,
    0x3F5B3BFF, 0x3EA45618, 0xBDE8A4F0, 0xBE0263BE
};

static uint32_t input_td[N_IN]      __attribute__((aligned(128)));
static uint32_t state_buf[STATE_LEN] __attribute__((aligned(128)));
static uint32_t output_td[N_OUT]    __attribute__((aligned(128)));

int main(void)
{
    int i;

    /* Cold filter state, for the same reason as the interpolator's history. */
    for (i = 0; i < STATE_LEN; i++)
        state_buf[i] = 0;

    decimator_2x_8_Taps_asm(output_td, input_td, (float *)filter_taps_u32,
                            state_buf, N_IN);

    return 0;                    /* `done`: the host reads output_td */
}
