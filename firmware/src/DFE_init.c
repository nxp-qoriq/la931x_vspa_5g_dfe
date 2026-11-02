// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#include "pair_config.h"
#include "ecpri_compression_decompression.h"
#include "cfr.h"
#include "fft_cp.h"
#include "fft_dif.h"
#include "diffft.h"
#include "Ant_TxRx_Buffer_manager.h"
#include "txWindowing.h"
#include "single_tone.h"
#include "bitRev.h"
#include "host_vspa_msg.h"
#include "global_var.h"

/*********************************************************************************************
 * VSPA DMA Manager
 * The DMA manager manages a group of DMA channels. It checks DMA channel availability in round-robin
 * mode. It pick the available channel for a DMA transfer. 
 * If the transfer needs a complete response, set VCPU_GO_ENABLE bit in control. users then can check 
 * VCPU_GO bit of the channel to know the transfer is completed.
 *  *********************************************************************************************/

extern struct_antman_ctrl_tx antman_ctrl_tx;
extern struct_antman_ctrl_tx antman_ctrl_rx;

unsigned char dma_chan_offset=0;
unsigned long long axi_flag_buf[DMA_CHAN_NUM][8]__attribute__ ((aligned (AXI_BURST_SIZE)))__attribute__((section (".bss_line_aligned")));   //each flag buf takes 64B and aligned to 64B

unsigned int current_dma_chan()
{
	return (dma_chan_offset + DMA_CHAN_START);
}
void advance_dma_chan()
{
	dma_chan_offset = (dma_chan_offset + 1) % DMA_CHAN_NUM;
}

//If a DMA complete check is needed, DMAC_TRIG_VCPU must be set in control in dma_startxxx()
//0: not completed.   1: completed
unsigned int dma_completed(unsigned int dma_chan)
{
	unsigned int go_status = dmac_event(1<<dma_chan);
	dmac_clear_event(go_status);
	return __min(1,go_status);
	
//	if(dmac_event(1<<dma_chan))
//	{
//		dmac_clear_event(1<<dma_chan);
//		return 1;
//	}
//	else
//		return 0;
}

//start DMA, DMA size must be no more than 128KB.
//If a DMA complete check is needed, DMAC_TRIG_VCPU must be set in control
//return value: 0-fail due to no available channel.  other value - channel ID used for the transfer.
unsigned int dma_start(unsigned int control, unsigned int size, unsigned int axi_addr, unsigned int vspa_addr)
{
	//find an available channel
	for(int i=0;i<DMA_CHAN_NUM;i++)
	{
		unsigned int dma_chan = current_dma_chan();
		//if(dmaChannelIsFifoAvailable(1<<dma_chan))
		if(!dmac_is_enabled(1<<dma_chan))
		{
			if(!(control&DMAC_TRIG_VCPU) || (!dmac_event(1<<dma_chan)))
			{
				unsigned int size1 = __min(65536, size);
				dmac_enable(control | dma_chan, size1, axi_addr, UNIT2BYTE(vspa_addr));
				unsigned int size2 = size - size1;
				if(size2)
					dmac_enable(control | dma_chan, size2, axi_addr+size1, UNIT2BYTE(vspa_addr)+size1);
				
				advance_dma_chan();
				return dma_chan;
			}
		}
		else
		{
			advance_dma_chan();
		}
	}
	return 0;
}

//start DMA with 2 transfers, each transfer size must be no more than 64KB.
//If a DMA complete check is needed, DMAC_TRIG_VCPU must be set in control
//return value: 0-fail due to no available channel.  other value - channel ID used for the transfer.
unsigned int dma_start_2xfr(unsigned int control1, unsigned int size1, unsigned int axi_addr1, unsigned int vspa_addr1,
                            unsigned int control2, unsigned int size2, unsigned int axi_addr2, unsigned int vspa_addr2)
{
	//find an available channel
	for(int i=0;i<DMA_CHAN_NUM;i++)
	{
		unsigned int dma_chan = current_dma_chan();
		//if(dmaChannelIsFifoAvailable(1<<dma_chan))
		if(!dmac_is_enabled(1<<dma_chan))
		{
			if(!((control1|control2)&DMAC_TRIG_VCPU) || (!dmac_event(1<<dma_chan)))
			{
				dmac_enable(control1 | dma_chan, size1, axi_addr1, UNIT2BYTE(vspa_addr1));
				__asm volatile("fnop .asmvol");
				dmac_enable(control2 | dma_chan, size2, axi_addr2, UNIT2BYTE(vspa_addr2));
				advance_dma_chan();
				return dma_chan;
			}
		}
		else
		{
			advance_dma_chan();
		}
	}
	return 0;
}

//start DMA, DMA size must be no more than 64KB.
//If a DMA complete check is needed, DMAC_TRIG_VCPU must be set in control
//return value: 0-fail due to no available channel.  other value - channel ID used for the transfer.
unsigned int dma_start_with_axi_slave_flag_set(unsigned int control, unsigned int size, unsigned int axi_addr, unsigned int vspa_addr, unsigned int core_id, unsigned long long axi_slave_flags)
{
	//find an available channel
	for(int i=0;i<DMA_CHAN_NUM;i++)
	{
		unsigned int dma_chan = current_dma_chan();
		if(!dmac_is_enabled(1<<dma_chan))
		{
			if(!(control&DMAC_TRIG_VCPU) || (!dmac_event(1<<dma_chan)))
			{
				dmac_enable(control | dma_chan, size, axi_addr, UNIT2BYTE(vspa_addr));
				axi_flag_buf[dma_chan_offset][0] = axi_slave_flags;
				dmac_enable(DMAC_WR | dma_chan, 8, ADDR_AXI_FLAG_CORE(core_id), UNIT2BYTE((unsigned int)axi_flag_buf[dma_chan_offset]));
				advance_dma_chan();
				return dma_chan;
			}
		}
		else
		{
			advance_dma_chan();
		}
	}
	return 0;
}

void __init(void)
{
	#ifdef __VSPA2__
	__ip_write(9, 0x10, 0x10);  //set bit4 of IP reg 9 for Errata A-011455: VSPA AXI slave��s failure to access VSPA DMEM
	#endif
	__ip_write(CONTROL, __ip_read(CONTROL) | 0x03300000); //enable mailbox go.
	__ip_write(SWVERSION, CURRENT_CORE_VERSION);

	#ifdef LOG_TYPE_INIT_BOOT
	log_2int(LOG_TYPE_INIT_BOOT, 0);
	#endif

	#if !RUN_IN_SIMULATOR
	boot();	
	#else
	dfe_ctrl.tx_sym_buf_base = 0x70000000;
	dfe_ctrl.tx_sym_buff_size = 16384;
	dfe_ctrl.tx_num_sym_in_buf = 28;
	dfe_ctrl.rx_sym_buf_base[0] = 0x70000000;
	dfe_ctrl.rx_sym_buf_base[1] = 0x70000000;
	dfe_ctrl.rx_sym_buff_size = 16384;
	dfe_ctrl.rx_num_sym_in_buff[0] = 28;
	__ip_write(IP_IDX_DFE_MODE_HI, 0x0e80fc80);
	__ip_write(IP_IDX_DFE_MODE_LO, 0x33000077);
	#endif
	
	//initialized AXIQ FIFO to avoid sending garbage, DMA a burst of 0 to TX AXIQ FIFO
	//beginning part of IPPU DMEM is bss and initialized to 0 by default 
#if SELF_CORE_ID <= 3
	dmac_enable(DMAC_WR | DMAC_FIFO_HS | DMAC_PRST_REQ | AXIQDMA_ID_TX(SELF_CORE_ID), UNIT2BYTE(AXI_BURST_SIZE), AXIQFIFO_ADDR_TX(SELF_CORE_ID), VCPU_SIZE_DRAM); 
#elif (SELF_CORE_ID >= 4) && (SELF_CORE_ID <= 5) 
	dmac_enable(DMAC_WR | DMAC_FIFO_HS | DMAC_PRST_REQ | AXIQDMA_ID_TX(SELF_CORE_ID), UNIT2BYTE(AXI_BURST_SIZE), HS_AXIQFIFO_ADDR_TX(SELF_CORE_ID-4), VCPU_SIZE_DRAM);
#endif
	
	//wait_for_cores to sync cycle counters. this is done once at power on.
#if(CORE_TYPE == TYPE_CORE_A || CORE_TYPE == TYPE_CORE_B)
	#ifdef LOG_TYPE_INIT_SYNC_CORES
	log_2int(LOG_TYPE_INIT_SYNC_CORES, 0);
	#endif
#endif
	ccnt_enable(0);	//set cycle counter to 0 and all cores will have synced cycle counter.
}

void init_coreB()
{
#if (OBS_CHAN_ENABLE == 1)
	antman_ctrl_rx.num_buffers = OBS_NUM_BUFFERS;
	for(int i=0;i<OBS_NUM_BUFFERS;i++)
	{
		antman_ctrl_rx.buf_stat_ant_rx[i].buf_stat_size = 0;
		antman_ctrl_rx.buf_stat_ant_rx[i].buf_addr = (unsigned int)obs_buffer_pool[i];
	}
	
	unsigned int ctrl_iqswap = OBS_CHAN_IQSWAP;
	
	if(HWDCM_ENABLE)
		ctrl_iqswap |= HWDCM_MASK;

	if(0 != Ant_buffer_rx_init(0, OBS_DCS_ID, ctrl_iqswap))
	{
		//AXIQ is in error state at the time of initialization
//		mailbox_send_msg(MAILBOX_USED_FOR_ERROR_REPORT,	SET_ERROR_TYPE(ERROR_REPORT_MSG_ERROR_TYPE_TXAXIQ_INIT_STATE_ERROR)|SET_ERROR_TR_ID(0)|SET_ERROR_ANT_ID(OBS_DCS_ID)|SET_ERROR_DATA_HI(0xFFF),0xFFFFFFFF);
		if(dfe_ctrl.error_info_MSB32 == 0)
		{
			dfe_ctrl.error_info_LSB32 = 0xFFFFFFFF;
			dfe_ctrl.error_info_MSB32 = SET_ERROR_TYPE(ERROR_REPORT_MSG_ERROR_TYPE_TXAXIQ_INIT_STATE_ERROR)|SET_ERROR_TR_ID(0)|SET_ERROR_ANT_ID(OBS_DCS_ID)|SET_ERROR_DATA_HI(0xFFF);
		}
	}
#endif
	

		#if(NUM_1T_IN_CORE && UPSAMPLING_RATIO==1 && ARCH_TYPE_T==ARCH_1T_2CORE)
		//split DPD into 2 cores
		#if !DEFAULT_DPD_DISABLE
		set_dpd_coeff_passthrough((float*)dpd_coeff);
		#endif
		#endif

#if NUM_1T_IN_CORE
		pipeline_reset_tx();
#endif
		
//	__ip_write(IPREG_CONTROL, __ip_read(IPREG_CONTROL) | 0x03300001); //clear host go/mailbox go, enable mailbox go.
//	__ip_write(IPREG_IRQEN, 0x00003000);        //enable IRQ to ARM for mailbox messages.
//	__ip_write(IPREG_IRQEN, 0x00000000);        //disable IRQ to ARM for mailbox messages.
	
	//init internal reg
//	__ip_write(IPREG_AXISLV_FLAGS0, 0xFFFFFFFF);
//	__ip_write(IPREG_AXISLV_FLAGS1, 0xFFFFFFFF);

}

void init_common()
{
	//abort all DMAs that may have transfers left from previous run
	dmac_reset();
//	dmac_clear();
	__ip_write(HOST_VCPU_FLAGS1, 0xFFFFFFFF);
	__ip_write(HOST_VCPU_FLAGS0, 0xFFFFFFFF);
	__ip_write(AXISLV_FLAGS1, 0xFFFFFFFF);
	__ip_write(AXISLV_FLAGS0, 0xFFFFFFFF);
	__ip_write(IP_IDX_DFE_MODE_HI, 0);
//	__ip_write(IP_IDX_TRX_NUM_SYMS, 0 );
//	__ip_write(IP_IDX_TX_POWER, 0 );
//	__ip_write(IP_IDX_TX_POWER_SAMPLES, 0 );
//	__ip_write(IP_IDX_RX_POWER, 0 );
//	__ip_write(IP_IDX_RX_POWER_SAMPLES, 0 );
	CLR_ALL_FAST_FLAGS;
	dfe_ctrl.ant_core_map_hi = ANT_CORE_MAPPING_RX;
	dfe_ctrl.ant_core_map_lo = ANT_CORE_MAPPING_TX;
			
	//initialize core B variables
#if ((CORE_A_ID == CORE_B_ID) || (CORE_TYPE == TYPE_CORE_B))
#if (NUM_1T_IN_CORE) && !BYPASS_IFFT
	dfe_ctrl.tx_circ_base = UNIT2BYTE((unsigned int)iFFTout_CFRinout);
	dfe_ctrl.tx_circ_size = IFFT_CIRC_SIZE;
	dfe_ctrl.addr_phcom_coeff_tx = ((unsigned int)phcom_coeff_tx)/BYTE2UNIT(128);
#endif
#if (NUM_1R_IN_CORE) && !BYPASS_FFT
	dfe_ctrl.rx_circ_base = UNIT2BYTE((unsigned int)FFTin);
	dfe_ctrl.rx_circ_size = FFT_CIRC_SIZE;
	dfe_ctrl.addr_phcom_coeff_rx = ((unsigned int)phcom_coeff_rx)/BYTE2UNIT(128);
#endif
#endif
	
#if ((CORE_A_ID == CORE_B_ID) || (CORE_TYPE == TYPE_CORE_A))
#if (NUM_1T_IN_CORE)
	dfe_ctrl.addr_DFE_qec_params_opt_tx = ((unsigned int)TX_QEC_BUF_ADDR)/BYTE2UNIT(128);
	dfe_ctrl.addr_ant_tx_dump = (UNIT2BYTE((unsigned int)&ant_tx_dump));
	dfe_ctrl.addr_antman_tx = (UNIT2BYTE((unsigned int)&antman_ctrl_tx));
#endif
#if (NUM_1R_IN_CORE)
	dfe_ctrl.addr_DFE_qec_params_opt_rx = ((unsigned int)DFE_QEC_PARAMS_BUF_RX)/BYTE2UNIT(128);
	dfe_ctrl.addr_ant_rx_dump = (UNIT2BYTE((unsigned int)ant_rx_dump));
	dfe_ctrl.addr_antman_rx = (UNIT2BYTE((unsigned int)&antman_ctrl_rx));
#endif
#endif
	
#if !BYPASS_FFT
	rx_bitrev_progress_ippu[0] = BITREV_NOT_STARTED;
	rx_bitrev_progress_ippu[1] = BITREV_NOT_STARTED;
#endif
	
#if !BYPASS_IFFT
	tx_bitrev_progress_ippu = BITREV_NOT_STARTED;
#endif
	
		dfe_ctrl.buf_stat_2xup1_in[0].offset_in_sframe = SIZE_PER_SYSPERIOD(BASEBAND_SPS_TX);
		dfe_ctrl.buf_stat_2xup1_in[0].buf_stat_size = BUF_STAT_SIZE_BUF_FIRST_MASK|BUF_STAT_SIZE_BUF_FIRST_SFRAME_MASK;
    	dfe_ctrl.kernels_disable = KERNELS_DISABLE_BITMASK_CFR;  //CFR is disabled by default.
		dfe_ctrl.cap_hi = ((ARCH_TYPE_R-ARCH_1R_1CORE) << 23)
                			| ((HIGHPHY_LOWPHY_IF_TYPE-1) << 22)
                			| ((ARCH_TYPE_T-ARCH_1T_1CORE) << 21)
//                			| (BANDWIDTH_CODE << 17)
                			| (RX_LP_FIR_FILTER_ENABLE<<13)
                			| (TXDCS_FDD<<12)
                			| (((BYPASS_IFFT) | (BYPASS_FFT)) << 11)
                			| (OBS_CHAN_ENABLE<<10)
                			| ((NUM_2XDOWN_TAPS/64) << 9)            //bit9: 0-8taps, 1-64taps
                			| (((1-DEFAULT_DPD_DISABLE))<<8)
                			| ((HWDCM_ENABLE) << 7)   //1: 2G mode
                			| ((DPD_SPS_RATIO-1)<<2)
                			| SINAD_TEST_ENABLE;
		
		dfe_ctrl.cap_lo = (    (PHASE_COMPENSATION_DISABLE<<31)
							|  (CFO_DISABLE << 30)
							| (1-BYPASS_ECPRI_COMMPRESSION)<<29)
                			| ((1-(BYPASS_FFT&1))<<28)
                			| (((1-DEFAULT_RX_QEC_DISABLE)*2 + (1-USING_OPTIMIZED_QEC))<<26) //QEC enable, QEC with timing skew compensation
                			| (((1-DEFAULT_TX_QEC_DISABLE)*2 + (1-USING_OPTIMIZED_QEC))<<22)
                			| ((TX_BLOCK_SIZE/512)<<13)								//bit 19-13 is block size, num of 512B.
                			| (((1-DEFAULT_DPD_DISABLE)*DEFAULT_DPD_MODEL_ID)<<8)	//bit 12-8.
                			| (NUM_UP_SAMPLING_FILTER_TAPS_4BITCODE<<4)
                			| (((1-DEFAULT_CFR_DISABLE)*(MAX_NUM_CFR_PASS-1))<<2)  //0-disabled, 1-3:2-4 PASS,
                			| ((1-(BYPASS_IFFT&1))<<1)
                			| (1-BYPASS_ECPRI_DECOMMPRESSION);

	
	//the following part only run in coreB if 1T2core or 1R2core.
#if(CORE_TYPE==TYPE_CORE_B || (CORE_TYPE==TYPE_CORE_A && (((ARCH_TYPE_T == ARCH_1T_1CORE) && NUM_1T_IN_CORE) || ((ARCH_TYPE_R == ARCH_1R_1CORE) && NUM_1R_IN_CORE))))
	//waiting for init msg
	#ifdef LOG_TYPE_INIT_START_MSG
	log_2int(LOG_TYPE_INIT_START_MSG, 0);
	#endif

	while(__ip_read(IP_IDX_DFE_MODE_HI) == 0)
	{
		#if (HOST_VSPA_MSG_HANDLING_ENABLE || DMA_MEASUREMENT_ENABLE) //this will enable host msg handling
		host_msg_handling();
		#endif
	}
	
#if (NUM_1T_IN_CORE)

		#if((HIGHPHY_LOWPHY_IF_TYPE==IF_TYPE_HOST_VSPA) && (!BYPASS_ECPRI_DECOMMPRESSION))
		ecpri_decomp_init();
		#endif

		dfe_ctrl.tx_iFFT_circ_base_axi32 = get_vspa_dmem_addr_axi32(SELF_CORE_ID, (unsigned int)iFFTout_CFRinout);

#endif
	
		if(!LPHY_ONLY_CONFIG)
		{
		#if (UPSAMPLING_RATIO >= 2) && NUM_1T_IN_CORE
			dfe_ctrl.tx_timing_offset = -(NUM_UP_SAMPLING_FILTER_TAPS/UPSAMPLING_RATIO/2); //timing advance to compensate upsampling filter delay
		#endif
				
	#if (NUM_1R_IN_CORE)
			int rx_timing_offset = 0;
			
		#if RX_FILTER_DELAY_COMPENSATION
		#if (DOWNSAMPLING_RATIO >= 2)
			//9310 2xdown filter is at DCS sampling rate if down sampling ratio is 4. 2nd 2xdown stage has no delay
			rx_timing_offset = NUM_2XDOWN_TAPS/2/DOWNSAMPLING_RATIO;  //set initial timing offset to compensate RX digital filter delay  //number samples
		#endif

		#if RX_LP_FIR_FILTER_ENABLE
			rx_timing_offset += 31;
		#endif
		#endif
			dfe_ctrl.rx_timing_offset = rx_timing_offset;
	#endif
		}
		
#if (NUM_1R_IN_CORE)
		#if(!BYPASS_ECPRI_COMMPRESSION)
		ecpri_comp_init();
		#endif

		

		#if (ARCH_TYPE_R != ARCH_1R_1CORE)
		dfe_ctrl.rx_FFTin_circ_base_axi32 = get_vspa_dmem_addr_axi32(SELF_CORE_ID, (unsigned int)FFTin);
		#endif
#endif

#else
	

#endif
#if FLEXIBLE_SLOT_FORMAT_ENABLE
		dfe_ctrl.tdd_pattern_entry.num_patterns = MAX_NUM_PATTERN_ENTRIES;  //for flexible slot format, all entries are used.
#endif
		
		
		dfe_ctrl.tx_freq_dump_flag <<= DUMP_INJECT_MODE_SHIFT;
}


#if(CORE_TYPE==TYPE_CORE_B)
void copy_needed_var_to_core_A()
{
	unsigned int size = UNIT2BYTE((unsigned int)&dfe_ctrl.flag_AtoB - (unsigned int)&dfe_ctrl);
	while(!(__ip_read( DMA_FIFO_STAT, 1<<DMA_CHAN_HIGH_PRIORITY))){}  //wait for DMA fifo available
	dmac_enable(DMAC_WR | DMA_CHAN_HIGH_PRIORITY, size, ADDR_AXI_VCPU_DMEM_CORE(CORE_A_ID)+UNIT2BYTE((unsigned int)&dfe_ctrl), UNIT2BYTE((unsigned int)&dfe_ctrl));
}
#endif

