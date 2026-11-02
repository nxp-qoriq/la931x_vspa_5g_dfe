// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __QEC_OPT_H__
#define __QEC_OPT_H__
	  				
//struct used for optimized QEC
//typedef struct{
//    float32_t 	iqimb0_f1;
//    float32_t 	iqimb0_f4;
//    float32_t 	pad00;
//    float32_t 	iqimb0_f2;
//    float32_t 	iqimb1_f1;
//    float32_t 	iqimb1_f4;
//    float32_t 	pad01;
//    float32_t 	iqimb1_f2;
//    float32_t 	iqimb2_f1;
//    float32_t 	iqimb2_f4;
//    float32_t 	pad02;
//    float32_t 	iqimb2_f2;
//    float32_t 	iqimb3_f1;
//    float32_t 	iqimb3_f4;
//    float32_t 	pad03;
//    float32_t 	iqimb3_f2;
//    cfloat32_t	fegain;
//    cfloat32_t	fegain_repeat;	
//    cfixed16_t	dcoff;
//	cfloat32_t	fegain_ori_backup;
//	float32_t	output_scaling_factor;
//	float32_t	pad04[8];     //to make the struct size to 1 line
//}qec_params_opt_t;
typedef struct{
    float32_t 	f1;
    float32_t 	f4;
    float32_t 	pad0;    //set to 0
    float32_t 	f2;
    float32_t	dcoff_I;
    float32_t	dcoff_Q;
    float32_t 	f1_backup;
    float32_t 	f4_backup;
    float32_t 	pad1;    //set to 0
    float32_t 	f2_backup;
	float32_t	pad2[22];     //to make the struct size to 1 line
}qec_params_opt_t;

//API for optimized QEC
//output: output buffer pointer, 128 bytes aligned,  half fixed
//input : input buffer  pointer, 128 bytes aligned,  half fixed. input buffer and output buffer can be the same buffer.
//core load 6% at 491Msps
void qec_opt_asm(cfixed16_t* output, cfixed16_t* input, unsigned int num_samples, qec_params_opt_t* qec_para);

//pow_acc: sample power accumulator buffer, 1 line.
//void qec_opt_pow_acc_asm(cfixed16_t* output, cfixed16_t* input, unsigned int num_samples, qec_params_opt_t* qec_para, void* pow_acc);

#endif /* __QEC_OPT_H__ */
