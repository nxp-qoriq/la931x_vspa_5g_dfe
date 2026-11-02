// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef X2_DECIM_FILTERS_H_
#define X2_DECIM_FILTERS_H_

#define NUM_X2_DECIM_TAPS						64

#define SIZE_X2_DECIM_TAP64FILTER_HISTORY		ALIGN(((NUM_X2_DECIM_TAPS-1)*4),128)
#define SIZE_X2_DECIM_TAP64FILTER_TAPS			(NUM_X2_DECIM_TAPS*4)

/****************************************************************************************************
 * API:         void X2_decim_tap64_filter(__fx16 *output , __fx16 *input, unsigned int num_samples, __fx16* history, float* filter_taps);
 * output:      output buffer, size is num_samples/2*4, 128 bytes aligned. output and input buffer can be at the same address
 * input:       input buffer, size is num_samples*4, 128 bytes aligned.
 * num_samples: num of input samples, must be at least 256 samples, must be multiple of 128 samples.
 * history:     dedicated buffer for history samples, size must be SIZE_X2_DECIM_TAP64FILTER_HISTORY, 128 bytes aligned
 * filter_taps: size must be SIZE_X2_DECIM_TAP64FILTER_TAPS, 128 bytes aligned
 * 
 * cycle count:
 * 4096 input samples costs 2073 cycles.  core load 20% at 245.76Msps
************************************************************************************************/

void X2_decim_tap64_filter(__fx16 *output , __fx16 *input, unsigned int num_samples, __fx16* history, float* filter_taps);

#endif
