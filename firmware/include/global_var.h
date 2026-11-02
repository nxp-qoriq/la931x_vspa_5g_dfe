// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __GLOBAL_VAR__
#define __GLOBAL_VAR__

#include "pair_config.h"
#include "qec.h"
#include "qec_opt.h"

/* All globally-shared mutable state lives here. */
extern struct_dfe_ctrl dfe_ctrl;
extern unsigned int axi_flag_msg_response[]__attribute__ ((aligned (AXI_BURST_SIZE)));
extern unsigned int axi_flag_msg_response1[]__attribute__ ((aligned (AXI_BURST_SIZE)));
extern unsigned int iFFTout_CFRinout[IFFT_CIRC_SIZE/4];
extern unsigned int FFTin[FFT_CIRC_SIZE/4];
extern unsigned int FFTin2nd[FFT_CIRC_SIZE/4];
extern int real_window[];
extern volatile unsigned int dbg[];

#define BITREV_NOT_STARTED		1
#define BITREV_STARTED			2
#define BITREV_DONE				0    //IPPU will clear to 0 to indicate bitrev done

extern volatile unsigned int	rx_bitrev_progress_ippu[],tx_bitrev_progress_ippu;
extern volatile unsigned int	rx_bitrev_done[], tx_bitrev_done;
extern float phcom_coeff_tx[NUM_SYM_PER_CP_PERIOD][2];
extern float phcom_coeff_rx[NUM_SYM_PER_CP_PERIOD][2];

#if USING_OPTIMIZED_QEC == 1
extern qec_params_opt_t DFE_qec_params_opt_tx;
extern qec_params_opt_t DFE_qec_params_opt_rx[];
#define DFE_QEC_PARAMS_BUF_TX	DFE_qec_params_opt_tx
#define DFE_QEC_PARAMS_BUF_RX	DFE_qec_params_opt_rx
#else
extern qec_params_t DFE_qec_params_tx;
extern qec_params_t DFE_qec_params_rx[];
extern int history_txqec[];
extern int history_rxqec[];
#define DFE_QEC_PARAMS_BUF_TX	DFE_qec_params_tx
#define DFE_QEC_PARAMS_BUF_RX	DFE_qec_params_rx
#endif

extern int history_rx_fir[MAX_NUM_1R_IN_CORE][SIZE_FIR_TAP63_HISTORY/4];
extern int rx_lpf_taps[];

#include "dump_inject.h"
#if NUM_1T_IN_CORE
extern struct_dump_inject dpd_dump;
extern struct_dump_inject ant_tx_dump;
#endif
#if NUM_1R_IN_CORE
extern struct_dump_inject ant_rx_dump[];
#endif

#endif
