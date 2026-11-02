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
#include "host_vspa_msg.h"
#include "global_var.h"

unsigned char tx_pattern_sym_idx;
unsigned int tx_sym_type;   //0 idle, 1 tx sym


#if IPPU_ARG_BANK
volatile uint32_t tx_bitrev_arg[MAX_NUM_IPPU_ARG] __attribute__((section(".ibss"))); 
volatile uint32_t rx_bitrev_arg[MAX_NUM_IPPU_ARG] __attribute__((section(".ibss"))); 
#else
volatile uint32_t tx_bitrev_arg[MAX_NUM_IPPU_ARG];
volatile uint32_t rx_bitrev_arg[MAX_NUM_IPPU_ARG];
#endif

#if (NUM_1T_IN_CORE)

/* TX pipeline (1T1R, 1 core, 25 MHz 30 kHz SCS)
 * DDR(1)   ->  <DMA>                  ->  IDMEM(2)
 * IDMEM(2) ->  <IFFT>                 ->  VDMEM(3)
 * VDMEM(3) ->  <IPPU bit-reversal>    ->  IDMEM(4)
 * IDMEM(4) ->  <CP + windowing>       ->  IDMEM(4)
 */


#if(!BYPASS_IFFT)

cfixed16_t tx_windowing_history_buff[WINDOW_SIZE]  __align_vec HISTORY_SECTION; //ALIGN32 is to align buffer size to 128


#if !TX_SYM_QUEUE_BANK
cfixed16_t sym_queue_tx[NUM_IFFT_IN_BUFFERS][TX_SYM_SIZE/4] __align_vec;
#else
cfixed16_t sym_queue_tx[NUM_IFFT_IN_BUFFERS][TX_SYM_SIZE/4] __align_vec __attribute__ ((section (".ibss1"))) ;
#endif
#if TX_BITREV_BANK == 0
cfixed16_t ifft_bitrev_in[TX_SYM_SIZE/4] __align_vec;
#else
cfixed16_t ifft_bitrev_in[TX_SYM_SIZE/4] __align_vec __attribute__ ((section (".ibss"))) ;
#endif
#endif


unsigned int axi_flag_tx_block_ack[32] __align_vec;
unsigned int axi_flag_tx_symbol_consumed[16]__attribute__ ((aligned (AXI_BURST_SIZE)));

#define CIRC_END_ADDRESS	((unsigned int)iFFTout_CFRinout + SIZEOFUNIT(iFFTout_CFRinout)-1)


void start_tx_ippu_bitrev_reversIn_linearOut(unsigned int pOut, unsigned int pIn, unsigned int pOutbase, unsigned int circ_size, unsigned int num_samples, unsigned int sample_offset, unsigned int pflag_addr, unsigned int pflag_value)
{
	ippu_proc_t ippu_func;
	#if TX_CIRC_BANK == 1
	{
		#if TX_SYM_QUEUE_BANK == 1
		tx_bitrev_arg[0] = ((pOut-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO) & (0xFFFFFFC0); //align output to start of first line
		tx_bitrev_arg[1] = (pIn-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
		tx_bitrev_arg[2] = (pOutbase-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
#if POINTS_FFT == 512
		ippu_func = (ippu_proc_t)bitRev512_reverseIn_linearOut_vspa2_b1_b1;
#elif  POINTS_FFT == 1024
		ippu_func = (ippu_proc_t)bitRev1024_reverseIn_linearOut_vspa2_b1_b1;
#elif  POINTS_FFT == 2048
		ippu_func = (ippu_proc_t)bitRev2048_reverseIn_linearOut_vspa2_b1_b1;
#endif
		#else
		#error TX_SYM_QUEUE_BANK not supported
		#endif
	}
	#else
	{
		#if TX_SYM_QUEUE_BANK == 0
		tx_bitrev_arg[0] = ((pOut)/IPPU_ADDR_RATIO) & (0xFFFFFFC0); //align output to start of first line
		tx_bitrev_arg[1] = (pIn)/IPPU_ADDR_RATIO;
		tx_bitrev_arg[2] = (pOutbase)/IPPU_ADDR_RATIO;
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
	tx_bitrev_arg[3] = BYTE2UNIT(circ_size)/IPPU_ADDR_RATIO;		// i/p circular buffer size

	
	unsigned int num_total_hwords = num_samples*2;
	unsigned int starting_point_first_line = (pOut/IPPU_ADDR_RATIO)&0x3F;
	unsigned int num_1st_line_hwords = 64-starting_point_first_line;
	unsigned int num_rest_lines = (num_total_hwords - num_1st_line_hwords + 63)/64;
	tx_bitrev_arg[4] = (starting_point_first_line)*4;	// starting point of first sample, 4-bit index
	tx_bitrev_arg[5] = num_1st_line_hwords/2 - 1;		// num of samples minus 1 of the unaligned samples in the first line
	tx_bitrev_arg[6] = sample_offset*2;				// offset from pIn, hwords. pointing to CP part
	tx_bitrev_arg[7] = num_rest_lines - 1;				// num of total lines (unaligned+aligned) minus 2.
	tx_bitrev_arg[8] = (pflag_addr-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;		// after IPPU done, value in this addr will be cleared, addr must be in VCPU
	tx_bitrev_arg[9] = (pflag_value-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
	
	ippu_arg_base(((uint32_t)tx_bitrev_arg - IPPU_ARG_OFFSET)/IPPU_ADDR_RATIO);
	ippu_enable(ippu_func);
	__nop(); //avoid issue in case the next instruction is checking IPPU status register
}

extern void vecclr_ippu_b0();
extern void vecclr_ippu_b1();

void vecclr_ippu_bank0(unsigned int addr, unsigned int num_lines)
{
	tx_bitrev_arg[0] = addr/IPPU_ADDR_RATIO;
	tx_bitrev_arg[1] = num_lines - 1;
	ippu_arg_base(((uint32_t)tx_bitrev_arg - IPPU_ARG_OFFSET)/IPPU_ADDR_RATIO);
	ippu_enable((ippu_proc_t)vecclr_ippu_b0);
}
void vecclr_ippu_bank1(unsigned int addr, unsigned int num_lines)
{
	tx_bitrev_arg[0] = (addr-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
	tx_bitrev_arg[1] = num_lines - 1;
	ippu_arg_base(((uint32_t)tx_bitrev_arg - IPPU_ARG_OFFSET)/IPPU_ADDR_RATIO);
	ippu_enable((ippu_proc_t)vecclr_ippu_b1);
}


void advance_tx_sym_idx(unsigned int adv)
{
	unsigned int sym_idx_after_adv = dfe_ctrl.tx_sym_idx_per_sframe + adv;
	if(sym_idx_after_adv >= NUM_SYM_PER_SYSPERIOD)
	{
		sym_idx_after_adv -= NUM_SYM_PER_SYSPERIOD;
		dfe_ctrl.tx_freq_dump_flag <<= DUMP_INJECT_MODE_SHIFT;
	}
	dfe_ctrl.tx_sym_idx_per_sframe 	= sym_idx_after_adv;
}


__inline__ unsigned int get_tx_sym_buf_addr(unsigned int sym_idx_in_buffer)
{
	unsigned int addr;
	unsigned int sym_buf_base = dfe_ctrl.tx_sym_buf_base;
	if(__ip_read(IP_IDX_DFE_MODE_HI, DFE_MODE_OPTION8))
	{
		unsigned int num_lcp = (sym_idx_in_buffer + (NUM_SYM_PER_CP_PERIOD - 1))  / NUM_SYM_PER_CP_PERIOD;
		unsigned int size = num_lcp * SIZE_SYMBOL_DATA_TIME_DOMAIN_LCP + (sym_idx_in_buffer-num_lcp)*SIZE_SYMBOL_DATA_TIME_DOMAIN_SCP;
		addr = size + sym_buf_base;
	}
	else
		addr = sym_buf_base + sym_idx_in_buffer * dfe_ctrl.tx_sym_buff_size;
	return addr;
}

//offset is in bytes,can be positive of negative
unsigned int circ_addr_advance(unsigned int addr, int offset)
{
	unsigned int offset_cur = UNIT2BYTE(addr - ((unsigned int)iFFTout_CFRinout));
	offset = (offset_cur + offset + IFFT_CIRC_SIZE) % IFFT_CIRC_SIZE;
	return (BYTE2UNIT(offset) + ((unsigned int)iFFTout_CFRinout));
}

void pipeline_reset_tx()
{
	dfe_ctrl.read_ptr_copy_to_2xup1 = 
	dfe_ctrl.write_ptr_ifft_out = (unsigned int)iFFTout_CFRinout;
	
	
#if(CORE_A_ID != CORE_B_ID)
	dmac_clear_event(1<<DMA_CHAN_USED_FOR_INTERCORE);
#endif

	dfe_ctrl.tx_circ_total_produced_size = dfe_ctrl.tx_circ_total_released_size; 
	
	//clear last block of history data in circular buffer
#if(NUM_CFR_ADVANCED_SAMPLES)
		vecclr((void*)(CIRC_END_ADDRESS+1-BYTE2UNIT(NUM_CFR_ADVANCED_SAMPLES*4)), ALIGN128(NUM_CFR_ADVANCED_SAMPLES*4)/128);
#endif
		
	dfe_ctrl.tx_timing_offset += dfe_ctrl.tx_timing_offset_total; //for start of FDD and start of each TDD window. pick up offset from previous window.
	dfe_ctrl.tx_timing_offset_total = 0;
}

void tx_ppln_stop()
{
	#if(!BYPASS_IFFT)
	unsigned int ifft_in_buf_wr_idx = dfe_ctrl.ifft_in_buf_wr_idx;
	dfe_ctrl.tx_ppln_state = LAST_SYM_IFFT_IN;
	dfe_ctrl.buf_stat_ifft_in[dfe_ctrl.ifft_in_buf_wr_idx] |= BUF_STAT_BIT_FIELD_LAST;
	
	#else
	dfe_ctrl.tx_ppln_state = LAST_SYM_CFR_OUT;
	#endif
}

void tx_sym_counter_update()
{
	dfe_ctrl.num_tx_symbols_counter += NUM_SYMBOLS_PER_DMA;
	unsigned int num_tx_symbols = dfe_ctrl.num_tx_symbols_curr_entry;
	if(dfe_ctrl.num_tx_symbols_counter == num_tx_symbols)
	{
		dfe_ctrl.num_tx_symbols_counter = 0;
		
		unsigned int num_tx_symbols_word = get_tx_pattern_size();
		unsigned int skip1 = GET_SKIP1(num_tx_symbols_word);
		advance_to_next_tx_pattern_size();
		if(skip1)
		{
			advance_tx_sym_idx(skip1);
			tx_ppln_stop();
			return;
		}

		num_tx_symbols_word = get_tx_pattern_size();
		if(num_tx_symbols_word & PATTERN_SIZE_VALID_MASK) 
		{
			if(GET_SKIP0(num_tx_symbols_word))
			{
				tx_ppln_stop();  //do not advance tx syn idx here, the advancing is done in DMA start when ppln resumes for next pattern/slot
			}
			else
			{
				num_tx_symbols = GET_NUM_SYMBOLS(num_tx_symbols_word);
				if(num_tx_symbols)
				{
					dfe_ctrl.num_tx_symbols_curr_entry = num_tx_symbols;
				}
				else
				{
					tx_ppln_stop();
				}
			}
		}
		else
		{
			tx_ppln_stop();
		}
	}
}

void ack_phy_tx_buf_rdy(unsigned int buf_idx)
{
#if HIGHPHY_LOWPHY_IF_TYPE == IF_TYPE_HOST_VSPA
	host_sym_rdy_clr(buf_idx);
	host_sym_tdflag_clr(buf_idx);  //clr timedomain flag bit as well
#else
	unsigned int rdy_bit_loc = FLAG_TX_SYM_BUF_RDY(ANTENNA_ID_TX, __min(MAX_NUM_TX_SYM_IN_BUF,dfe_ctrl.tx_num_sym_in_buf), buf_idx);
	clr_axi_slave(rdy_bit_loc);
	host_sym_rdy_clr_bitloc(rdy_bit_loc);
	axi_flag_tx_symbol_consumed[0] = rdy_bit_loc;
	axi_flag_tx_symbol_consumed[1] = 0;
	dmac_enable(DMAC_WR | DMA_CHAN_HIGH_PRIORITY, 8, ADDR_AXI_FLAG_CORE(PHY_TX_CORE_ID), UNIT2BYTE((uint32_t)axi_flag_tx_symbol_consumed));					
#endif
}


#if(!BYPASS_IFFT)
unsigned int run_ifft()
{
	/****************************************************************************************************************
	 * iFFT
	 * If iFFT input data is ready and iFFT output buffer has available free space, do iFFT
	 ******************************************************************************************************************/
	return 0;   //1024 iFFT is done right after TX symbol DMA finishes, not here
}
#endif


unsigned int get_available_size_for_ifft_out()
{
	unsigned int tx_circ_total_released_size = dfe_ctrl.tx_circ_total_released_size;
	unsigned int tx_circ_total_produced_size = dfe_ctrl.tx_circ_total_produced_size;

	
	return (IFFT_CIRC_SIZE+tx_circ_total_released_size-tx_circ_total_produced_size);
}


unsigned int get_tx_1block_from_cfr_out()
{
	
	if(LPHY_ONLY_CONFIG)
		return 0;  //if LowPHY only, do not DMA blocks to the following modules.
	
	if(dfe_ctrl.size_cfr_out_valid == 0)
		return 0;

	struct_buf_stat_td* wr_ptr_buf_stat_2xup1_in = dfe_ctrl.wr_ptr_buf_stat_2xup1_in;
	unsigned int buf_stat_size = wr_ptr_buf_stat_2xup1_in->buf_stat_size;
	if(buf_stat_size & BUF_STAT_SIZE_BUF_VALID_MASK) //a block is already in stat struct. no need to push a new block. otherwise a block is lost
		return 0;
	
	unsigned int size;
	if(dfe_ctrl.size_cfr_out_valid >= (TX_BLOCK_SIZE + (NUM_CFR_ADVANCED_SAMPLES*4)))
	{	//full size block
		size = TX_BLOCK_SIZE;
		dfe_ctrl.size_cfr_out_valid -= TX_BLOCK_SIZE;
		if((dfe_ctrl.size_cfr_out_valid == 0) && (dfe_ctrl.tx_ppln_state == LAST_SYM_CFR_OUT))
		{	//last block, full size
			dfe_ctrl.tx_ppln_state = LAST_SYM_COPY_TO_CORE_A;
			buf_stat_size |= BUF_STAT_SIZE_BUF_LAST_MASK;   	
			
		}
	}
	else if(dfe_ctrl.tx_ppln_state == LAST_SYM_CFR_OUT)
	{	//last block, not full size
		dfe_ctrl.tx_ppln_state = LAST_SYM_COPY_TO_CORE_A;
		buf_stat_size |= BUF_STAT_SIZE_BUF_LAST_MASK;   	
		size = dfe_ctrl.size_cfr_out_valid;
		dfe_ctrl.size_cfr_out_valid = 0;
		unsigned size_clr = TX_BLOCK_SIZE-size;
		mem_clr((void*)(dfe_ctrl.read_ptr_copy_to_2xup1 + BYTE2UNIT(size)), size_clr);
	}
	else
		return 0;
	
	#ifdef LOG_TYPE_COPY_BLOCK_TO_CORE_A
	log_2int(LOG_TYPE_COPY_BLOCK_TO_CORE_A|(get_timestamp32()&0x00FFFFF0)|(dfe_ctrl.tx_ppln_state), (size<<16)|dfe_ctrl.size_cfr_out_valid);
	#endif
	
	wr_ptr_buf_stat_2xup1_in->buf_addr[0] = dfe_ctrl.read_ptr_copy_to_2xup1;
	unsigned int size_pre = buf_stat_size & BUF_STAT_SIZE_BUF_SIZE_MASK;
	buf_stat_size &= ~BUF_STAT_SIZE_BUF_SIZE_MASK;
	buf_stat_size |= BUF_STAT_SIZE_BUF_VALID_MASK | size;
	
	unsigned int offset_in_sframe = wr_ptr_buf_stat_2xup1_in->offset_in_sframe;
	offset_in_sframe += size_pre;
	unsigned int size_sysperiod = SIZE_PER_SYSPERIOD(BASEBAND_SPS_TX);
	if (offset_in_sframe >= size_sysperiod)   //check next block sample idx
	{
		offset_in_sframe -= size_sysperiod;		//next block is first block of a sframe
		buf_stat_size |= BUF_STAT_SIZE_BUF_FIRST_SFRAME_MASK;
	}
	wr_ptr_buf_stat_2xup1_in->offset_in_sframe = offset_in_sframe;
	dfe_ctrl.read_ptr_copy_to_2xup1 = circ_addr_advance(dfe_ctrl.read_ptr_copy_to_2xup1, size);
	wr_ptr_buf_stat_2xup1_in->buf_stat_size = buf_stat_size;   //in !T1R, 2xup1 buffer struct is only 1 stuct, no need to increase rd/wr pointer
	return 0;
}

#if (!BYPASS_IFFT)

void tx_circ_data_inc(unsigned int data_size)
{
	dfe_ctrl.tx_circ_total_produced_size += data_size;
	
	dfe_ctrl.size_cfr_out_valid += data_size;
}

#if POINTS_FFT<TX_BLOCK_SIZE
#define MIN_TX_BITREV_PROCESS_SIZE		TX_BLOCK_SIZE		//bytes
#else
#define MIN_TX_BITREV_PROCESS_SIZE		POINTS_FFT		//bytes
#endif

void check_ippu_bitrev_done_tx_windowing()
{
	unsigned int buf_stat = dfe_ctrl.buf_stat_ifft_in[dfe_ctrl.ifft_in_buf_rd_idx];
	unsigned int data_size = buf_stat & BUF_STAT_BIT_FIELD_SYM_SIZE_MASK;
	unsigned int tx_bitrev_progress = tx_bitrev_progress_ippu;
	if(tx_bitrev_progress == BITREV_STARTED)
	{
		return;   //bitrev in progress
	}
	else if(tx_bitrev_progress == BITREV_DONE)
	{
		tx_bitrev_progress_ippu = BITREV_NOT_STARTED;
		
		if(!(buf_stat&(BUF_STAT_BIT_FIELD_BITREV_INP|BUF_STAT_BIT_FIELD_TIMEDOMAIN)))
		{
			//first bitrev block, do tx windowing
			#ifdef LOG_TYPE_TX_WINDOWING_START
			log_2int(TYPE_STAMPED(LOG_TYPE_TX_WINDOWING_START), buf_stat);
			#endif
			
			unsigned int* dst = (unsigned int*)dfe_ctrl.write_ptr_ifft_out;
			int ifft_cp_size = data_size - POINTS_FFT*4;
			
			if(ifft_cp_size>=0)
			txWindowing_circ_w16((cfixed16_t *)dst, tx_windowing_history_buff, (float16_t*)real_window, ifft_cp_size/4, (unsigned int *restrict)iFFTout_CFRinout, IFFT_CIRC_SIZE);
			
			buf_stat |= BUF_STAT_BIT_FIELD_BITREV_INP;
		}
		unsigned int process_size = __min(data_size, MIN_TX_BITREV_PROCESS_SIZE);
		data_size -= process_size;
		dfe_ctrl.write_ptr_ifft_out = circ_addr_advance(dfe_ctrl.write_ptr_ifft_out, process_size);
		if(data_size == 0)
		{
			//last block bitrev
			if(buf_stat & BUF_STAT_BIT_FIELD_LAST)
			{
				memclr32((void*)tx_windowing_history_buff,(size_t)(SIZEOF8(tx_windowing_history_buff)/4));
				dfe_ctrl.tx_ppln_state = LAST_SYM_CFR_OUT;	
				
				//for the last block, increase or decrease samples due to timing offset compensation.
				//first pad 0 from current circ buffer pointer with a block of 0 to clear the garbage which may be sent to AXIQ
				int timing_offset = -dfe_ctrl.tx_timing_offset_total*4;
				if(timing_offset > 0)
				{
					mem_clr_circ((void*)dfe_ctrl.write_ptr_ifft_out, timing_offset, iFFTout_CFRinout, IFFT_CIRC_SIZE);
				}
				
				process_size += timing_offset;
			}

			dfe_ctrl.buf_stat_ifft_in[dfe_ctrl.ifft_in_buf_rd_idx] = 0;
			dfe_ctrl.ifft_in_buf_rd_idx = (dfe_ctrl.ifft_in_buf_rd_idx + 1) % NUM_IFFT_IN_BUFFERS;
			#if NUM_IFFT_IN_BUFFERS == 1
			PHY_DFE_interf_tx_check_dma_start(); //start DMA right after the last bitrev block done if only one tx symbol buffer used
			#endif
		}
		else
		{
			buf_stat = ((buf_stat & (~BUF_STAT_BIT_FIELD_SYM_SIZE_MASK)) | data_size) + (1<<BUF_STAT_BIT_FIELD_TX_SYM_BUF_IDX_IDX);
			dfe_ctrl.buf_stat_ifft_in[dfe_ctrl.ifft_in_buf_rd_idx] = buf_stat;
		}
		tx_circ_data_inc(process_size);
		#ifdef LOG_TYPE_CIRC_PRODUCED
		log_2int(TYPE_STAMPED(LOG_TYPE_CIRC_PRODUCED), (process_size<<16)|data_size);
		#endif
	}	
}

void start_ippu_bitrev_tx()
{

	unsigned int buf_stat = dfe_ctrl.buf_stat_ifft_in[dfe_ctrl.ifft_in_buf_rd_idx];
	unsigned int data_size = buf_stat & BUF_STAT_BIT_FIELD_SYM_SIZE_MASK;
	unsigned int size_for_ifft_out = get_available_size_for_ifft_out();
	unsigned int process_size = __min(data_size, MIN_TX_BITREV_PROCESS_SIZE);
	
	//for last block in last symbol, consider the timing offset compensation size
	unsigned int check_size = process_size;
	int timing_offset_compensation_size = -dfe_ctrl.tx_timing_offset_total*4;
	if((process_size == data_size) && (buf_stat & BUF_STAT_BIT_FIELD_LAST) && (timing_offset_compensation_size > 0))
		check_size += timing_offset_compensation_size;
		
	if((buf_stat & BUF_STAT_BIT_FIELD_FULL) && (size_for_ifft_out >= check_size) && (tx_bitrev_progress_ippu == BITREV_NOT_STARTED))
	{
		unsigned int* dst = (unsigned int*)dfe_ctrl.write_ptr_ifft_out;
		unsigned int* src = (unsigned int*)sym_queue_tx[dfe_ctrl.ifft_in_buf_rd_idx];
		unsigned int bitrev_block_idx = (buf_stat >> BUF_STAT_BIT_FIELD_TX_SYM_BUF_IDX_IDX)&BUF_STAT_BIT_FIELD_TX_SYM_BUF_IDX_MASK;

		if(!(buf_stat & (BUF_STAT_BIT_FIELD_TIMEDOMAIN|BUF_STAT_BIT_FIELD_IDLE)))
		{
			#ifdef LOG_TYPE_TX_IPPU_START
			log_2int(TYPE_STAMPED(LOG_TYPE_TX_IPPU_START), process_size);
			#endif

			unsigned int sample_offset = (POINTS_FFT*2 - data_size/4) % POINTS_FFT;
			tx_bitrev_progress_ippu = BITREV_STARTED;
			start_tx_ippu_bitrev_reversIn_linearOut((unsigned int)dst, (unsigned int)src, (unsigned int)iFFTout_CFRinout, IFFT_CIRC_SIZE, process_size/4, sample_offset, (unsigned int)&tx_bitrev_progress_ippu, (unsigned int)&tx_bitrev_done);
		}
		else
		{
			unsigned int usample = (buf_stat >> BUF_STAT_BIT_FIELD_USAMPLE_IDX) & BUF_STAT_BIT_FIELD_USAMPLE_MASK;
#if TXDCS_FDD
			if(buf_stat & BUF_STAT_BIT_FIELD_IDLE)
			{
				mem_clr_circ((void*)dst, process_size, iFFTout_CFRinout, IFFT_CIRC_SIZE);	
			}
			else
			{
				memcpy_dst_circ((void*)dst, (void*)&src[usample+bitrev_block_idx*MIN_TX_BITREV_PROCESS_SIZE/4], process_size, iFFTout_CFRinout, IFFT_CIRC_SIZE);	
			}
#else
			memcpy_dst_circ((void*)dst, (void*)&src[usample+bitrev_block_idx*MIN_TX_BITREV_PROCESS_SIZE/4], process_size, iFFTout_CFRinout, IFFT_CIRC_SIZE);	
#endif
			tx_bitrev_progress_ippu = BITREV_DONE;  //to simulate bitrev done as bitrev is not needed for time domain
		}
	}
}

#endif

unsigned int check_phy_tx_buf_rdy(unsigned int buf_idx)
{
	#if RUN_IN_SIMULATOR
	return 1;
	#endif
	
	if(__ip_read(IP_IDX_DFE_MODE_HI, DFE_MODE_TX_HANDSHAKE_BYPASS))//(dfe_ctrl.dfe_mode_config_hi&DFE_MODE_TX_HANDSHAKE_BYPASS)
		return 1;
	
#if HIGHPHY_LOWPHY_IF_TYPE == IF_TYPE_HOST_VSPA
	return host_sym_rdy(buf_idx);
	
#else
	unsigned int rdy_bit_loc = FLAG_TX_SYM_BUF_RDY(ANTENNA_ID_TX, __min(MAX_NUM_TX_SYM_IN_BUF,dfe_ctrl.tx_num_sym_in_buf), buf_idx);
	return (check_axi_slave(rdy_bit_loc) | host_sym_rdy_bitloc(rdy_bit_loc));
#endif
}

//return value: 0-no ifft, 1-ifft is done
unsigned int tx_sym_ready_processing()
{
	#ifdef LOG_TYPE_TX_DMA_END
	log_2int(TYPE_STAMPED(LOG_TYPE_TX_DMA_END), dfe_ctrl.num_tx_sym_from_hiphy);
	#endif

	unsigned int ifft_in_buf_wr_idx = dfe_ctrl.ifft_in_buf_wr_idx;
	unsigned int status = 0;
	unsigned int buf_stat = dfe_ctrl.buf_stat_ifft_in[ifft_in_buf_wr_idx];
	
#if TXDCS_FDD
	if(!(buf_stat & BUF_STAT_BIT_FIELD_IDLE))
	{
		dfe_ctrl.num_tx_sym_from_hiphy += NUM_SYMBOLS_PER_DMA;
	}
#else
	dfe_ctrl.num_tx_sym_from_hiphy += NUM_SYMBOLS_PER_DMA;
#endif
	
#if(!BYPASS_IFFT)
	unsigned int* src = (unsigned int*)sym_queue_tx[ifft_in_buf_wr_idx];
	unsigned int sym_idx_per_cp_period = dfe_ctrl.tx_sym_idx_per_sframe % NUM_SYM_PER_CP_PERIOD;
	unsigned short num_sc_in_a_sym = GET_NUM_SC;

#if TXDCS_FDD
	if(!(buf_stat&(BUF_STAT_BIT_FIELD_TIMEDOMAIN|BUF_STAT_BIT_FIELD_IDLE)))
#else
	if(!(buf_stat&BUF_STAT_BIT_FIELD_TIMEDOMAIN))
#endif
	{
		//for freq domain symbols, if decompression is enabled, do decompression
		
#if !PHASE_COMPENSATION_DISABLE
		//do phase compensation
		int* pcoeff = (int*)phcom_coeff_tx[sym_idx_per_cp_period];
		phcom_asm(src, src, (float*)pcoeff, (num_sc_in_a_sym*4+127)/128);
#endif
			
#if !BYPASS_IFFT
		//1. use memcpy to split symbol, 1st half to tail, 2nd half head, mid part cleared to 0
		CODE_MEA_IFFT(unsigned int ts_ifft = get_timestamp32();)

		void* next_dst = memcpy_dst_circ((void*)(&src[POINTS_FFT-num_sc_in_a_sym/2]), (void*)src, num_sc_in_a_sym*4, src, POINTS_FFT*4);
		mem_clr(next_dst, POINTS_FFT*4-num_sc_in_a_sym*4);
		
		//2. call iFFT to transform inplace at src.
		#if POINTS_FFT == 1024
		ifftDIF1024_sc128_hfx_hfx_vspa2((cfixed16_t*)src, (cfixed16_t*)src, (cfixed16_t*)src, BYTE2UNIT(POINTS_FFT*4)); //unit size.
		#elif POINTS_FFT == 512
		ifftDIF512_hfx_hfx((cfixed16_t*)src, (cfixed16_t*)src, (cfixed16_t*)src, BYTE2UNIT(POINTS_FFT*4)); //unit size.
		#else
		ERROR
		#endif
		
		status = 1;
		CODE_MEA_IFFT(dfe_ctrl.cycle_count_ifft = (get_timestamp32() - ts_ifft);)
#endif
	}
	
	dfe_ctrl.buf_stat_ifft_in[ifft_in_buf_wr_idx] = (buf_stat | BUF_STAT_BIT_FIELD_FULL) & (~(BUF_STAT_BIT_FIELD_TX_SYM_BUF_IDX_MASK<<BUF_STAT_BIT_FIELD_TX_SYM_BUF_IDX_IDX));
		
#else
	unsigned int src_size = dfe_ctrl.tx_sym_src_size;
	dfe_ctrl.tx_circ_total_produced_size += src_size;
	
	dfe_ctrl.write_ptr_ifft_out = circ_addr_advance(dfe_ctrl.write_ptr_ifft_out, src_size);
	dfe_ctrl.size_cfr_out_valid += src_size;
#endif
	
#if HIGHPHY_LOWPHY_IF_TYPE == IF_TYPE_VSPA_VSPA
	if(0)
#endif
	{
		unsigned int tx_sym_buf_idx = (buf_stat >> BUF_STAT_BIT_FIELD_TX_SYM_BUF_IDX_IDX) & BUF_STAT_BIT_FIELD_TX_SYM_BUF_IDX_MASK;
		for(int i=0;i<NUM_SYMBOLS_PER_DMA;i++)
			ack_phy_tx_buf_rdy(tx_sym_buf_idx + i);
	}
	
	advance_tx_sym_idx(NUM_SYMBOLS_PER_DMA);
	
#if TXDCS_FDD

#else
	if(!FDD_MODE_TX)
		tx_sym_counter_update();
#endif
	
	dfe_ctrl.ifft_in_buf_wr_idx = (ifft_in_buf_wr_idx + 1) % NUM_IFFT_IN_BUFFERS;
	return status;
}

//return value: 1 - ifft is done, 0 - no ifft.
unsigned int PHY_DFE_interf_tx_check_dma_finish()
{
	/****************************************************************************************************************
	 * read eCPRI symbols from eCPRI host
	 * If decompression is enable, do decompression which will read the symbol directly from eCPRI host.
	 * If decompression is disabled, use DMA to copy the symbol, DMA will convert the samples from 2'complement integer to Half_fixed.
	 ******************************************************************************************************************/
	unsigned int status = 0;
	#if(NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM == 2)
	unsigned int dma_chan_mask = (1<<DMA_CHAN_FETCH_DU_TX_SYMBOLS(0)) | (1<<DMA_CHAN_FETCH_DU_TX_SYMBOLS(1));	
	#elif (NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM == 1)
	unsigned int dma_chan_mask = (1<<DMA_CHAN_FETCH_DU_TX_SYMBOLS(0));
	#else
	ERROR
	#endif
	
	if(GET_FAST_FLAGS(FAST_FLAG_TX_SYM_DMA_INP_MASK) && (*dfe_ctrl.tx_sym_dma_flag_addr != SPECIAL_SAMPLE_PATTERN))
	{
		CLR_FAST_FLAGS(FAST_FLAG_TX_SYM_DMA_INP_MASK);
		status = tx_sym_ready_processing();
	}
	return status;
}

//return value: 1 - fetched a symbol, 0 - no symbol fetched.
unsigned int PHY_DFE_interf_tx_check_dma_start()
{
	
	unsigned int tx_fdd = FDD_MODE_TX;
	unsigned int status = 0;
	unsigned int flag_AtoB = dfe_ctrl.flag_AtoB;
	unsigned int num_tx_symbols_counter = dfe_ctrl.num_tx_symbols_counter;
	
#if !TXDCS_FDD
	if(!tx_fdd)
	{
		//flow control, when tx allowed is low, slowly process TX data to minimize impact to RX path
		unsigned int tx_ppln_state = dfe_ctrl.tx_ppln_state;
		
		if(tx_ppln_state == WAITING_TX_ALLOWED)
		{
		#if !RUN_IN_SIMULATOR
			//if current time is more than 1 symbol away to tx allow rising edge, do not start TX
			if((get_ts_txallowed() - get_timestamp32()) > NUM_CYCLES_FETCH_TX_SYMBOL_BEFORE_TXALLOWED)
				return status;
		#endif
			dfe_ctrl.tx_ppln_state = tx_ppln_state = READY;
		}

		else if(tx_ppln_state != READY)
		{
			if(dfe_ctrl.num_tx_ppln_togglings == dfe_ctrl.num_tx_windows_togglings)
			{
				unsigned int pttn_tx_symbols = get_tx_pattern_size();
				if(pttn_tx_symbols & PATTERN_SIZE_VALID_MASK)
				{
					unsigned int num_tx_symbols = GET_NUM_SYMBOLS(pttn_tx_symbols);
					unsigned int skip0 = GET_SKIP0(pttn_tx_symbols);
					unsigned int skip1 = GET_SKIP1(pttn_tx_symbols);
					if(num_tx_symbols)
					{
						advance_tx_sym_idx(skip0);
						pipeline_reset_tx();
						dfe_ctrl.tx_ppln_state = WAITING_TX_ALLOWED;
						dfe_ctrl.num_tx_ppln_togglings += 2;
						
						unsigned int flag_first = BUF_STAT_SIZE_BUF_FIRST_MASK;
						unsigned int starting_offset_in_sframe = 4*convert_sym_idx_to_sample_idx(dfe_ctrl.tx_sym_idx_per_sframe);
						if(starting_offset_in_sframe <= dfe_ctrl.buf_stat_2xup1_in[0].offset_in_sframe)
						{
							flag_first |= BUF_STAT_SIZE_BUF_FIRST_SFRAME_MASK;
						}
						dfe_ctrl.buf_stat_2xup1_in[0].offset_in_sframe = starting_offset_in_sframe;
						dfe_ctrl.buf_stat_2xup1_in[0].buf_stat_size = flag_first;
						
						dfe_ctrl.num_tx_symbols_curr_entry=num_tx_symbols;
					}
					else
					{
						advance_tx_sym_idx(skip0+skip1);
						advance_to_next_tx_pattern_size();
					}
				}
				else
				{
					//reached end of pattern entries, check TDD STOP
					if(GET_FAST_FLAGS(FAST_FLAG_TX_TDDSTOP_MASK))
					{
						CLR_FAST_FLAGS(FAST_FLAG_TX_TDDSTOP_MASK);
						dfe_ctrl.tx_sym_idx_per_sframe = 0;
						dfe_ctrl.buf_stat_2xup1_in[0].offset_in_sframe = SIZE_PER_SYSPERIOD(BASEBAND_SPS_TX);
					}
				}
			}
			
			return status;
		}
	}


	else
#endif
	{
	}

	unsigned int ifft_in_buf_wr_idx = dfe_ctrl.ifft_in_buf_wr_idx;
	if(!GET_FAST_FLAGS(FAST_FLAG_TX_SYM_DMA_INP_MASK))
	{
		unsigned int tx_sym_idx_per_sframe = dfe_ctrl.tx_sym_idx_per_sframe;
		unsigned int sym_size,in_sym_addr,dest_sym_addr;
		
		#if(!BYPASS_IFFT)
		if((!(dfe_ctrl.buf_stat_ifft_in[ifft_in_buf_wr_idx] & BUF_STAT_BIT_FIELD_FULL)))
			
		#else
		sym_size = SIZE_SYMBOL_DATA_TIME_DOMAIN_LCP+(NUM_SYMBOLS_PER_DMA-1)*SIZE_SYMBOL_DATA_TIME_DOMAIN_SCP;
		if(tx_sym_idx_per_sframe % NUM_SYM_PER_CP_PERIOD)
			sym_size = SIZE_SYMBOL_DATA_TIME_DOMAIN_SCP*NUM_SYMBOLS_PER_DMA;
		
		if(get_available_size_for_ifft_out() >= sym_size) 	
		#endif
			
		{
			
			
		#if TXDCS_FDD
			if(!tx_fdd)
			{
				unsigned int tx_pattern_size = get_tx_pattern_size();
				unsigned int skip0 = GET_SKIP0(tx_pattern_size);
				unsigned int num_tx_symbols = GET_NUM_SYMBOLS(tx_pattern_size);
				unsigned int skip1 = GET_SKIP1(tx_pattern_size);
				
				if(tx_pattern_sym_idx < skip0)
				{
					tx_sym_type = BUF_STAT_BIT_FIELD_IDLE;
				}
				else if(tx_pattern_sym_idx < (skip0+num_tx_symbols))
				{
					tx_sym_type = 0;
				}
				else if(tx_pattern_sym_idx < (skip0+num_tx_symbols+skip1))
				{
					tx_sym_type = BUF_STAT_BIT_FIELD_IDLE;
				}
		
				tx_pattern_sym_idx++;
				
				if (tx_pattern_sym_idx == (skip0+num_tx_symbols+skip1))
				{
					advance_to_next_tx_pattern_size();
					tx_pattern_sym_idx = 0;
				}
			}
			else
			{
				tx_sym_type = 0;
			}
		#endif	

			unsigned int handshake_bit_idx = tx_sym_idx_per_sframe % MAX_NUM_TX_SYM_IN_BUF;
			
#if TXDCS_FDD
			if((tx_sym_type == BUF_STAT_BIT_FIELD_IDLE) || check_phy_tx_buf_rdy(tx_sym_buf_idx))
#else
			if(check_phy_tx_buf_rdy(handshake_bit_idx))	
#endif
			{
				unsigned int buf_stat_ifft_in = handshake_bit_idx << BUF_STAT_BIT_FIELD_TX_SYM_BUF_IDX_IDX;
				in_sym_addr = get_tx_sym_buf_addr(tx_sym_idx_per_sframe % dfe_ctrl.tx_num_sym_in_buf);
				unsigned int usize = in_sym_addr & (UNIT2BYTE(AXI_BURST_SIZE)-1);
				in_sym_addr -= usize;  //align in_sym_addr to AXI_BURST_SIZE

#if(!BYPASS_IFFT)
				buf_stat_ifft_in |= ((usize>>2)<<BUF_STAT_BIT_FIELD_USAMPLE_IDX);
				dest_sym_addr = UNIT2BYTE((uint32_t)sym_queue_tx[ifft_in_buf_wr_idx]);
				unsigned int tx_sym_idx_per_cp_period = tx_sym_idx_per_sframe % NUM_SYM_PER_CP_PERIOD;
				unsigned int cp_size = get_cp_size(tx_sym_idx_per_cp_period);
				sym_size = cp_size + POINTS_FFT*4;
				
				
				unsigned int sym_size_with_timing_offset = sym_size + dfe_ctrl.tx_timing_offset*4;
				#if POINTS_FFT == 4096
				sym_size_with_timing_offset = ROUND(sym_size_with_timing_offset,128);  //4096 IFFT bitrev only support line aligned CP size.
				dfe_ctrl.tx_timing_offset_total += (sym_size_with_timing_offset - sym_size)/4;
				#else
				dfe_ctrl.tx_timing_offset_total += dfe_ctrl.tx_timing_offset;
				#endif
				dfe_ctrl.tx_timing_offset = 0;
				buf_stat_ifft_in |= sym_size_with_timing_offset;
				
				
				unsigned int num_sc = GET_NUM_SC;
				if(/*host_sym_tdflag(tx_sym_buf_idx) ||*/ (__ip_read(IP_IDX_DFE_MODE_HI, DFE_MODE_OPTION8)))
				{
					buf_stat_ifft_in |= BUF_STAT_BIT_FIELD_TIMEDOMAIN;
				}
				else
				{
					sym_size = num_sc*4;
				}
				
#if TXDCS_FDD
				buf_stat_ifft_in |= tx_sym_type;
#else
				
#endif

				unsigned int dumping_flag = 0;
				if(dfe_ctrl.tx_freq_dump_flag & DUMP_INJECT_MODE_ONCE)
				{
					if(tx_sym_idx_per_sframe < dfe_ctrl.tx_freq_dump_num_sym)
					{
						dumping_flag = 0xFFFFFFFF;
					}
					else
					{
						dfe_ctrl.tx_freq_dump_flag = 0;
					}
				}

				dfe_ctrl.buf_stat_ifft_in[ifft_in_buf_wr_idx] = buf_stat_ifft_in;

				unsigned int dma_mode = DMAC_RDC|DMAC_MBRE;

				//in the following cases, no need to fetch symbol data from DDR
				//1. during single tone, this is to enable 6T6R single tone
				//2. when symbol buffer is in DFE
				unsigned int dma_size = sym_size + usize;
				dfe_ctrl.tx_sym_src_size = sym_size;		
				unsigned int* tx_sym_dma_flag_addr = (unsigned int*)BYTE2UNIT(dest_sym_addr + dma_size - 4);  //point to the last sample of input symbol
				dfe_ctrl.tx_sym_dma_flag_addr = tx_sym_dma_flag_addr;
				dfe_ctrl.tx_sym_idx_request_for_host = 0x80000000|tx_sym_idx_per_sframe;
				
#if TXDCS_FDD
				#if HIGHPHY_LOWPHY_IF_TYPE == IF_TYPE_VSPA_VSPA
				if(0)
				#else
				if(!((dfe_ctrl.tx_single_tone_amplitude[0]&SINGLE_TONE_VALID_FLAG)  || (tx_sym_type == BUF_STAT_BIT_FIELD_IDLE) ))
				#endif
					
#else
				#if HIGHPHY_LOWPHY_IF_TYPE == IF_TYPE_VSPA_VSPA
				if(0)
				#else
				if(!(dfe_ctrl.tx_single_tone_amplitude[0]&SINGLE_TONE_VALID_FLAG))
				#endif
#endif
				{
					*tx_sym_dma_flag_addr = SPECIAL_SAMPLE_PATTERN;
					
					#if(NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM == 2)
					unsigned int size1 = __min(ALIGN(dma_size/2, UNIT2BYTE(AXI_BURST_SIZE)), dma_size);
					dmac_enable(dma_mode | DMA_CHAN_FETCH_DU_TX_SYMBOLS(1), size1, in_sym_addr, dest_sym_addr);
					unsigned int size2 = dma_size - size1;
					dmac_enable(dma_mode | DMA_CHAN_FETCH_DU_TX_SYMBOLS(0), size2, in_sym_addr+size1, dest_sym_addr+size1);
					#else
					dmac_enable(dma_mode | DMA_CHAN_FETCH_DU_TX_SYMBOLS(0), dma_size, in_sym_addr, dest_sym_addr);
					#endif
				}
				else
				{
					*tx_sym_dma_flag_addr = 0;
				}
				
				SET_FAST_FLAGS(FAST_FLAG_TX_SYM_DMA_INP_MASK);

				#ifdef LOG_TYPE_TX_SYMBOL_DMA_START
				log_2int(TYPE_STAMPED(LOG_TYPE_TX_SYMBOL_DMA_START), (dma_size<<16)|tx_sym_idx_per_sframe);
				#endif	

#else
				dfe_ctrl.tx_sym_src_size = sym_size;		

				dfe_ctrl.buf_stat_ifft_in[ifft_in_buf_wr_idx] = buf_stat_ifft_in | sym_size;
				unsigned int dma_size = sym_size + usize;
				dest_sym_addr = UNIT2BYTE(dfe_ctrl.write_ptr_ifft_out) - usize;
				unsigned int* tx_sym_dma_flag_addr = (unsigned int*)BYTE2UNIT(dest_sym_addr + dma_size - 4);  //point to the last sample of input symbol
				dfe_ctrl.tx_sym_dma_flag_addr = tx_sym_dma_flag_addr;
				unsigned int size_to_circ_end = UNIT2BYTE(CIRC_END_ADDRESS + 1) - dest_sym_addr;
				
				//in the following cases, no need to fetch symbol data from DDR
				//1. during single tone
				//2. when symbol buffer is in DFE
				#if HIGHPHY_LOWPHY_IF_TYPE == IF_TYPE_VSPA_VSPA
				if(0)
				#else
				if(!(dfe_ctrl.tx_single_tone_amplitude[0]&SINGLE_TONE_VALID_FLAG))
				#endif
				{
					
				*tx_sym_dma_flag_addr = SPECIAL_SAMPLE_PATTERN;
				if(size_to_circ_end >= dma_size)
				{
					unsigned int ts_dma = get_timestamp32();
					#if(NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM == 2)
						unsigned int size1 = __min(ALIGN(dma_size/2, UNIT2BYTE(AXI_BURST_SIZE)), dma_size);
						dmac_enable(DMAC_RDC | DMAC_TRIG_VCPU | DMAC_MBRE | DMA_CHAN_FETCH_DU_TX_SYMBOLS(0), size1, in_sym_addr, dest_sym_addr);
						unsigned int size2 = dma_size - size1;
						dmac_enable(DMAC_RDC | DMAC_TRIG_VCPU | DMAC_MBRE | DMA_CHAN_FETCH_DU_TX_SYMBOLS(1), size2, in_sym_addr+size1, dest_sym_addr+size1);
					#elif(NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM == 1)
						dmac_enable(DMAC_RDC | DMAC_TRIG_VCPU | DMAC_MBRE | DMA_CHAN_FETCH_DU_TX_SYMBOLS(0), dma_size, in_sym_addr, dest_sym_addr);
					#else
						ERROR
					#endif
				}
				else
				{
					unsigned int size1 = size_to_circ_end;
					unsigned int size2 = dma_size - size1;

					#if(NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM == 2)
					unsigned int size10=__min(ALIGN(size1/2,UNIT2BYTE(AXI_BURST_SIZE)), size1);
					dmac_enable(DMAC_RDC | DMAC_MBRE | DMA_CHAN_FETCH_DU_TX_SYMBOLS(0), size10, in_sym_addr, dest_sym_addr);
					unsigned int size11=size1-size10;
					dmac_enable(DMAC_RDC | DMAC_MBRE | DMA_CHAN_FETCH_DU_TX_SYMBOLS(1), size11, in_sym_addr+size10, dest_sym_addr+size10);

					unsigned int size20=__min(ALIGN(size2/2,UNIT2BYTE(AXI_BURST_SIZE)), size2);
					dmac_enable(DMAC_RDC | DMAC_TRIG_VCPU | DMAC_MBRE | DMA_CHAN_FETCH_DU_TX_SYMBOLS(0), size20, in_sym_addr+size1, UNIT2BYTE((unsigned int)iFFTout_CFRinout));				
					unsigned int size21=size2-size20;
					dmac_enable(DMAC_RDC | DMAC_TRIG_VCPU | DMAC_MBRE | DMA_CHAN_FETCH_DU_TX_SYMBOLS(1), size21, in_sym_addr+size1+size20, UNIT2BYTE(((unsigned int)iFFTout_CFRinout))+size20);				

					
					#else
					dmac_enable(DMAC_RDC | DMAC_MBRE | DMA_CHAN_FETCH_DU_TX_SYMBOLS(0), size1, in_sym_addr, dest_sym_addr);
					dmac_enable(DMAC_RDC | DMAC_TRIG_VCPU | DMAC_MBRE | DMA_CHAN_FETCH_DU_TX_SYMBOLS(0), size2, in_sym_addr+size1, UNIT2BYTE((unsigned int)iFFTout_CFRinout));
					#endif
				}		
				#ifdef LOG_TYPE_TX_SYMBOL_DMA_START
				log_2int(TYPE_STAMPED(LOG_TYPE_TX_SYMBOL_DMA_START), (dma_size<<16)|(tx_sym_idx_per_sframe&0xFFFF));
				#endif	
				SET_FAST_FLAGS(FAST_FLAG_TX_SYM_DMA_INP_MASK);
				}
				else
				{
					*tx_sym_dma_flag_addr = 0;
					status = tx_sym_ready_processing();
				}
#endif
			}
			else
			{
				//TX symbol buffer not ready.
				#ifdef LOG_TYPE_TX_HOST_SYM_NOT_READY
				log_2int(TYPE_STAMPED(LOG_TYPE_TX_HOST_SYM_NOT_READY), tx_sym_idx_per_sframe);
				#endif
			}
		}
	}
	return status;
}

//return value: 1=ifft or bitrev is done, 0-no ifft or bitrev is done.
unsigned int tx_processing_symbol_level()
{
	
	unsigned int status = PHY_DFE_interf_tx_check_dma_finish();

	#if !BYPASS_IFFT
	{
		check_ippu_bitrev_done_tx_windowing();
		
		
		status |= run_ifft();
	}
	
	#else
	
	#endif
	
	PHY_DFE_interf_tx_check_dma_start();	//iFFT has consumed DMA output, put it here to start next DMA in time
	
	return status;
}

#endif  //NUM_1T_IN_CORE
