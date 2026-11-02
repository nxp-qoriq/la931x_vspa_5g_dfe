// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP



#ifndef FFT_CP_H_
#define FFT_CP_H_

void start_ippu_ifft_bitrev(unsigned int ipOut, unsigned int vpIn, unsigned int ipOutbase, unsigned int circ_size, unsigned int vpFlag);
void start_ippu_tx_bitrev_cp_insert(unsigned int ipOut, unsigned int vpIn, unsigned int ipOutbase, unsigned int circ_size, unsigned int nL_cp);

void fft_cp_remove( uint32_t *ippu_sym_out, 
					uint32_t *vcpu_sym_in, 
					uint32_t fft_size,	
					uint32_t cp_samples,
					uint32_t circ_base,
					uint32_t circ_size,
					cfixed16_t *vcpu_scratch_buff);
					//cfixed16_t *ippu_scratch_buff);
		
enum cp_samples_t {
	CP_NONE = 0,
	CP_NORMAL = 288,
	CP_LONG_MU0 = 320,
	CP_LONG_MU1 = 352,
	CP_LONG_MU2 = 416,
	CP_LONG_MU3 = 544
};

#endif /* FFT_CP_H_ */
