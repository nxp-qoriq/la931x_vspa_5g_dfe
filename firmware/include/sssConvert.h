// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef __sssConvert__
#define __sssConvert__

#include "vspa.h"
#include "vcpu.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! @brief      Reference SSS Conversion
//!
//!             This function converts a real length-127 SSS (with one 16bit 0 filler at end) into 
//!             a padded (with 1+j0's) complex float16 version for input to Channel Estimation.  
//!             It also scales up the input by 64 - the output looks like this:
//!             out[0:63]:      1+j0        (64 of these)
//!             out[64:190]:    64*(SSS in) (127 of these, with imag part = 0)
//!             out[191:255]:   1+j0        (65 of these)
//!
//!             NOTE: input size = 256B, output size = 1024B
//!
//! @param[out] py      Points to where to put the padded complex output (vector-aligned)
//! @param[in]  px      Points to packed real fixed16_t 127x1 SSS input + one 0 (vector-aligned)
//! @return     void
//! @cycles		23
///////////////////////////////////////////////////////////////////////////////////////////////////
void sssConvert(cfloat16_t *py, 
				fixed16_t  *px);

#endif // __sssConvert__
