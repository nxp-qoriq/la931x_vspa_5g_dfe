// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __timing_estimator__
#define __timing_estimator__

#include "vspa.h"
#include "vcpu.h"
#include "atan.h"

// -----------------------------------------------------------------------------
//! @brief        Kernels that estimates the timing offset for SS channel estimates
//!	
//! @param[out]   out_p   	Pointer to output buffer in fixed point Q17 - scaled by pi			 
//! @param[in]    in_p    	Pointer to input buffer (16 half-fixed) channel estimates
//! @param[in]    scratch_p Pointer to scratch buffer (32 single-float) 2 DMEM line
//! @return       void
//!
//!	alignment	  out_p     - not aligned
//! 			  in_p		- DMEM aligned
//!				  scratch_p	- DMEM aligned
//! @cycle    	  
//! @attention    
// -----------------------------------------------------------------------------                
void tim_off_est_asm(int32_t *out_p,
					cfixed16_t *in_p,
					cfloat32_t *scratch_p);


// -----------------------------------------------------------------------------
//! @brief        Kernels that estimates the timing offset for SS channel estimates
//!					using internal single precision math
//!	
//! @param[out]   out_p   	Pointer to output buffer in fixed point Q17 - scaled by pi			 
//! @param[in]    in_p    	Pointer to input buffer (16 half-fixed) channel estimates
//! @param[in]    scratch_p Pointer to scratch buffer (32 single-float) 2 DMEM line
//! @return       void
//!
//!	alignment	  out_p     - not aligned
//! 			  in_p		- DMEM aligned
//!				  scratch_p	- DMEM aligned
//! @cycle    	  
//! @attention    
// -----------------------------------------------------------------------------  
void tim_off_est_sp_asm(int32_t *out_p,
					cfixed16_t *in_p,
					cfloat32_t *scratch_p);

//! @param[in]    ATAN2_COEFF_addr   ATAN2_COEFF buffer address 
int tim_off_est_sp_asm_rc(int32_t *null,
					cfixed16_t *in_p,
					cfloat32_t *scratch_p,
					uint32_t  	ATAN2_COEFF_addr);
						   
#endif // __timing_estimator__
