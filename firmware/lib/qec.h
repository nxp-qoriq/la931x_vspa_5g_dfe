// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef __QEC_H__
#define __QEC_H__
#include "pair_config.h"

#define QEC_MAX_FD_TAP_CNT     16
#define QEC_PARAMS_SIZE_BYTES  164 /*does NOT include state*/

/* Structure that can be used for QEC integration*/

typedef struct{
//	cfixed16_t  state[32]  	_VSPA_VECTOR_ALIGN;	
#if USING_OPTIMIZED_QEC == 0
    cfloat32_t	FD_taps[QEC_MAX_FD_TAP_CNT];
#endif
    uint32_t 	int_del;
    uint32_t 	FD_tap_cnt;
    float32_t 	f1;
    float32_t 	f2;
    float32_t   f4;
    cfloat32_t	g;	
    cfloat32_t	dc;	
	cfloat32_t	fegain_ori_backup;
}qec_params_t;



void qec_skew_6t_static_imb_asm(cfixed16_t *pOut,         /*pOut and pIn must be in different banks*/
							    cfixed16_t *pIn,
							    cfloat32_t *FD_taps,
							    cfixed16_t *state,
							    unsigned int line_cnt,    /*Must be multiple of 2 or will be rounded up to the nearest one*/
							    uint32_t int_del,		  /*Between 0 and 5*/ 
							    float32_t f1,
							    float32_t f2);

void qec_skew_7t_static_imb_asm(cfixed16_t *pOut,         /*pOut and pIn must be in different banks*/ 
							    cfixed16_t *pIn,
							    cfloat32_t *FD_taps,
							    cfixed16_t *state,
							    unsigned int line_cnt,    /*Must be multiple of 2 or will be rounded up to the nearest one*/
							    uint32_t int_del,		  /*Between 0 and 6*/ 
							    float32_t f1,
							    float32_t f2);

void qec_skew_Nt_static_imb_asm(cfixed16_t *pOut,         /*pOut and pIn must be in different banks*/ 
								cfixed16_t *pIn,
								cfloat32_t *FD_taps,
								cfixed16_t *state,
								unsigned int line_cnt,    /*Must be multiple of 2 or will be rounded up to the nearest one*/
								uint32_t int_del,         /*Between 0 and (FD_tap_cnt-1)*/ 
								uint32_t FD_tap_cnt,	  /*Between 8 and 16*/ 
								float32_t f1,
								float32_t f2);

void qec_static_imb_asm(cfixed16_t *pOut,                 /*pOut and pIn must be in different banks*/ 
						cfixed16_t *pIn,
						unsigned int line_cnt,            /*Must be multiple of 4 or will be rounded up to the nearest one*/
						float32_t f1,
						float32_t f4,
						float32_t f2);


void qec_lo_gain_asm(cfixed16_t *pOut,                    /*pOut and pIn must be in different banks*/ 
					 cfixed16_t *pIn,
					 unsigned int line_cnt, 
					 float32_t g_re, 
					 float32_t g_im,
					 float32_t dc_re, 
					 float32_t dc_im);
				   
#endif /* __QEC_H__ */
