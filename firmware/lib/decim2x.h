// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef DECIM2X_H_
#define DECIM2X_H_

#include "vspa.h"

#define MAX_FILTER_STATE_LENGTH   32 /* used by resampling kernels */
#define MAX_TAPS_LENGTH           32 /* used by resampling kernels */

#define INTERP1_TAPS      32 /* between 8 and 32, even number */
#define INTERP1_GAIN      3 
#define INTERP2_TAPS      32 /* between 8 and 32, even number */
#define INTERP2_GAIN      2

#define DECIM_TAPS 8
#define DECIM_GAIN 2

//typedef struct decim_2x_params_s{
//    float32_t *p_filt_h;
//    uint32_t n_h_taps;
//    cfixed16_t filtState[MAX_FILTER_STATE_LENGTH] _VSPA_VECTOR_ALIGN;
//} decim_2x_params_t;
//
//decim_2x_params_t decim_2x_params;
//
//typedef union decim_filt_s
//{
//	float32_t coeffs[MAX_TAPS_LENGTH];
//	uint32_t  word[MAX_TAPS_LENGTH];  
//} decim_filt_t;
//
//decim_filt_t decimFilt _VSPA_VECTOR_ALIGN = {
//		.word = 
//		{
//			#include "decim_coeffs.hex"	
//		}
//};
void decimator_2x_6_Taps_asm(cfixed16_t *pOut, cfixed16_t *pIn,
        float32_t *pTaps, cfixed16_t *filtState, unsigned int n_samples);
void decimator_2x_8_Taps_asm(cfixed16_t *pOut, cfixed16_t *pIn,
        float32_t *pTaps, cfixed16_t *filtState, unsigned int n_samples);
void decimator_4x_8_Taps_asm(cfixed16_t *pOut, cfixed16_t *pIn,
        float32_t *pTaps, cfixed16_t *filtState, unsigned int n_samples);
void gen_decx2_NTaps_align_asm(cfixed16_t *pOut, cfixed16_t *pIn,
        float32_t *pTaps, cfixed16_t *pFiltState, uint32_t NTaps, uint32_t N);      //N: in samples



void decim_2x_handler(cfixed16_t *pOut, cfixed16_t *pIn, uint32_t nTaps, float32_t *pTaps, cfixed16_t *filtState, unsigned int n_samples) 
{
	
	if(nTaps == 6)
		decimator_2x_6_Taps_asm(pOut, pIn, pTaps, filtState, n_samples);
	else if (nTaps == 8) 
		decimator_2x_8_Taps_asm(pOut, pIn, pTaps, filtState, n_samples);
	else
		gen_decx2_NTaps_align_asm(pOut, pIn, pTaps, filtState, nTaps, n_samples);
}


//void make_decim_2x_params(  )  __attribute__((noinline))
//{
//	
//	for(int i=0; i<DECIM_TAPS; i++)
//		decimFilt.coeffs[i] = DECIM_GAIN*decimFilt.coeffs[i];
//	
//    memset(decim_2x_params.filtState, 0, MAX_FILTER_STATE_LENGTH * sizeof(cfixed16_t));     //zeroing history buffer
//    decim_2x_params.n_h_taps = DECIM_TAPS;
//    decim_2x_params.p_filt_h = decimFilt.coeffs;
//
//    return ;
//}


#endif /* DECIM2X_H_ */
