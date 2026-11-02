// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef ECPRI_COMPRESSION_DECOMPRESSION_H_
#define ECPRI_COMPRESSION_DECOMPRESSION_H_

#define VERSION_ECPRI_COMP_DECOMP	0x210

#define HALF_FIXED				0
#define HALF					1

//size of scratch buffer, users must define scratch buffers on VCPU and IPPU and pass the buffer pointer to ecpri_compression/decompression_para
#define NUM_BYTE_SCRATCH_VCPU			(64*28*4 + 64*2*56)
#define NUM_BYTE_SCRATCH_IPPU			(64*48*2 + 64*28*4 + 160)

typedef struct ecpri_compression_para
{
	unsigned int input_scale_down_nbit;				//all input scaled down by n bit, possible value 0,1,2,3,4,5,6,7. After scaled down the input data must be in the range of [-1,1)
	void*        scratch_buffer_vcpu;				//must be 128 byte aligned, must be on VCPU DMEM, size must be NUM_BYTE_SCRATCH_VCPU
	void*        scratch_buffer_ippu;				//must be 128 byte aligned, must be on IPPU DMEM, size must be NUM_BYTE_SCRATCH_IPPU
	unsigned int num_prb; 							//number of PRB in a symbol, one PRB contains 12 samples
													//only 273 and 264 are supported. for other number of PRB, use 273 or 264
													//by clearing the last PRBs to 0.
} s_ecpri_compression_para;

typedef struct ecpri_decompression_para
{
	void*        scratch_buffer_vcpu;				//must be 128 byte aligned, must be on VCPU DMEM, size must be NUM_BYTE_SCRATCH_VCPU
	void*        scratch_buffer_ippu;				//must be 128 byte aligned, must be on IPPU DMEM, size must be NUM_BYTE_SCRATCH_IPPU
	unsigned int output_scale_down_nbit;			//all output scaled down by n bit, possible value 0,1,2,3,4,5,6,7
	unsigned int output_data_type;					//HALF_FIXED or HALF;
	unsigned int num_prb; 							//number of PRB in a symbol, one PRB contains 12 samples
													//only 273 and 264 are supported. for other number of PRB, use 273 or 264
													//by clearing the last PRBs to 0.
} s_ecpri_decompression_para;

//NOTE: compression input data type can be half_fixed or half, see release note for more info. default configuraiton is half_fixed
//      decompression output data type can be half_fixed or half, configured at runtime using above para struct at init stage.

/**********************************************************************************************
 *   void ecpri_compression_init(struct ecpri_compression_para *para);
 *   void ecpri_decompression_init(struct ecpri_decompression_para *para);
 *   
 * Function: initialize the data buffers and control information for the compression/decompression functions.
 *           
 * para:    commpression/decompression parameters, see the structure definition.
 * return value:  version number of the kernel package
 * 
 * call this function just once at the init stage.
 **********************************************************************************************/
int ecpri_compression_init(struct ecpri_compression_para *para);
int ecpri_decompression_init(struct ecpri_decompression_para *para);

/**********************************************************************************************
 *   void ecpri_compression_symbol(void* out_buf, void* in_buf);
 *   
 * Function: compress specified num of PRB from 16 bit to 9 bit, using block floating method
 *           parameters that are initialized by above init functions.
 * in_buf  : input buffer, 128 bytes aligned, must on IPPU DMEM
 * out_buf : output buffer, 128 bytes aligned, must on IPPU DMEM
 * Performance: compress 3276 sub-carrier symbol costs 5.7 us.
 **********************************************************************************************/
void ecpri_compression_symbol(void* out_buf, void* in_buf);

/**********************************************************************************************
 *   void ecpri_decompression_symbol(void* out_buf, void* in_buf);
 *   
 * Function: decompress specified num of PRB from 9 bit to 16 bit, using block floating method
 *           parameters that are initialized by above init functions.
 * in_buf  : input buffer, 128 bytes aligned, must on IPPU DMEM
 * out_buf : output buffer, 128 bytes aligned, must on IPPU DMEM
 * Performance: compress 3276 sub-carrier symbol costs 5.7 us.
 **********************************************************************************************/
void ecpri_decompression_symbol(void* out_buf, void* in_buf);


/***********************************************************************************************
Example of using this lib
1. Define scratch buffers on VCPU and IPPU:
#include "ecpri_compression_decompression.h"
char scratch_vcpu[NUM_BYTE_SCRATCH_VCPU] __attribute__ ((aligned (128))) ;
char scratch_ippu[NUM_BYTE_SCRATCH_IPPU] __attribute__ ((aligned (128))) __attribute__ ((section (".ibss"))) ;
2. One-time initialization
For compression:
	struct ecpri_compression_para 	para_comp;

	para_comp.input_scale_down_nbit = 0;
	para_comp.scratch_buffer_vcpu = scratch_vcpu;
	para_comp.scratch_buffer_ippu = scratch_ippu;
	para_comp.num_prb = 273;
	ecpri_compression_init(&para_comp);
	
For decompression
	struct ecpri_decompression_para para_decomp;
	
	para_decomp.scratch_buffer_vcpu = scratch_vcpu;
	para_decomp.scratch_buffer_ippu = scratch_ippu;
	para_decomp.output_scale_down_nbit = 0;
	para_decomp.output_data_type = HALF_FIXED;
	para_decomp.num_prb = 273;
	ecpri_decompression_init(&para_decomp);

3. call compression/decompression at runtime
	ecpri_compression_symbol(sym_buf_out, sym_buf_in);  //sym_buf_in and sym_buf_out can be different or same address, note that input and output size are different
	ecpri_decompression_symbol(sym_buf_out, sym_buf_in);//sym_buf_in and sym_buf_out can be different or same address, note that input and output size are different
***********************************************************************************************/

#endif /* ECPRI_COMPRESSION_DECOMPRESSION_H_ */
