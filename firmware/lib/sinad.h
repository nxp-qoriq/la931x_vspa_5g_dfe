// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __MAIN_H__
#define __MAIN_H__

typedef struct {
	unsigned int start_point	;	//start location in spectrum
	unsigned int end_point		;
	//unsigned int n_sample_ignored;
	//unsigned int DC_bins 		= 10; 	//skip DC bins
	unsigned int f_start	;		
	unsigned int f_len	; 	//valid bins before/after tone location
} snr_params_t;

//snr_params_t snr_params={1,2,3,4};


//void SINAD_test(float* sinad, cfixed16_t* input_samples, unsigned int start_point, unsigned int end_point, unsigned int n_sample_ignored, void* vcpu_scratch, void* ippu_scratch);
//scratch buffer size:
//VCPU DMEM 64KB
//IPPU DEME   96KB
void SINAD_test(float* sinad, cfixed16_t* input_samples, snr_params_t* snr_para, void* vcpu_scratch, void* ippu_scratch);


#endif
