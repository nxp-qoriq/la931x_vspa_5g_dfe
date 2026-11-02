// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#include "Ant_TxRx_Buffer_manager.h"
#include "dump_inject.h"
#include "host_vspa_msg.h"
#include "global_var.h"

#if (NUM_1T_IN_CORE)
extern int history_txqec[128/4];
struct_antman_ctrl_tx antman_ctrl_tx ANTMAN_BUF_SECTION; 
extern struct_antman_ctrl_rx antman_ctrl_rx;

extern int history_upfilter[SIZE_X2_X4_FILTER_HISTORY/4];

void Ant_buffer_tx_axiq_reset()
{
	#if RUN_IN_SIMULATOR
	return;
	#endif

	if(GET_DFE_MODE_HI_BITS(DFE_MODE_TX_DIS))
		return;
	
	dmac_abort(1<<antman_ctrl_tx.axiq_dma_chan);
	//configure a short DMA with PTR_RST to reset AXIQ, 9310 AXIQ can only reset by ptr_rst
	dmac_enable(DMAC_PRST_REQ | DMA_TRIG | DMAC_WRC | DMAC_FIFO_HS | antman_ctrl_tx.axiq_dma_chan, 64, antman_ctrl_tx.axiq_fifo_addr, UNIT2BYTE(antman_ctrl_tx.rd_ptr_buf_stat_ant_tx->buf_addr[0]));
	delay_cycles(10000);
	//if AXIQ is in normal state, this DMA will be pending there, then abort it.
	//if AXIQ is in Flush mode, this DMA will be consumed and Flush mode will be ended, then this abort will take no effect.
	dmac_abort(1<<antman_ctrl_rx.axiq_dma_chan);

	//disable AXIQ, reset AXIQ(LA12xx only), clr errors
	__ip_write(antman_ctrl_tx.axiq_control_gpo, antman_ctrl_tx.axiq_control_enable_bitfield, 0);
	delay_cycles(32); //wait at least 8 clocks for reset to complete
	__ip_write(antman_ctrl_tx.axiq_control_gpo, antman_ctrl_tx.axiq_control_fifo_rst_clr_err_bitfield, antman_ctrl_tx.axiq_control_fifo_rst_clr_err_bitfield);
	delay_cycles(32); //wait at least 8 clocks for reset to complete
	__ip_write(antman_ctrl_tx.axiq_control_gpo, antman_ctrl_tx.axiq_control_fifo_rst_clr_err_bitfield, 0);
}


void Ant_buffer_tx_axiq_enable()
{
	//enable AXIQ
	
	__ip_write(antman_ctrl_tx.axiq_control_gpo, antman_ctrl_tx.axiq_control_enable_bitfield, antman_ctrl_tx.axiq_control_enable_bitfield);
}

void tx_error_handling(unsigned int data_hi, unsigned int data_lo)
{
	if(dfe_ctrl.error_info_MSB32 == 0)
	{
		dfe_ctrl.error_info_LSB32 = data_lo;
		dfe_ctrl.error_info_MSB32 = data_hi;
	}
	log_2int(data_hi, data_lo);
	LOG_STOP;
	
#if CORE_STOP_ON_ERROR
	while(1) 
	{
		check_msg();   //check msg for restart
	}
#endif
}

void Ant_buffer_tx_dma_feed()
{
	#if RUN_IN_SIMULATOR
	if((!FDD_MODE_TX) && (dfe_ctrl.flag_AtoB & FLAG_AtoB_RX_ACTIVE))
		return;
	#endif
	
	unsigned int nvb = antman_ctrl_tx.num_valid_buffers;
	unsigned int ndma = antman_ctrl_tx.num_DMA_in_progress;
	struct_buf_stat_td* rbid = antman_ctrl_tx.rd_ptr_buf_stat_ant_tx;
	if(!nvb)
		return;
	
	//start AXIQ DMA
	if(ndma < 2)
	{
		unsigned int buf_stat_size = rbid->buf_stat_size;
		unsigned int offset_in_sframe = rbid->offset_in_sframe;
		unsigned int dma_ctrl = DMA_TRIG | DMAC_WRC | DMAC_FIFO_HS | antman_ctrl_tx.axiq_dma_chan;
		unsigned int dma_size = buf_stat_size&BUF_STAT_SIZE_BUF_SIZE_MASK;//SIZE_TX_ANT_BUFFER;

		uint32_t src = rbid->buf_addr[0];//buffer_base + rbid * SIZE_TX_ANT_BUFFER;
		
		//dump a block
		dump_inject_data_1block(&ant_tx_dump, src, dma_size, offset_in_sframe);

		if(buf_stat_size&BUF_STAT_SIZE_BUF_FIRST_MASK)
		{
			//first block of each pattern in TDD, 
			//clear the first a few samples to keep DAC output valotage to 0 until tx allowed is enabled
			int* temp = (int*)src;
			for(int i=0;i<NUM_SAMPLES_CLR_FOR_DAC_AFTER_TXALLOWED_FALLING;i++)
				temp[i] = 0;

			dma_ctrl |= DMAC_TRIG_VCPU; //set VCPU GO for first DMA
		}
		else if(buf_stat_size&BUF_STAT_SIZE_BUF_LAST_MASK)
		{
			//this is the last transfer in DL, 
			unsigned int dma_plus_flush_size = ALIGN(dma_size+SIZE_AXIQ_FIFO+SIZE_FOR_FLUSH_MODE, SIZE_AXIQ_THRESHOLD);
			mem_clr((void*)(src+BYTE2UNIT(dma_size)), dma_plus_flush_size-dma_size);  //clear data after dma size will drive DAC output pin to 0 during GP+UL period
			dma_size = dma_plus_flush_size;
			
			//add DMA_PTR_RST to reset Flush mode, increase transfer size to feed extra ext_trigger.
			dma_ctrl |= (DMAC_PRST_REQ | DMAC_TRIG_VCPU);  //set VCPU GO for last DMA		
			
			//clear history buffes for TDD
			vecclr((void*)history_upfilter, SIZEOFVEC(history_upfilter));

			#if USING_OPTIMIZED_QEC == 0
			vecclr(history_txqec,SIZEOFVEC(history_txqec));
			#endif
		}


		dmac_enable(dma_ctrl, dma_size, antman_ctrl_tx.axiq_fifo_addr, UNIT2BYTE(src));

	#if  ARCH_TYPE_T != ARCH_1T_1CORE
		#ifdef LOG_TYPE_TX_AXIQ_DMA_START
		log_2int(TYPE_STAMPED(LOG_TYPE_TX_AXIQ_DMA_START), buf_stat_size);
		#ifdef LOG_TYPE_TX_AXIQ_DMA_START_SIDX
		log_2int(offset_in_sframe, dma_size);
		#endif
		#endif
	#endif
		
		rbid++;
		unsigned int num_buffers = NUM_TX_ANT_BUFFERS;

		if(rbid >= &antman_ctrl_tx.buf_stat_ant_tx[num_buffers])
			rbid -= num_buffers;
		
		antman_ctrl_tx.rd_ptr_buf_stat_ant_tx = rbid;
		antman_ctrl_tx.num_valid_buffers = nvb - 1;
		antman_ctrl_tx.num_DMA_in_progress = ndma + 1;
	}
}

//return value: 0-success, non-zero: AXIQ error
int Ant_buffer_tx_init(unsigned int dcs_id, unsigned int iq_swap)
{
	unsigned int reg_value, reg_mask;

	antman_ctrl_tx.axiq_dma_chan = AXIQDMA_ID_TX0;
	antman_ctrl_tx.axiq_fifo_addr = AXIQFIFO_ADDR_TX0+AXIQFIFO_OFFSET_THRESHOLD_TX;
	antman_ctrl_tx.axiq_control_gpo = GPO(AXIQ_GPO_TX0);
	antman_ctrl_tx.axiq_status_gpi = GPI(AXIQ_GPI_TX0);
	antman_ctrl_tx.axiq_control_enable_bitfield = LS_AXIQ_CTL_TX0_CH_EN_MASK;
	antman_ctrl_tx.axiq_status_enable_bitfield = AXIQ_STATUS_ENABLE_BITFIELD_TX0;
	antman_ctrl_tx.axiq_underflow_overflow_bitfield = AXIQ_STATUS_UNDERF_BITFIELD_TX0;
	antman_ctrl_tx.axiq_control_fifo_rst_clr_err_bitfield = LS_AXIQ_CTL_TX0_CLEAR_ERROR_MASK;		
	reg_value = LS_AXIQ_CTL_TX0_CH_EN_MASK | (FIFO_THRESHOLD_CONFIG_TX << LS_AXIQ_CTL_TX0_FIFO_THRESH_LSB) | (iq_swap<<LS_AXIQ_CTL_TX0_SWAP_B);
	reg_mask = LS_AXIQ_CTL_TX0_CH_EN_MASK | LS_AXIQ_CTL_TX0_FIFO_THRESH_MASK | LS_AXIQ_CTL_TX0_SWAP_MASK;


	//for LA9310, enable status bit will not be set until tx_allowed is asserted, so no need to check enable status
	__ip_write(antman_ctrl_tx.axiq_control_gpo, reg_mask, reg_value);
	
	
	#ifdef LOG_TYPE_INIT_TX_ANTMAN
	log_2int(LOG_TYPE_INIT_TX_ANTMAN|antman_ctrl_tx.dcs_id, __ip_read(antman_ctrl_tx.axiq_status_gpi));
	#endif

	return 0;  //error just cleared, no need to check error here.  if there is error, it will be checked at runtime.
}

void Ant_buffer_tx_dma_check()
{
	unsigned int dma_mask = 1<<antman_ctrl_tx.axiq_dma_chan;
	unsigned int num = antman_ctrl_tx.num_DMA_in_progress;
	unsigned int neb = antman_ctrl_tx.num_empty_buffers;
	if(num == 0)
		return;
	
	unsigned int num_completed;
	if(!dmac_is_enabled(dma_mask))
	{
		num_completed = num;
	}
	else if((num == 2) && dmac_is_available(dma_mask))
	{
		num_completed = 1;
	}
	else
		return;

	unsigned int flag_AtoB = dfe_ctrl.flag_AtoB;
	neb += num_completed;
	antman_ctrl_tx.num_empty_buffers = neb;	
	num -= num_completed;
	antman_ctrl_tx.num_DMA_in_progress = num;
	#ifdef LOG_TYPE_TX_AXIQ_DMA_COMP
	log_2int(TYPE_STAMPED(LOG_TYPE_TX_AXIQ_DMA_COMP),(neb<<20)|(num<<16));
	#endif
	
	unsigned int go_status = dmac_event(dma_mask);
	if(go_status)
	{
		dmac_clear_event(go_status);
		dfe_ctrl.flag_AtoB = flag_AtoB ^ FLAG_AtoB_TX_ACTIVE;
		dfe_ctrl.num_tx_windows_togglings++;
	}
}

//return num of valid buffers
unsigned int Ant_buffer_tx_check_status()
{
	//log DCS toggling
	unsigned int status = __ip_read(antman_ctrl_tx.axiq_status_gpi);

	
	unsigned int nvb = antman_ctrl_tx.num_valid_buffers;

	if(status & antman_ctrl_tx.axiq_underflow_overflow_bitfield)
	{
		unsigned int sym_idx = dfe_ctrl.tx_sym_idx_per_sframe & 0x3FFF;
		tx_error_handling(SET_ERROR_TYPE(ERROR_REPORT_MSG_ERROR_TYPE_TX_UNDERFLOW)|(antman_ctrl_tx.num_DMA_in_progress<<14)|(sym_idx), antman_ctrl_tx.rd_ptr_buf_stat_ant_tx->offset_in_sframe/4);
	}
	return nvb;
}

//return value: 0-fail, non-zero: success
int Ant_buffer_tx_apply_buffer(struct_buf_stat_td** ptr)
{
	Ant_buffer_tx_dma_check();
	Ant_buffer_tx_dma_feed();

	struct_buf_stat_td* wbid = antman_ctrl_tx.wr_ptr_buf_stat_ant_tx;
	#if SHARED_TXRX_AXIQ_BUFFER
	if((wbid == &antman_ctrl_tx.buf_stat_ant_tx[NUM_TX_ANT_BUFFERS-1]) && ((!(get_tx_allowed_status())) || (dfe_ctrl.flag_AtoB&FLAG_AtoB_RX_ACTIVE)))
		return 0;  //the 3rd TX AXIQ buffer can be used by TX only when TX Allowed starts.
	#endif
	
	*ptr = wbid;
	return antman_ctrl_tx.num_empty_buffers;
}

//this function is optimized. Do not change
void Ant_buffer_tx_write_complete()  
{
	struct_buf_stat_td* wid = antman_ctrl_tx.wr_ptr_buf_stat_ant_tx;
	unsigned int nvb = antman_ctrl_tx.num_valid_buffers+1;
	unsigned int neb = antman_ctrl_tx.num_empty_buffers-1;
	antman_ctrl_tx.num_valid_buffers = nvb;
	antman_ctrl_tx.num_empty_buffers = neb;
	
	#if NUM_TX_ANT_BUFFERS > 2
	Ant_buffer_tx_dma_check();
	#endif
	Ant_buffer_tx_dma_feed();
	
	#ifdef LOG_TYPE_TX_1BLOCK_TO_ANT_BUFFER
	log_2int(TYPE_STAMPED(LOG_TYPE_TX_1BLOCK_TO_ANT_BUFFER),(antman_ctrl_tx.num_DMA_in_progress<<28)|(nvb<<24)|(neb&0xFF));
	#endif
	wid++;
		unsigned int num_buffers = NUM_TX_ANT_BUFFERS;

	if(wid >= &antman_ctrl_tx.buf_stat_ant_tx[num_buffers])
		wid -= num_buffers;
	antman_ctrl_tx.wr_ptr_buf_stat_ant_tx = wid;
}

#endif
