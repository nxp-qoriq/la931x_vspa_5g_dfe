// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __MIXER_H__
#define __MIXER_H__

//! @brief      Mixer operation via elementwise vector multiplication in time domain: y = x.*m,  
//!             where x is the input signal in LINEAR buffer, m is an NCO-generated complex sequence, and y is the mixed signal in linear buffer.
//! @param[Out] py          Pointer to y.  Vector-aligned. In complex 16-bit half-fixed. In linear buffer.
//! @param[in]  px          Pointer to x.  Vector-aligned. In complex 16-bit half-fixed. In linear buffer.
//! @param[in]  PhaseIn     input NCO phase represented as an unsigned 32-bit integer
//! @param[in]  FreqIn      NCO base frequency represented as a 32-bit signed integer (in 1's complement format)
//! @param[in]  L           number of input DMEM lines
//! @return     The NCO phase ready for mixing the next batch of input samples.
//!
//! @attention  This function operates in-place (mixer output is written in-place)
//! @stack      none
//!
//! @CycleCount Assembly version:   17+2*L cycles (VALU eff = 2L/(17+2L). 48% at L=8; 78% at L=31. 100% is upperlimit of AUeff.)
//!                                 [21 25 33] cycles at L=[2 4 8]
//! @PMEMsize   Assembly version:   152 bytes (152 = 19 x 8)
extern unsigned int mixer_asm( 
   cfixed16_t *py,
   cfixed16_t *px,
   uint32_t PhaseIn,
   int FreqIn,
   size_t L
);

//! @brief      Mixer operation via elementwise vector multiplication in time domain: y = x.*m,  
//!             where x is the input signal in CIRCULAR buffer, m is an NCO-generated complex sequence, and y is the mixed signal in linear buffer.
//! @param[Out] py          Pointer to y.  Vector-aligned. In complex 16-bit half-fixed. In linear buffer.
//! @param[in]  px          Pointer to x.  In complex 16-bit half-fixed. In circular buffer.
//!                         Function "mixerc_asm":          Non-Vector-aligned.
//!                         Function "mixerc_vecalgn_asm":      Vector-aligned. 
//! @param[in]  px_cbuf_beg Beginning address of circular buffer for "x".
//! @param[in]  PhaseIn     input NCO phase represented as an unsigned 32-bit integer
//! @param[in]  FreqIn      NCO base frequency represented as a 32-bit signed integer (in 1's complement format)
//! @param[in]  L           number of input DMEM lines. L>=1.
//! @param[in]  sz_px_cbuf  Size of circular buffer for "x". The unit is the minimum addressable unit of VSPA.
//! @return     The NCO phase ready for mixing the next batch of input samples.
//!
//! @attention  This function operates in-place (mixer output is written in-place)
//! @stack      none
//!
//! @CycleCount mixerc_asm:             19+3*L cycles. (VALU eff = 2L/(19+3L). 37% at L=8; 55% at L=31.  67% is upperlimit of AUeff.). [25 31 43] cycles at L=[2 4 8].
//!             mixerc_vecalgn_asm:     19+2*L cycles. (VALU eff = 2L/(19+2L). 46% at L=8; 77% at L=31. 100% is upperlimit of AUeff.). [23 27 35] cycles at L=[2 4 8].
//! @PMEMsize   mixerc_asm:             176 bytes
//!             mixerc_vecalgn_asm:     168 bytes
extern unsigned int mixerc_asm( 
   cfixed16_t *py,
   cfixed16_t *px,
   cfixed16_t *px_cbuf_beg,   
   uint32_t PhaseIn,
   int FreqIn,
   size_t L,
   size_t sz_px_cbuf
);
extern unsigned int mixerc_vecalgn_asm( 
   cfixed16_t *py,
   cfixed16_t *px,
   cfixed16_t *px_cbuf_beg,   
   uint32_t PhaseIn,
   int FreqIn,
   size_t L,
   size_t sz_px_cbuf
);

#endif // __MIXER_H__


