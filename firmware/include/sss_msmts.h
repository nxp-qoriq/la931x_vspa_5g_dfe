// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef __SSS_MEASURE_H__
#define __SSS_MEASURE_H__


#ifndef __ASSEMBLER__

// Q16 32bit signed integer: 1 sign bit | 15 integer bits | 16 fractional bits
typedef int INT32_Q16_T; // s iiiiiiiiiiiiiii.ffffffffffffffff


typedef struct {
    INT32_Q16_T rssi_db;
    INT32_Q16_T rsrp_db;
    INT32_Q16_T rsrq_db;
    INT32_Q16_T sinr_db;
} sss_msmt_T;

// ---------------------------------------------------------------------------
//! @brief          sss_msmts
//!
//! @param[in]       h_pss   		LS estimated 256 samples of PSS sequence
//! @param[in]       h_sss   		LS estimated 256 samples of SS sequence
//! @param[out]      pOut   		Pointer to store measures (db 16Q16 format)
//!									The order is :: RSSI,RSRP,RSRQ and SINR
//!									size should be 4 words - sss_msmt_T struct
//! @return          Void.
//! @cycle         	 260
//! @stack         	 0
//!
//! Calulates below based on LS estimated SSS sequence.
//!  RSSI,RSRP,RSRQ and SINR
// ---------------------------------------------------------------------------
extern void sss_msmts(	cfixed16_t const* h_pss,
						cfixed16_t const* h_sss,
						sss_msmt_T* pOut
						);

#endif //__ASSEMBLER__
#endif // __SSS_MEASURE_H__

