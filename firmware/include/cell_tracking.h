// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __CELL_TRACKING_H__
#define __CELL_TRACKING_H__

//#include "cpe-pdsch-common.h"
#include "nr-dl-z.h"
//#include "cpe-csirs.h"

//! @brief  Slotted SSB Indication: "No Detect" Cell ID, reported when SSB is NOT detected
#define L1C_V2A_CELL_ID_NO_SSB_DETECTED     0x0000DEAD

/*
//! @brief      Processing task control block data type.
struct cpe_slot_dl_z_tcb_t {
    struct {
        uint32_t symb14;    //!< Symbol index, 0-based, modulo 14.
    } index;
    uint32_t ssb_bitmap_td[CPE_MAX_NUM_CC]; //!< Working copy of ssb symbol bitmap (to support >1 SSB per slot)
    uint32_t nsym_dl;                       //!< # symbols for cpe_dl_z_process()
    uint32_t scs_ssb[CPE_MAX_NUM_CC];       //!< Sub-Carrier Spacing (SCS) for slotted SSB measurement.
    uint32_t ssb_sym[CPE_MAX_NUM_CC];       //!< Slotted SSB: TD bitmap of SSS symbols
    uint32_t ssb_map[CPE_MAX_NUM_CC];
    uint32_t ssb_path_map[CPE_MAX_NUM_CC];  //!< Slotted SSB: RX path bitmap
    uint32_t dmrs_sym[CPE_MAX_NUM_CC];
    uint32_t dmrs_map[CPE_MAX_NUM_CC];
    uint32_t msmt_flag;
    uint32_t fft_size[CPE_MAX_NUM_CC];
    uint32_t hram_cc_offset[CPE_MAX_NUM_CC];//!< Offset in HRAM for each CC. Skip CCs in case of multiple CCs use case
    uint32_t guard_size[CPE_MAX_NUM_CC];    //!< Guard size in words(REs) - (fft_size - re_size)/2, floor aligned to AXI_BUS

    struct cpe_l1c_dl_slot_config_t const *pcfg;   //!< L1C dl slot configuration.
    uint32_t axis_hi_mask_sticky;                  //!< current slot axis_hi sticky flag
    uint32_t csirs_pending;                        //!< slot update arrived, slot config. contains CSIRS
    uint32_t csirs_running;                        //!< CSIRS entry setup and running
    uint32_t pwr_meas_pending;                     //!< slot update arrived, slot config. contains pwr meas.
    uint32_t last_sym_bit;                         //!< bit-field for last symbol for AXIS to exit

    #if (CSIRS_SUPPORT_Z)
    struct cpe_csirs_tcb_t csirs;
    #endif
	uint8_t ssb_cnt[CPE_MAX_NUM_CC];        //!< field that tracks which SSB within the slot is being measured
    uint8_t pad[4];     /* end of structure padding to ensure that memclr(ptr, sizeof(struct cpe_slot_dl_z_tcb_t)/2)
                            will clear all useful data; this is needed because the size passed to memclr call is in
                            uint16_t units and after adding new members in the future structure size will not be a
                            multiple of two /
};
*/

/*
//! @brief  RX Symbol Power Debug Struct
struct rx_sym_pwr_dbg {
    uint32_t sym;
    uint32_t sum_iq;        // from AXIQ
    uint32_t avg_iq;        // sum_iq/num_iq, positive real half fixed
    float32_t pwr_fd_lin;   // linear I^2+Q^2 RMS power of 8192 FFT output
};
*/

/*
//! @brief      DL-Z processing initialization.
//void cpe_dl_z_init(struct cpe_l1c_dl_slot_config_t const* slotconfig);

//! @brief      DL-Z processing entry point.
//! @return     This function never returns.
void cpe_dl_z_entry(void) __noreturn;

//! @brief      DL-Z processing exit tasks
void cpe_dl_z_finish(void);

//! @brief      DL-Z pwr meas. config check/init
uint32_t pwr_meas_process_init(void);
*/
void celltracking_ssb( );

void process_pss( 	uint32_t cc, uint32_t rx_path ,
							struct pss_sss_scratch_b0_t *scratch_b0,
							struct pss_sss_scratch_b1_t *scratch_b1,
							struct pss_sss_scratch_b2_t *scratch_b2);

void process_sss( 	uint32_t cc, uint32_t rx_path ,
							struct pss_sss_scratch_b0_t *scratch_b0,
							struct pss_sss_scratch_b1_t *scratch_b1,
							struct pss_sss_scratch_b2_t *scratch_b2);
#endif
