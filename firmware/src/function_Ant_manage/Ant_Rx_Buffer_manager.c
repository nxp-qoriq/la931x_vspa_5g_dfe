// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


/*Update history:
 *Dec 16,2021: added DMA_AXI2DMEM_CONVERT for DMA from RX FIFO to RX ant buffer, before this change, users should provide half_fixed test vector to 0x70000000 for testing
 *             after this change, users need provide 2'complement integer to 0x70000000.  No impact for testing on hardware. 
 *Dec 16,2021: Above updates are based on v1.0
 */

#include "Ant_TxRx_Buffer_manager.h"
#include "host_vspa_msg.h"
#include "global_var.h"

extern struct_dfe_ctrl dfe_ctrl;
extern int reset_stat;

#if(CORE_TYPE==TYPE_CORE_A)
#if(NUM_1R_IN_CORE+OBS_CHAN_ENABLE)
struct_antman_ctrl_rx antman_ctrl_rx ANTMAN_BUF_SECTION;
#define ANTMAN_NEEDED
#endif
#endif

#if(CORE_TYPE==TYPE_CORE_B)
#if(OBS_CHAN_ENABLE)
struct_antman_ctrl_rx antman_ctrl_rx[OBS_CHAN_ENABLE] ANTMAN_BUF_SECTION;
#define ANTMAN_NEEDED
#endif
#endif

#ifdef ANTMAN_NEEDED

void Ant_buffer_rx_axiq_reset()
{
	#if RUN_IN_SIMULATOR
	return;
	#endif

	if(GET_DFE_MODE_HI_BITS(DFE_MODE_RX_DIS))
		return;
	
	//abort currently active and pending AXIQ DMA
	dmac_abort(1<<antman_ctrl_rx.axiq_dma_chan);
	
	//configure a short DMA with PTR_RST to reset AXIQ, 9310 AXIQ can only reset by ptr_rst
	dmac_enable(DMAC_PRST_REQ | DMA_TRIG | DMAC_RDC | DMAC_FIFO_HS | antman_ctrl_rx.axiq_dma_chan, 64, antman_ctrl_rx.axiq_fifo_addr, UNIT2BYTE(antman_ctrl_rx.wr_ptr_buf_stat_ant_rx->buf_addr[0]));
	delay_cycles(10000);
	//if AXIQ is in normal state, this DMA will be pending there, then abort it.
	//if AXIQ is in Flush mode, this DMA will be consumed and Flush mode will be ended, then this abort will take no effect.
	dmac_abort(1<<antman_ctrl_rx.axiq_dma_chan);

	//disable AXIQ, reset AXIQ(LA12xx only), clr errors
	__ip_write(antman_ctrl_rx.axiq_control_gpo, antman_ctrl_rx.axiq_control_enable_bitfield, 0);
	delay_cycles(32);
	__ip_write(antman_ctrl_rx.axiq_control_gpo, antman_ctrl_rx.axiq_control_fifo_rst_clr_err_bitfield, antman_ctrl_rx.axiq_control_fifo_rst_clr_err_bitfield);
	delay_cycles(32);
	__ip_write(antman_ctrl_rx.axiq_control_gpo, antman_ctrl_rx.axiq_control_fifo_rst_clr_err_bitfield, 0);
}

void Ant_buffer_rx_axiq_enable()
{
	//enable AXIQ
	__ip_write(antman_ctrl_rx.axiq_control_gpo, antman_ctrl_rx.axiq_control_enable_bitfield, antman_ctrl_rx.axiq_control_enable_bitfield);
}


void rx_error_handling(unsigned int data_hi, unsigned int data_lo)
{
	if(dfe_ctrl.error_info_MSB32 == 0)
	{
		dfe_ctrl.error_info_LSB32 = data_lo;
		dfe_ctrl.error_info_MSB32 = data_hi;
	}

	log_2int(data_hi, data_lo);
	LOG_STOP;
	
#if CORE_STOP_ON_ERROR && (!SINAD_TEST_ENABLE)
	while(1) 
	{
		check_msg();   //check msg for restart
	}
#endif
	
}


//return value: non-zero enabled, 0 disabled
unsigned int get_rx_allowed_status()
{
	return __min(1,__ip_read(antman_ctrl_rx.axiq_status_gpi, antman_ctrl_rx.rx_allowed_bitfield));
}

unsigned int convert_sample_idx_to_sym_idx(unsigned int sample_idx)
{
	unsigned num_cp_period = sample_idx/NUM_SAMPLE_PER_CP_PERIOD;
	unsigned num_sample_remain = sample_idx % NUM_SAMPLE_PER_CP_PERIOD;
	unsigned num_sym_remain = num_sample_remain / NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP;
	return (num_cp_period*NUM_SYM_PER_CP_PERIOD+num_sym_remain);
}
#if RX_FILTER_DELAY_COMPENSATION
#define NUM_SAMPLE_RX_FILTER_DELAY_COMPENSATION		(512/DOWNSAMPLING_RATIO) //num of sample
#else
#define NUM_SAMPLE_RX_FILTER_DELAY_COMPENSATION		0
#endif

void Ant_buffer_rx_dma_feed()
{
	unsigned int rx_size = antman_ctrl_rx.rx_size;
	unsigned int sym_idx = antman_ctrl_rx.sym_idx;
	unsigned int fdd = FDD_MODE_RX;
	struct_buf_stat_td* wr_ptr_buf_stat_ant_rx = antman_ctrl_rx.wr_ptr_buf_stat_ant_rx;
	unsigned int buf_stat = wr_ptr_buf_stat_ant_rx->buf_stat_size;
	if((antman_ctrl_rx.num_DMA_in_progress < 2) && (!(buf_stat&BUF_STAT_SIZE_BUF_VALID_MASK)))
	{
		unsigned short ant_buf_size = SIZE_RX_ANT_BUFFER;
		unsigned int valid_size = ant_buf_size;
		unsigned int dma_ctrl = DMA_TRIG | DMAC_RDC | DMAC_FIFO_HS; //must not use multi-burst for AXIQ
		unsigned int ext_size = antman_ctrl_rx.rxext_size;
		ext_size = 0;
		unsigned int cur_ext_size = 0;
		
#if RXDCS_FDD
		
#else
		if(!fdd)
		{
			if((rx_size | ext_size) == 0)
			{
				unsigned int num_rx_symbols = get_rx_pattern_size();//dfe_ctrl.tdd_pattern_entry.patterns_size[antman_ctrl_rx.pattern_size_index].num_rx_symbols;
				if(num_rx_symbols & PATTERN_SIZE_VALID_MASK)
				{
					advance_to_next_rx_pattern_size();//antman_ctrl_rx.pattern_size_index = pattern_size_index_next;//advance_pattern_size_idx(&dfe_ctrl.tdd_pattern_entry, antman_ctrl_rx.pattern_size_index);
					unsigned int skip0 = GET_SKIP0(num_rx_symbols);//DOWNSAMPLING_RATIO*4*dfe_ctrl.tdd_pattern_entry.patterns_size[antman_ctrl_rx.pattern_size_index].num_rx_samples_skip0;
					sym_idx += skip0;
					unsigned int skip1 = GET_SKIP1(num_rx_symbols); //DOWNSAMPLING_RATIO*4*dfe_ctrl.tdd_pattern_entry.patterns_size[antman_ctrl_rx.pattern_size_index].num_rx_samples_skip1;
					unsigned int num_sym = GET_NUM_SYMBOLS(num_rx_symbols);
					if(num_sym)
					{
						if(sym_idx >= NUM_SYM_PER_SYSPERIOD)
						{
							sym_idx -= NUM_SYM_PER_SYSPERIOD;		
							buf_stat |= BUF_STAT_SIZE_BUF_FIRST_SFRAME_MASK;
						}
						rx_size = (get_num_samples_in_symbol_idx_range(sym_idx, num_sym)+NUM_SAMPLE_RX_FILTER_DELAY_COMPENSATION)*4*DOWNSAMPLING_RATIO;
						antman_ctrl_rx.offset_in_sframe = convert_sym_idx_to_sample_idx(sym_idx)*4*DOWNSAMPLING_RATIO;
						antman_ctrl_rx.skip1 = skip1;
						dfe_ctrl.rx_win_start_sym_idx = sym_idx;
						
						buf_stat |= BUF_STAT_SIZE_BUF_FIRST_MASK;
						sym_idx += num_sym + skip1;
						antman_ctrl_rx.sym_idx = sym_idx;
					}
					else
					{
						sym_idx += skip1;
						antman_ctrl_rx.sym_idx = sym_idx;
						return;
					}
				}
				else
				{
					//reached end of pattern entries, check TDD STOP
					if(GET_FAST_FLAGS(FAST_FLAG_RX_TDDSTOP_MASK))
					{
						CLR_FAST_FLAGS(FAST_FLAG_RX_TDDSTOP_MASK);
						antman_ctrl_rx.sym_idx = NUM_SYM_PER_SYSPERIOD;
						wr_ptr_buf_stat_ant_rx->buf_stat_size = 0;
					}					
					return;
				}
			}
		
			if(rx_size)
			{
				if(rx_size <= ant_buf_size)
				{
					unsigned int skip1 = antman_ctrl_rx.skip1;
					if(skip1 == 0)
					{
						unsigned int num_rx_symbols = get_rx_pattern_size();
						if(num_rx_symbols & PATTERN_SIZE_VALID_MASK)
						{
							if(!GET_SKIP0(num_rx_symbols))
							{
								unsigned int num_sym = GET_NUM_SYMBOLS(num_rx_symbols);
								skip1 = GET_SKIP1(num_rx_symbols);
								if(num_sym)
								{
									rx_size += get_num_samples_in_symbol_idx_range(sym_idx, num_sym)*4*DOWNSAMPLING_RATIO;
									antman_ctrl_rx.skip1 = skip1;
									advance_to_next_rx_pattern_size();
									sym_idx += num_sym + skip1;
									antman_ctrl_rx.sym_idx = sym_idx;
								}
							}
						}
					}
	
					if(rx_size <= ant_buf_size)
					{
						valid_size = rx_size;
						rx_size = 0;
						buf_stat |= BUF_STAT_SIZE_BUF_LAST_MASK;
						
						{
							dma_ctrl |= DMAC_PRST_REQ;
						}
					}
					else
					{
						rx_size -= ant_buf_size;
					}
				}
				else
				{
					rx_size -= ant_buf_size;
				}
			}
		}
#endif
		
		buf_stat |= (valid_size+cur_ext_size) | BUF_STAT_SIZE_BUF_VALID_MASK;
		unsigned int offset_in_sframe = antman_ctrl_rx.offset_in_sframe;
		antman_ctrl_rx.num_DMA_in_progress++;
		
		unsigned int size_sysperiod = SIZE_PER_SYSPERIOD(RX_AXIQ_SPS)>>antman_ctrl_rx.sps_div;
		if (offset_in_sframe >= size_sysperiod)
		{
			offset_in_sframe -= size_sysperiod;
			buf_stat |= BUF_STAT_SIZE_BUF_FIRST_SFRAME_MASK;
		}
		wr_ptr_buf_stat_ant_rx->offset_in_sframe = offset_in_sframe;
		
			
		unsigned int dcs_id2 = (antman_ctrl_rx.dcs_id >> ANT_ID_BITIDX(1)) & 0x7;
		unsigned int dma_src_rx0 = antman_ctrl_rx.axiq_fifo_addr;
		unsigned int dma_src_rx1 = AXIQFIFO_ADDR_RX(dcs_id2);
		
		
		dmac_enable(dma_ctrl|antman_ctrl_rx.axiq_dma_chan, valid_size+cur_ext_size, dma_src_rx0, UNIT2BYTE(wr_ptr_buf_stat_ant_rx->buf_addr[0]));
		if(dcs_id2 != 0x7)
			dmac_enable(dma_ctrl|AXIQDMA_ID_RX(dcs_id2), valid_size+cur_ext_size, dma_src_rx1, UNIT2BYTE(wr_ptr_buf_stat_ant_rx->buf_addr[1]));
		
		offset_in_sframe += valid_size+cur_ext_size;	
		antman_ctrl_rx.offset_in_sframe = offset_in_sframe;
		wr_ptr_buf_stat_ant_rx->buf_stat_size = buf_stat;
		
		#ifdef LOG_TYPE_RX_AXIQ_DMA_START
		log_2int(TYPE_STAMPED(LOG_TYPE_RX_AXIQ_DMA_START), buf_stat); 
		#ifdef LOG_TYPE_RX_AXIQ_DMA_START_SIDX
		log_2int(wr_ptr_buf_stat_ant_rx->offset_in_sframe, size_sysperiod);
		#endif
		#endif

		unsigned int num_buffers = NUM_RX_ANT_BUFFERS;
		wr_ptr_buf_stat_ant_rx++;
		if(wr_ptr_buf_stat_ant_rx == &antman_ctrl_rx.buf_stat_ant_rx[num_buffers])
			wr_ptr_buf_stat_ant_rx = &antman_ctrl_rx.buf_stat_ant_rx[0];
	}
	antman_ctrl_rx.rx_size = rx_size;
	antman_ctrl_rx.wr_ptr_buf_stat_ant_rx = wr_ptr_buf_stat_ant_rx;
}

#if (OBS_CHAN_ENABLE == 1)
void Ant_buffer_rx_dma_feed_obs()
{
	struct_buf_stat_td* wr_ptr_buf_stat_ant_rx = antman_ctrl_rx.wr_ptr_buf_stat_ant_rx;
	if((antman_ctrl_rx.num_DMA_in_progress < 2) && (wr_ptr_buf_stat_ant_rx->buf_stat_size == 0))
	{
		unsigned int dma_size_rx = OBS_SIZE_BUFFER;
		wr_ptr_buf_stat_ant_rx->buf_stat_size = OBS_SIZE_BUFFER;
		unsigned int dma_ctrol = DMA_TRIG | DMAC_RDC | DMAC_FIFO_HS | DMAC_PRST_REQ | antman_ctrl_rx.axiq_dma_chan;
		unsigned int AXIQ_DMA_dest = wr_ptr_buf_stat_ant_rx->buf_addr[0];
		dmac_enable(dma_ctrol, dma_size_rx, antman_ctrl_rx.axiq_fifo_addr, UNIT2BYTE(AXIQ_DMA_dest));
		antman_ctrl_rx.num_DMA_in_progress++;
		#ifdef LOG_TYPE_RX_AXIQ_DMA_START
		log_2int(TYPE_STAMPED(LOG_TYPE_RX_AXIQ_DMA_START),dma_size_rx);
		#endif
		wr_ptr_buf_stat_ant_rx++;
		if(wr_ptr_buf_stat_ant_rx == &antman_ctrl_rx.buf_stat_ant_rx[OBS_NUM_BUFFERS])
			wr_ptr_buf_stat_ant_rx = &antman_ctrl_rx.buf_stat_ant_rx[0];
		antman_ctrl_rx.wr_ptr_buf_stat_ant_rx = wr_ptr_buf_stat_ant_rx;
	}
}
#endif

//return value: 0-success, non-zero: AXIQ error
int Ant_buffer_rx_init(unsigned int dcs_ids, unsigned int ctrl_iqswap)
{
	unsigned int iq_swap = (ctrl_iqswap>>IQSWAP_BITIDX(0))&1;
	unsigned int iq_swap2 = (ctrl_iqswap>>IQSWAP_BITIDX(1))&1;
	unsigned int hw2xdecim = ctrl_iqswap >> HWDCM_BITIDX;
	unsigned int reg_value, reg_mask;
	unsigned int dcs_id2 = (dcs_ids >> ANT_ID_BITIDX(1)) & 0x7;
	unsigned int dcs_id = (dcs_ids >> ANT_ID_BITIDX(0)) & 0x7;

	antman_ctrl_rx.axiq_fifo_addr = AXIQFIFO_ADDR_RX0 + dcs_id * 0x1000 +AXIQFIFO_OFFSET_THRESHOLD_RX;
	antman_ctrl_rx.axiq_dma_chan = (AXIQDMA_ID_RX0+dcs_id);		
	antman_ctrl_rx.dcs_id = dcs_ids;
	antman_ctrl_rx.axiq_control_gpo = GPO(4);
	antman_ctrl_rx.axiq_status_gpi = GPI(0);
	antman_ctrl_rx.axiq_control_enable_bitfield = LS_AXIQ_CTL_RX0_CH_EN_MASK<<(dcs_id*8);
	antman_ctrl_rx.axiq_control_fifo_rst_clr_err_bitfield = LS_AXIQ_CTL_RX0_CLEAR_ERROR_MASK<<(dcs_id*8);
	antman_ctrl_rx.axiq_status_err_bitfield = 0xc<<(dcs_id*4);
	antman_ctrl_rx.axiq_status_enable_bitfield = LS_AXIQ_STS_RX0_CH_EN_MASK<<(dcs_id*4);
	antman_ctrl_rx.axiq_underflow_overflow_bitfield = 0xc<<(dcs_id*4);
	reg_value = (LS_AXIQ_CTL_RX0_CH_EN_MASK | (FIFO_THRESHOLD_CONFIG_RX<<LS_AXIQ_CTL_RX0_FIFO_THRESH_LSB) | (iq_swap<<LS_AXIQ_CTL_RX0_SWAP_B))<<(dcs_id*8);
	reg_mask = (LS_AXIQ_CTL_RX0_CH_EN_MASK | LS_AXIQ_CTL_RX0_FIFO_THRESH_MASK | LS_AXIQ_CTL_RX0_SWAP_MASK)<<(dcs_id*8);
	if(dcs_id2 != 0x7)
	{
		reg_value |= (LS_AXIQ_CTL_RX0_CH_EN_MASK | (FIFO_THRESHOLD_CONFIG_RX<<LS_AXIQ_CTL_RX0_FIFO_THRESH_LSB) | (iq_swap2<<LS_AXIQ_CTL_RX0_SWAP_B))<<(dcs_id2*8);
		reg_mask |= (LS_AXIQ_CTL_RX0_CH_EN_MASK | LS_AXIQ_CTL_RX0_FIFO_THRESH_MASK | LS_AXIQ_CTL_RX0_SWAP_MASK)<<(dcs_id2*8);
		SET_FAST_FLAGS(FAST_FLAG_2RX_MASK);
	}

	//for LA9310, enable status bit will not be set until tx_allowed is asserted, so no need to check enable status
	__ip_write(antman_ctrl_rx.axiq_control_gpo, reg_mask, reg_value);
	
	
	#ifdef LOG_TYPE_INIT_RX_ANTMAN
	log_2int(LOG_TYPE_INIT_RX_ANTMAN|antman_ctrl_rx.dcs_id, __ip_read(antman_ctrl_rx.axiq_status_gpi));
	#endif

	return 0;  //error just cleared, no need to check error here.  if there is error, it will be checked at runtime.
}

void Ant_buffer_rx_dma_check()
{
	unsigned int dma_mask = 1<<antman_ctrl_rx.axiq_dma_chan;
	unsigned int num = antman_ctrl_rx.num_DMA_in_progress;
	if(num == 0)
		return;

	unsigned int num_completed;
	if(!dmac_is_enabled(dma_mask))
		num_completed = num;
	else if((num == 2) && dmac_is_available(dma_mask))
		num_completed = 1;
	else
		return;

	unsigned int nvb = antman_ctrl_rx.num_valid_buffers;
	nvb += num_completed;
	antman_ctrl_rx.num_valid_buffers = nvb;
	num -= num_completed;
	antman_ctrl_rx.num_DMA_in_progress = num;
	#ifdef LOG_TYPE_RX_AXIQ_DMA_COMP
	log_2int(TYPE_STAMPED(LOG_TYPE_RX_AXIQ_DMA_COMP),(nvb<<20)|(num<<16));
	#endif
}

void Ant_buffer_rx_check_status()
{
	unsigned int status = __ip_read(antman_ctrl_rx.axiq_status_gpi);

	unsigned int rx_allowed_status = 0;
	
	

	status = status & antman_ctrl_rx.axiq_underflow_overflow_bitfield;
	if(status)
	{
		unsigned int sym_idx = dfe_ctrl.FFTout_valid & 0x3FFF;
		rx_error_handling(SET_ERROR_TYPE(ERROR_REPORT_MSG_ERROR_TYPE_RX_OVERFLOW) | (antman_ctrl_rx.num_DMA_in_progress<<14) | (rx_allowed_status<<13) | (sym_idx&0x1FFF), antman_ctrl_rx.rd_ptr_buf_stat_ant_rx->offset_in_sframe/4);
	
	}
}

//return value: 0-normal, non-zero:overflow
unsigned int Ant_buffer_rx_check_status_obs()
{
	return __ip_read(antman_ctrl_rx.axiq_status_gpi, antman_ctrl_rx.axiq_underflow_overflow_bitfield);
}

//return value: non-zero: success, 0 no valid buffer
int Ant_buffer_rx_poll_buffer(struct_buf_stat_td** ptr)
{
	#if RUN_IN_SIMULATOR
	if((!FDD_MODE_RX) && (dfe_ctrl.flag_AtoB & FLAG_AtoB_TX_ACTIVE))
		return 0;
	#endif

	Ant_buffer_rx_dma_check();
	Ant_buffer_rx_dma_feed();

	*ptr = antman_ctrl_rx.rd_ptr_buf_stat_ant_rx;//antman_ctrl_rx.buffer_base + antman_ctrl_rx.read_buffer_id * antman_ctrl_rx.size_buffer_ant_data_plus_reserve_flush);
	return antman_ctrl_rx.num_valid_buffers;
}

void Ant_buffer_rx_free_buffer()  
{
	struct_buf_stat_td* rd_ptr_buf_stat_ant_rx = antman_ctrl_rx.rd_ptr_buf_stat_ant_rx;
		unsigned int num_buffers = NUM_RX_ANT_BUFFERS;
	unsigned int num_valid_buffers = antman_ctrl_rx.num_valid_buffers;
	rd_ptr_buf_stat_ant_rx->buf_stat_size = 0; //mark this buffer is consumed.	
	rd_ptr_buf_stat_ant_rx++;
	if(rd_ptr_buf_stat_ant_rx == &antman_ctrl_rx.buf_stat_ant_rx[num_buffers])
		rd_ptr_buf_stat_ant_rx = &antman_ctrl_rx.buf_stat_ant_rx[0];
	
	antman_ctrl_rx.rd_ptr_buf_stat_ant_rx = rd_ptr_buf_stat_ant_rx;
	antman_ctrl_rx.num_valid_buffers = num_valid_buffers-1;
	
	#if NUM_RX_ANT_BUFFERS > 2
	Ant_buffer_rx_dma_check();
	#endif
	Ant_buffer_rx_dma_feed();
}


#endif
