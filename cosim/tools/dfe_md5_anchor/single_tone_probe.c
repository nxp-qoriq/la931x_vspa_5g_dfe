// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 NXP
/*
 * single_tone_probe.c — drive the DFE library's REAL single_tone_asm /
 * single_tone_add_asm kernels on the native VSPA ISS, under host control.
 *
 * This reproduces, exactly, the TX antenna-buffer producer of the LA9310 DFE
 * firmware in single-tone mode (DFE_ref_la9310/src/main_pair_coreA.c:449-461):
 *
 *     single_tone_asm    (ptr_ant, tx_single_tone_freq[0], offset_in_sframe/4,
 *                         tx_single_tone_amplitude[0] & ~SINGLE_TONE_VALID_FLAG,
 *                         tx_ant_buf_size/128);
 *     for (i = 1..3) if (amplitude[i] & SINGLE_TONE_VALID_FLAG)
 *         single_tone_add_asm(ptr_ant, ptr_ant, freq[i], offset_in_sframe/4,
 *                             amplitude[i] & ~SINGLE_TONE_VALID_FLAG, lines);
 *
 * The bytes this writes into the antenna buffer are what the AXIQ DMA sends to
 * the DAC, and what dump_time_domain_tx.sh copies to file (via DMAC_WRC, which
 * converts DMEM sign-magnitude -> two's complement on the way to AXI).
 *
 * Host protocol (all through DMEM, polled):
 *   write cfg_freq / cfg_phase / cfg_amp [/ cfg_freq2 / cfg_amp2] / cfg_lines
 *   set go = 1;  wait for done to increment;  read tone_buf.
 *   cfg_amp2 == 0  =>  single tone only.
 */

#include "single_tone.h"
#include "vspa.h"

#define NS     2048                 /* samples per call: one 30 kHz tone period */
#define LINES  (NS * 4 / 128)       /* 64 lines x 128 B (32 samples each)       */

/* interleaved 16-bit I,Q — exactly the antenna-buffer wire format */
short tone_buf[NS * 2] __align_vec;

volatile unsigned int cfg_freq  = 0;   /* NCO word: 2^32 * f / fs */
volatile unsigned int cfg_phase = 0;   /* = offset_in_sframe / 4 (sample index) */
volatile unsigned int cfg_amp   = 0;   /* bit15:0 = I amp, bit31:16 = Q amp */
volatile unsigned int cfg_freq2 = 0;
volatile unsigned int cfg_amp2  = 0;
volatile unsigned int cfg_lines = LINES;
volatile unsigned int go        = 0;
volatile unsigned int done      = 0;

void main(void)
{
    while (1) {
        while (go == 0) { }

        single_tone_asm((void *)tone_buf, cfg_freq, cfg_phase,
                        cfg_amp, cfg_lines);

        if (cfg_amp2 != 0) {
            single_tone_add_asm((void *)tone_buf, (void *)tone_buf,
                                cfg_freq2, cfg_phase, cfg_amp2, cfg_lines);
        }

        go = 0;
        done = done + 1;
    }
}
