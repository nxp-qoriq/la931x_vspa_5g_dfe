// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#include "pair_config.h"
#include "fft_cp.h"
#include "fft_dif.h"
#include "diffft.h"
#include "Ant_TxRx_Buffer_manager.h"
#include "txWindowing.h"
#include "bitRev.h"
#include "phase_compensation.h"
#include "cell_tracking.h"
#include "vector.h"
#include "chEst.h"
#include "sssConvert.h"
#include "fineCFO.h"
#include "timing_estimator.h"
#include "host_vspa_msg.h"
#include "global_var.h"

extern void bitRev_fft_4096_b1_b1_copy_dst_to_src( void );

#if POINTS_FFT == 512
#define cell_tracking_scratch_buf	src_addr
#endif

unsigned char rx_pattern_sym_idx;
unsigned int rx_sym_type;   //0 idle, 1 tx sym

typedef struct pss_buf_struct_s
{
	cfixed16_t pss_input_res[NR_SSB_NFFT];		//1024B  
	cfixed16_t pss_csi_out[NR_PSS_NFFT];		//1024B
	union
	{
		struct
		{
			int		pad[256];
			cfloat16_t pssRef256[NR_PSS_NFFT] ;		//1024B
		};
		cfloat32_t pss_eqw[NR_PSS_NFFT];		//2048B
	};
} pss_buf_struct;


typedef struct sss_buf_struct_s
{
	union
	{
		cfixed16_t sss_input_res[NR_SSB_NFFT];	//1024B
		cfixed16_t sss_csi[NR_SSS_NFFT];
		struct slotted_ssb_det_t ssb_det;
	};
	union
	{
		cfloat32_t pss_eqw[NR_PSS_NFFT];		//2048B pss_eqw first half
		struct
		{
			cfloat16_t sss_eq[NR_SSS_NFFT];			//1024B	
			fixed16_t sss_seq[128];
		};
		struct
		{
			cfloat16_t sss_conv_out[NR_SSB_NFFT];	//1024B	
			cfixed16_t pss_csi_out[NR_PSS_NFFT];	//1024B
		};
	};
	uint16_t  SSSDET_REF_D1[ 18 ] __attribute__((aligned( 32 * UPHW )));	// 36B    	
	uint16_t  SSSDET_REF_D0[ 72 ] __attribute__((aligned( 4 * UPHW )));  	// 144B
	uint32_t  ATAN2_COEFF_BUFF[2 * ATAN2_NUM_COEFF];						// 32B
	sssdet_param_T sssdet_param __attribute__((aligned(2 * UPHW))) ;
	uint32_t  pad[(256+32)/4];
	fixed16_t scratch_512B[256];
} sss_buf_struct;

#define SIZE_SSB_DET	256
#define SIZE_PSS_EQW	2048
#define SIZE_PSS_CSI	1024
#define SIZE_SSB_PARAMS	256
#define SIZE_PSS_REF	1024
typedef struct celltrack_extbuf_s
{
	union
	{
		struct slotted_ssb_det_t ext_ssb_det;	//final output
		unsigned int ssb_det[256/4];		//align to 256
	};
	unsigned int pss_csi_out[NR_SSS_NFFT];	//1024 bytes PSS CSI
	unsigned int pss_eqw[NR_SSS_NFFT*2];	//2048 bytes PSS EQW
	union
	{
		struct
		{
			uint16_t  SSSDET_REF_D1[ 18 ] __attribute__((aligned( 32 * UPHW )));	// 36B    	
			uint16_t  SSSDET_REF_D0[ 72 ] __attribute__((aligned( 4 * UPHW )));  	// 144B
			uint32_t  ATAN2_COEFF_BUFF[2 * ATAN2_NUM_COEFF];						// 32B
			sssdet_param_T sssdet_param __attribute__((aligned(2 * UPHW))) ;
		};
		unsigned int ssb_params[256/4];		//align to 256
	};
	unsigned int pssRef256[3][NR_SSS_NFFT];		//3*1024 bytes PSS Ref
} struct_celltrack_extbuf;


#if(NUM_1R_IN_CORE)
extern cfixed16_t sym_queue_tx[TX_SYM_SIZE/4];
extern struct_dfe_ctrl dfe_ctrl;
extern volatile uint32_t rx_bitrev_arg[MAX_NUM_IPPU_ARG];
extern struct_antman_ctrl_rx antman_ctrl_rx;

unsigned int axi_flag_rx_symbol_ready[16]__attribute__ ((aligned (AXI_BURST_SIZE)));

                                                             
unsigned int convert_2implement(unsigned int data)
{
	if(data > 0x80000000)
		data = ((~data)+1) | 0x80000000;
	else
		data &= 0x7FFFFFFF;
	return data;
}

void pipeline_reset_rx()
{
	//align released size to produced size for a new TX window
	dfe_ctrl.rx_circ_total_released_size = dfe_ctrl.rx_circ_total_produced_size;
	dfe_ctrl.read_offset_fft_in = 0;

	//dynamic timing offset compensation.
	dfe_ctrl.rx_timing_offset += dfe_ctrl.rx_timing_offset_total; //for start of FDD and start of each TDD window. pick up offset from previous window.
	dfe_ctrl.rx_timing_offset_total = 0;
	
	//clear RX circ buffer
	vecclr((void*)FFTin, FFT_CIRC_SIZE/128);
}
void set_rx_slot_sym_idx(unsigned int idx)
{
	unsigned int rx_num_sym_in_buff = dfe_ctrl.rx_dumping_num_sym_in_buff;
	unsigned int rx_sym_idx_per_sframe = dfe_ctrl.rx_sym_idx_per_sframe;
	
	if(idx >= rx_num_sym_in_buff)
	{
		for(unsigned int i=0;i<NUM_1R_IN_CORE;i++)
		{
			if(dfe_ctrl.rx_sym_dumping_flag[i] & DUMPING_FLAG_DUMPING)
				dfe_ctrl.rx_sym_dumping_flag[i] = 0;
		}
	}
	
	if(idx >= NUM_SYM_PER_SYSPERIOD)
		idx -= NUM_SYM_PER_SYSPERIOD;
	
	if(idx < rx_sym_idx_per_sframe)
	{
		for(unsigned int i=0;i<NUM_1R_IN_CORE;i++)
		{
			dfe_ctrl.rx_sym_dumping_flag[i] = dfe_ctrl.rx_sym_dumping_flag[i] << 1;
		}
	}
	dfe_ctrl.rx_sym_idx_per_sframe 	= idx;
}


#if(!BYPASS_FFT)
#if RX_SYM_QUEUE_BANK
cfixed16_t sym_queue_rx[RX_SYM_SIZE/4] __align_vec __attribute__ ((section (".ibss1"))) ;
#if FDD_1T2R
cfixed16_t sym_queue_rx2nd[RX_SYM_SIZE/4] __align_vec __attribute__ ((section (".ibss1"))) ;
#endif
#else
cfixed16_t sym_queue_rx[RX_SYM_SIZE/4] __align_vec;// __attribute__ ((section (".ibss"))) ;
#if FDD_1T2R
cfixed16_t sym_queue_rx2nd[RX_SYM_SIZE/4] __align_vec;
#endif
#endif
#endif


void start_rx_ippu_bitrev_reversIn_linearOut(unsigned int pOut, unsigned int pIn, unsigned int pOutbase, unsigned int circ_size, unsigned int num_samples, unsigned int sample_offset, unsigned int pflag_addr, unsigned int pflag_value)
{
	ippu_proc_t ippu_func;
	#if RX_SYM_QUEUE_BANK == 1
	{
		rx_bitrev_arg[0] = ((pOut-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO) & (0xFFFFFFC0); //align output to start of first line
		rx_bitrev_arg[1] = (pIn-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
		rx_bitrev_arg[2] = (pOutbase-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
#if POINTS_FFT == 512
		ippu_func = (ippu_proc_t)bitRev512_reverseIn_linearOut_vspa2_b1_b1;
#elif  POINTS_FFT == 1024
		ippu_func = (ippu_proc_t)bitRev1024_reverseIn_linearOut_vspa2_b1_b1;
#elif  POINTS_FFT == 2048
		ippu_func = (ippu_proc_t)bitRev2048_reverseIn_linearOut_vspa2_b1_b1;
#endif
	}
	#else
	{
		#if RX_CIRC_BANK == 0
		rx_bitrev_arg[0] = ((pOut)/IPPU_ADDR_RATIO) & (0xFFFFFFC0); //align output to start of first line
		rx_bitrev_arg[1] = (pIn)/IPPU_ADDR_RATIO;
		rx_bitrev_arg[2] = (pOutbase)/IPPU_ADDR_RATIO;
#if POINTS_FFT == 512
		ippu_func = (ippu_proc_t)bitRev512_reverseIn_linearOut_vspa2_b0_b0;
#elif  POINTS_FFT == 1024
		ippu_func = (ippu_proc_t)bitRev1024_reverseIn_linearOut_vspa2_b0_b0;
#elif  POINTS_FFT == 2048
		ippu_func = (ippu_proc_t)bitRev2048_reverseIn_linearOut_vspa2_b0_b0;
#endif
		#else
		#error TX_SYM_QUEUE_BANK not supported
		#endif
	}
	#endif
	rx_bitrev_arg[3] = BYTE2UNIT(circ_size)/IPPU_ADDR_RATIO;		// i/p circular buffer size

	
	unsigned int num_total_hwords = num_samples*2;
	unsigned int starting_point_first_line = (pOut/IPPU_ADDR_RATIO)&0x3F;
	unsigned int num_1st_line_hwords = 64-starting_point_first_line;
	unsigned int num_rest_lines = (num_total_hwords - num_1st_line_hwords + 63)/64;
	rx_bitrev_arg[4] = (starting_point_first_line)*4;	// starting point of first sample, 4-bit index
	rx_bitrev_arg[5] = num_1st_line_hwords/2 - 1;		// num of samples minus 1 of the unaligned samples in the first line
	rx_bitrev_arg[6] = sample_offset*2;				// offset from pIn, hwords. pointing to CP part
	rx_bitrev_arg[7] = num_rest_lines - 1;				// num of total lines (unaligned+aligned) minus 2.
	rx_bitrev_arg[8] = (pflag_addr-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;		// after IPPU done, value in this addr will be cleared, addr must be in VCPU
	rx_bitrev_arg[9] = (pflag_value-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
	
	ippu_arg_base(((uint32_t)rx_bitrev_arg - IPPU_ARG_OFFSET)/IPPU_ADDR_RATIO);
	ippu_enable(ippu_func);
	__nop(); //avoid issue in case the next instruction is checking IPPU status register
}


__inline__ void start_ippu_bitRev_reverseIn_linearOut_CopyDestToSrc_vspa2(unsigned int pOut, unsigned int pIn, unsigned int pOutbase, unsigned int circ_size, unsigned int num_sc, unsigned int pFlag_addr, unsigned int pFlag_value)
{
	ippu_proc_t ippu_func;
#if RX_CIRC_BANK == 1
	{
		rx_bitrev_arg[0] = (pOut-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
		rx_bitrev_arg[2] = (pOutbase-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
		#if RX_SYM_QUEUE_BANK == 1
		{
			rx_bitrev_arg[1] = (pIn-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
#if POINTS_FFT == 1024
			ippu_func = (ippu_proc_t)fft_bitRev1024_vspa2_b1_b1;
#elif POINTS_FFT == 512
			ippu_func = (ippu_proc_t)fft_bitRev512_vspa2_b1_b1;
#endif
		}
		#else
		{
			rx_bitrev_arg[1] = (pIn)/IPPU_ADDR_RATIO;
#if POINTS_FFT == 1024
			ippu_func = (ippu_proc_t)fft_bitRev1024_vspa2_b0_b1;
#elif POINTS_FFT == 512
			ippu_func = (ippu_proc_t)fft_bitRev512_vspa2_b0_b1;
#endif
		}
		#endif
	}
#else
	{
		rx_bitrev_arg[0] = (pOut)/IPPU_ADDR_RATIO;
		rx_bitrev_arg[2] = (pOutbase)/IPPU_ADDR_RATIO;
		#if RX_SYM_QUEUE_BANK == 1
		{
			rx_bitrev_arg[1] = (pIn-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
#if POINTS_FFT == 1024
			ippu_func = (ippu_proc_t)fft_bitRev1024_vspa2_b1_b0;
#elif POINTS_FFT == 512
			ippu_func = (ippu_proc_t)fft_bitRev512_vspa2_b1_b0;
#endif
			
		}
		#else
		{
			rx_bitrev_arg[1] = (pIn)/IPPU_ADDR_RATIO;
#if POINTS_FFT == 1024
			ippu_func = (ippu_proc_t)fft_bitRev1024_vspa2_b0_b0;
#elif POINTS_FFT == 512
			ippu_func = (ippu_proc_t)fft_bitRev512_vspa2_b0_b0;
#endif
		}
		#endif
	}
#endif
	
	rx_bitrev_arg[3] = BYTE2UNIT(circ_size)/IPPU_ADDR_RATIO;	// i/p circular buffer size
	rx_bitrev_arg[4] = ((num_sc/2*4)+127)/128*2-2;//20-2;  //max 20 lines for 10Mhz scs15 and 20Mhz scs30. ippu requires num of lines minus 2.
	rx_bitrev_arg[5] = num_sc;//20*32;
	rx_bitrev_arg[6] = (pFlag_addr-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
	rx_bitrev_arg[7] = (pFlag_value-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
	
	ippu_arg_base(((uint32_t)rx_bitrev_arg - IPPU_ARG_OFFSET)/IPPU_ADDR_RATIO);
	ippu_enable(ippu_func); //, IPPU_PEND_NONE|IPPU_TRIG_DMAC);
	__nop(); //avoid issue in case the next instruction is checking IPPU status register
}

unsigned int get_rx_dump_sym_buf_addr(unsigned int rx_sym_idx_per_sframe, unsigned int rid)
{
	unsigned int addr;
	unsigned int rx_dump_sym_buf_base = dfe_ctrl.rx_dumping_sym_buf_base[rid];
	unsigned int sym_idx_in_buffer = rx_sym_idx_per_sframe % dfe_ctrl.rx_dumping_num_sym_in_buff;
	if(RX_OPTION8_CFG)
	{
		unsigned int num_lcp = (sym_idx_in_buffer + (NUM_SYM_PER_CP_PERIOD - 1))  / NUM_SYM_PER_CP_PERIOD;
		unsigned int size = num_lcp * SIZE_SYMBOL_DATA_TIME_DOMAIN_LCP + (sym_idx_in_buffer-num_lcp)*SIZE_SYMBOL_DATA_TIME_DOMAIN_SCP;
		addr = size + rx_dump_sym_buf_base;
	}
	else
		addr = rx_dump_sym_buf_base + sym_idx_in_buffer * dfe_ctrl.rx_dumping_sym_buff_size;
	return addr;
}

unsigned int get_valid_size_for_fft_in()
{
	unsigned int valid_size = dfe_ctrl.rx_circ_total_produced_size - dfe_ctrl.rx_circ_total_released_size;
	if(valid_size <= FFT_CIRC_SIZE)
		return valid_size;
	else
		return 0;              //in case timing offset compensation, the last block of TDD window may release more data, causing valid size bigger than circ size.
}


#if(!BYPASS_FFT)
//return value: 1-fft is run, 0-fft is not run
unsigned int run_fft()
{
	unsigned int FFTout_valid = dfe_ctrl.FFTout_valid;
	unsigned int size_fft_in_valid = get_valid_size_for_fft_in();
	unsigned short num_sc_in_a_sym = GET_NUM_SC;
	unsigned int rx_sym_idx_per_sframe = dfe_ctrl.rx_sym_idx_per_sframe;
	unsigned int status = 0;
	

	if((FFTout_valid & (BIT_MASK_FFT_OUT_VALID|BIT_MASK_FFT_OUT_1ST_HALF_READY|BIT_MASK_FFT_OUT_2ND_HALF_READY)) == 0)
	{
		unsigned int CP_size = get_cp_size(rx_sym_idx_per_sframe % NUM_SYM_PER_CP_PERIOD);
		unsigned int copy_size = POINTS_FFT*4/2;	//copy half of FFT size from circ buffer to FFTout buffer

		signed int rx_timing_offset = dfe_ctrl.rx_timing_offset;
		signed int rx_timing_offset_exe = 0;
		if((rx_sym_idx_per_sframe % NUM_SYM_PER_SLOT) == 0)
		{
			CP_size = CP_size + rx_timing_offset*4;
			rx_timing_offset_exe = rx_timing_offset;
			rx_timing_offset = 0;
		}
		
		if(size_fft_in_valid >= (copy_size+CP_size))
		{
			dfe_ctrl.rx_timing_offset_total += rx_timing_offset_exe;
			dfe_ctrl.rx_timing_offset = rx_timing_offset;
			
			dfe_ctrl.FFTout_valid = BIT_MASK_FFT_OUT_1ST_HALF_READY;
		
			unsigned int read_offset_fft_in = dfe_ctrl.read_offset_fft_in;
			unsigned int* ptrcirc_fft_in = FFTin + ((read_offset_fft_in+CP_size+FFT_CIRC_SIZE)%FFT_CIRC_SIZE)/4;
			dfe_ctrl.rx_circ_total_released_size += (copy_size + CP_size);
			dfe_ctrl.read_offset_fft_in = (read_offset_fft_in + copy_size + CP_size) % FFT_CIRC_SIZE;
			memcpy_src_circ(sym_queue_rx, ptrcirc_fft_in, copy_size, FFTin, FFT_CIRC_SIZE);
#if NUM_1R_IN_CORE == 2
			if(__ip_read(IP_IDX_DFE_MODE_LO, DFE_MODE_RX_ANT_MASK(1)) != DFE_MODE_RX_ANT_MASK(1)) //share TX CIRC buffer for 2nd RX
#if FDD_1T2R
				memcpy_src_circ(sym_queue_rx2nd, ptrcirc_fft_in-FFTin+FFTin2nd, copy_size, FFTin2nd, IFFT_CIRC_SIZE);
#else
				memcpy_src_circ(sym_queue_tx, ptrcirc_fft_in-FFTin+iFFTout_CFRinout, copy_size, iFFTout_CFRinout, IFFT_CIRC_SIZE);
#endif
#endif

		}
	}
	else if((FFTout_valid & (BIT_MASK_FFT_OUT_VALID|BIT_MASK_FFT_OUT_1ST_HALF_READY|BIT_MASK_FFT_OUT_2ND_HALF_READY)) == BIT_MASK_FFT_OUT_1ST_HALF_READY)
	{
		unsigned int copy_size = POINTS_FFT*4/2;

		if((rx_bitrev_progress_ippu[0] == BITREV_NOT_STARTED) && (size_fft_in_valid >= (copy_size)))
		{
			unsigned int read_offset_fft_in = dfe_ctrl.read_offset_fft_in;
			unsigned int* ptrcirc_fft_in = FFTin + read_offset_fft_in/4;
			unsigned int *dest = (unsigned int*)sym_queue_rx;
			dfe_ctrl.FFTout_valid = BIT_MASK_FFT_OUT_1ST_HALF_READY | BIT_MASK_FFT_OUT_2ND_HALF_READY | BIT_MASK_FFT_OUT_IPPU_INP;
			memcpy_src_circ(&sym_queue_rx[POINTS_FFT/2], ptrcirc_fft_in, copy_size, FFTin, FFT_CIRC_SIZE);
#if NUM_1R_IN_CORE == 2
			if(__ip_read(IP_IDX_DFE_MODE_LO, DFE_MODE_RX_ANT_MASK(1)) != DFE_MODE_RX_ANT_MASK(1)) //share TX CIRC buffer for 2nd RX
#if FDD_1T2R
				memcpy_src_circ(&sym_queue_rx2nd[POINTS_FFT/2], ptrcirc_fft_in-FFTin+FFTin2nd, copy_size, FFTin2nd, IFFT_CIRC_SIZE);
#else
				memcpy_src_circ(&sym_queue_tx[POINTS_FFT/2], ptrcirc_fft_in-FFTin+iFFTout_CFRinout, copy_size, iFFTout_CFRinout, IFFT_CIRC_SIZE);
#endif
#endif
			
#if RXDCS_FDD
			if(!FDD_MODE_RX)
			{
				unsigned int rx_pattern_size = get_rx_pattern_size();
				unsigned int skip0 = GET_SKIP0(rx_pattern_size);
				unsigned int num_rx_symbols = GET_NUM_SYMBOLS(rx_pattern_size);
				unsigned int skip1 = GET_SKIP1(rx_pattern_size);
				
				if(rx_pattern_sym_idx < skip0)
				{
					rx_sym_type = BIT_MASK_FFT_OUT_IDLE;
				}
				else if(rx_pattern_sym_idx < (skip0+num_rx_symbols))
				{
					rx_sym_type = 0;
				}
				else if(rx_pattern_sym_idx < (skip0+num_rx_symbols+skip1))
				{
					rx_sym_type = BIT_MASK_FFT_OUT_IDLE;
				}
		
				rx_pattern_sym_idx++;
				
				if (rx_pattern_sym_idx == (skip0+num_rx_symbols+skip1))
				{
					advance_to_next_rx_pattern_size();
					rx_pattern_sym_idx = 0;
				}
			}
			else
			{
				rx_sym_type = 0;
			}
	
#endif
			//redcap 1024 point FFT. the FFT reads input from RX circ buffer 1K points, output to RX sym buffer and set RX sym buf vaild bit.
			//FFT bitrev will read from RX sym buf and output to circ buffer again but with smaller size (FFT input is 1Ksamples, bitrev output is 640 samples for 20Mhz SCS30

			//1. do FFT from circ buffer to rx symbol buffer
			
#if RXDCS_FDD
			if(rx_sym_type == BIT_MASK_FFT_OUT_IDLE)
			{
				FFTout_valid = rx_sym_type | BIT_MASK_FFT_OUT_VALID | rx_sym_idx_per_sframe;

				for(unsigned int i=0;i<NUM_1R_IN_CORE;i++)
				{
					if(dfe_ctrl.rx_sym_dumping_flag[i]&DUMPING_FLAG_DUMPING)
						FFTout_valid |= BIT_MASK_FFT_OUT_DUMPING(i);
				}
				
				dfe_ctrl.FFTout_valid = FFTout_valid;
			}
			else
			{
#endif
				
				//2. do bitreversal from rx symbol buffer to circ buffer, output is 20lines
				//3. copy symbol samples from 20 lines to rx symbol buffer. 
	
				unsigned int read_offset_fft_in_addr = BYTE2UNIT(read_offset_fft_in + copy_size);  //goto the end of fft input buffer, note that this addr may exceed this circ upper limit
				read_offset_fft_in_addr &= ~(LINE_SIZE-1);  //align to first address of a line.
				read_offset_fft_in_addr = read_offset_fft_in_addr + BYTE2UNIT(FFT_CIRC_SIZE) - 2*ALIGN(BYTE2UNIT(num_sc_in_a_sym/2*4), LINE_SIZE);  //go back 20 lines
				read_offset_fft_in_addr %= BYTE2UNIT(FFT_CIRC_SIZE);
				
				unsigned int dst_aligned = (((unsigned int)FFTin) + read_offset_fft_in_addr);  //move to aligned address. vspa2 line size = 64

				FFTout_valid |= rx_sym_idx_per_sframe;

				for(unsigned int i=0;i<NUM_1R_IN_CORE;i++)
				{
					if(dfe_ctrl.rx_sym_dumping_flag[i]&DUMPING_FLAG_DUMPING)
						FFTout_valid |= BIT_MASK_FFT_OUT_DUMPING(i);
				}

				#ifdef LOG_TYPE_FFT_START
				log_2int(TYPE_STAMPED(LOG_TYPE_FFT_START), FFTout_valid);
				#endif
		
				//bitrev will do bitrev and output exact num of sc to RX symbol buffer. set flag to indicate FFTout is availabe
				#if LOG_TYPE_RX_IPPU_START
				log_2int(TYPE_STAMPED(LOG_TYPE_RX_IPPU_START), (unsigned int)dst_aligned);
				#endif

				CODE_MEA_FFT(unsigned int ts_fft = get_timestamp32();)
#if POINTS_FFT == 1024
				fftDIF1024_sc128_hfx_hfx_vspa2((cfixed16_t*)dest, (cfixed16_t*)dest, (cfixed16_t*)dest, BYTE2UNIT(POINTS_FFT*4)); //circ size is 16-bit size.
#elif POINTS_FFT == 512
				fftDIF512_hfx_hfx((cfixed16_t*)dest, (cfixed16_t*)dest, (cfixed16_t*)dest, BYTE2UNIT(POINTS_FFT*4)); //unit size.
#endif

				rx_bitrev_progress_ippu[0] = BITREV_STARTED;//FFTout_valid;
				rx_bitrev_done[0] = BITREV_DONE;
						
				start_ippu_bitRev_reverseIn_linearOut_CopyDestToSrc_vspa2(dst_aligned, (unsigned int)dest, (unsigned int)FFTin, FFT_CIRC_SIZE,
						num_sc_in_a_sym,
						(unsigned int)&rx_bitrev_progress_ippu[0],
						(unsigned int)&rx_bitrev_done[0]);

				CODE_MEA_FFT(dfe_ctrl.cycle_count_fft = (get_timestamp32() - ts_fft);)

#if NUM_1R_IN_CORE == 2
#if FDD_1T2R
				void* dest_2ndR = sym_queue_rx2nd;	
#else
				void* dest_2ndR = sym_queue_tx;
#endif
				if(__ip_read(IP_IDX_DFE_MODE_LO, DFE_MODE_RX_ANT_MASK(1)) != DFE_MODE_RX_ANT_MASK(1)) //share TX CIRC buffer for 2nd RX
				{
					rx_bitrev_done[0] = BITREV_STARTED;  //if 2ndR is start, do not set 1st R done when it's done.
#if POINTS_FFT == 1024
					fftDIF1024_sc128_hfx_hfx_vspa2((cfixed16_t*)dest_2ndR, (cfixed16_t*)dest_2ndR, (cfixed16_t*)dest_2ndR, BYTE2UNIT(POINTS_FFT*4)); //circ size is 16-bit size.
#elif POINTS_FFT == 512
					fftDIF512_hfx_hfx((cfixed16_t*)dest_2ndR, (cfixed16_t*)dest_2ndR, (cfixed16_t*)dest_2ndR, BYTE2UNIT(POINTS_FFT*4)); //unit size.
#endif
					start_ippu_bitRev_reverseIn_linearOut_CopyDestToSrc_vspa2(dst_aligned-(unsigned int)FFTin+(unsigned int)iFFTout_CFRinout, (unsigned int)dest_2ndR, (unsigned int)iFFTout_CFRinout, IFFT_CIRC_SIZE, 
							num_sc_in_a_sym, 
							(unsigned int)&rx_bitrev_progress_ippu[0], 
							(unsigned int)&rx_bitrev_done[1]);
				}
#endif
				status = 1;
#if RXDCS_FDD
			}
#endif

			#ifdef LOG_TYPE_FFT_END
			log_2int(TYPE_STAMPED(LOG_TYPE_FFT_END), rx_sym_idx_per_sframe);
			#endif

			dfe_ctrl.FFTout_valid = FFTout_valid;
			dfe_ctrl.rx_release_size_backup = copy_size;
			dfe_ctrl.read_offset_fft_in = (read_offset_fft_in + copy_size) % FFT_CIRC_SIZE;
		}
	}
	return status;
}

#endif

unsigned int is_phy_rx_buf_available(unsigned int buf_idx)
{
	if(__ip_read(IP_IDX_DFE_MODE_HI, DFE_MODE_RX_HANDSHAKE_BYPASS))
		return 1;

#if HIGHPHY_LOWPHY_IF_TYPE == IF_TYPE_HOST_VSPA
	return !vspa_sym_buf_full(buf_idx);
#else
	return 1;
#endif
}


void ack_phy_rx_buf_available(unsigned int sym_idx)
{
	dfe_ctrl.rx_sym_idx_ready_for_host = 0x80000000|sym_idx;  //indicate symbol idx ready for host
	
#if HIGHPHY_LOWPHY_IF_TYPE == IF_TYPE_HOST_VSPA
	vspa_sym_rdy_set(sym_idx%MAX_NUM_RX_BITS_IN_USE);
#else
	//send GPIO msg to HighPHY RX core to notify which symbol (0-13) is ready,
	send_msg_ext_go(PHY_RX_CORE_ID, sym_idx%MAX_NUM_RX_SYM_IN_BUF); 
	//host flag 14 bits
	ext_word_write(ADDR_AXI_VCPU_IP_CORE(PHY_RX_CORE_ID, (HOST_VCPU_FLAGS0+ANTENNA_ID_RX/2)*4), 1<<((sym_idx%MAX_NUM_RX_SYM_IN_BUF)+16*(ANTENNA_ID_RX%2)));
#endif
}

//return value: 0-no cell tracking done, 1-cell tracking done
unsigned int rx_sym_ready_processing()
{
	unsigned int FFTout_valid = dfe_ctrl.FFTout_valid;
	unsigned int rx_sym_idx_per_sframe = FFTout_valid&BIT_MASK_FFT_OUT_SYM_IDX_MASK;
	unsigned int status = 0;
#if RXDCS_FDD
	if(!(FFTout_valid&BIT_MASK_FFT_OUT_IDLE))
	{
		dfe_ctrl.num_rx_sym_to_hiphy++;
	}
#else
	dfe_ctrl.num_rx_sym_to_hiphy++;
#endif	
		
		
	ack_phy_rx_buf_available(rx_sym_idx_per_sframe); 
	
	
#if(!BYPASS_FFT)

	#if CELLTRACK_ENABLE
	//handle PSS and SSS here
	unsigned int celltrack_config_hi = dfe_ctrl.celltrack_config_hi;
	unsigned int celltrack_config_lo = dfe_ctrl.celltrack_config_lo;
	unsigned int num_sym_ssb_period = (1<<GET_CELLTRACK_SSB_PERIOD(celltrack_config_hi))*(5*NUM_SYM_1ms);
	unsigned int ssb_sym_id = GET_CELLTRACK_SSB_SYM_ID(celltrack_config_hi);
	unsigned int ssb_re_offset = GET_CELLTRACK_SSB_RE_OFFSET(celltrack_config_lo);
	struct_celltrack_extbuf* celltrack_extbuf = (struct_celltrack_extbuf*)BYTE2UNIT(GET_CELLTRACK_EXTBUF_ADDR(celltrack_config_lo));
	if(celltrack_config_hi)
	{
		int* src_addr = (int*)sym_queue_rx;
		#if POINTS_FFT == 512
		pss_buf_struct* pss_buf = (pss_buf_struct*)cell_tracking_scratch_buf;
		sss_buf_struct* sss_buf = (sss_buf_struct*)cell_tracking_scratch_buf;
		#else
		pss_buf_struct* pss_buf = (pss_buf_struct*)src_addr;
		sss_buf_struct* sss_buf = (sss_buf_struct*)src_addr;
		#endif
		if((rx_sym_idx_per_sframe % num_sym_ssb_period) == ssb_sym_id)
		{
			unsigned int id2_ind = GET_CELLTRACK_NID2(celltrack_config_hi);
			unsigned int axi_addr = UNIT2BYTE((unsigned int)celltrack_extbuf->pssRef256[id2_ind]);
			dmac_enable(DMAC_RD /*| DMAC_MBRE*/ | DMA_CHAN_CELL_TRACKING, SIZE_PSS_REF, axi_addr, UNIT2BYTE((unsigned int)&pss_buf->pssRef256[0]));
			unsigned int* psrc = (unsigned int*)src_addr;
			memcpy(&pss_buf->pss_input_res[0], psrc+ssb_re_offset-RE_OFFSET_BETWEEN_SSB_AND_INPUTWINDOW, SIZEOF8(pss_buf->pss_input_res));
			while(dmac_is_enabled(1<<DMA_CHAN_CELL_TRACKING)) {}

#ifdef LOG_TYPE_CELLTRACK_PSS_START
			log_2int(TYPE_STAMPED(LOG_TYPE_CELLTRACK_PSS_START), FFTout_valid);
#endif
			chEst(
	        	pss_buf->pss_input_res, //scratch_b0->pss_csi_in.pss_input_res,  // in
	        	pss_buf->pss_csi_out, //scratch_b1->pss_csi_out,       		// out
	        	pss_buf->pssRef256,//scratch_b0->pss_csi_in.pssRef256,      // PSS reference,
	            NVEC32(NR_SSB_NFFT));

	        // Equalizer Weights (= simply 1/Chan)
	        rcpv(pss_buf->pss_eqw,                           // out
	        	pss_buf->pss_csi_out, //scratch_b1->pss_csi_out,     // in
	            NVEC32(NR_SSB_NFFT));

			axi_addr = UNIT2BYTE((unsigned int)celltrack_extbuf->pss_csi_out);
			dmac_enable(DMAC_WR | DMA_CHAN_CELL_TRACKING, SIZE_PSS_CSI+SIZE_PSS_EQW, axi_addr, UNIT2BYTE((unsigned int)&pss_buf->pss_csi_out[0]));
			status = 1;
#ifdef LOG_TYPE_CELLTRACK_PSS_START
			log_2int(TYPE_STAMPED(LOG_TYPE_CELLTRACK_PSS_START), FFTout_valid);
#endif
		}
		else if((rx_sym_idx_per_sframe % num_sym_ssb_period) == (ssb_sym_id+2))
		{
			unsigned int axi_addr = UNIT2BYTE((unsigned int)celltrack_extbuf->pss_eqw);
			dmac_enable(DMAC_RD /*| DMAC_MBRE*/ | DMA_CHAN_CELL_TRACKING, SIZE_PSS_EQW+SIZE_SSB_PARAMS, axi_addr, UNIT2BYTE((unsigned int)&sss_buf->pss_eqw[0]));
			unsigned int* psrc = (unsigned int*)src_addr;
			memcpy(&sss_buf->sss_input_res[0], psrc+ssb_re_offset-RE_OFFSET_BETWEEN_SSB_AND_INPUTWINDOW, SIZEOF8(sss_buf->sss_input_res));
			while(dmac_is_enabled(1<<DMA_CHAN_CELL_TRACKING)) {}

#ifdef LOG_TYPE_CELLTRACK_SSS_START
			log_2int(TYPE_STAMPED(LOG_TYPE_CELLTRACK_SSS_START), FFTout_valid);
#endif			
			// targeted PSS/NID2, targeted SSS/NID1
		    int32_t id1_ind = GET_CELLTRACK_NID1(celltrack_config_hi);//ptr_ssb->Nid1;
		    int32_t id2_ind = GET_CELLTRACK_NID2(celltrack_config_hi);//ptr_ssb->Nid2;

		    sss_buf->sssdet_param.thre_sss = 0.05;
		    sss_buf->sssdet_param.scaling = 0.0310058593750;
		    /*
		    sssdet_param1->thre_sss = 0.05;
		    sssdet_param1->scaling = 0.0310058593750;
		    //sssdet_param = (uint32_t)(*sssdet_param1);
		    sssdet_param = (uint32_t)&pss_sss_temp_b0.sss_det_targeted.sssdet_param;
		    */

		    // Equalize SSS symbol using PSS-based channel weights
			chp_chf_csp_vmult_asm( 	sss_buf->sss_eq,            // out
									sss_buf->sss_input_res,  // in
									sss_buf->pss_eqw,                     // eq weights
									NVEC32(NR_SSB_NFFT));

			unsigned int sss_det = sssdet_targeted_hpgemv_rc_v1( sss_buf->sss_eq + SLOTTED_PSS_SSS_OFST, // in
		                                      	  (uint32_t *)sss_buf->scratch_512B,    // scratch
		                                      	  sss_buf->sss_seq ,        // out:  seq out
		                                      	  id1_ind,                                 // Nid1
		                                      	  id2_ind,                                 // Nid2
		                                      	  (uint32_t)(&sss_buf->sssdet_param),
		                                      	  (uint32_t)(&sss_buf->SSSDET_REF_D0),
		                                      	  (uint32_t)(&sss_buf->SSSDET_REF_D1));

		    // SSS Measurements
		    // TBD TECH - we calculate measurements even if cell not detected (using id=0 blind parts)
		    // (L1C needs for targeted, and RSSI could still be useful in general)

		    // Reference SSS Conversion
		    sssConvert( (cfloat16_t *) sss_buf->sss_conv_out,        	// out, SSS reference
		                (fixed16_t *)  sss_buf->sss_seq); 	// in

			axi_addr = UNIT2BYTE((unsigned int)celltrack_extbuf->pss_csi_out);
			dmac_enable(DMAC_RD /*| DMAC_MBRE*/ | DMA_CHAN_CELL_TRACKING, SIZE_PSS_CSI, axi_addr, UNIT2BYTE((unsigned int)&sss_buf->pss_csi_out[0]));

		    // SSS LS Channel Estimate
		    chEst( //scratch_b2->sss_csi,               // out
		    		sss_buf->sss_input_res,          // in: RX
		    		sss_buf->sss_csi,               // out
		    		sss_buf->sss_conv_out,      // in: SSS reference
		           NR_SSB_NFFT/32);                     // no. of DMEM lines


		    // Timing Offset estimation (actually FD phase ramp estimate)
		    int32_t timing_offset = tim_off_est_sp_asm_rc( 0,
		        					sss_buf->sss_csi,           		 // in
		                            (cfloat32_t *) sss_buf->scratch_512B, // scratch
		                            (uint32_t)(&sss_buf->ATAN2_COEFF_BUFF));

		    uint32_t cell_id;
		    if(sss_det) 
		    {
		    	// compute cell ID to report to L1C
		    	cell_id = NR_CELLID(id1_ind, id2_ind);
		            /*
		            ssbSmooth(scratch_b0->sss_chest+64,
		                      scratch_b0->sss_chest+64,
		                      (fixed16_t *)chSmoothCoefSlotted.data, //chSmoothCoef,
		                      NVEC16(NR_SSB_NFFT));        // length
		            ssbSmooth(scratch_b1->pss_chest[rx_path][cc][id2_ind]+64,
		                      scratch_b1->pss_chest[rx_path][cc][id2_ind]+64,
		                      (fixed16_t *)chSmoothCoefSlotted.data, //chSmoothCoef,
		                      NVEC16(NR_SSB_NFFT));        // length
		            */
		    } 
		    else 
		    {
		    	cell_id = L1C_V2A_CELL_ID_NO_SSB_DETECTED;
		    }

			while(dmac_is_enabled(1<<DMA_CHAN_CELL_TRACKING)){} //wait DMA completes

		    // SSS measurements
		    sss_msmts( sss_buf->pss_csi_out , 						// in: PSS channel estimate
		    			sss_buf->sss_csi,                       // in: SSS channel estimate
		               (sss_msmt_T*)&sss_buf->ssb_det.ss_rssi_db);                                   // out

		    // Fine CFO estimation
		    int32_t cfo = fineCFO_rc(sss_buf->pss_csi_out,   	// in: PSS channel estimate
		    					sss_buf->sss_csi,                // in: SSS channel estimate
		    					(cfixed16_t *) sss_buf->scratch_512B,   // scratch
		    					(uint32_t)(&sss_buf->ATAN2_COEFF_BUFF));

		    // convert CFO from 7.68MSPS units to ADC sampling rate unit
		    //cfo = CFO_7_68_TO_30_72(cfo);
		    //cfo = CFO_7_68_TO_61_44(cfo);
		    cfo = cfo / ((RX_DCS_SPS/DCS_SPS_7M) * (SCS_30K/SCS));

		    // convert FFT phase slope into timing error in samples @ ADC sampling rate
		    //timing_offset = FD_PHASE_RAMP_TO_TD_61_44(timing_offset);
		    timing_offset = timing_offset * ((RX_DCS_SPS/DCS_SPS_7M) * 256 * (SCS_30K/SCS));

		    // Send Slotted SSB Detect Indication to L1C

		    //slotted_ssb_ind.ssb_det.report_id = ptr_ssb->report_id;
		    sss_buf->ssb_det.cell_id = cell_id;

		    //slotted_ssb_ind.ssb_det.sym_cc_ant_scs = L1SP_V2A_SSB_BUILD_SYM_CC_ANT_SCS(/*loc_ssb*/ (0x1 << ptcb->ssb_sym[cc]), cc, path, ptcb->scs_ssb[cc]);
		    sss_buf->ssb_det.time_ofst = timing_offset;
		    sss_buf->ssb_det.cfo = cfo;
			
			axi_addr = UNIT2BYTE((unsigned int)celltrack_extbuf->ssb_det);
			dmac_enable(DMAC_WR | DMA_CHAN_CELL_TRACKING, SIZE_SSB_DET, axi_addr, UNIT2BYTE((unsigned int)&sss_buf->ssb_det));
			status = 1;
			dfe_ctrl.celltrack_config_hi = 0;  //stop cell tracking.
#ifdef LOG_TYPE_CELLTRACK_SSS_END
			log_2int(TYPE_STAMPED(LOG_TYPE_CELLTRACK_SSS_END), FFTout_valid);
#endif
		}
	}
	#endif //CELLTRACK_ENABLE
	
	dfe_ctrl.FFTout_valid = 0;

#else
	dfe_ctrl.read_offset_fft_in = (dfe_ctrl.read_offset_fft_in + dfe_ctrl.rx_sym_dest_size) % FFT_CIRC_SIZE;
//	#if (ARCH_TYPE_R == ARCH_1R_1CORE)
//	dfe_ctrl.size_fft_in_valid -= dfe_ctrl.rx_sym_dest_size;
//	#else
	dfe_ctrl.rx_circ_total_released_size += dfe_ctrl.rx_sym_dest_size;
//	#endif

	set_rx_slot_sym_idx(dfe_ctrl.rx_sym_idx_per_sframe+1);
#endif
	return status;
}

//return value: 1=a job has run, 0-no job run
unsigned int PHY_DFE_interf_rx_check_dma_finish()
{
	unsigned int status = 0;
	//if(GET_FAST_FLAGS(FAST_FLAG_RX_SYM_DMA_INP_MASK) && (!dmac_is_enabled(1<<DMA_CHAN_SEND_DU_RX_SYMBOLS(0))))
	if(GET_FAST_FLAGS(FAST_FLAG_RX_SYM_DMA_INP_MASK) && (dmac_event(1<<DMA_CHAN_SEND_DU_RX_SYMBOLS(0))))
	{
		#ifdef LOG_TYPE_RX_DMA_END
		log_2int(TYPE_STAMPED(LOG_TYPE_RX_DMA_END), dfe_ctrl.num_rx_sym_to_hiphy);
		#endif

		dmac_clear_event(1<<DMA_CHAN_SEND_DU_RX_SYMBOLS(0));
		CLR_FAST_FLAGS(FAST_FLAG_RX_SYM_DMA_INP_MASK);
		status = rx_sym_ready_processing();
	}
	return status;
}

//return value: 1=a job has run, 0-no job run
unsigned int PHY_DFE_interf_rx_check_dma_start()
{
	unsigned int status = 0;
	
#if DFE_ONLY
	if(GET_DFE_MODE_HI_BITS(DFE_MODE_DFE_ONLY))
	{
		if(GET_DFE_MODE_HI_BITS(DFE_MODE_RX_HANDSHAKE_BYPASS))
		{
			dfe_ctrl.rx_circ_total_released_size = dfe_ctrl.rx_circ_total_produced_size;
		}
		return 0;
	}
#endif
	
#if !BYPASS_FFT
	//check rx ippu done
	if(rx_bitrev_progress_ippu[0] == BITREV_DONE)
	{
		rx_bitrev_progress_ippu[0] = BITREV_NOT_STARTED;
		unsigned int FFTout_valid = dfe_ctrl.FFTout_valid;
		dfe_ctrl.FFTout_valid = FFTout_valid | BIT_MASK_FFT_OUT_VALID;
		
		dfe_ctrl.rx_circ_total_released_size += dfe_ctrl.rx_release_size_backup;
		set_rx_slot_sym_idx((FFTout_valid&BIT_MASK_FFT_OUT_SYM_IDX_MASK)+1);
	#if POINTS_FFT == 4096
		//ra-org the num of sc from middle of buffer to beginning of buffer
		if(!((dfe_ctrl.kernels_disable & KERNELS_DISABLE_BITMASK_FFT) || RX_OPTION8_CFG))
		{
			unsigned int num_sc = GET_NUM_SC;
			memcpy(sym_queue_rx, &sym_queue_rx[(POINTS_FFT-num_sc)/2], num_sc*4);
		}
	#endif
	}
#endif
	
	/****************************************************************************************************************
	 * send symbols to eCPRI host, if compression is enabled, do compression, the compression kernel will write the symbol to eCPRI host
	 * if compression is disabled, use DMA to copy the symbol to eCPRI host, the DMA will convert samples to 2'complement integer format.
	 ******************************************************************************************************************/
#if(!BYPASS_FFT)
	unsigned int FFTout_valid = dfe_ctrl.FFTout_valid;
	unsigned int num_sc = GET_NUM_SC;
	unsigned int sym_data_size = num_sc*4;
	if((!GET_FAST_FLAGS(FAST_FLAG_RX_SYM_DMA_INP_MASK)) && (FFTout_valid & BIT_MASK_FFT_OUT_VALID))
	{
		unsigned int rx_sym_idx_per_sframe = FFTout_valid&BIT_MASK_FFT_OUT_SYM_IDX_MASK;
		//unsigned int rx_buf_sym_idx = rx_sym_idx_per_sframe % __min(MAX_NUM_SYM_IN_BUF,dfe_ctrl.rx_num_sym_in_buff[0]);
		
		if(1)//(is_phy_rx_buf_available(rx_buf_sym_idx))
		{
			unsigned int dma_mode = DMAC_WRC;

			
			if(RX_OPTION8_CFG)
			{
				unsigned int CP_size = get_cp_size(rx_sym_idx_per_sframe%NUM_SYM_PER_CP_PERIOD);
				sym_data_size = POINTS_FFT*4+CP_size;
			}
			
			unsigned int* src_addr[2];
			src_addr[0] = (unsigned int*)sym_queue_rx;
#if NUM_1R_IN_CORE == 2
#if FDD_1T2R
			src_addr[1] = (unsigned int*)sym_queue_rx2nd;
#else
			src_addr[1] = (unsigned int*)sym_queue_tx;
#endif
#endif

			unsigned int size_wrap = 0;
			dfe_ctrl.rx_sym_dest_size = sym_data_size;
			unsigned int dma_size = sym_data_size;
#if RXDCS_FDD
			//DAM will send RX symbols to host when:
			//1. RX freq domain dumping (configured via dump_freq_domain_rx msg)
			//2. DUMPING_FLAG_NO_SYM_TO_HOST is 0 (configured via dump_freq_domain_rx msg) and sym buf is not in DFE
			if(__ip_read(IP_IDX_DFE_MODE_HI, DFE_MODE_NO_RX_SYM_TO_HOST) || (FFTout_valid&BIT_MASK_FFT_OUT_IDLE))
			
#else
			//DAM will send RX symbols to host when:
			//1. RX freq domain dumping (configured via dump_freq_domain_rx msg)
			//2. DUMPING_FLAG_NO_SYM_TO_HOST is 0 (configured via dump_freq_domain_rx msg) and sym buf is not in DFE
			if(__ip_read(IP_IDX_DFE_MODE_HI, DFE_MODE_NO_RX_SYM_TO_HOST))
#endif
			{
				dma_size = 16;
			}

			//do phase compensation
			int* pcoeff = (int*)phcom_coeff_rx[rx_sym_idx_per_sframe%NUM_SYM_PER_CP_PERIOD];
			//for(int i=0;i<NUM_1R_IN_CORE;i++)
			for(int i=GET_FAST_FLAGS(FAST_FLAG_2RX_MASK);i>=0;i--)
			{
#if !PHASE_COMPENSATION_DISABLE
				phcom_asm(src_addr[i], src_addr[i], (float*)pcoeff, (sym_data_size+127)/128);
#endif
								
				if(FFTout_valid&BIT_MASK_FFT_OUT_DUMPING(i))
				{
					unsigned int dest_addr_dump = get_rx_dump_sym_buf_addr(rx_sym_idx_per_sframe,i);
					dmac_enable(DMAC_WRC | DMA_CHAN_SEND_DU_RX_SYMBOLS(i), sym_data_size, dest_addr_dump, UNIT2BYTE((unsigned int)src_addr[i]));
				}
				
				unsigned int dest_addr = dfe_ctrl.rx_sym_buf_base[i] + (rx_sym_idx_per_sframe % dfe_ctrl.rx_num_sym_in_buff[0]) * dfe_ctrl.rx_sym_buff_size;
				dmac_enable(dma_mode | ((1-i)*DMAC_TRIG_VCPU) | DMA_CHAN_SEND_DU_RX_SYMBOLS(i), dma_size, dest_addr, UNIT2BYTE((unsigned int)src_addr[i]));
								
			}
				
			SET_FAST_FLAGS(FAST_FLAG_RX_SYM_DMA_INP_MASK);
			

			#ifdef LOG_TYPE_RX_DMA_START
			log_2int(TYPE_STAMPED(LOG_TYPE_RX_DMA_START), (sym_data_size<<16)|rx_sym_idx_per_sframe);
			#endif

			status = 1;
		}
	}
#else
//	#ifndef LA9310
	unsigned int CP_size = get_cp_size(dfe_ctrl.rx_sym_idx_per_sframe % NUM_SYM_PER_CP_PERIOD);
	unsigned int sym_data_size = POINTS_FFT*4+CP_size;
//	#else
//	unsigned int sym_data_size = POINTS_FFT*4;
//	#endif
	
	unsigned int size_fft_in_valid = get_valid_size_for_fft_in();

	if((size_fft_in_valid >= sym_data_size) && (!GET_FAST_FLAGS(FAST_FLAG_RX_SYM_DMA_INP_MASK)))
	{
		unsigned int rx_sym_idx_per_sframe = dfe_ctrl.rx_sym_idx_per_sframe;
		unsigned int dumping_flag = dfe_ctrl.rx_sym_dumping_flag[0] & DUMPING_FLAG_DUMPING;
		//unsigned int rx_buf_sym_idx = rx_sym_idx_per_sframe % __min(MAX_NUM_SYM_IN_BUF,dfe_ctrl.rx_num_sym_in_buff[0]);
		
		if(1)//(is_phy_rx_buf_available(rx_buf_sym_idx))
		{
			unsigned int dma_mode = DMAC_WRC;
			
			unsigned int dest_addr = dfe_ctrl.rx_sym_buf_base[0] + (rx_sym_idx_per_sframe % dfe_ctrl.rx_num_sym_in_buff[0]) * dfe_ctrl.rx_sym_buff_size;
			if(dumping_flag)
			{
				unsigned int rx_sym_buf_base = dfe_ctrl.rx_dumping_sym_buf_base[0];
				unsigned int sym_idx_in_buffer = rx_sym_idx_per_sframe % dfe_ctrl.rx_dumping_num_sym_in_buff;
				unsigned int num_lcp = (sym_idx_in_buffer + (NUM_SYM_PER_CP_PERIOD - 1))  / NUM_SYM_PER_CP_PERIOD;
				dest_addr = rx_sym_buf_base + num_lcp * SIZE_SYMBOL_DATA_TIME_DOMAIN_LCP + (sym_idx_in_buffer-num_lcp)*SIZE_SYMBOL_DATA_TIME_DOMAIN_SCP;
			}
			
			unsigned int size_to_circ_end = FFT_CIRC_SIZE - dfe_ctrl.read_offset_fft_in;
			unsigned int src_addr = ((unsigned int)FFTin) + BYTE2UNIT(dfe_ctrl.read_offset_fft_in);
			//while(dmac_is_enabled(1<<DMA_CHAN_SEND_DU_RX_SYMBOLS(0)){} //wait DMA completes
			unsigned int size1, size_wrap, event_trig=0;
			if(size_to_circ_end >= sym_data_size)
			{
				size1 = sym_data_size;
				size_wrap = 0;
				event_trig = DMAC_TRIG_VCPU;
			}
			else
			{
				size1 = size_to_circ_end;
				size_wrap = sym_data_size-size_to_circ_end;
			}
			dfe_ctrl.rx_sym_dest_size = sym_data_size;

			//if(dumping_flag || (!(dfe_ctrl.dfe_mode_config_hi&DFE_MODE_NO_RX_SYM_TO_HOST))) 
			if(dumping_flag || (!(__ip_read(IP_IDX_DFE_MODE_HI, DFE_MODE_NO_RX_SYM_TO_HOST)))) 
			{
			
			if(size1 <= 64*1024)
				dmac_enable(dma_mode | event_trig | DMA_CHAN_SEND_DU_RX_SYMBOLS(0), size1, dest_addr, UNIT2BYTE(src_addr));
			else
			{
				unsigned int size10=64*1024;
				dmac_enable(dma_mode | DMA_CHAN_SEND_DU_RX_SYMBOLS(0), size10, dest_addr, UNIT2BYTE(src_addr));
				unsigned int size11=size1-size10;
				dmac_enable(dma_mode | event_trig | DMA_CHAN_SEND_DU_RX_SYMBOLS(0), size11, dest_addr+size10, UNIT2BYTE(src_addr)+size10);
			}

			if(size_wrap)
			{
				if(size_wrap <= 64*1024)
					dmac_enable(dma_mode | DMAC_TRIG_VCPU | DMA_CHAN_SEND_DU_RX_SYMBOLS(0), size_wrap, dest_addr+size1, UNIT2BYTE((unsigned int)FFTin));
				else
				{
					unsigned int size10=64*1024;
					dmac_enable(dma_mode | DMA_CHAN_SEND_DU_RX_SYMBOLS(0), size10, dest_addr+size1, UNIT2BYTE((unsigned int)FFTin));
					unsigned int size11 = size_wrap-size10;
					dmac_enable(dma_mode | DMAC_TRIG_VCPU | DMA_CHAN_SEND_DU_RX_SYMBOLS(0), size11, dest_addr+size1+size10, UNIT2BYTE(((unsigned int)FFTin))+size10);
				}
			}
			}
			SET_FAST_FLAGS(FAST_FLAG_RX_SYM_DMA_INP_MASK);
			
			#ifdef LOG_TYPE_RX_DMA_START
			log_2int(TYPE_STAMPED(LOG_TYPE_RX_DMA_START), (sym_data_size<<16)|rx_sym_idx_per_sframe);
			#endif
		}
//		else
//		{
//			//TX symbol buffer not ready.
//			#ifdef LOG_TYPE_RX_HOST_SYM_BUF_NOT_FREE
//			log_2int(TYPE_STAMPED(LOG_TYPE_RX_HOST_SYM_BUF_NOT_FREE), rx_sym_idx_per_sframe);
//			#endif
//		}
	}
#endif

	return status;
}

//return value: 1-job is run, 0-no job is run
unsigned int rx_processing_symbol_level()
{
#if DFE_ONLY
	if(GET_DFE_MODE_HI_BITS(DFE_MODE_DFE_ONLY))
	{
		if(GET_DFE_MODE_HI_BITS(DFE_MODE_RX_HANDSHAKE_BYPASS))
		{
			dfe_ctrl.rx_circ_total_released_size = dfe_ctrl.rx_circ_total_produced_size;
		}
		return 0;
	}
#endif
	
	unsigned int status = 0;
	PHY_DFE_interf_rx_check_dma_finish();
	
	#if(!BYPASS_FFT)
	status = run_fft();
	#endif
	PHY_DFE_interf_rx_check_dma_start();
	return status;
}
#endif

#if (OBS_CHAN_ENABLE == 1)
unsigned int obs_dgb[8];
void obs_chan_handling(unsigned int antman_chan_id)
{
	struct_buf_stat_td* ant_rx_buf_stat;
	unsigned char num_blk_valid = Ant_buffer_rx_poll_buffer(&ant_rx_buf_stat);
	if(num_blk_valid)
	{
		//successfully get an ant buffer, dump it to memory
		obs_dgb[0]++;
		unsigned int addr, ant_id, dump_flag=0;
		for(int i=0;i<8;i++)
		{
			if(check_msg_ext_go(i))
			{
				unsigned int msg = read_msg_ext_go(i);
				if((msg&IC_MSG_ID_MASK) == IC_MSG_ID_OBS_DUMP)
				{
					clr_msg_ext_go(i);
					dump_flag = 1;
					ant_id = (msg>>25)&0x7;
					addr = msg << 7;
					obs_dgb[1]++;
					break;
				}
			}
		}
		
		unsigned char obs_dump_flag = dfe_ctrl.obs_dump_flag;
		if(dfe_ctrl.obs_dump_flag_pre != obs_dump_flag)
		{
			dfe_ctrl.obs_dump_flag_pre = obs_dump_flag;
			dump_flag = 1;
			ant_id = ANTENNA_ID_TX;
			addr = dfe_ctrl.obs_dump_addr;
			obs_dgb[2]++;
		}
		
		if(dump_flag)
		{
			void* ptr_rx_ant = (void*)ant_rx_buf_stat->buf_addr[0];
			unsigned int *flag_addr = (unsigned int *)((unsigned int)ptr_rx_ant + BYTE2UNIT(OBS_SIZE_BUFFER));
			*flag_addr = 0x12345600+ant_id+(OBS_DCS_ID<<4);
			unsigned int size = OBS_SIZE_BUFFER + 4;
			obs_dgb[3]=addr;
			dmac_enable(DMAC_WRC | DMA_CHAN_DUMP_ANT_DATA, size, addr, UNIT2BYTE((unsigned int)ptr_rx_ant));
			#ifdef log_2int_OBS_BLOCK_RECEIVED
			log_2int(TYPE_STAMPED(log_2int_OBS_BLOCK_RECEIVED), num_blk_valid);
			#endif	
		}
		
		Ant_buffer_rx_free_buffer(antman_chan_id);		//release the buffer immediately
	}
	Ant_buffer_rx_dma_feed_obs(antman_chan_id);
	if(Ant_buffer_rx_check_status_obs(antman_chan_id))
	{
//		mailbox_send_msg(MAILBOX_USED_FOR_ERROR_REPORT,SET_ERROR_TYPE(ERROR_REPORT_MSG_ERROR_TYPE_RX_OVERFLOW)|SET_ERROR_TR_ID(0)|SET_ERROR_ANT_ID(antman_ctrl_rx.dcs_id), get_timestamp32());
		if(dfe_ctrl.error_info_MSB32 == 0)
		{
			dfe_ctrl.error_info_LSB32 = get_timestamp32();
			dfe_ctrl.error_info_MSB32 = SET_ERROR_TYPE(ERROR_REPORT_MSG_ERROR_TYPE_RX_OVERFLOW)|SET_ERROR_TR_ID(0)|SET_ERROR_ANT_ID(antman_ctrl_rx.dcs_id);
		}
		Ant_buffer_rx_axiq_disable(antman_chan_id);
		Ant_buffer_rx_axiq_enable(antman_chan_id);
	}	
}
#endif
