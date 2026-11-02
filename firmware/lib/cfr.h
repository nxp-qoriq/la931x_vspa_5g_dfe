// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP



#ifndef CFR_H_
#define CFR_H_

/*
 * CFR VSPA3 KERNEL
 * 
 * Version history:
 * V1.3 Jun 28,2022 1. added NUM_HISTORY_SAMPLES define, changed history to status to avoid misunderstanding. History samples are in the circular buffer, 
 * 					the pointer named history in v1.2 is actually status only.
 * 					User should make sure  NUM_HISTORY_SAMPLES+1K_samples+NUM_ADVANCED_SAMPLES are ready before calling cfr to process the 1K samples.
 * 					all the 3 parts must be 128 bytes aligned.
 * 					2. added support for cpulse length of 129 and 65
 * 
 * V1.2 Jan 24,2022 1. bug fix: added push/pop at the beginning and end of code
 *                  2. NUM_ADVANCED_SAMPLES is enlarged by 8 samples as 8x peak may be at 1 or 2 samples on the right of 1x peak which needs more advance
 * 
 * V1.1 2021Nov18. 	1. Updated peak search strategy, searching 1 max peak in every 128 samples, and search 4 max values from the 8 max peaks.
 * 						See design doc for detail of the algorithm.
 * 					2. Bug fix.
 * 					
 * V1.0 2021Nov04. 	First version
 * 
 * CAUTION:   		All the values defined in this header file MUST NOT be changed. Other values are not tested thus may not work.
 * SPECIAL NOTE:  	The Filter Coefficients NEED special data structure, see below for detail.
 * 
 * The kernel just support the following configurations:
1.	1024 samples peak cancellation each call
2.	Cancellation of 4 peaks (1 max peak every 256 samples) each call.
3.	Sub-sampling filter can be optional.
4.	8x interpolation window 32 samples
5.	Configurable Skip and Threshold for each call.  Skip must be even number between 2-62.
6.  All filter coefficients can be changed for each call.
 * 
 */

#define CFR_BLOCK_SIZE				1024	//must not be changed
#define LEN_CPULSE					257		//Can be 385 or 257 or 129 or 65 

#if(LEN_CPULSE != 65 && LEN_CPULSE != 129 && LEN_CPULSE != 257 && LEN_CPULSE != 385)
ERROR
#endif

//this is the number of samples that must be available after a block before calling CFR to process the block,
//CFR needs these advanced samples.
#define NUM_ADVANCED_SAMPLES		(LEN_CPULSE/2+32)  // this is the minimum advanced samples needed (aligned to 128 bytes)
#define NUM_HISTORY_SAMPLES			NUM_ADVANCED_SAMPLES

//CFR PARAMETER BUFFER size. __fp16 coeff[DPD_COEFF_SIZE_BYTES/2]; data struct:
// 	16 taps of FIR filter coefficient. Note for the order.  				half float.
//  20+8+8 taps of 8x filter coefficient. Must be in interleaved order.  	half float, offset = 16.
//      1st 2x:  e0,o0,e1,o1,...,e9,o9,  Followed by
//		2nd 2x:  e0,o0,e1,o1,...,e3,o3,  Followed by
//		3rd 2x:  e0,o0,e1,o1,...,e3,o3.  
//		NOTE: e0-e9 are coefficients for even number output, o0-o9 are for odd numbered output
//		Refer to CFR design document algorithm part, interleave h_2x_0(k) and h_2x_1(k) to get the required format, same for 
//			h_4x_0/1 and h_8x_0.
//  1 Threshold for peak search power level.  								half float, offset = 52.
//  1 reserve
//  1 Threshold for cancellation weight.  									half float. offset = 54.
//  1 reserve
//  1 Nskip. 																short.		offset = 56.

//  total 57 elements.
#define CFR_PARA_SIZE_BYTES			(1*128)

//status buffer size. struct:
//Line 0: 1 peak power of last sample, located at offset 62
//Line 1: 1 peak flag of last sample, located at offset 62
//Line 2: 1 last peak location
#define CFR_STATUS_SIZE_BYTES	(128*3)   

//scratch buffer size, struct:
//1 line for FIR output for previous block
//32 lines for FIR output for current block, if FIR is called.  No this part if FIR is not called
//1 line for FIR output for next block
//1 line for power for previous block
//16  lines for power for current block
#define CFR_SCRATCH_SIZE_BYTES	(128+CFR_BLOCK_SIZE*4+128 + 128 + CFR_BLOCK_SIZE*2)


#ifndef CFR_H_DEF_ONLY
typedef struct struct_cfr_para_s
{
	__fp16 coef_fir[16];
	__fp16 coef_interp8x[20+8+8];   //must be in interleaved order
	__fp16 threshold_p;		//threshold for peak search, only the peak samples with power higher than threshold can be picked for cancellation
	__fp16 reserved1;
	__fp16 threshold_c;		//threshold for peak-canceled power, the new peak power will be no higher than this threshold.
	__fp16 reserved2;
	short Nskip;			//can be any integer.
} struct_cfr_para;
typedef struct struct_stat_s
{
	__fp16	peak_flag[64];
	short	max_1xpeak_loc[8];
	__fp16	max_1xpeak_pow[8];
	short	max_8xpeak_loc[8];
	__fp16	canncel_w[4][2];
} struct_stat;
typedef struct struct_cpulse_s
{
	__fp16	zero_pad0[32];		//must be set to 0's
	__fp16	cpulse[LEN_CPULSE];
	__fp16	zero_pad1[31];		//must be set to 0's
} struct_cpulse;

//int factor_win_base[4]__attribute__ ((aligned (128))) = {0x00800000,0x01800100,0x02800200,0x03800300};
#endif

#define PARA_OFS_8xTAPS			16
#define PARA_OFS_THRES_P		52
#define PARA_OFS_THRES_C		54
#define PARA_OFS_NSKIP			56

#define HIST_STAT_OFS_PEAK_FLAG				0
#define HIST_STAT_OFS_MAX_PEAK_LOC			(64*2+HIST_STAT_OFS_PEAK_FLAG)
#define HIST_STAT_OFS_MAX_PEAK_POW			(8*2+HIST_STAT_OFS_MAX_PEAK_LOC)
#define HIST_STAT_OFS_MAX_8XPEAK_LOC		(8*2+HIST_STAT_OFS_MAX_PEAK_POW)
#define HIST_STAT_OFS_CANCEL_W				(4*4+HIST_STAT_OFS_MAX_8XPEAK_LOC)
		


/* 
 * inout :  input/output buffer pointer, 128 bytes aligned.  pointing to a block of 1024 samples in side the circular buffer
 * para  :  parameter buffer pointer, 128 bytes aligned. must be declared by struct_cfr_para and configure all the parameters before calling the kernel
 * status:  status buffer pointer, 128 bytes aligned, size must be defined as CFR_STATUS_SIZE_BYTES. the buffer must be defined
 * 			as global and can not be used for other purpose.
 * scratch: scratch buffer pointer, 128 bytes aligned. size must be defined as CFR_SCRATCH_SIZE_BYTES. value doesn't matter. Can be used for 
 * 			other purpose after CFR kernel returns.
 * cpulse:  cancellation pulses pointer. 128 bytes aligned. must be declared by struct_cpulse.
 * 			If cpulse buffer is put on different DMEM banks from inout buffer, a slightly less core load can be achieved (about 0.5%)
 * circular_base: the base address of the input/output buffer.
 * circular_size_byte: size of the input/output circular buffer.
 */
#ifndef CFR_H_DEF_ONLY
void cfr_with_fir(	__fx16* 			inout, 
					struct_cfr_para* 	para, 
					struct_stat*		status, 
					__fp16*				scratch, 
					struct_cpulse*		cpulse, 
					__fx16* 			circular_base, 
					unsigned int 		circular_size_byte);

void cfr(	__fx16* 			inout, 
			struct_cfr_para* 	para, 
			struct_stat*		status, 
			__fp16* 			scratch, 
			struct_cpulse* 		cpulse,
			__fx16* 			circular_base,
			unsigned int 		circular_size_byte);
#endif

/*
 The 2 APIs have the same interface and parameters, but with different function name. One is used when sub-sampling FIR filter is used, another is used when sub-sampling FIR filter is not used.
For detail of the parameters refer to the cfr.h head file.
Each calling of the API will do peak cancellation on a 1024 sample window starting from the pointer inout passed to the API.

Suggested use of the API:
Input samples circular buffer size 3K samples(12KB): __fx16 x[3][1024][2];  initialize the last 32 samples (1 line) to 0, which will be used as history data for current block.
After initialization, do the following to call the API for the first time.
1.	Prepare input samples from x[0][0] �x[1][255]
2.	Call API with inout=x[0][0];
3.	X[0][0]-x[0][767] are output that can be sent to next module for processing. x[768][1023] is within the cancellation window of next block and may be further peak cancelled if the peaks in the next block is at the beginning part of the block.
After the first time, do the following to call the API each time.
4.	Prepare input samples from x[1][256] �x[2][255]
5.	Call API with inout=x[1][0];
6.	X[0][768]-x[1][767] are output that can be sent to next module for processing.
7.	Prepare input samples from x[2][256] �x[0][255]
8.	Call API with inout=x[2][0];
9.	X[1][768]-x[2][767] are output that can be sent to next module for processing.
10.	Prepare input samples from x[0][256] �x[1][255]
11.	Call API with inout=x[0][0];
12.	X[2][768]-x[0][767] are output that can be sent to next module for processing.
13.	And so on.
The kernel can be called multiple times to cancel peaks multiple times in the same block, like below to cancel 3 times:
14.	Prepare input samples from x[1][256] �x[2][255]
15.	Call API with inout=x[1][0];
16.	Call API with inout=x[1][0];
17.	Call API with inout=x[1][0];
18.	X[0][768]-x[1][767] are output that can be sent to next module for processing.

Performance of the kernel.
With sub-sampling filter:
cycle count: 849, input samples: 1024, cycle per samples: 0.829102,  core load for sampling rate 122.88Msps: 0.165820
Without sub-sampling filter:
cycle count: 561, input samples: 1024, cycle per samples: 0.547852,  core load for sampling rate 122.88Msps: 0.109570
    
 */
#endif /* CFR_H_ */
