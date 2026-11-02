/* SPDX-License-Identifier: BSD-3-Clause
 * compute_x2_interp.c — 2x polyphase FIR interpolator (32 taps), as a kernel.
 *
 * The transmit rate converter: 1088 complex samples in, 2176 out. Linked from
 * lib/lib_DFE_Ref_vspa2.elb (X2_interp_tap32_filter).
 *
 * The lengths are not free. The asm loops over whole 64-INPUT-sample blocks
 * (`sr g0, g0, 6` = floor(n/64)) and silently DROPS a partial trailing block, and
 * because the host zero-fills the output buffer the dropped samples come back as a
 * clean 0.0 rather than as garbage — a defect that reads as a smooth loss, not an
 * error. N_IN must therefore stay a multiple of 64: 1088 = 17 * 64. (The earlier
 * 1044 = 16*64 + 20 left the last 20 input samples unfiltered, zeroing the final
 * 40 outputs of every symbol.)
 *
 * The host writes input_td and reads output_td directly out of DMEM, by symbol.
 */
#include <stdint.h>

extern void X2_interp_tap32_filter(void *output, void *input,
                                   unsigned int num_samples,
                                   void *history, float *filter_taps);

#define N_IN         1088            /* 17 * 64 — the SX kernel processes whole
                                       * 64-sample blocks; a non-multiple silently
                                       * corrupts the tail (see kernels/README.md) */
#define N_OUT        2176             /* 2 * N_IN */
#define HIST_LEN     15
#define NUM_HW_TAPS  32

/* h_2x_32tap_stop10000_FR1_gain2p4, in the hardware's reversed-pair order. */
static const uint32_t filter_taps[NUM_HW_TAPS] = {
    0x3BF66D0F, 0x3B145943, 0xBC2029E4, 0x3BC6E0FE,
    0x3C1D0E2D, 0xBC914B26, 0xBB7807E3, 0x3D136719,
    0xBC4E8404, 0xBD814983, 0x3D4EF22B, 0x3DD52378,
    0xBE1B9A7D, 0xBE34750A, 0x3F82D7F9, 0x3ECB4074,
    0x3ECB4074, 0x3F82D7F9, 0xBE34750A, 0xBE1B9A7D,
    0x3DD52378, 0x3D4EF22B, 0xBD814983, 0xBC4E8404,
    0x3D136719, 0xBB7807E3, 0xBC914B26, 0x3C1D0E2D,
    0x3BC6E0FE, 0xBC2029E4, 0x3B145943, 0x3BF66D0F
};

/* 128-byte aligned: the VSPA's vector loads require it. */
static uint32_t input_td[N_IN]       __attribute__((aligned(128)));
static uint32_t history_buf[HIST_LEN] __attribute__((aligned(128)));
static uint32_t output_td[N_OUT]     __attribute__((aligned(128)));

int main(void)
{
    int i;

    /* Zero history: each symbol is filtered independently, so the co-simulation
     * and the Python model agree on a cold start. */
    for (i = 0; i < HIST_LEN; i++)
        history_buf[i] = 0;

    X2_interp_tap32_filter(output_td, input_td, N_IN,
                           history_buf, (float *)filter_taps);

    return 0;                    /* `done`: the host reads output_td */
}
