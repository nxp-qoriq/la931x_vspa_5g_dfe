// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#include "pair_config.h"
#include "txWindowing.h"
#include "qec.h"
#include "qec_opt.h"


struct_dfe_ctrl dfe_ctrl __attribute__ ((aligned (AXI_BURST_SIZE))) __attribute__ ((section (".vbss0")));

//TX CIRC buffer must not be put at beginning of bank0 or bank1 as ippu will decrease circular buffer by 4K points
//if the decreased pointer goes below 0, it can not wrap back into circular buffer, this could be silicon bug
//Put this buffer at least 16KB away from beginning of bank0 or bank1, in case FFT 4096 is used
#if TX_CIRC_BANK
unsigned int iFFTout_CFRinout[IFFT_CIRC_SIZE/4] __align_vec TX_CIRC_SECTION;
#else
unsigned int iFFTout_CFRinout[IFFT_CIRC_SIZE/4] __align_vec;
#endif

//circular buffer for FFT in, this buffer can not be put at address 0 due to IPPU simulator bug
//this buffer mst be put in VCPU DMEM
unsigned int FFTin[FFT_CIRC_SIZE/4] __align_vec RX_CIRC_SECTION;
#if FDD_1T2R
unsigned int FFTin2nd[FFT_CIRC_SIZE/4] __align_vec RX_CIRC_SECTION;
#endif

volatile unsigned int dbg[16];  //used for debugging

int real_window[] __align_vec = {
#if WINDOW_SIZE == 64
#include "real_win_vals_64.hex"
#elif WINDOW_SIZE == 32
#include "real_win_vals_32.hex"
#elif WINDOW_SIZE == 16
	#if POINTS_FFT == 512
	#include "real_win10_vals_16.hex"  //512 iFFF uses shorter tx windowing size
	#else
	#include "real_win_vals_16.hex"
	#endif
#else
ERROR;
#endif
};

#if POINTS_FFT != 4096  // the following must be on ippu DMEM, 512/1024/2048 bit rev use IPPU for the flags
volatile unsigned int	tx_bitrev_progress_ippu __attribute__((section (".ibss")));   //this will be written by IPPU, must be on IPPU DMEM due to Errata
volatile unsigned int	tx_bitrev_done __attribute__((section (".ibss")));
volatile unsigned int	rx_bitrev_progress_ippu[MAX_NUM_1R_IN_CORE] __attribute__((section (".ibss")));   //this will be written by IPPU, must be on IPPU DMEM due to Errata
volatile unsigned int	rx_bitrev_done[MAX_NUM_1R_IN_CORE] __attribute__((section (".ibss")));

#else  //4096 bit rev uses VCPU for the flags
volatile unsigned int	tx_bitrev_progress_ippu;   //this will be written by IPPU, must be on IPPU DMEM due to Errata
volatile unsigned int	tx_bitrev_done;
volatile unsigned int	rx_bitrev_progress_ippu[MAX_NUM_1R_IN_CORE];   //this will be written by IPPU, must be on IPPU DMEM due to Errata
volatile unsigned int	rx_bitrev_done[MAX_NUM_1R_IN_CORE];

#endif




float phcom_coeff_tx[NUM_SYM_PER_CP_PERIOD][2] __attribute__((section (".data_aligned"))) __align_vec =  //phase compensation coeff, each coeff has I and Q
#if NUM_SYM_PER_CP_PERIOD == 7
{1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0 };
#elif NUM_SYM_PER_CP_PERIOD == 14
{1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0};
#elif NUM_SYM_PER_CP_PERIOD == 28
{1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0};
#elif NUM_SYM_PER_CP_PERIOD == 56
{1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0};
#endif


float phcom_coeff_rx[NUM_SYM_PER_CP_PERIOD][2] __attribute__((section (".data_aligned"))) __align_vec =  //phase compensation coeff, each coeff has I and Q
#if NUM_SYM_PER_CP_PERIOD == 7
{1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0 };
#elif NUM_SYM_PER_CP_PERIOD == 14
{1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0};
#elif NUM_SYM_PER_CP_PERIOD == 28
{1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0};
#elif NUM_SYM_PER_CP_PERIOD == 56
{1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0,
 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0, 1.0, 0};
#endif

#if USING_OPTIMIZED_QEC == 1
#if QEC_IN_DPD_COEFF_SIZE_BYTES == 0
qec_params_opt_t DFE_qec_params_opt_tx __attribute__((section (".data_aligned"))) __align_vec =
{
//	1,1,0,0, 1,1,0,0, 1,1,0,0, 1,1,0,0, 1,0, 1,0, 0,0, 1,0, 1.0
//	1,1,0,0, 1,1,0,0, 1,1,0,0, 1,1,0,0, 1,1, 1,1, 0,0, 1,1, 1.0   //from v5.0, gain_im=gain_re
	1,1,0,0, 0,0, 1,1,0,0   //from v5.0.1, gain multiplied on f1,f2,f4
};
#endif
#else
qec_params_t DFE_qec_params_tx __attribute__((section (".data_aligned"))) __align_vec =
{
	0,0, 0,0, 0,0, 0,0, 0,0, 1,0, 0,0, 0,0,     0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 
//	0,0,   1,0,1,    1,0, 0,0, 1,0       //set to passthrough with filter taps count FD_tap_cnt = 0, 2nd value in this line
	0,0,   1,0,1,    1,1, 0,0, 1,1       //set to passthrough with filter taps count FD_tap_cnt = 0, 2nd value in this line
};
int history_txqec[128/4] __align_vec HISTORY_SECTION;		
#endif


#if USING_OPTIMIZED_QEC == 1
qec_params_opt_t DFE_qec_params_opt_rx[MAX_NUM_1R_IN_CORE] __attribute__((section (".data_aligned"))) __align_vec =
{
//	1,1,0,0, 1,1,0,0, 1,1,0,0, 1,1,0,0, 1,0, 1,0, 0,0, 1,0,   1.0,   0,0,0,0,0,0,0,0,
//	1,1,0,0, 1,1,0,0, 1,1,0,0, 1,1,0,0, 1,0, 1,0, 0,0, 1,0,   1.0,   0,0,0,0,0,0,0,0
//	1,1,0,0, 1,1,0,0, 1,1,0,0, 1,1,0,0, 1,1, 1,1, 0,0, 1,1,   1.0,   0,0,0,0,0,0,0,0,  //for v5.0
//	1,1,0,0, 1,1,0,0, 1,1,0,0, 1,1,0,0, 1,1, 1,1, 0,0, 1,1,   1.0,   0,0,0,0,0,0,0,0
	1,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, //for v5.0.1
	1,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
};
#else
qec_params_t DFE_qec_params_rx __attribute__((section (".data_aligned"))) __align_vec =
{
	0,0, 0,0, 0,0, 0,0, 0,0, 1,0, 0,0, 0,0,     0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 
//	0,0,   1,0,1,    1,0, 0,0, 1,0       //set to passthrough with filter taps count FD_tap_cnt = 0, 2nd value in this line
	0,0,   1,0,1,    1,1, 0,0, 1,1       //set to passthrough with filter taps count FD_tap_cnt = 0, 2nd value in this line
};
int history_rxqec[128/4] __align_vec HISTORY_SECTION;		
#endif

#if RX_LP_FIR_FILTER_ENABLE
int history_rx_fir[MAX_NUM_1R_IN_CORE][SIZE_FIR_TAP63_HISTORY/4] __align_vec HISTORY_SECTION;
int rx_lpf_taps[SIZE_FIR_TAP63_FILTER_TAPS/4] __attribute__((section (".data_aligned"))) __align_vec =
{
//#include "para_files/rx_lpf_63taps.txt"
		0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,
		0x3f800000,
		0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0
};
#endif

/***********************************************************************************
 *                DPD  Parameters
 ************************************************************************************/

