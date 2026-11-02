// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef PROJ_CONFIG_H_
#define PROJ_CONFIG_H_

// Configuration: 25 MHz, 30 kHz SCS, 61+61 subcarriers, 1T2R, 1 core
#define BANDWIDTH_CODE				BANDWIDTH_25M_30K_61_61

#define FDD_ONLY					0

#define NUM_2XDOWN_TAPS				8  //8 or 64
#define CYCLE_COUNT_MEA_DISABLE		1

/*******************************************************************************************************
 * DFE architecture defines
 * TX/RX ANT ID,	core B,		coreA
 *		0			0			0
 *******************************************************************************************************/
#define HIGHPHY_LOWPHY_IF_TYPE		IF_TYPE_HOST_VSPA
#define ARCH_TYPE_T					ARCH_1T_1CORE
#define ARCH_TYPE_R					ARCH_1R_1CORE

//these 2 MACROs define which cores communicate with the host
#define ANT_CORE_MAPPING_TX			0xFFFFFFF0 //ANT0->core0
#define ANT_CORE_MAPPING_RX			0xFFFFFF00 //ANT0->core0, ANT1->core0

#define CORES_USED_AS_DFE			0x01   //core0 used as DFE

#define TXDCS_FDD					0
#define RXDCS_FDD					0
#define	LPHY_ONLY_SUPPORT			0
#define RX_OPTION8_DISABLE			1   //memory for rx symbol buffer is not enough for RX option8

// Bandwidth: 25 MHz, 30 kHz SCS
#define	CELLTRACK_ENABLE			1
#define TX_BLOCK_SIZE				1280  //must be less than 64 KB
#define IFFT_CIRC_SIZE				(TX_BLOCK_SIZE*3)
#define FFT_CIRC_SIZE				IFFT_CIRC_SIZE

// DFE kernel enable/disable
#define DEFAULT_CFR_DISABLE			1
#define DEFAULT_DPD_DISABLE			1
#define DEFAULT_TX_QEC_DISABLE		0
#define DEFAULT_RX_QEC_DISABLE		0
#define BYPASS_IFFT					0
#define BYPASS_FFT					0
#define BYPASS_ECPRI_COMMPRESSION	1
#define BYPASS_ECPRI_DECOMMPRESSION	1
#define PHASE_COMPENSATION_DISABLE	0
#define CFO_DISABLE					0

//DFE ARCH and CORE partitioning defines, users can change to their need.
//core0 configurations
#define CORE_TYPE			TYPE_CORE_A
#define CORE_A_ID			SELF_CORE_ID
#define ANTENNA_ID_TX		CORE_A_ID
#define ANTENNA_ID_RX		CORE_A_ID
#define NUM_1T_IN_CORE		1
#define NUM_1R_IN_CORE		2
#define CORE_B_ID			(CORE_A_ID)
#define CORE_B_TR_ID		0

#define TX_SYMBOL_TYPE			TYPE_SYMBOL_TIMEDOMAIN	//TX support both freq and time domain.
#define RX_SYMBOL_TYPE			TYPE_SYMBOL_FREQDOMAIN	//RX support freq domain only.

//buffer bank defines
#define RX_CIRC_BANK			1
#define TX_SYM_QUEUE_BANK		1
#define RX_SYM_QUEUE_BANK		1
#define TRX_ANT_BUFFER0_BANK	0
#define TRX_ANT_BUFFER_BANK		0
#define UP_SAMPLING_TAPS_BANK	0
#define DOWN_SAMPLING_TAPS_BANK	0
#define HISTORY_ON_BANK			0
#define ANTMAN_BUF_ON_BANK		0
#define LOG_BUFFER_BANK			0
#define FLEXIBLE_SLOT_FORMAT_ENABLE	1
#define TIME_LEN_SYSPERIOD			40 //system period in ms
#define USING_OPTIMIZED_QEC			1

__inline__ unsigned int __max(unsigned int a, unsigned int b)
{
	if(a>=b)	return a;
	else		return b;
}
__inline__ unsigned int __min(unsigned int a, unsigned int b)
{
	if(a<=b)	return a;
	else		return b;
}

#endif /* PROJ_CONFIG_H_ */
