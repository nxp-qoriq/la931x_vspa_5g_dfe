// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef PAIR_CONFIG_TYPES_H
#define PAIR_CONFIG_TYPES_H

/* Pipeline-state enum, mailbox/pattern struct types, and the project-wide
 * dfe_ctrl_s control block. Included via pair_config.h after the derivation
 * macros are in scope.
 */

/*****************************************************************************
 * DFE TX pipeline control
 * DFE TX data flow requires some special control because CFR requires circular buffer which convert symbol awareness
 * by IFFT to symbol-unawareness by CFR and all later kernels.
 * 1. The pipeline starts when the first symbol of a DL pattern is notified by host.
 * 2. The pipeline is flushed after the last symbol is received from host
 * 3. Then the pipeline waits until all the DL data of the pattern transmitted over DAC and tx_allowed signal is de-asserted
 * 4. Until tx_allowed is de-asserted, the pipeline is ready to start for next DL pattern.
 * 1-4 repeats for each DL pattern.
 */

enum
{
	DISABLED=0,
	WAITING_TX_ALLOWED,
	READY,
	LAST_SYM_IFFT_IN,
	LAST_SYM_IFFT_OUT,
	LAST_SYM_CFR_IN,
	LAST_SYM_CFR_OUT,
	LAST_SYM_2xUP1,
	LAST_SYM_2xUP2,
	LAST_SYM_COPY_TO_CORE_A,
};

#define MAX_NUM_PATTERN_ENTRIES					4	//expanding the patterns to 20 slots, a common period for FR1 and FR2
#define NUM_SLOTS_COMMON_PERIOD					20
#define PATTERN_SIZE_SYMBOL_MASK				0xFF
#define PATTERN_SIZE_SYMBOL_SKIP0_BITIDX		0
#define PATTERN_SIZE_SYMBOL_BITIDX				8
#define PATTERN_SIZE_SYMBOL_SKIP1_BITIDX		16
#define PATTERN_SIZE_SYMBOL_SKIP1_MASK			(PATTERN_SIZE_SYMBOL_MASK<<PATTERN_SIZE_SYMBOL_SKIP1_BITIDX)
//#define PATTERN_SIZE_STOP_BITIDX				30
//#define PATTERN_SIZE_STOP_MASK					(1<<PATTERN_SIZE_STOP_BITIDX)
#define PATTERN_SIZE_VALID_BITIDX				31
#define PATTERN_SIZE_VALID_MASK					(1<<PATTERN_SIZE_VALID_BITIDX)
#define GET_SKIP0(x)		(((x)>>PATTERN_SIZE_SYMBOL_SKIP0_BITIDX)&PATTERN_SIZE_SYMBOL_MASK)
#define GET_NUM_SYMBOLS(x)	(((x)>>PATTERN_SIZE_SYMBOL_BITIDX)&PATTERN_SIZE_SYMBOL_MASK)
#define GET_SKIP1(x)		(((x)>>PATTERN_SIZE_SYMBOL_SKIP1_BITIDX)&PATTERN_SIZE_SYMBOL_MASK)

typedef struct tdd_pattern_entry_s
{
	unsigned short 	num_patterns;
	unsigned short 	num_patterns_onetime;
	unsigned int	tx_entry[MAX_NUM_PATTERN_ENTRIES];
	unsigned int	ts_txallowed[MAX_NUM_PATTERN_ENTRIES];
	unsigned int	rx_entry[MAX_NUM_PATTERN_ENTRIES];
} struct_tdd_pattern_entry;


typedef struct struct_obs_s
{
	unsigned int	mem_addr;
} struct_obs;

#define	MSG_RECEIVED_BIT_FIELD_DFE_MODE			(1<<0)
#define	MSG_RECEIVED_BIT_FIELD_BUF_STRUCT		(1<<1)
#define	MSG_RECEIVED_BIT_FIELD_STATIC_PATTERN	(1<<2)

#define MSG_RECEIVED_BIT_FIELD_INIT_MSGS		(MSG_RECEIVED_BIT_FIELD_DFE_MODE)

//#define FFLAG_OPTION8				(1<<8)
//#define FFLAG_CPE					(1<<10)
//#define FFLAG_TX_FDD				(1<<12)
//#define FFLAG_RX_FDD				(1<<13)
//#define FFLAG_TX_HANDSHAKE_BYPASS	(1<<14)
//#define FFLAG_RX_HANDSHAKE_BYPASS	(1<<15)
//#define FFLAG_TX_DIS				(1<<22)
//#define FFLAG_RX_DIS				(1<<23)
//
//#define FFLAG_DFE_MODE	(FFLAG_OPTION8|FFLAG_CPE|FFLAG_TX_FDD|FFLAG_RX_FDD|FFLAG_TX_HANDSHAKE_BYPASS|FFLAG_RX_HANDSHAKE_BYPASS|FFLAG_TX_DIS|FFLAG_RX_DIS)
//
//#define FAST_FLAG				GPO(SELF_CORE_ID)  //the reg used as fast flag
//#define SET_FAST_FLAG(mask,v)	__ip_write(FAST_FLAG, (mask), (v))
//#define CLR_FAST_FLAG(mask)		__ip_write(FAST_FLAG, (mask), 0)
//#define GET_FAST_FLAG(mask)		__ip_read(FAST_FLAG, (mask))

#ifndef OBS_CHAN_ENABLE
#define OBS_CHAN_ENABLE			0
#endif

typedef struct ant_buf_s
{
	//In TDD the last block of TX window may overwrite the beginning part of rx_buffer_pool.
	//This is ok as RX buffer pool has not started to receive data at this time.
	#if NUM_1T_IN_CORE
	int tx_buffer_pool[SIZE_TX_ANT_BUFFER/4];
	#endif
	#if NUM_1R_IN_CORE
	int rx_buffer_pool[SIZE_RX_ANT_BUFFER/4];
#if FDD_1T2R
	int rx2nd_buffer_pool[SIZE_RX_ANT_BUFFER/4];
#endif
	#endif
} struct_ant_buf;

typedef struct ant_buf_extra_tx_s
{
	//In TDD the last block of TX window may overwrite the beginning part of rx_buffer_pool.
	//This is ok as RX buffer pool has not started to receive data at this time.
	#if NUM_1T_IN_CORE && NUM_EXTRA_TX_ANT_BUFFERS
	int extra_tx_buffer_pool[NUM_EXTRA_TX_ANT_BUFFERS][SIZE_TX_ANT_BUFFER/4];
	#endif

	struct_ant_buf ant_buf1[NUM_ANT_BUFFERS-1];

	#if NUM_1R_IN_CORE && NUM_EXTRA_RX_ANT_BUFFERS
	int extra_rx_buffer_pool[NUM_EXTRA_RX_ANT_BUFFERS][SIZE_RX_ANT_BUFFER/4];
	#endif
#if FDD_1T2R
	int rx2nd_extra_buffer_pool[NUM_EXTRA_RX_ANT_BUFFERS][SIZE_RX_ANT_BUFFER/4];
#endif
} struct_ant_buf_extra;


#define BUF_STAT_SIZE_BUF_VALID_MASK		(1<<31)
#define BUF_STAT_SIZE_BUF_LAST_MASK			(1<<30)		//last block of a TDD window
#define BUF_STAT_SIZE_BUF_FIRST_MASK		(1<<29)		//first block of a TDD window or first block of FDD
#define BUF_STAT_SIZE_BUF_FIRST_SFRAME_MASK	(1<<28)		//first block of a SFRAME for FDD or TDD
#define BUF_STAT_SIZE_BUF_STAT_MASK			(BUF_STAT_SIZE_BUF_VALID_MASK|BUF_STAT_SIZE_BUF_LAST_MASK|BUF_STAT_SIZE_BUF_FIRST_MASK|BUF_STAT_SIZE_BUF_FIRST_SFRAME_MASK)
#define BUF_STAT_SIZE_BLOCK_IDX_START		16     //bit19-16
#define BUF_STAT_SIZE_BLOCK_IDX_MASK		0xF
#define BUF_STAT_SIZE_BUF_SIZE_MASK			(0xFFFF)
typedef struct buf_stat_td_s
{
	unsigned int buf_stat_size;
	unsigned int buf_addr[MAX_NUM_1R_IN_CORE];
	unsigned int offset_in_sframe;
//	unsigned int sym_idx;
} struct_buf_stat_td;

#ifndef CELLTRACK_ENABLE
#define	CELLTRACK_ENABLE 0
#endif

typedef struct dfe_ctrl_s
{
	//core A 's variables needed by coreB go first in the struct, in order to copy them to core B using same struct.
	volatile unsigned int	test_cmd_MSB __attribute__ ((aligned (64)));  //offset 0
	volatile unsigned int	test_cmd_LSB;

#define DFE_MODE_DFE_ONLY				(1<<1)
#define DFE_MODE_LSDIV2_BITIDX			2
#define DFE_MODE_LSDIV2_BITMASK			(1<<DFE_MODE_LSDIV2_BITIDX)
#define DFE_MODE_HSDIV2_BITIDX			3
#define DFE_MODE_HSDIV2_BITMASK			(1<<DFE_MODE_HSDIV2_BITIDX)
#define DFE_MODE_BWDIV_BITIDX			4
#define DFE_MODE_BWDIV_MASK				0x3
#define DFE_MODE_BWDIV_BITMASK			(DFE_MODE_BWDIV_MASK<<DFE_MODE_BWDIV_BITIDX)
#define DFE_MODE_HWDCM_BITIDX			6
#define DFE_MODE_HWDCM_BITMASK			(1<<DFE_MODE_HWDCM_BITIDX)
#define DFE_MODE_COMP_DIS				(1<<7)
#define DFE_MODE_OPTION8				(1<<8)
#define DFE_MODE_LOWPHYONLY				(1<<9)
#define DFE_MODE_CPE					(1<<10)
#define DFE_MODE_NO_RX_SYM_TO_HOST		(1<<11)
#define DFE_MODE_TX_FDD					(1<<12)
#define DFE_MODE_RX_FDD					(1<<13)
#define DFE_MODE_TX_HANDSHAKE_BYPASS	(1<<14)
#define DFE_MODE_RX_HANDSHAKE_BYPASS	(1<<15)
#define DFE_MODE_RESTART_TX				(1<<20)
#define DFE_MODE_RESTART_RX				(1<<21)
#define DFE_MODE_TX_DIS					(1<<22)
#define DFE_MODE_RX_DIS					(1<<23)
#define DFE_MODE_TX_ANT_MASK(trid)		(7<<(0+(1-trid)*16))
#define DFE_MODE_RX_ANT_MASK(trid)		(7<<(4+(1-trid)*16))

	volatile unsigned int	rsv_dfe_mode_config_hi;
	volatile unsigned int	rsv_dfe_mode_config_lo;

#define KERNELS_DISABLE_BITMASK_CFR				(1<<0)
#define KERNELS_DISABLE_BITMASK_CORE_HALT		(1<<8)
#define KERNELS_DISABLE_BITMASK_DOWNSAMPLING	(1<<10)
#define KERNELS_DISABLE_BITMASK_RX_LPF			(1<<11)
#define KERNELS_DISABLE_BITMASK_FFT				(1<<12)
	volatile unsigned short	kernels_disable;							//offset 0x10
	volatile unsigned short	tx_ppln_state;
	volatile unsigned short	flag_test_msg;
	volatile unsigned short	reserved_16;
	volatile unsigned int	tx_iFFT_circ_base_axi32;   //used for tx timedomain injection intermediate buffer
	volatile unsigned int	tx_mixer_freq_update;						//offset 0x1c		//update by host at any time.
	volatile unsigned int	rx_mixer_freq_update[MAX_NUM_1R_IN_CORE];	//offset 0x20-24	//update by host at any time.
	struct_tdd_pattern_entry		tdd_pattern_entry;					//offset 0x28
#if ARCH_TYPE_R != ARCH_1R_1CORE
	volatile unsigned int	rx_FFTin_circ_base_axi32;
#else
	volatile unsigned int	reserved_1c;
#endif
	volatile unsigned int	rx_circ_total_released_size;			//0x60
	volatile signed int		tx_timing_offset_total;					//0x64 positive delay, negative advance
	volatile signed int		rx_timing_offset_total;					//0x68
	volatile unsigned int 	tx_sym_idx_per_sframe;					//0x6c


#define BIT_MASK_FFT_OUT_VALID			0x80000000
#define BIT_MASK_FFT_OUT_DUMPING(i)		(0x10000000<<(i))	//2bit for 2R
#define BIT_MASK_FFT_OUT_1ST_HALF_READY	0x08000000
#define BIT_MASK_FFT_OUT_2ND_HALF_READY	0x04000000
#define BIT_MASK_FFT_OUT_IPPU_INP		0x02000000
#define BIT_MASK_FFT_OUT_IDLE			0x01000000
#define BIT_MASK_FFT_OUT_SYM_IDX_MASK	0xFFFF

	volatile unsigned int	FFTout_valid;							//0x70
	volatile unsigned short	ifft_in_buf_wr_idx;						//0x78
	volatile unsigned short	rsv_fft_out_buf_rd_idx;					//0x7a




//#define FLAG_AtoB_TX_SINGLE_TONE					(1<<1)
#define FLAG_AtoB_QEC_SENDING_ZERO					(1<<2)
//#define FLAG_AtoB_OBS_DUMP_ENABLE_IDX				3
#define FLAG_AtoB_TX_ACTIVE_BITIDX					4
#define FLAG_AtoB_TX_ACTIVE							(1<<FLAG_AtoB_TX_ACTIVE_BITIDX)
#define FLAG_AtoB_RX_ACTIVE							(1<<5)
//#define FLAG_AtoB_DOWNSAMPLEING_64TAPS_IDX			6
//#define FLAG_AtoB_RX_SINGLE_TONE					(1<<7)
//#define FLAG_AtoB_RX_FIRST_BLOCK					(1<<8)
//#define FLAG_AtoB_TX_ACTIVE_PRE_BITIDX				9
//#define FLAG_AtoB_TX_ACTIVE_PRE						(1<<FLAG_AtoB_TX_ACTIVE_PRE_BITIDX)

	volatile unsigned short	flag_AtoB __attribute__ ((aligned (64))); //offset 0,  base 0x80
	volatile unsigned short	cycle_count_upsampling; 			//offset 2
	volatile unsigned short	cycle_count_dpd; 					//offset 4
	volatile unsigned short	cycle_count_qec_tx; 				//offset 6
	volatile unsigned short cycle_count_qec_rx; 				//offset 8
	volatile unsigned short cycle_count_downsampling; 			//offset 0x0a
	volatile unsigned short	addr_upsampling_filter_taps; 		//offset 0x0c
	volatile unsigned short	addr_downsampling_filter_taps; 		//offset 0x0e
	volatile unsigned int 	timestamp_tx_allowed_first_enabled;	//offset 0x10
	volatile unsigned int 	timestamp_rx_allowed_first_enabled;	//offset 0x14
	volatile unsigned int	rx_single_tone_freq[MAX_NUM_1R_IN_CORE];			//offset 0x18-1c
	volatile unsigned int	rx_single_tone_amplitude[MAX_NUM_1R_IN_CORE];		//offset 0x20-24
#define SINGLE_TONE_VALID_FLAG	1  //the last bit of single_tone_amplitude means this single tone is valid
	volatile unsigned int	tx_single_tone_freq[MAX_NUM_SINGLE_TONE];      //offset 0x28-34
	volatile unsigned int	tx_single_tone_amplitude[MAX_NUM_SINGLE_TONE]; //offset 0x38-44
	volatile unsigned short	addr_rx_lpf_taps;						       //offset 0x48
	unsigned short			rsv_4a;
	volatile unsigned int	rsv_rx_symbol_intermediate_buffer_addr;					//offset 0x4c
	volatile unsigned int	rsv_num_samples_received;					       	//offset 0x50


//#define STARTING_OFFSET_FLAG_MASK		0xC0000000   //the 2 MSB is used as counter flag to indicate the value has changed
//#define STARTING_OFFSET_INCREMENT		0x40000000
//#define STARTING_OFFSET_SYM_IDX_MASK	0x0000FFFF
//	volatile unsigned int	starting_offset_sym_idx;



	//the following is for RX variable which are needed by coreB, put RX variables here for
	//being DMAed together with TX variables for performance
	volatile unsigned char  obs_dump_flag;
	volatile unsigned char  num_tx_windows_togglings;
	volatile unsigned int	obs_dump_addr;
	volatile unsigned int   rx_allowed_status;




	unsigned short	boundary_local_var_start __attribute__ ((aligned (64)));	//offset 0, base 0x100
	unsigned short 	cycle_count_decomp;											//offset 2
	unsigned short 	cycle_count_ifft;											//offset 4
	unsigned short 	cycle_count_ifft_bitrev;									//offset 6
	unsigned short 	cycle_count_cfr;											//offset 8
	unsigned short 	cycle_count_fft;											//offset 0xa
	unsigned short 	cycle_count_fft_bitrev;										//offset 0xc
	unsigned short 	cycle_count_comp;											//offset 0xe
	unsigned int 	num_tx_sym_from_hiphy;										//offset 0x10
	unsigned int 	num_rx_sym_to_hiphy; 										//offset 0x14
	unsigned int 	cap_hi; 													//offset 0x18
	unsigned int 	cap_lo; 													//offset 0x1c
	unsigned int 	tx_sym_dma_size_cycle_count;								//offset 0x20
	unsigned int	tx_sym_buf_base;											//offset 0x24	//must be 4096 bytes aligned
	unsigned int	tx_num_sym_in_buf;											//offset 0x28
	unsigned int	tx_sym_buff_size;											//offset 0x2C	//buffer size for each symbol, must be multiple of 128 bytes
	unsigned int 	rx_sym_buf_base[MAX_NUM_1R_IN_CORE];						//offset 0x30-34	//must be 4096 bytes aligned
	unsigned int	rx_num_sym_in_buff[MAX_NUM_1R_IN_CORE];						//offset 0x38-3c
	unsigned int	rx_sym_buff_size;											//offset 0x40	//buffer size for each symbol, must be multiple of 128 bytes
#define DUMPING_FLAG_WAITING_IDX			0
#define DUMPING_FLAG_DUMPING_IDX			(DUMPING_FLAG_WAITING_IDX+1)
#define DUMPING_FLAG_WAITING				(1<<DUMPING_FLAG_WAITING_IDX)
#define DUMPING_FLAG_DUMPING				(1<<DUMPING_FLAG_DUMPING_IDX)
	unsigned short	rx_sym_dumping_flag[MAX_NUM_1R_IN_CORE];					//offset 0x44
	unsigned int	ext_log_buf_base;											//offset 0x48
	unsigned int	ext_log_buf_size;											//offset 0x4c
	unsigned int	ant_core_map_hi;											//offset 0x50
	unsigned int	ant_core_map_lo;											//offset 0x54
#define RE_OFFSET_BETWEEN_SSB_AND_INPUTWINDOW	8
	unsigned int	celltrack_config_hi;										//offset 0x58
	unsigned int	celltrack_config_lo;										//offset 0x5c

	unsigned int	tx_circ_base;												//offset 0x60
	unsigned int	tx_circ_size;												//offset 0x64
	unsigned int	tx_circ_total_released_size;								//offset 0x68
	unsigned int	addr_antman_tx;												//offset 0x6c
	unsigned int	rx_circ_base;												//offset 0x70
	unsigned int	rx_circ_size;												//offset 0x74
	unsigned int	rx_circ_total_produced_size;								//offset 0x78
	unsigned int	addr_antman_rx;												//offset 0x7c
	unsigned short	addr_phcom_coeff_tx;										//offset 0x80
	unsigned short	addr_phcom_coeff_rx;										//offset 0x82
	unsigned short	addr_DFE_qec_params_opt_tx;									//offset 0x84
	unsigned short	addr_DFE_qec_params_opt_rx;									//offset 0x86
	unsigned int	addr_ant_tx_dump;											//offset 0x88
	unsigned int	addr_ant_rx_dump;											//offset 0x8c
	  signed int	tx_timing_offset;											//offset 0x90
	  signed int	rx_timing_offset;											//offset 0x94
	volatile unsigned int	tx_circ_total_write_size;							//offset 0x98
	volatile unsigned int	rx_inject_addr;										//offset 0x9c
	volatile unsigned int	rx_inject_size;										//offset 0xa0
	volatile unsigned short tx_freq_dump_flag;									//offset 0xa4
	volatile unsigned short tx_freq_dump_num_sym;								//offset 0xa6
	volatile unsigned int   tx_freq_dump_addr;									//offset 0xa8
	volatile unsigned int   tx_freq_dump_sym_size;								//offset 0xac
	volatile unsigned int   error_info_LSB32;									//offset 0xb0
	volatile unsigned int   error_info_MSB32;									//offset 0xb4
	unsigned short	peak_cycle_count;											//offset 0xb8
	unsigned short	min_cycle_count;											//offset 0xba
	unsigned int	rx_dumping_sym_buf_base[MAX_NUM_1R_IN_CORE];				//offset 0xbc
	unsigned int	rx_dumping_num_sym_in_buff;									//offset 0xc4
	unsigned int	rx_dumping_sym_buff_size;									//offset 0xc8
	unsigned int	rx_sym_idx_ready_for_host;									//offset 0xcc
	unsigned int	tx_sym_idx_request_for_host;								//offset 0xd0
	unsigned short	cycle_count_pre_peak;
	unsigned int	tx_circ_total_produced_size;

	unsigned int	tx_circ_total_write_size_pre;
	unsigned int	ext_log_buf_offset;
	unsigned int	rsv_tx_mixer_freq_active;		//in use, copy from mixer_freq_udpate at slot boundary.
	unsigned int	rsv_tx_mixer_phase;
	unsigned int	rsv_rx_mixer_freq_active[MAX_NUM_1R_IN_CORE];		//in use, copy from mixer_freq_udpate at slot boundary.
	unsigned int	rsv_rx_mixer_phase[MAX_NUM_1R_IN_CORE];
	unsigned int	rsv_tx_pow_num_lines_acc;
	unsigned int	rsv_rx_pow_num_lines_acc;
	unsigned short	flag_test_msg_pre;
	unsigned char	tx_timedomain_injection_dma_in_progress;
	unsigned char	obs_dump_flag_pre;

	unsigned int*	tx_sym_dma_flag_addr;

	struct_buf_stat_td		buf_stat_2xup1_in[NUM_2XUP_IN_BUFFERS];
	struct_buf_stat_td		buf_stat_dpd_in;



#define BUF_STAT_BIT_FIELD_TIMEDOMAIN				(1<<31)
#define BUF_STAT_BIT_FIELD_FULL						(1<<30)
#define BUF_STAT_BIT_FIELD_LAST						(1<<29)
#define BUF_STAT_BIT_FIELD_BITREV_2ND				(1<<28)
#define BUF_STAT_BIT_FIELD_BITREV_INP				(1<<27)
#define BUF_STAT_BIT_FIELD_IDLE						(1<<26)
#define BUF_STAT_BIT_FIELD_TX_SYM_BUF_IDX_IDX		21
#define BUF_STAT_BIT_FIELD_TX_SYM_BUF_IDX_MASK		0x1F		//bit 25-21
#define BUF_STAT_BIT_FIELD_USAMPLE_IDX				17			//bit 20-17
#define BUF_STAT_BIT_FIELD_USAMPLE_MASK				0xF
#define BUF_STAT_BIT_FIELD_SYM_SIZE_MASK			0x1FFFF		//bit 16-0

	unsigned int	buf_stat_ifft_in[NUM_IFFT_IN_BUFFERS];

	unsigned int	buf_stat_ifft_bitrev_in;//[NUM_IFFT_OUT_BUFFERS];
	unsigned char 	msg_received_status;
	unsigned char	ifft_in_buf_rd_idx;

	unsigned int 	tx_sym_src_size;	//total size of the symbol transfered from host
	short			rsv_output_scaling_factor;

	unsigned int	rx_win_start_sym_idx;

	unsigned int	size_cfr_in_valid;
	unsigned int	size_cfr_out_valid;
	unsigned int	read_ptr_copy_to_2xup1;
	unsigned int	write_ptr_ifft_out;
	unsigned int	read_ptr_cfr;
	unsigned short	num_tx_symbols_counter;
	unsigned short	num_tx_symbols_skip1;
	unsigned short	num_tx_symbols_curr_entry;
	unsigned int	flow_control_timestamp_tx;
	unsigned int	rsv_dma_fetch_tx_symbol_in_progress;

//	unsigned char	tx_allowed_entry_idx;
	unsigned char	tx_pattern_entry_idx;
	unsigned char	rx_pattern_entry_idx;

//	unsigned char	pattern_size_idx;

	unsigned char	time_domain_injecting_flag;

	struct_buf_stat_td*	wr_ptr_buf_stat_2xup1_in;
	struct_buf_stat_td*	rd_ptr_buf_stat_2xup1_in;

	unsigned int 	addr_2xup1_in;
	unsigned int	addr_2xup1_in_status;
	unsigned char	tx_block_idx;
	unsigned char	tx_block_num_empty;
	unsigned char	tx_block_num_empty_pre;
	unsigned char	num_tx_ppln_togglings;


	unsigned int 	rx_sym_idx_per_sframe;
	unsigned int 	rx_sym_dest_size;
	unsigned int	rx_release_size_backup;

	unsigned int	rx_FFTin_circ_wr_offset;
	unsigned int	read_offset_fft_in;  //offset in byte

	unsigned char	rsv_fft_out_buf_wr_idx;
	unsigned char 	flex_sf_pattern_entry_wr_idx;
	unsigned int	rsv_rx_sym_buf_busy_bits;

	unsigned int 	rsv_dma_write_symbol_in_progress;

	unsigned int	bitrev_sym_size;
	unsigned int 	flag_log_stop_idx;

#if (ARCH_TYPE_R != ARCH_1R_1CORE)
	unsigned int	flag_2xdown_out_ready[NUM_2XDOWN_OUT_BUFFERS];  //1: 2xdown output data is ready
	unsigned char	buf_2xdown_out_rd_ptr;
	unsigned char	buf_2xdown_out_wr_ptr;
#endif

#if SINAD_TEST_ENABLE
	unsigned short	sinad_start_idx_sig;
	unsigned char	sinad_num_idx_sig;
	unsigned char	sinad_num_idx_dc;
	unsigned int	sinad_output_addr;
#endif
} struct_dfe_ctrl;

#endif /* PAIR_CONFIG_TYPES_H */
