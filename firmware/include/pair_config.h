// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef PAIR_CONFIG_H
#define PAIR_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <vspa/intrinsics.h>
#include "chip.h"
#include "iohw.h"
#include "vcpu.h"
#include "ippu.h"
#include "gpio.h"
#include "host.h"
#include "vspa.h"
#include "cfr.h"
#include "filter.h"
#include "ecpri_compression_decompression.h"
#include "fft_cp.h"
#include "mem.h"
#include "dmac.h"
#include "inter_core_msg.h"
#include "ccnt.h"
#include "cycle.h"

#define CORE_STOP_ON_ERROR			1

//test in simulator
#define RUN_IN_SIMULATOR			0		//set to 1 to run on simulator for breakpoint debugging

#if RUN_IN_SIMULATOR
	#define DMA_TRIG				0
#else
	#define DMA_TRIG				DMAC_PEND_EXT
#endif

#define DFEREF_V_TITLE				0xDFEF
#define DFEREF_V_NUM_MAJOR			5
#define DFEREF_V_NUM_MINOR			0
#define DFEREF_V_NUM_REV			9

#define DFEREF_V_NUM				((DFEREF_V_NUM_MAJOR<<8)|(DFEREF_V_NUM_MINOR<<4)|DFEREF_V_NUM_REV)


#define TYPE_UNUSED				-1
#define TYPE_CORE_A				1
#define TYPE_CORE_B				2
#define TYPE_CORE_PHY_TX		3
#define TYPE_CORE_PHY_RX		4

#define TYPE_SYMBOL_FREQDOMAIN	1
#define TYPE_SYMBOL_TIMEDOMAIN	2

//IF_TYPE_VSPA_VSPA: DFE will use vspa-vspa interface to communicate with HighPHY for symbol data with up to 28 symbols buffer outside VSPA DMEM
//IF_TYPE_HOST_VSPA: DFE will use host-vspa interface to communicate with HighPHY for symbol data with up to 2 symbols buffer inside VSPA DMEM
#define IF_TYPE_VSPA_VSPA 		1
#define IF_TYPE_HOST_VSPA		2


//define how mnay num of cores are involved in each TX path or RX path
#define ARCH_1T_1CORE	1
#define ARCH_1T_2CORE	2
#define ARCH_1R_1CORE	3
#define ARCH_1R_2CORE	4

#define CORE_CLOCK			491520

#define BANDWIDTH_10M		10
#define BANDWIDTH_25M		25
#define BANDWIDTH_50M		50
#define BANDWIDTH_100M		100
#define BANDWIDTH_200M		200
#define BANDWIDTH_400M		400
#define BANDWIDTH_800M		800
#define SCS_15K				15
#define SCS_30K				30
#define SCS_60K				60
#define SCS_120K			120
#define SCS_480K			480
#define DCS_SPS_1966M		1966080
#define DCS_SPS_983M		(DCS_SPS_1966M/2)
#define DCS_SPS_491M		(DCS_SPS_1966M/4)
#define DCS_SPS_245M		(DCS_SPS_1966M/8)
#define DCS_SPS_122M		(DCS_SPS_1966M/16)
#define DCS_SPS_61M			(DCS_SPS_1966M/32)
#define DCS_SPS_30M			(DCS_SPS_61M/2)
#define DCS_SPS_15M			(DCS_SPS_30M/2)
#define DCS_SPS_7M			(DCS_SPS_15M/2)
#define LOWEST_SPS			7680  //5M 15K 512FFT SPS

#define BANDWIDTH_10M_15K_61_61				10151
#define BANDWIDTH_100M_T4x_30K_491_245		100300
#define BANDWIDTH_100M_30K_491_245			100301
#define BANDWIDTH_100M_30K_245_245			100302
#define BANDWIDTH_50M_30K_245_122			50301
#define BANDWIDTH_25M_30K_122_122			25300
#define BANDWIDTH_25M_30K_122_61			25301	//this option should not be used on LA12xx due to ADC 61Msps SINIAD limitation
#define BANDWIDTH_25M_30K_61_61				25302
#define BANDWIDTH_10M_30K_61_61				15301
#define BANDWIDTH_25M_T4x_60K_122_122		20600
#define BANDWIDTH_25M_T2x_60K_61_61			20601
#define BANDWIDTH_25M_60K_61_61				20602
#define BANDWIDTH_200M_60K_983_983			2000601
#define BANDWIDTH_400M_120K_1966_1966		4001201
#define BANDWIDTH_400M_120K_983_983			4001202
#define BANDWIDTH_800M_120K_1966_1966		8001201

#define ALIGN128(x)	(((x)+127)/128*128)
#define ALIGN64(x)	(((x)+63)/64*64)
#define ALIGN32(x)	(((x)+31)/32*32)
#define ALIGN16(x)	(((x)+15)/16*16)
#define ALIGN(x,n)	(((x)+(n)-1)/(n)*(n))
#define ALIGN_DOWN(x,n)	((x)/(n)*(n))
#define ROUND(x,n)	(((x)+(n)/2)/(n)*(n))

#define OBS_CHAN_TYPE_NONE			0  //do not change,  NO OBS
#define OBS_CHAN_TYPE_DEDICATED		1  //do not change,  using dedicated OBS channels
#define OBS_CHAN_TYPE_SHARED		2  //do not change,  using shared OBS channels

#define SPECIAL_SAMPLE_PATTERN		0x7fff8000

#define PROJ_1T1R_1CORE_LS_9310     23   // PROJ_CONFIG_OPTION value (kept for build flag compat)
#include "proj_config.h"
#include "bandwidth_code.h"

#include "debug.h"

#define MAX_NUM_1R_IN_CORE			2

//#define MAILBOX_USED_FOR_ERROR_REPORT		0	//0:mailbox 0, 1:mailbox 1
#define MAX_NUM_IPPU_ARG			10


#ifndef OBS_CHAN_TYPE
#define OBS_CHAN_TYPE				OBS_CHAN_TYPE_NONE
#endif


#ifndef NUM_2XDOWN_TAPS
#define NUM_2XDOWN_TAPS				8   //8 or 64, 64 taps core load will make 4T4R FDD or TDD not working well.
#endif

#define NUM_SAMPLES_EXTRA_FOR_PRODUCING_FOR_RX_FILTER_DELAY_COMPENSATION	32

#ifndef TRX_ANT_BUFFER0_BANK
#define	TRX_ANT_BUFFER0_BANK	0
#endif
#ifndef TRX_ANT_BUFFER_BANK
#define	TRX_ANT_BUFFER_BANK		1
#endif

#if TRX_ANT_BUFFER_BANK
#define SECTION_TRX_ANT_BUFFER	__attribute__((section (".ibss")))
#else
#define SECTION_TRX_ANT_BUFFER	__attribute__((section (".bss")))
#endif

#ifndef TX_BITREV_BANK
#define TX_BITREV_BANK		0
#endif

#ifndef TX_SYM_QUEUE_BANK
#define	TX_SYM_QUEUE_BANK	1
#endif
#ifndef RX_SYM_QUEUE_BANK
#define RX_SYM_QUEUE_BANK	1
#endif
#ifndef TX_CIRC_BANK
#define TX_CIRC_BANK		1
#endif
#ifndef RX_CIRC_BANK
#define RX_CIRC_BANK		0
#endif

#if NUM_1R_IN_CORE == 0
#define TX_CIRC_SECTION		__attribute__((section(".ibss1")))
#else
#define TX_CIRC_SECTION		__attribute__((section(".ibss0")))
#endif

//define IPPU arg bank
#ifdef __VSPA2__
#define IPPU_ARG_BANK		1   //VSPA2 must be on IPPU
#define IPPU_ADDR_RATIO		1
#define IPPU_ARG_OFFSET		BYTE2UNIT(VCPU_SIZE_DRAM)
#else
#define IPPU_ARG_BANK		0
#define IPPU_ADDR_RATIO		2
#define IPPU_ARG_OFFSET		0
#endif

	#define AXI_BURST_SIZE	8		//8 half words,  16B, 128bit

#define SIZEOFUNIT(x)		(sizeof(x))
#define UNIT2BYTE(x)		((x)<<(2-UPHW))
#define BYTE2UNIT(x)		((x)>>(2-UPHW))
#define LINE_SIZE			(NLANE16*UPHW)

//SW VERSION definition which will be set in the SWVERSION register,
//by reading the SWVERSION register, host can know DFE is initialized.
#define DFE_INITIALIZED					(1<<12)
#define DFEREF_VERSION 					((DFEREF_V_TITLE<<16) | (CORE_A_ID<<13) | DFEREF_V_NUM)

/* DPD permanently disabled on LA9310. */
#define DEFAULT_DPD_MODEL_ID	0
	
#ifndef RX_FILTER_DELAY_COMPENSATION
#define RX_FILTER_DELAY_COMPENSATION	1
#endif
		
#ifndef USING_OPTIMIZED_QEC
#define USING_OPTIMIZED_QEC		1
#endif

#ifndef QEC_IN_DPD_COEFF_SIZE_BYTES
#define QEC_IN_DPD_COEFF_SIZE_BYTES	0
#endif
		
#ifndef TX_QEC_BUF_ADDR
#if USING_OPTIMIZED_QEC
#define TX_QEC_BUF_ADDR	(&DFE_qec_params_opt_tx)
#else
#define TX_QEC_BUF_ADDR	(&DFE_qec_params_tx)	
#endif
#endif
		

#if(CORE_TYPE==TYPE_CORE_B || (CORE_TYPE==TYPE_CORE_A && (((ARCH_TYPE_T == ARCH_1T_1CORE) && NUM_1T_IN_CORE) || ((ARCH_TYPE_R == ARCH_1R_1CORE) && NUM_1R_IN_CORE))))
#define CURRENT_CORE_VERSION	DFEREF_VERSION
#else
#define CURRENT_CORE_VERSION	0xFFFFFFFF //DFEREF_V_NUM
#endif

#ifndef CORES_USED_AS_DFE
	#define CORES_FOR_SYNC			(CORE_BITMASK(CORE_A_ID)|CORE_BITMASK(CORE_B_ID))
#else
	#if RUN_IN_SIMULATOR
		#define CORES_FOR_SYNC			(CORE_BITMASK(CORE_A_ID)|CORE_BITMASK(CORE_B_ID))
	#else
		#define CORES_FOR_SYNC			CORES_USED_AS_DFE
	#endif
#endif
		
#if(CORE_TYPE == TYPE_UNUSED)
	//the following defines has no meaning for unused cores, just make the compiling pass.
	#define CORE_A_ID			0
	#define CORE_B_ID			0
	#define ANTENNA_ID_TX		0
	#define ANTENNA_ID_RX		0
	#define NUM_1T_IN_CORE		1
	#define NUM_1R_IN_CORE		1
	#define	CORE_B_TR_ID		0
#elif (CORE_TYPE == TYPE_CORE_PHY_TX || CORE_TYPE == TYPE_CORE_PHY_RX)
	//the following defines has no meaning for PHY TX/RX cores, just make the compiling pass.
	#define NUM_1T_IN_CORE 		1
	#define NUM_1R_IN_CORE 		1
	#define CORE_A_ID			0
#endif


#define MAX_NUM_SINGLE_TONE			4
#define MAX_NUM_CFR_PASS			4

#ifndef NUM_2XUP_IN_BUFFERS
#if(ARCH_TYPE_T	== ARCH_1T_1CORE)
	#define NUM_2XUP_IN_BUFFERS									1
#elif BANDWIDTH_TX == BANDWIDTH_800M
	#define NUM_2XUP_IN_BUFFERS									4
#elif BANDWIDTH_CODE == BANDWIDTH_100M_T4x_30K_491_245
	#define NUM_2XUP_IN_BUFFERS									3
#elif PROJ_CONFIG_OPTION == PROJ_4T4R_8CORES_LS
	#define NUM_2XUP_IN_BUFFERS									4
#else
	#define NUM_2XUP_IN_BUFFERS									2
#endif
#endif

#ifndef NUM_2XDOWN_OUT_BUFFERS
#if(ARCH_TYPE_R	== ARCH_1R_1CORE)
	#define NUM_2XDOWN_OUT_BUFFERS								1
#else
	#define NUM_2XDOWN_OUT_BUFFERS								2
#endif
#endif

#if(BANDWIDTH_TX == BANDWIDTH_RX)
	#define BANDWIDTH		BANDWIDTH_TX
#else
	#if(NUM_1T_IN_CORE==0)
		#define BANDWIDTH		BANDWIDTH_RX
	#elif(NUM_1R_IN_CORE==0)
		#define BANDWIDTH		BANDWIDTH_TX
	#elif(CORE_TYPE == TYPE_UNUSED)
		#define BANDWIDTH		BANDWIDTH_TX
	#else
		#error 1T1R in same core but TX/RX bandwidth is different, unsupported configuration
	#endif
#endif
		
#if BANDWIDTH == 0
#error ERROR BANDWIDTH
#endif
	
#if (SCS == 0)
#error WRONG SCS
#elif (SCS == SCS_15K)
	#define NUM_SLOTS_1ms										1

#if BANDWIDTH==BANDWIDTH_10M
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		1
	#define NUM_SYMBOLS_PER_DMA									1
	#define NUM_IFFT_IN_BUFFERS									1
		
	#define MAX_NUM_SUBCARRIERS									624
	#define NUM_SAMPLES_LCP										(320/4)
	#define NUM_SAMPLES_SCP										(288/4)
	#define POINTS_FFT											(1024)
	#define BASEBAND_SPS_TX										15360
	#define BASEBAND_SPS_RX										BASEBAND_SPS_TX
		
#elif BANDWIDTH==BANDWIDTH_50M
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		2
	#define NUM_SYMBOLS_PER_DMA									1
	#define NUM_IFFT_IN_BUFFERS									2
	
	#define MAX_NUM_SUBCARRIERS									3240
	#define NUM_SAMPLES_LCP										320
	#define NUM_SAMPLES_SCP										288
	#define POINTS_FFT											(4096)
	#define BASEBAND_SPS_TX										61440
	#define BASEBAND_SPS_RX										61440
		
#else
#error WRONG BANDWIDTH
#endif

#elif (SCS == SCS_30K)
	#define NUM_SLOTS_1ms										2
		
	#define NUM_IFFT_IN_BUFFERS									1

#if (BANDWIDTH == BANDWIDTH_10M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		1
	#define NUM_SYMBOLS_PER_DMA									1
	
	#define POINTS_FFT											512
	#define NUM_SAMPLES_LCP										(352/8)
	#define NUM_SAMPLES_SCP										(288/8)
	#define BASEBAND_SPS_TX										15360
	#define BASEBAND_SPS_RX										BASEBAND_SPS_TX
	#define MAX_NUM_SUBCARRIERS									288 //456 //936

#elif (BANDWIDTH == BANDWIDTH_25M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		1
	#define NUM_SYMBOLS_PER_DMA									1
	
	#define POINTS_FFT											1024
	#define NUM_SAMPLES_LCP										(352/4)
	#define NUM_SAMPLES_SCP										(288/4)
	#define BASEBAND_SPS_TX										30720
	#define BASEBAND_SPS_RX										BASEBAND_SPS_TX
	#define MAX_NUM_SUBCARRIERS									780

#elif (BANDWIDTH == BANDWIDTH_50M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		1
	#define NUM_SYMBOLS_PER_DMA									1
	
	#define POINTS_FFT											2048
	#define NUM_SAMPLES_LCP										(352/2)
	#define NUM_SAMPLES_SCP										(288/2)
	#define BASEBAND_SPS_TX										61440
	#define BASEBAND_SPS_RX										BASEBAND_SPS_TX
	#define MAX_NUM_SUBCARRIERS									1596


#elif (BANDWIDTH==BANDWIDTH_100M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		2
	#define NUM_SYMBOLS_PER_DMA									1
	#define NUM_IFFT_IN_BUFFERS									2
	
	#define MAX_NUM_SUBCARRIERS									3276
	#define NUM_SAMPLES_LCP										352
	#define NUM_SAMPLES_SCP										288
	#define POINTS_FFT											(4096)
	#define BASEBAND_SPS_TX										122880
	#define BASEBAND_SPS_RX										122880

#elif (BANDWIDTH==BANDWIDTH_200M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		2
	#define NUM_SYMBOLS_PER_DMA									1
	#define NUM_IFFT_IN_BUFFERS									2    //2 means pingpong, this is needed when FR2 and IFFT input is from DDR.

	#define MAX_NUM_SUBCARRIERS									(3276*2)	//3168	//3276
	#define NUM_SAMPLES_LCP										(352*2)		//352 //544
	#define NUM_SAMPLES_SCP										(288*2)
	#define POINTS_FFT											(4096*2)
	#if BANDWIDTH_CODE==BANDWIDTH_100M_T4x_30K_491_245
	#define BASEBAND_SPS_TX										491520
	#else
	#define BASEBAND_SPS_TX										245760
	#endif
	#define BASEBAND_SPS_RX										245760
		
#elif (BANDWIDTH==BANDWIDTH_400M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		2
	#define NUM_SYMBOLS_PER_DMA									1
	#define NUM_IFFT_IN_BUFFERS									2    //2 means pingpong, this is needed when FR2 and IFFT input is from DDR.

	#define MAX_NUM_SUBCARRIERS									(3276*4)	//3168	//3276
	#define NUM_SAMPLES_LCP										(352*4)		//352 //544
	#define NUM_SAMPLES_SCP										(288*4)
	#define POINTS_FFT											(4096*4)
	#define BASEBAND_SPS_TX										491520
	#if BANDWIDTH_CODE==BANDWIDTH_100M_T4x_30K_491_245
	#define BASEBAND_SPS_RX										245760
	#else
	#define BASEBAND_SPS_RX										491520
	#endif
#else
#error ERROR bandwidth
#endif

		
#elif SCS==SCS_60K
	#define NUM_SLOTS_1ms										4
	#define NUM_IFFT_IN_BUFFERS									1

#if (BANDWIDTH==BANDWIDTH_25M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		1
	#define NUM_SYMBOLS_PER_DMA									1

	#define MAX_NUM_SUBCARRIERS									372
	#define NUM_SAMPLES_LCP										52
	#define NUM_SAMPLES_SCP										36
	#define POINTS_FFT											512
	#define BASEBAND_SPS_TX										30720
	#define BASEBAND_SPS_RX										30720
		
#elif (BANDWIDTH==BANDWIDTH_50M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		1
	#define NUM_SYMBOLS_PER_DMA									1

	#define MAX_NUM_SUBCARRIERS									(372*2)
	#define NUM_SAMPLES_LCP										(52*2)
	#define NUM_SAMPLES_SCP										(36*2)
	#define POINTS_FFT											(512*2)
	#define BASEBAND_SPS_TX										(30720*2)
	#define BASEBAND_SPS_RX										(30720*2)

#elif (BANDWIDTH==BANDWIDTH_100M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		1
	#define NUM_SYMBOLS_PER_DMA									1

	#define MAX_NUM_SUBCARRIERS									(372*4)
	#define NUM_SAMPLES_LCP										(52*4)
	#define NUM_SAMPLES_SCP										(36*4)
	#define POINTS_FFT											(512*4)
	#define BASEBAND_SPS_TX										(30720*4)
	#define BASEBAND_SPS_RX										(30720*4)

#elif (BANDWIDTH==BANDWIDTH_200M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		2
	#define NUM_SYMBOLS_PER_DMA									1
	#define NUM_IFFT_IN_BUFFERS									2    //2 means pingpong, this is needed when FR2 and IFFT input is from DDR.

	#define MAX_NUM_SUBCARRIERS									3168	//3168	//3276
	#define NUM_SAMPLES_LCP										416		//352 //544
	#define NUM_SAMPLES_SCP										288
	#define POINTS_FFT											(4096)
	#define BASEBAND_SPS_TX										245760
	#define BASEBAND_SPS_RX										245760
#endif
	
#elif SCS==SCS_120K
	#define NUM_SLOTS_1ms										8		
		
#if (BANDWIDTH==BANDWIDTH_400M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		2
	#define NUM_SYMBOLS_PER_DMA									1
	#define NUM_IFFT_IN_BUFFERS									2    //2 means pingpong, this is needed when FR2 and IFFT input is from DDR.
	
	#define MAX_NUM_SUBCARRIERS									3168	//3168	//3276
	#define NUM_SAMPLES_LCP										544		//352 //544
	#define NUM_SAMPLES_SCP										288
	#define POINTS_FFT											(4096)
	#define BASEBAND_SPS_TX										491520
	#define BASEBAND_SPS_RX										491520
	
#elif (BANDWIDTH==BANDWIDTH_800M)
	#define NUM_DMA_CHANNELS_READING_SYMBOLS_FROM_HOST_MEM		2
	#define NUM_SYMBOLS_PER_DMA									2
	#define NUM_IFFT_IN_BUFFERS									2    //2 means pingpong, this is needed when FR2 and IFFT input is from DDR.
	
	#define MAX_NUM_SUBCARRIERS									(2*3168)	//3168	//3276
	#define NUM_SAMPLES_LCP										(2*544)	
	#define NUM_SAMPLES_SCP										(2*288)
	#define POINTS_FFT											(2*4096)
	#define BASEBAND_SPS_TX										983040
	#define BASEBAND_SPS_RX										983040
#endif
		
#else
#error ERROR: undefined SCS
#endif

#if (ANTENNA_ID_RX <=3 )  //FR1
	#define FR1									1
	#define FR2									0

	#define TX_QEC_BEFORE_UPSAMPLING			0
	#define RX_QEC_AFTER_DOWNSAMPLING			0


#else //FR2
	#define FR1									0
	#define FR2									1

	#ifndef TX_QEC_BEFORE_UPSAMPLING
	#define TX_QEC_BEFORE_UPSAMPLING			1
	#endif
	#ifndef RX_QEC_AFTER_DOWNSAMPLING
	#define RX_QEC_AFTER_DOWNSAMPLING			1
	#endif
#endif


#ifndef HWDCM_ENABLE
	#define HWDCM_ENABLE			0
#endif

#if RX_DCS_SPS < DCS_SPS_983M && HWDCM_ENABLE != 0
	#ERROR, LSDCS must have HWDCM_ENABLE 0
#endif


//if it's 1T1R on 1core, the following core load is for coreA.
#ifndef LOAD_TX_COREB
	#define LOAD_TX_COREB				(85*(1-FR2)+70*FR2)
#endif
#ifndef LOAD_RX_COREB
	#define LOAD_RX_COREB				(50*(1-FR2)+70*FR2)
#endif

#define	NUM_SYMBOLS_LOAD_BEFORE_TX_ALLOWED_RISING			3
#if (NUM_1T_IN_CORE*NUM_1R_IN_CORE != 0)
#define TX_FLOW_CONTROL_PER_SYM_INTERVAL 					0//(NUM_CYCLE_PER_SYMBOL_LCP(BASEBAND_SPS_TX)*LOAD_TX_COREB/(100-LOAD_RX_COREB))
#endif

#define RX_AXIQ_SPS											(RX_DCS_SPS/(HWDCM_ENABLE+1))
#define UPSAMPLING_RATIO									(TX_DCS_SPS/BASEBAND_SPS_TX)
#define DOWNSAMPLING_RATIO									(RX_AXIQ_SPS/BASEBAND_SPS_RX)

#if (NUM_1T_IN_CORE && ((UPSAMPLING_RATIO < 1) || (UPSAMPLING_RATIO > 4)))
#error ERROR BANDWIDTH CONFIG, Please fix the config
#endif
#if (NUM_1R_IN_CORE && ((DOWNSAMPLING_RATIO < 1) || (DOWNSAMPLING_RATIO > 4)))
#error ERROR BANDWIDTH CONFIG, Please fix the config
#endif

#define DPD_SPS_RATIO	UPSAMPLING_RATIO

#define NUM_SYM_PER_SLOT									14
#define NUM_SYM_1ms											(NUM_SYM_PER_SLOT*NUM_SLOTS_1ms)
#define NUM_SYM_PER_CP_PERIOD								(NUM_SYM_PER_SLOT*NUM_SLOTS_1ms/2)	//NUM_SYM_PER_SLOT	//(NUM_SYM_PER_SLOT*FR_RATIO)
#ifndef TIME_LEN_SYSPERIOD
#define TIME_LEN_SYSPERIOD									40 //system period in ms, must be at least 40 otherwise dumping will not work if dump size equals to this length(default dump size is 20ms)
#endif
#define NUM_SLOT_PER_SYSPERIOD								(TIME_LEN_SYSPERIOD*NUM_SLOTS_1ms)
#define NUM_SYM_PER_SYSPERIOD								(NUM_SLOT_PER_SYSPERIOD*NUM_SYM_PER_SLOT)
#define SIZE_PER_SYSPERIOD(sps)								((sps)*4*TIME_LEN_SYSPERIOD)
#define NUM_DIFF_LCS_SCP									((NUM_SAMPLES_LCP)-(NUM_SAMPLES_SCP))
#define NUM_PRB												(MAX_NUM_SUBCARRIERS/12)  //each PRB has 12 subcarriers
#define MAX_POINTS_TIME_DOMAIN_PER_SYMBOL					(POINTS_FFT + NUM_SAMPLES_LCP)
#define SIZE_FFT											(POINTS_FFT*4)	
#define SIZE_SYMBOL_DATA_TIME_DOMAIN_LCP					((POINTS_FFT+NUM_SAMPLES_LCP)*4)	//Long CP
#define SIZE_SYMBOL_DATA_TIME_DOMAIN_SCP					((POINTS_FFT+NUM_SAMPLES_SCP)*4)	//Short CP
#define NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_LCP				((POINTS_FFT+NUM_SAMPLES_LCP))	//Long CP
#define NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP				((POINTS_FFT+NUM_SAMPLES_SCP))	//Short CP
#define NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN(sym_id)			(NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP+NUM_DIFF_LCS_SCP*(((sym_id)%NUM_SYM_PER_CP_PERIOD)==0))
#define NUM_SAMPLE_PER_CP_PERIOD							((NUM_SYM_PER_CP_PERIOD-1)*NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP+NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_LCP)
#define NUM_CYCLE_PER_SAMPLE(sps)							(CORE_CLOCK/LOWEST_SPS)/((sps)/LOWEST_SPS)	//do not put the whole expression into () 
#define NUM_SAMPLE_PER_CYCLE(sps)							((sps)/LOWEST_SPS)/(CORE_CLOCK/LOWEST_SPS)	//do not put the whole expression into () 
#define NUM_CYCLE_PER_SYMBOL_LCP(sps)						(NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_LCP*NUM_CYCLE_PER_SAMPLE(sps))
#define NUM_CYCLE_PER_SYMBOL_SCP(sps)						(NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP*NUM_CYCLE_PER_SAMPLE(sps))
#define NUM_CYCLE_PER_MS(ms)								(CORE_CLOCK*(ms))
#define NUM_CYCLE_PER_US(us)								(CORE_CLOCK/1000*(us))
#define MAX_TIMING_OFFSET_SAMPLES_AT_CIRC_BUFFER			ALIGN_DOWN((NUM_SAMPLES_SCP),32)		//max timing offset each symbol is CP size.
#define NUM_CYCLES_FETCH_TX_SYMBOL_BEFORE_TXALLOWED			(NUM_CYCLE_PER_SYMBOL_LCP(BASEBAND_SPS_TX)*3/2) //NUM_CYCLE_PER_SYMBOL_LCP(BASEBAND_SPS_TX) //NUM_CYCLE_PER_US(35) 
		
#if (SCS == SCS_15K)
#define NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(slot_id)			(NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP*NUM_SYM_PER_SLOT+NUM_DIFF_LCS_SCP*2)
#else
#define NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(slot_id)			(NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP*NUM_SYM_PER_SLOT+NUM_DIFF_LCS_SCP*(((slot_id)%(NUM_SLOTS_1ms/2))==0))
#endif

//mixer and power measurement period for FDD, 20 slots
#define NUM_SAMPLES_MIXER_PERIOD							(NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(0)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(1)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(2)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(3)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(4)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(5)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(6)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(7)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(8)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(9)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(10)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(11)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(12)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(13)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(14)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(15)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(16)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(17)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(18)+ \
															 NUM_SAMPLE_PER_SLOT_TIME_DOMAIN(19))
		
#if(TX_SYMBOL_TYPE == TYPE_SYMBOL_FREQDOMAIN)
	#define TX_SYM_SIZE 		ALIGN128(POINTS_FFT*4)
#else
	#define TX_SYM_SIZE 		ALIGN128(MAX_POINTS_TIME_DOMAIN_PER_SYMBOL*4)
#endif
#if(RX_SYMBOL_TYPE == TYPE_SYMBOL_FREQDOMAIN)
	#define RX_SYM_SIZE 		ALIGN128(POINTS_FFT*4)
#else
	#define RX_SYM_SIZE 		ALIGN128(MAX_POINTS_TIME_DOMAIN_PER_SYMBOL*4)
#endif

#define MAX_NUM_ANT_BUFFERS				4
		
#ifndef NUM_TX_ANT_BUFFERS		
#define NUM_TX_ANT_BUFFERS				2
#endif
#ifndef NUM_RX_ANT_BUFFERS		
#define NUM_RX_ANT_BUFFERS				2
#endif

#if NUM_TX_ANT_BUFFERS > NUM_RX_ANT_BUFFERS
#define NUM_ANT_BUFFERS					NUM_RX_ANT_BUFFERS
#define NUM_EXTRA_RX_ANT_BUFFERS		0
#define NUM_EXTRA_TX_ANT_BUFFERS		(NUM_TX_ANT_BUFFERS-NUM_RX_ANT_BUFFERS)
#else
#define NUM_ANT_BUFFERS					NUM_TX_ANT_BUFFERS
#define NUM_EXTRA_TX_ANT_BUFFERS		0
#define NUM_EXTRA_RX_ANT_BUFFERS		(NUM_RX_ANT_BUFFERS-NUM_TX_ANT_BUFFERS)
#endif
	
#if NUM_TX_ANT_BUFFERS > MAX_NUM_ANT_BUFFERS
#error WRONG NUM TX BUFFERS
#endif
#if NUM_RX_ANT_BUFFERS > MAX_NUM_ANT_BUFFERS
#error WRONG NUM RX BUFFERS
#endif
	
#ifndef TX_BLOCK_SIZE
#define TX_BLOCK_SIZE		(POINTS_FFT*4)							//size of block before up-sampling
#endif
		
#ifndef SIZE_TX_ANT_BUFFER
#if((TX_BLOCK_SIZE*UPSAMPLING_RATIO) > (40*1024))
#define SIZE_TX_ANT_BUFFER		(40*1024)
#undef  TX_BLOCK_SIZE
#define TX_BLOCK_SIZE			(SIZE_TX_ANT_BUFFER/UPSAMPLING_RATIO)
#else
#define SIZE_TX_ANT_BUFFER		(TX_BLOCK_SIZE*UPSAMPLING_RATIO)
#endif
#else
#undef  TX_BLOCK_SIZE
#define TX_BLOCK_SIZE			(SIZE_TX_ANT_BUFFER/UPSAMPLING_RATIO)		
#endif
		
#ifndef RX_BLOCK_SIZE
#define RX_BLOCK_SIZE			(TX_BLOCK_SIZE*BANDWIDTH_RX/BANDWIDTH_TX)							//size of block after down-sampling
#endif
		
#define SIZE_RX_ANT_BUFFER	(RX_BLOCK_SIZE*DOWNSAMPLING_RATIO)
	
#ifndef IFFT_CIRC_SIZE
#define IFFT_CIRC_SIZE				(TX_BLOCK_SIZE*4)
#endif
#ifndef FFT_CIRC_SIZE
#define FFT_CIRC_SIZE				(RX_BLOCK_SIZE*4)
#endif

#define NUM_CFR_ADVANCED_SAMPLES	0   /* CFR permanently disabled on LA9310 */
#define NUM_CIRC_SAMPLES_CFR		0


#ifndef IFFT_CIRC_SIZE
#define IFFT_CIRC_SIZE		ALIGN(NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_LCP*4*2,TX_BLOCK_SIZE)
#endif
#ifndef FFT_CIRC_SIZE
#define FFT_CIRC_SIZE		ALIGN(NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_LCP*4*2,RX_BLOCK_SIZE)
#endif

#if NUM_1T_IN_CORE && ((IFFT_CIRC_SIZE % TX_BLOCK_SIZE) != 0)
#error ERROR
#endif
#if NUM_1R_IN_CORE && ((FFT_CIRC_SIZE % RX_BLOCK_SIZE) != 0)
#error ERROR
#endif
#if(TX_BLOCK_SIZE < (NUM_SAMPLES_LCP*4+128))
ERROR
#endif
#if(RX_BLOCK_SIZE < (NUM_SAMPLES_LCP*4+128))
ERROR
#endif

#define NUM_BLOCKS_IN_IFFTOUT_CIRC_BUF		(IFFT_CIRC_SIZE/TX_BLOCK_SIZE)
#define NUM_BLOCKS_IN_FFTIN_CIRC_BUF		(FFT_CIRC_SIZE/RX_BLOCK_SIZE)

#ifndef SINAD_TEST_ENABLE
#define SINAD_TEST_ENABLE	0
#endif

#ifndef PHASE_COMPENSATION_DISABLE
#define PHASE_COMPENSATION_DISABLE		(1-FR1)
#endif
#ifndef CFO_DISABLE
#define CFO_DISABLE						0
#endif

#ifndef TXDCS_FDD
#define TXDCS_FDD			0
#endif
#ifndef RXDCS_FDD
#define RXDCS_FDD			TXDCS_FDD
#endif

#if(SINAD_TEST_ENABLE)
#if(SIZE_RX_ANT_BUFFER<32768)
ERROR
#endif
#endif

#if(ARCH_TYPE_T	== ARCH_1T_1CORE && NUM_2XUP_IN_BUFFERS	!= 1)
#Error. 1T 1core must set NUM_2XUP_IN_BUFFERS 1
#endif
#if(ARCH_TYPE_R	== ARCH_1R_1CORE && NUM_2XDOWN_OUT_BUFFERS	!= 1)
#Error. 1R 1core must set NUM_2XDOWN_OUT_BUFFERS 1
#endif

#ifndef TX_QEC_BEFORE_UPSAMPLING
#define TX_QEC_BEFORE_UPSAMPLING	0
#endif

/*************************************************************************************************
 * DMA channels assignment for dynamic channel assignment
 *************************************************************************************************/
//for dynamic channel assignment
#if(CORE_TYPE==TYPE_CORE_A)
		#define DMA_CHAN_START				7	//must not be channel 0
		#define DMA_CHAN_NUM				2
#else
	#define DMA_CHAN_START					1	//must not be channel 0
	#define DMA_CHAN_NUM					8
#endif

//for static channel assignment
#define DMA_CHAN_HIGH_PRIORITY						10
#define DMA_CHAN_DUMP_ANT_DATA						12 //28   
#define DMA_CHAN_DUMP_TX_SYMBOLS					DMA_CHAN_DUMP_ANT_DATA
#define DMA_CHAN_FETCH_DU_TX_SYMBOLS(dma_id)		(13+dma_id)
#define DMA_CHAN_SEND_DU_RX_SYMBOLS(dma_id)			(13+dma_id) //TX RX use same chan to avoid TX underflow
#define DMA_CHAN_CELL_TRACKING						DMA_CHAN_SEND_DU_RX_SYMBOLS(1)
#define DMA_CHAN_USED_FOR_PARA_UPDATE				15
#define DMA_CHAN_INJECT_ANT_DATA					9
#define DMA_CHAN_USED_FOR_LOG 						10 //10  to use the same TX symbol fetching DMA to avoid underflow


//DU-DFE MSG define, //tr_id=0,1,2,3,4,5.//addr is 128 byte aligned
#define GET_DU_DFE_MSG_TX_SYM_ANT_ID(msg32bit)		((msg32bit)>>28)
#define GET_DU_DFE_MSG_TX_SYM_ADDR(msg32bit)		(((msg32bit)&0x1FFFFFF)<<7)


//AXI flags used on core A
//only the 16LSB of flag0 register are used for flags. 16MSB and 32bit flag1 registers are used for test cmd and para
#define FLAG_TX_DATA_BLOCK_ACK			0x00000002
#define FLAG_SLOT_PATTERN_ACK			0x00000004
#define FLAG_TX_LAST_SYMBOL				0x00000020       //flag for last DL symbol of the TDD pattern
//#define FLAG_TEST_CMD					0x00000040


#define FLAG_TX_SYM_BUF_RDY(ant_id, num_sym, buf_id)			(0x00000100<<(((ant_id)%4)*(num_sym)+(buf_id)))	//flag 8-15 used here
#define FLAG_TX_SYM_BUF_CONSUMED(ant_id, num_sym, buf_id)		(0x00000100<<(((ant_id)%4)*(num_sym)+(buf_id)))

#if(CORE_TYPE==TYPE_CORE_A || CORE_TYPE==TYPE_CORE_B)
#ifndef ANTENNA_ID_TX
#ifndef ANTENNA_ID_RX
ERROR
#endif
#endif
#else
#define ANTENNA_ID_RX	0
#define ANTENNA_ID_TX   0
#define CORE_B_ID   	0
#define CORE_B_TR_ID   	0
#endif

#if (CORE_TYPE == TYPE_CORE_A && ARCH_TYPE_T == ARCH_1T_2CORE)
	#define IFFT_SCRATCH_SIZE   4
#else
	#if BYPASS_IFFT
		#define IFFT_SCRATCH_SIZE   4
	#else
		#define IFFT_SCRATCH_SIZE   POINTS_FFT
	#endif
#endif
#if (CORE_TYPE == TYPE_CORE_A && ARCH_TYPE_R == ARCH_1R_2CORE)
	#define FFT_SCRATCH_SIZE   4
#else
	#if BYPASS_FFT
		#define FFT_SCRATCH_SIZE   4
	#else
		#define FFT_SCRATCH_SIZE   POINTS_FFT
	#endif
#endif

#define DPD_SCRATCH_SIZE   4   /* DPD permanently disabled on LA9310 */

#if SINAD_TEST_ENABLE
#define SINA_VCPU_SCRATCH_SIZE 65536
#define SINA_IPPU_SCRATCH_SIZE (96*1024)
#else
#define SINA_VCPU_SCRATCH_SIZE 4
#endif

#if ((CORE_TYPE == TYPE_CORE_A) && NUM_1R_IN_CORE && (DOWNSAMPLING_RATIO >= 4))
#define SCRATCH_2XDOWN_SIZE		(SIZE_RX_ANT_BUFFER/2)
#else
#define SCRATCH_2XDOWN_SIZE		4
#endif

#define NUM_BYTE_COMP_SCRATCH_VCPU	4   /* eCPRI compression permanently bypassed on LA9310 */

//sections defines
#if RX_CIRC_BANK
#define RX_CIRC_SECTION	__attribute__ ((section (".ibss0")))
#else
#define RX_CIRC_SECTION
#endif

#if DOWN_SAMPLING_TAPS_BANK
#define DOWN_SAMPLING_TAPS_SECTION	__attribute__ ((section (".idata")))
#else
#define DOWN_SAMPLING_TAPS_SECTION
#endif

#if HISTORY_ON_BANK
#define HISTORY_SECTION	__attribute__ ((section (".ibss")))
#else
#define HISTORY_SECTION
#endif

#if ANTMAN_BUF_ON_BANK
#define ANTMAN_BUF_SECTION	__attribute__ ((section (".ibss")))
#else
#define ANTMAN_BUF_SECTION
#endif

#ifndef DFE_ONLY
#define DFE_ONLY				0
#endif
#ifndef LPHY_ONLY_SUPPORT
#define	LPHY_ONLY_SUPPORT		1
#endif

#if LPHY_ONLY_SUPPORT
#define LPHY_ONLY_CONFIG		__ip_read(IP_IDX_DFE_MODE_HI, DFE_MODE_LOWPHYONLY)
#else
#define LPHY_ONLY_CONFIG		0
#endif

#if HIGHPHY_LOWPHY_IF_TYPE == IF_TYPE_HOST_VSPA
#define MAX_NUM_TX_SYM_IN_BUF	28
#define MAX_NUM_TX_BITS_IN_USE 	28
#define MAX_NUM_RX_SYM_IN_BUF	28
#define MAX_NUM_RX_BITS_IN_USE 	28
#else
#define MAX_NUM_TX_SYM_IN_BUF	2
#define MAX_NUM_TX_BITS_IN_USE 	2
#define MAX_NUM_RX_SYM_IN_BUF	14
#define MAX_NUM_RX_BITS_IN_USE 	2
#endif

#ifndef RX_LP_FIR_FILTER_ENABLE
#define RX_LP_FIR_FILTER_ENABLE	0
#endif

#ifndef FDD_1T2R
#define FDD_1T2R		0
#endif


#if FDD_1T2R

#else

#endif

//enable/disable cycle count measurement for each modules
#if CYCLE_COUNT_MEA_DISABLE
#define CODE_MEA_IFFT(x)
#define CODE_MEA_FFT(x)
#define CODE_MEA_QEC_TX(x)
#define CODE_MEA_QEC_RX(x)
#define CODE_MEA_UP_SAMPLING(x)
#define CODE_MEA_DOWN_SAMPLING(x)

#else
#define CODE_MEA_IFFT(x)				x
#define CODE_MEA_FFT(x)					x
#define CODE_MEA_QEC_TX(x)				x
#define CODE_MEA_QEC_RX(x)				x
#define CODE_MEA_UP_SAMPLING(x)			x
#define CODE_MEA_DOWN_SAMPLING(x)		x

#endif

/* Pipeline-state enum, mailbox/pattern struct types, dfe_ctrl_s control block. */
#include "pair_config_types.h"


#ifndef NUM_UP_SAMPLING_FILTER_TAPS
#if(UPSAMPLING_RATIO >= 4)
	#if FR2==0  //FR1 using 4x64 taps
	#define NUM_UP_SAMPLING_FILTER_TAPS		64
	extern int filter_taps_upsampling[SIZE_X4_INTERP_TAP64_FILTER_TAPS/4] __align_vec;
	#else
	#define NUM_UP_SAMPLING_FILTER_TAPS		32
	
	extern int filter_taps_upsampling[SIZE_X4_INTERP_TAP32_FILTER_TAPS/4] __align_vec;
	#endif

#elif(UPSAMPLING_RATIO >= 2)
	#if FR2==0  //FR1 using 2x32 taps
	#define NUM_UP_SAMPLING_FILTER_TAPS		32
	extern int filter_taps_upsampling[SIZE_X2_INTERP_TAP32_FILTER_TAPS/4] __align_vec;
	#else
#if BANDWIDTH_TX == BANDWIDTH_800M
	#define NUM_UP_SAMPLING_FILTER_TAPS		16
	extern int filter_taps_upsampling[SIZE_X2_INTERP_TAP16_FILTER_TAPS/4] __align_vec;
#else
	#define NUM_UP_SAMPLING_FILTER_TAPS		32
	extern int filter_taps_upsampling[SIZE_X2_INTERP_TAP32_FILTER_TAPS/4] __align_vec;
#endif
	#endif
#else
	#define NUM_UP_SAMPLING_FILTER_TAPS		4	//unused, just to make 4bitcode to 0
#endif
#endif
	
#if NUM_UP_SAMPLING_FILTER_TAPS == 64
#define NUM_UP_SAMPLING_FILTER_TAPS_4BITCODE	0xF
#elif NUM_UP_SAMPLING_FILTER_TAPS == 32
#define NUM_UP_SAMPLING_FILTER_TAPS_4BITCODE	0xE
#elif NUM_UP_SAMPLING_FILTER_TAPS == 48
#define NUM_UP_SAMPLING_FILTER_TAPS_4BITCODE	0xD
#else
#define NUM_UP_SAMPLING_FILTER_TAPS_4BITCODE	(NUM_UP_SAMPLING_FILTER_TAPS/2-2)
#endif
/* extern axi_flag_msg_response{,1} moved to global_var.h */

/* ASM/C function decls, inline helpers, FDD_MODE/RX_OPTION8_CFG defines. */
#include "pair_config_funcs.h"

#endif /* PAIR_CONFIG_H */
