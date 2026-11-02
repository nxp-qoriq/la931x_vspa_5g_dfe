// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


/* ANTENNA ID  vs  TR_ID
 * ANTENNA ID is the ID mapping to 4 DCS channels,  ANTENNA_D = 0,1,2,3 mapping to DCS0-0, DCS0-1, DCS1-0, DCS1-1
 * TR_ID is the 1T1R ID used in 2T2R using 3 cores arch, TR_ID=0,1 mapping to the 1st and 2nd 1T1R.  In all other arch
 * such as 1T1R using 2 cores, 2T2R using 4 cores, 3T3R using 6 cores, 4T4R using 8 cores,  TR_ID must be set to 0
 * These 2 MACROs are set in the project/setting/macros.
 */


#include "pair_config.h"
#include "decim2x.h"
#include "X2_decim_tap64_filter.h"
#include "Ant_TxRx_Buffer_manager.h"
#include "single_tone.h"
#include "sinad.h"
#include "dump_inject.h"
#include "txWindowing.h"
#include "pow.h"
#include "mixer.h"
#if USING_OPTIMIZED_QEC == 0
#include "qec.h"
void qec(cfixed16_t* output, cfixed16_t* input, unsigned int in_samples, qec_params_t* qec_para, void* history);
#else
#include "qec_opt.h"
#endif
#include "host_vspa_msg.h"
#include "global_var.h"

#if (UPSAMPLING_RATIO != 2) || (DOWNSAMPLING_RATIO != 2) || (NUM_2XDOWN_TAPS != 8)
#error "DFE_ref_la9310 only supports BANDWIDTH_25M_30K_61_61 (2x/2x, 8-tap decim)."
#endif

#if !DEFAULT_DPD_DISABLE || !DEFAULT_CFR_DISABLE || \
    !BYPASS_ECPRI_COMMPRESSION || !BYPASS_ECPRI_DECOMMPRESSION
#error "LA9310 build does not support DPD, CFR, or eCPRI compression/decompression. Keep these all set to 1."
#endif

extern struct_dfe_ctrl dfe_ctrl;


//define TX RX antenna buffer pool
#if SHARED_TXRX_AXIQ_BUFFER
struct
{
	//In TDD the last block of TX window may overwrite the beginning part of rx_buffer_pool.
	//This is ok as RX buffer pool has not started to receive data at this time.
	#if NUM_1T_IN_CORE
	#if NUM_1R_IN_CORE
	int tx_buffer_pool[2][SIZE_TX_ANT_BUFFER/4];
	union { //TX buffers from 3rd buffer will share with RX buffers.
	int tx_buffer_pool1[NUM_TX_ANT_BUFFERS-2][SIZE_TX_ANT_BUFFER/4];
	int rx_buffer_pool[NUM_RX_ANT_BUFFERS][SIZE_RX_ANT_BUFFER/4];
	};
	#else
	int tx_buffer_pool[NUM_TX_ANT_BUFFERS][SIZE_TX_ANT_BUFFER/4]; 
	#endif
	#else
	int rx_buffer_pool[NUM_RX_ANT_BUFFERS][SIZE_RX_ANT_BUFFER/4];
	#endif
} ant_buf_shared __align_vec SECTION_TRX_ANT_BUFFER;

#else
#if (TRX_ANT_BUFFER0_BANK==0)
struct_ant_buf ant_buf0 __align_vec __attribute__((section (".bss_line_aligned")));
#else
struct_ant_buf ant_buf0 __align_vec __attribute__((section (".ibss")));
#endif
#if (TRX_ANT_BUFFER_BANK==0)
struct_ant_buf_extra ant_buf __align_vec __attribute__((section (".bss_line_aligned")));
#else
struct_ant_buf_extra ant_buf __align_vec __attribute__((section (".ibss")));
#endif
#endif

#if NUM_1T_IN_CORE
int qec_scratch[SIZE_TX_ANT_BUFFER/4] __align_vec;
#else
int qec_scratch[SIZE_RX_ANT_BUFFER/4] __align_vec;
#endif

#if(CORE_TYPE==TYPE_CORE_A)
extern int reset_stat;

extern struct_antman_ctrl_tx antman_ctrl_tx;
extern struct_antman_ctrl_rx antman_ctrl_rx;

#if(NUM_1R_IN_CORE)
int rx_pow_acc[SIZE_BYTE_POW_ACC/4];
void set_rx_slot_sym_idx(unsigned int idx);

int history_2xdown[MAX_NUM_1R_IN_CORE][128/4] __align_vec HISTORY_SECTION;
int filter_taps_downsampling[NUM_2XDOWN_TAPS] __align(BYTE2UNIT(NUM_2XDOWN_TAPS*4)) DOWN_SAMPLING_TAPS_SECTION = {
#include "para_files/2xdown_coeff.txt"
};
#endif

extern union
{
	int scratch_vcpu[NUM_BYTE_COMP_SCRATCH_VCPU/4];
	cfixed16_t vcpu_scratch_buff[IFFT_SCRATCH_SIZE];
	cfixed16_t vcpu_scratch_buff_fft[FFT_SCRATCH_SIZE];
	int sinad_vcpu_scratch[SINA_VCPU_SCRATCH_SIZE/4];
	int scratch_2xdown[SCRATCH_2XDOWN_SIZE/4];
	__fp16 		dpd_scratch[DPD_SCRATCH_SIZE/2];
}vcpu_scratch __align_vec;

#if SINAD_TEST_ENABLE
char sinad_ippu_scratch[SINA_IPPU_SCRATCH_SIZE] __align_vec __attribute__ ((section (".ibss")));
#endif

#if(CORE_A_ID != CORE_B_ID)
void copy_needed_var_to_core_B()
{
	unsigned int size = UNIT2BYTE((unsigned int)&dfe_ctrl.boundary_local_var_start - (unsigned int)&dfe_ctrl.flag_AtoB);
	while(!(__ip_read( DMA_FIFO_STAT, 1<<DMA_CHAN_HIGH_PRIORITY))){}  //wait for DMA fifo available
	dmac_enable(DMAC_WR | DMA_CHAN_HIGH_PRIORITY, size, ADDR_AXI_VCPU_DMEM_CORE(CORE_B_ID)+UNIT2BYTE((unsigned int)&dfe_ctrl.flag_AtoB), UNIT2BYTE((unsigned int)&dfe_ctrl.flag_AtoB));
}
#endif

//FOR TX
#if (NUM_1T_IN_CORE)
/***********************************************************************************
 *                up sampling  Parameters
 ************************************************************************************/
#if UP_SAMPLING_TAPS_BANK
	#define UP_SAMPLING_TAPS_ATTR __attribute__ ((section (".idata")))
#else
	#define UP_SAMPLING_TAPS_ATTR
#endif

int filter_taps_upsampling[SIZE_X2_INTERP_TAP32_FILTER_TAPS/4] __attribute__((section (".data_aligned"))) __align_vec UP_SAMPLING_TAPS_ATTR = {
	#include "para_files/h_2x_32tap_stop10000_FR1_gain2p4.bin.txt"
};

//status of tx block buffers, each buffer has a status, half line long written by coreB with single burst.
unsigned int tx_block_status[NUM_2XUP_IN_BUFFERS][64/4] __align_vec;

int history_upfilter[SIZE_X2_X4_FILTER_HISTORY/4] __align_vec HISTORY_SECTION;


__inline void tx_block_release()
{
	//single core arch: release a block to the circular buffer
	dfe_ctrl.tx_circ_total_released_size += TX_BLOCK_SIZE;
	#ifdef LOG_TYPE_CIRC_RELEASED
	log_2int(TYPE_STAMPED(LOG_TYPE_CIRC_RELEASED), TX_BLOCK_SIZE);
	#endif
}

#endif




//FOR RX
#if(NUM_1R_IN_CORE)

__fx16 buf_2xdown_out[NUM_2XDOWN_OUT_BUFFERS][RX_BLOCK_SIZE/2] __align_vec;

#endif


#if USING_OPTIMIZED_QEC == 0
void qec(cfixed16_t* output, cfixed16_t* input, unsigned int in_samples, qec_params_t* qec_para, void* history)
{
	/* QEC stage 1 */
	switch(qec_para->FD_tap_cnt) {
	case 0:
		qec_static_imb_asm( (cfixed16_t*) qec_scratch, 
							(cfixed16_t*) input,
							(in_samples)>>5, /* number of lines (4*in_samples/32) */
							qec_para->f1, 
							qec_para->f4,
							qec_para->f2);	
		break;
	case 6:
		qec_skew_6t_static_imb_asm( (cfixed16_t*) qec_scratch, 
									(cfixed16_t*) input,
									qec_para->FD_taps,
									history,
									(in_samples)>>5, /* number of lines (4*in_samples/32) */
									qec_para->int_del, 
									qec_para->f1,
									qec_para->f2);
		break;
	case 7:
		qec_skew_7t_static_imb_asm( (cfixed16_t*) qec_scratch, 
											(cfixed16_t*) input,
											qec_para->FD_taps,
											history,
											(in_samples)>>5, /* number of lines (4*in_samples/32) */
											qec_para->int_del, 
											qec_para->f1,
											qec_para->f2);
		break;
	default:
		qec_skew_Nt_static_imb_asm( (cfixed16_t*) qec_scratch, 
											(cfixed16_t*) input,
											qec_para->FD_taps,
											history,
											(in_samples)>>5, /* number of lines (4*in_samples/32) */
											qec_para->int_del, 
											qec_para->FD_tap_cnt,
											qec_para->f1,
											qec_para->f2);
		break;		
		
	}
	
	/* QEC stage 2 */
	qec_lo_gain_asm((cfixed16_t*) output, 
					(cfixed16_t*) qec_scratch,
					(in_samples)>>5, 
					qec_para->g.real, 
					qec_para->g.imag,
					qec_para->dc.real, 
					qec_para->dc.imag);

}
#endif

void init_coreA()
{
	#ifdef LOG_TYPE_INIT_START_MSG
	log_2int(TYPE_STAMPED(LOG_TYPE_INIT_START_MSG), 0);
	#endif
	

#if (NUM_1T_IN_CORE) && (UPSAMPLING_RATIO > 1)
	dfe_ctrl.addr_upsampling_filter_taps = ((unsigned int)filter_taps_upsampling)/BYTE2UNIT(128);
#endif
#if (NUM_1R_IN_CORE)
#if DOWNSAMPLING_RATIO > 1
	dfe_ctrl.addr_downsampling_filter_taps = ((unsigned int)filter_taps_downsampling)/BYTE2UNIT(128);
#endif
	#if RX_LP_FIR_FILTER_ENABLE
	dfe_ctrl.addr_rx_lpf_taps = ((unsigned int)rx_lpf_taps)/BYTE2UNIT(128);
	#endif
#endif

	#if(CORE_A_ID != CORE_B_ID)
	copy_needed_var_to_core_B();   //copy variable to coreB before waiting for var from core B
	#endif
	
	unsigned int dfe_mode_config_hi = __ip_read(IP_IDX_DFE_MODE_HI);
	while(dfe_mode_config_hi == 0) 
	{
		dfe_mode_config_hi = __ip_read(IP_IDX_DFE_MODE_HI);
	}   //wait until core B sends flag_tx_IQ_swap
	unsigned int dfe_mode_lo = __ip_read(IP_IDX_DFE_MODE_LO);
	
	if(GET_NUM_SC == 0)
		__ip_write(IP_IDX_NSC, MAX_NUM_SUBCARRIERS);
	
#if (NUM_1T_IN_CORE)

	#if QEC_IN_DPD_COEFF_SIZE_BYTES
	qec_params_opt_t* p_qec = (qec_params_opt_t*)TX_QEC_BUF_ADDR;
	p_qec->f1 = 1;
	p_qec->f4 = 1;
	p_qec->f1_backup = 1;
	p_qec->f4_backup = 1;
	#endif
	
	unsigned int ant_map_tx = GET_DFE_MODE_ANT_MAP_TX(dfe_mode_lo);
	antman_ctrl_tx.num_empty_buffers = NUM_ANT_BUFFERS+NUM_EXTRA_TX_ANT_BUFFERS;
	
#if SHARED_TXRX_AXIQ_BUFFER
	for(int i=0;i<NUM_TX_ANT_BUFFERS;i++)
		antman_ctrl_tx.buf_stat_ant_tx[i].buf_addr[0] = (unsigned int)ant_buf_shared.tx_buffer_pool[i];
#else
	antman_ctrl_tx.buf_stat_ant_tx[0].buf_addr[0] = (unsigned int)ant_buf0.tx_buffer_pool;
	for(int i=1;i<NUM_ANT_BUFFERS;i++)
		antman_ctrl_tx.buf_stat_ant_tx[i].buf_addr[0] = (unsigned int)ant_buf.ant_buf1[i-1].tx_buffer_pool;
	
#if NUM_EXTRA_TX_ANT_BUFFERS
	for(int i=NUM_ANT_BUFFERS;i<NUM_ANT_BUFFERS+NUM_EXTRA_TX_ANT_BUFFERS;i++)
	{
		antman_ctrl_tx.buf_stat_ant_tx[i].buf_addr[0] = (unsigned int)ant_buf.extra_tx_buffer_pool[i-NUM_ANT_BUFFERS];
	}
#endif
#endif
	
	antman_ctrl_tx.wr_ptr_buf_stat_ant_tx  =
	antman_ctrl_tx.rd_ptr_buf_stat_ant_tx  = &antman_ctrl_tx.buf_stat_ant_tx[0];

	dfe_ctrl.wr_ptr_buf_stat_2xup1_in =
	dfe_ctrl.rd_ptr_buf_stat_2xup1_in = (struct_buf_stat_td*)dfe_ctrl.buf_stat_2xup1_in;

#endif
	
	
#if(NUM_1R_IN_CORE)
		
	unsigned int ant_map_rx = GET_DFE_MODE_ANT_MAP_RX(dfe_mode_lo);
	unsigned int ant_map_rx2 = GET_DFE_MODE_ANT_MAP_RX2(dfe_mode_lo);
	
#if SHARED_TXRX_AXIQ_BUFFER
	for(int i=0;i<NUM_RX_ANT_BUFFERS;i++)
		antman_ctrl_rx.buf_stat_ant_rx[i].buf_addr[0] = (unsigned int)ant_buf_shared.rx_buffer_pool[i];
#else
	antman_ctrl_rx.buf_stat_ant_rx[0].buf_addr[0] = (unsigned int)ant_buf0.rx_buffer_pool;
#if NUM_1R_IN_CORE == 2
#if FDD_1T2R
	antman_ctrl_rx.buf_stat_ant_rx[0].buf_addr[1] = (unsigned int)ant_buf0.rx2nd_buffer_pool;
#else
	antman_ctrl_rx.buf_stat_ant_rx[0].buf_addr[1] = (unsigned int)ant_buf0.tx_buffer_pool;
#endif
#endif
	for(int i=1;i<NUM_ANT_BUFFERS;i++)
	{
		antman_ctrl_rx.buf_stat_ant_rx[i].buf_addr[0] = (unsigned int)ant_buf.ant_buf1[i-1].rx_buffer_pool;
#if NUM_1R_IN_CORE == 2
#if FDD_1T2R
		antman_ctrl_rx.buf_stat_ant_rx[i].buf_addr[1] = (unsigned int)ant_buf.ant_buf1[i-1].rx2nd_buffer_pool;
#else
		antman_ctrl_rx.buf_stat_ant_rx[i].buf_addr[1] = (unsigned int)ant_buf.ant_buf1[i-1].tx_buffer_pool;
#endif
#endif
	}
#endif
	

#if NUM_EXTRA_RX_ANT_BUFFERS
	for(int i=NUM_ANT_BUFFERS;i<NUM_ANT_BUFFERS+NUM_EXTRA_RX_ANT_BUFFERS;i++)
	{
		antman_ctrl_rx.buf_stat_ant_rx[i].buf_addr[0] = (unsigned int)ant_buf.extra_rx_buffer_pool[i-NUM_ANT_BUFFERS];
	#if NUM_1R_IN_CORE == 2
		#if FDD_1T2R
		antman_ctrl_rx.buf_stat_ant_rx[i].buf_addr[1] = (unsigned int)ant_buf.rx2nd_extra_buffer_pool[i-NUM_ANT_BUFFERS];
		#else
		Not supported
		#endif
	#endif
	}
#endif

	
	unsigned int ctrl_iqswap = (GET_DFE_MODE_RX_IQSWAP(dfe_mode_lo)<<IQSWAP_BITIDX(0)) | (GET_DFE_MODE_RX2_IQSWAP(dfe_mode_lo)<<IQSWAP_BITIDX(1));

	
	antman_ctrl_rx.wr_ptr_buf_stat_ant_rx =
	antman_ctrl_rx.rd_ptr_buf_stat_ant_rx = &antman_ctrl_rx.buf_stat_ant_rx[0];
	antman_ctrl_rx.buf_stat_ant_rx[0].buf_stat_size = BUF_STAT_SIZE_BUF_FIRST_MASK|BUF_STAT_SIZE_BUF_FIRST_SFRAME_MASK; //mark the first block is rx_rising and sframe start
	antman_ctrl_rx.offset_in_sframe = SIZE_PER_SYSPERIOD(RX_AXIQ_SPS);
	
#endif
	
	
#if (OBS_CHAN_ENABLE == 1)
	unsigned int ctrl_iqswap = OBS_CHAN_IQSWAP;
	unsigned int dcs_id = OBS_DCS_ID;
	antman_ctrl_rx[NUM_1R_IN_CORE].num_buffers = OBS_NUM_BUFFERS;
	
	for(int i=0;i<OBS_NUM_BUFFERS;i++)
	{
		antman_ctrl_rx[NUM_1R_IN_CORE].buf_stat_ant_rx[i].buf_stat_size = 0;
		antman_ctrl_rx[NUM_1R_IN_CORE].buf_stat_ant_rx[i].buf_addr[0] = (unsigned int)obs_buffer_pool[i];
	}

	if(0 != Ant_buffer_rx_init(NUM_1R_IN_CORE, dcs_id, ctrl_iqswap))
	{
		//AXIQ is in error state at the time of initialization
		//mailbox_send_msg(MAILBOX_USED_FOR_ERROR_REPORT,SET_ERROR_TYPE(ERROR_REPORT_MSG_ERROR_TYPE_AXIQ_STATE_ERROR)|SET_ERROR_TR_ID(0)|SET_ERROR_ANT_ID(dcs_id)|SET_ERROR_DATA_HI(0xFFF), 0xFFFFFFFF);
		if(dfe_ctrl.error_info_MSB32 == 0)
		{
			dfe_ctrl.error_info_LSB32 = 0xFFFFFFFF;
			dfe_ctrl.error_info_MSB32 = SET_ERROR_TYPE(ERROR_REPORT_MSG_ERROR_TYPE_AXIQ_STATE_ERROR)|SET_ERROR_TR_ID(0)|SET_ERROR_ANT_ID(dcs_id)|SET_ERROR_DATA_HI(0xFFF);
		}
	}
#endif
	
}

void main()
{
	init_common();
	init_coreA();
	
	#if (CORE_A_ID == CORE_B_ID)
	init_coreB();
	#endif
	
	__ip_write(SWVERSION, DFE_INITIALIZED, DFE_INITIALIZED);
	#ifdef LOG_TYPE_INIT_DONE
	log_2int(TYPE_STAMPED(LOG_TYPE_INIT_DONE), 0);
	#endif
	
	dfe_ctrl.min_cycle_count = -1;
	dfe_ctrl.cycle_count_pre_peak = get_timestamp32();
	
	while(1)
	{
		dfe_ctrl.boundary_local_var_start++;  //used as core alive flag
		check_msg();
		
		unsigned int dfe_mode_config_hi = __ip_read(IP_IDX_DFE_MODE_HI);

		/************************************************************************************************
		 * Run up-sampling, DPD, and QEC
		 *************************************************************************************************/
#if NUM_1T_IN_CORE
		if(!(dfe_mode_config_hi&DFE_MODE_TX_DIS))
		{
			struct_buf_stat_td* ant_buf_stat;
			unsigned int ant_buf_available = Ant_buffer_tx_apply_buffer(&ant_buf_stat);

			#if !BYPASS_IFFT
			start_ippu_bitrev_tx();  //bitrev will be run in IPPU
			#endif
			get_tx_1block_from_cfr_out();
			
			struct_buf_stat_td* rd_ptr_buf_stat_2xup1_in = dfe_ctrl.rd_ptr_buf_stat_2xup1_in;
			unsigned int buf_stat_size = rd_ptr_buf_stat_2xup1_in->buf_stat_size;
			unsigned int buf_size_up_in = BUF_STAT_SIZE_BUF_SIZE_MASK & buf_stat_size;
			
			unsigned int usr = UPSAMPLING_RATIO;
			unsigned int tx_ant_buf_size = SIZE_TX_ANT_BUFFER;
			unsigned int offset_in_sframe = rd_ptr_buf_stat_2xup1_in->offset_in_sframe*usr;
			unsigned int run_flag_tx = 0;
			if((buf_stat_size & BUF_STAT_SIZE_BUF_VALID_MASK) && (ant_buf_available))
			{
				short* ptr_ant = (short*)ant_buf_stat->buf_addr[0];

				//ant buffer available, start 2x_up_2
				unsigned int buf_stat = buf_stat_size & ~BUF_STAT_SIZE_BUF_SIZE_MASK;
				void* pin = (void*)rd_ptr_buf_stat_2xup1_in->buf_addr[0];
				unsigned int buf_size = buf_size_up_in*usr;
				ant_buf_stat->buf_stat_size = buf_stat|buf_size;
				ant_buf_stat->offset_in_sframe = offset_in_sframe;
				rd_ptr_buf_stat_2xup1_in->buf_stat_size = buf_stat_size&BUF_STAT_SIZE_BUF_SIZE_MASK;
				rd_ptr_buf_stat_2xup1_in++;
				if(rd_ptr_buf_stat_2xup1_in == &dfe_ctrl.buf_stat_2xup1_in[NUM_2XUP_IN_BUFFERS])
					rd_ptr_buf_stat_2xup1_in -= NUM_2XUP_IN_BUFFERS;

				dfe_ctrl.rd_ptr_buf_stat_2xup1_in = rd_ptr_buf_stat_2xup1_in;
				unsigned int tx_single_tone_amplitude_1st=dfe_ctrl.tx_single_tone_amplitude[0];
				if(tx_single_tone_amplitude_1st&SINGLE_TONE_VALID_FLAG)
				{
					tx_block_release();   //in case tx block is not released at up sampling stage

					//generate single tone
					single_tone_asm(ptr_ant, dfe_ctrl.tx_single_tone_freq[0], offset_in_sframe/4, tx_single_tone_amplitude_1st&(~SINGLE_TONE_VALID_FLAG), tx_ant_buf_size/128);
					for(int i=1;i<MAX_NUM_SINGLE_TONE;i++)
					{
						unsigned int tx_single_tone_amplitude=dfe_ctrl.tx_single_tone_amplitude[i];
						if(tx_single_tone_amplitude&SINGLE_TONE_VALID_FLAG)
							single_tone_add_asm(ptr_ant, ptr_ant, dfe_ctrl.tx_single_tone_freq[i], offset_in_sframe/4, tx_single_tone_amplitude&(~SINGLE_TONE_VALID_FLAG), tx_ant_buf_size/128);
					}
					pin = ptr_ant;
				}
				else 
				{
					/*****************************************************************************
					 * Run up-sampling 
					 * 
					 *******************************************************************************/
					#ifdef LOG_TYPE_TX_UPSAMPLING_START
					log_2int(TYPE_STAMPED(LOG_TYPE_TX_UPSAMPLING_START), offset_in_sframe);
					#endif
					CODE_MEA_UP_SAMPLING(unsigned int ts_upsampling = get_timestamp32();)
					X2_interp_tap32_filter((__fx16*)ptr_ant, (__fx16*)pin, TX_BLOCK_SIZE/4, (__fx16*)history_upfilter, (float*)filter_taps_upsampling);
					CODE_MEA_UP_SAMPLING(dfe_ctrl.cycle_count_upsampling = (get_timestamp32() - ts_upsampling);)

					pin = ptr_ant;
					tx_block_release();

				}

			#if (!CFO_DISABLE)
				mixer_asm((void*)pin, (void*)pin, offset_in_sframe/4, dfe_ctrl.tx_mixer_freq_update, tx_ant_buf_size/128);
			#endif

			#if (!DEFAULT_TX_QEC_DISABLE) && (QEC_IN_DPD_COEFF_SIZE_BYTES == 0)
				CODE_MEA_QEC_TX(unsigned int ts_qec_tx = get_timestamp32();)
				unsigned int qec_samples = tx_ant_buf_size/4;
				#if USING_OPTIMIZED_QEC == 1
				qec_opt_asm((void*)ptr_ant, (void*)pin, qec_samples, &DFE_qec_params_opt_tx);
				#else
				qec((void*)ptr_ant, (void*)pin, qec_samples, &DFE_qec_params_tx, history_txqec);
				#endif //USING_OPTIMIZED_QEC
				CODE_MEA_QEC_TX(dfe_ctrl.cycle_count_qec_tx = (get_timestamp32()-ts_qec_tx);)
			#endif

				Ant_buffer_tx_write_complete();
				run_flag_tx = 1;
			}
			else
			{
				#if NUM_1R_IN_CORE && (ARCH_TYPE_R==ARCH_1T_1CORE)
				PHY_DFE_interf_rx_check_dma_finish();
				PHY_DFE_interf_rx_check_dma_start();	
				#endif
			
			}
			Ant_buffer_tx_check_status();   //service TX AXIQ DMA after handling TX blocks.

			if((!run_flag_tx) || (FDD_MODE_TX) || (!(dfe_ctrl.flag_AtoB&FLAG_AtoB_RX_ACTIVE)))
				tx_processing_symbol_level();

			#if NUM_TX_ANT_BUFFERS == 2
			Ant_buffer_tx_dma_check();  //put it here so  that dfe_ctrl.flag_AtoB&FLAG_AtoB_TX_ACTIVE can be updated in time
			#endif
		}

#endif	
		
		
#if NUM_1R_IN_CORE
		if(!(dfe_mode_config_hi&DFE_MODE_RX_DIS))
		{
		unsigned int loop_count_rx = 1;
		if( (!(dfe_mode_config_hi&DFE_MODE_CPE)) && (!FDD_MODE_TX))
			loop_count_rx = 4;   //in gNB TDD mode, during switch period, TX will occupy a lot of core load, run RX multiple times can help RX catch up
		
		for(int count=0;count<loop_count_rx;count++)
		{
		/************************************************************************************************
		 * Poll RX antenna data buffer
		 * only when the 2xdown output buffer is empty, poll RX antenna buffer and handle the antenna data.
		 *************************************************************************************************/
		struct_buf_stat_td* ant_rx_buf_stat;
		unsigned int ant_buf_status = Ant_buffer_rx_poll_buffer(&ant_rx_buf_stat);
		/****************************************************************************************************************
		 * Check if fft in data buffer has enough free space for a new block
		 ******************************************************************************************************************/
		if(((FFT_CIRC_SIZE - get_valid_size_for_fft_in()) >= RX_BLOCK_SIZE) && ant_buf_status)
		{
				//successfully get an ant buffer
				unsigned int buf_stat_size = ant_rx_buf_stat->buf_stat_size;
				unsigned int offset_in_sframe = ant_rx_buf_stat->offset_in_sframe;
				unsigned int buf_valid = buf_stat_size&BUF_STAT_SIZE_BUF_VALID_MASK;
				
				#ifdef LOG_TYPE_RX_BLOCK_INFO
				log_2int(TYPE_STAMPED(LOG_TYPE_RX_BLOCK_INFO), offset_in_sframe);
				#endif

				#if(SINAD_TEST_ENABLE)
				if(dfe_ctrl.sinad_num_idx_sig)
				{
					//backup source data to offset 128 bytes of output buffer
					dmac_enable(DMAC_WRC|DMA_CHAN_DUMP_ANT_DATA, 32768, dfe_ctrl.sinad_output_addr+128, (int)ant_rx_buf_stat->buf_addr[0]);
					__nop();
					while(dmac_is_enabled(1<<DMA_CHAN_DUMP_ANT_DATA)) {}
					
					static float sinad_output[6] __attribute__ ((aligned (64)));
					snr_params_t snr_para;
					snr_para.start_point = dfe_ctrl.sinad_num_idx_dc; //10-1;	//start location in fft spectrum
					if((RX_DCS_SPS/(HWDCM_ENABLE+1)) == DCS_SPS_983M)
						snr_para.end_point	= 4095;//3333-1;
					else
						snr_para.end_point	= 3332;//3333-1;
					snr_para.f_start	    = dfe_ctrl.sinad_start_idx_sig; // 1667;			//the bin of tone
					snr_para.f_len			= dfe_ctrl.sinad_num_idx_sig;//6; 	//valid bins before/after tone location
					*(int*)&sinad_output[2] = snr_para.start_point;
					*(int*)&sinad_output[3] = snr_para.end_point;
					*(int*)&sinad_output[4] = snr_para.f_start;
					*(int*)&sinad_output[5] = snr_para.f_len;
					SINAD_test(sinad_output, (cfixed16_t*)ant_rx_buf_stat->buf_addr[0], &snr_para, vcpu_scratch.sinad_vcpu_scratch, sinad_ippu_scratch);

					dmac_enable(DMAC_WR|DMA_CHAN_DUMP_ANT_DATA, 24, dfe_ctrl.sinad_output_addr, (int)sinad_output);
					dfe_ctrl.sinad_num_idx_sig = 0;
				}
				#endif

				unsigned short flag_AtoB = dfe_ctrl.flag_AtoB;
				if(buf_stat_size & BUF_STAT_SIZE_BUF_FIRST_MASK)	
				{

					flag_AtoB |= FLAG_AtoB_RX_ACTIVE;
					
					dfe_ctrl.rx_FFTin_circ_wr_offset = 0;  //reset RX circ write ptr for each new TX window

					pipeline_reset_rx();
					set_rx_slot_sym_idx(dfe_ctrl.rx_win_start_sym_idx);
				}

				unsigned int addr_2xdown_out[2];
				addr_2xdown_out[0] = ((unsigned int)FFTin) + BYTE2UNIT(dfe_ctrl.rx_FFTin_circ_wr_offset);
#if NUM_1R_IN_CORE == 2
#if FDD_1T2R
				addr_2xdown_out[1] = ((unsigned int)FFTin2nd) + BYTE2UNIT(dfe_ctrl.rx_FFTin_circ_wr_offset);
#else
				addr_2xdown_out[1] = ((unsigned int)iFFTout_CFRinout) + BYTE2UNIT(dfe_ctrl.rx_FFTin_circ_wr_offset);
#endif
#endif

				void* out_ptr[2];
				#if(DOWNSAMPLING_RATIO >= 2)
				out_ptr[0] = (void*)ant_rx_buf_stat->buf_addr[0];
				out_ptr[1] = (void*)ant_rx_buf_stat->buf_addr[1];
				#else
				out_ptr[0] = (void*)addr_2xdown_out[0];
				out_ptr[1] = (void*)addr_2xdown_out[1];
				#endif


		
				#if FR1
				#endif
				unsigned short ant_buf_size = SIZE_RX_ANT_BUFFER;
				unsigned int dsr = DOWNSAMPLING_RATIO;

				unsigned int buf_size_applied = buf_stat_size&BUF_STAT_SIZE_BUF_SIZE_MASK;

				unsigned int buf_size;
				for(int i=0;i<=GET_FAST_FLAGS(FAST_FLAG_2RX_MASK);i++)
				{
					void* out_ptr_curr = out_ptr[i];

					//inject single tone for RX if requested
					unsigned int rx_single_tone_amplitude = dfe_ctrl.rx_single_tone_amplitude[i];
					if(rx_single_tone_amplitude&SINGLE_TONE_VALID_FLAG)
					{
						single_tone_asm((void*)ant_rx_buf_stat->buf_addr[i], dfe_ctrl.rx_single_tone_freq[i], offset_in_sframe/4, rx_single_tone_amplitude&(~SINGLE_TONE_VALID_FLAG), ant_buf_size/128);
					}
					/******************************************************************************************
					 * RX QEC
					 *****************************************************************************************/
					CODE_MEA_QEC_RX(unsigned int ts_qec_rx = get_timestamp32();)
					#if USING_OPTIMIZED_QEC == 1
					qec_opt_asm(out_ptr_curr, (void*)ant_rx_buf_stat->buf_addr[i], ant_buf_size/4, &DFE_qec_params_opt_rx[i]);
					#else
					qec(out_ptr_curr, (void*)ant_rx_buf_stat->buf_addr[i], ant_buf_size/4, &DFE_qec_params_rx[i], history_rxqec);
					#endif //USING_OPTIMIZED_QEC
					CODE_MEA_QEC_RX(dfe_ctrl.cycle_count_qec_rx = (get_timestamp32() - ts_qec_rx);)
					
					#if (!CFO_DISABLE)
					mixer_asm(out_ptr_curr, out_ptr_curr, offset_in_sframe/4, dfe_ctrl.rx_mixer_freq_update[i], ant_buf_size/128);
					#endif	//FR1

					buf_size = buf_size_applied;
					
					if(ant_rx_dump[i].special_control < 0x80000000)
						dump_inject_data_1block(&ant_rx_dump[i], (unsigned int)out_ptr_curr, buf_size, offset_in_sframe);

										
					/******************************************************************************************
					 * Down-sampling
					 *****************************************************************************************/
					CODE_MEA_DOWN_SAMPLING(unsigned int ts_2xdown = get_timestamp32();)
					/*************************************************
					 *  2x down sampling
					 *************************************************/
					decimator_2x_8_Taps_asm( (cfixed16_t *)addr_2xdown_out[i], (cfixed16_t *)out_ptr_curr,
							(float32_t *)filter_taps_downsampling,
							(cfixed16_t *)history_2xdown[i],
							ant_buf_size/4);
					buf_size /= 2;
					
					
					#if RX_LP_FIR_FILTER_ENABLE
					if(!(dfe_ctrl.kernels_disable & KERNELS_DISABLE_BITMASK_RX_LPF))
						fir_filter_63taps((__fx16*)addr_2xdown_out[i], (__fx16*)addr_2xdown_out[i], RX_BLOCK_SIZE/4, (__fx16*)history_rx_fir[i], (float*)rx_lpf_taps);
					#endif
					
					CODE_MEA_DOWN_SAMPLING(dfe_ctrl.cycle_count_downsampling = (get_timestamp32()-ts_2xdown);)
					if(ant_rx_dump[i].special_control >= 0x80000000)
						dump_inject_data_1block(&ant_rx_dump[i], (unsigned int)addr_2xdown_out[i], buf_size, offset_in_sframe/dsr);
				}

					
				if(buf_stat_size & BUF_STAT_SIZE_BUF_LAST_MASK)
				{
					flag_AtoB = (flag_AtoB & ~FLAG_AtoB_RX_ACTIVE);//|FLAG_AtoB_RX_FIRST_BLOCK;  //set the First flag for next block at the last block.
					vecclr(history_2xdown, SIZEOFVEC(history_2xdown));     //clear the history after the last block of the UL slot finishes
				#if RX_LP_FIR_FILTER_ENABLE
					vecclr(history_rx_fir, SIZEOFVEC(history_rx_fir)); 
				#endif			
					#if USING_OPTIMIZED_QEC == 0
					vecclr(history_rxqec,1);
					#endif
				}
	
				dfe_ctrl.flag_AtoB = flag_AtoB;

				dfe_ctrl.rx_FFTin_circ_wr_offset = (dfe_ctrl.rx_FFTin_circ_wr_offset + RX_BLOCK_SIZE)%FFT_CIRC_SIZE;
				dfe_ctrl.rx_circ_total_produced_size += RX_BLOCK_SIZE;
				
				#ifdef LOG_TYPE_RX_1BLOCK_FREED
				log_2int(TYPE_STAMPED(LOG_TYPE_RX_1BLOCK_FREED), buf_stat_size);
				#endif
				
				Ant_buffer_rx_free_buffer(); 			
		}
		
		rx_processing_symbol_level();
		
		Ant_buffer_rx_check_status(); //service RX AXIQ DMA after handling RX blocks.
		}//end for
		}//end RX
#endif
				
		#if (OBS_CHAN_ENABLE == 1)
		obs_chan_handling(NUM_1R_IN_CORE);
		#endif

#if 0
		unsigned short cc = get_timestamp32();
		unsigned short interval = cc - dfe_ctrl.cycle_count_pre_peak;
		dfe_ctrl.cycle_count_pre_peak = cc;
		dfe_ctrl.peak_cycle_count = __max(dfe_ctrl.peak_cycle_count, interval);
		dfe_ctrl.min_cycle_count = __min(dfe_ctrl.min_cycle_count, interval);
#endif
	}
}

#endif
