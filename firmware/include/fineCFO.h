// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __FINECFO__
#define __FINECFO__

#include "sss_msmts.h"

//! @brief      Refine CFO by FFT256 of PSS and SSS.
//!             This function is called when both PSS and SSS indicate an SS block is present.
//!
//! @param[in]  hPSS        		Pointer to the channel estimation for PSS in frequency domain: a 256x1 vector. Vector-aligned. Linear buffer. In complex 16-bit half-fixed.
//! @param[in]  hSSS        		Pointer to the channel estimation for PSS in frequency domain: a 256x1 vector. Vector-aligned. Linear buffer. In complex 16-bit half-fixed.
//! @param[in]  cfoScratchBuff 		1 DMEM line buffer.
//! @param[in]  ATAN2_COEFF_addr 	ATAN2_COEFF_address.
//!
//! @return     fineCFO integer value nco refined.
//! @stack      1 vector line.
//! @attention  Function "atan2_x64_csp_sp_asm_rc" is called by "jsr" in the assmebly.
//!
//! @CycleCount "fineCFO_rc":        48+64 = 112 cycles
//extern int32_t fineCFO(cfixed16_t *hPSS, cfixed16_t *hSSS, cfixed16_t *cfoScratchBuff);
extern int32_t fineCFO_rc(cfixed16_t *hPSS, cfixed16_t *hSSS, cfixed16_t *cfoScratchBuff, uint32_t ATAN2_COEFF_addr);

#endif // __FINECFO__
