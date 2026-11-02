// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __txWindowing__
#define __txWindowing__


#define WINDOW_SIZE  16 //64 //16


// -----------------------------------------------------------------------------
//! @brief        Function perform a windowing algorithm to avoid discontinuities between 2 successive symbols in time 
//!				  domain and update history buffer for the following windowing instance.
//!	
//! @param[in/out]  inp_out_p   	pointer to the current symbol samples in time domain (dmem aligned) and also  to the output buffer (function operates in place)			 
//! @param[in]    	history_p		pointer to the window length with samples from the previous symbol (dmem aligned)
//! @param[in] 		real_window_p 	pointer to the real window (dmem aligned)
//! @param[in] 		CPlen			number of samples that constitutes cyclic prefix for current symbol
//! @return         void
//! @cycle    	    18 
//! @attention    	Function is used for a window length of 2*16 samples.
// -----------------------------------------------------------------------------                
void txWindowing_w16( cfixed16_t *inp_out_p,
				  	  cfixed16_t *history_p,
				  	  float16_t  *real_window_p,
				  	  uint32_t	  CPlen);

// -----------------------------------------------------------------------------
//! @brief        Function perform a windowing algorithm to avoid discontinuities between 2 successive symbols in time 
//!				  domain and update history buffer for the following windowing instance.
//!	
//! @param[in/out]  inp_out_p   	pointer to the current symbol samples in time domain (dmem aligned) and also  to the output buffer (function operates in place)			 
//! @param[in]    	history_p		pointer to the �window length samples from the previous symbol (dmem aligned)
//! @param[in] 		real_window_p 	pointer to the real window (dmem aligned)
//! @param[in] 		CPlen			number of samples that constitutes cyclic prefix for current symbol
//! @return         void
//! @cycle    	    17 
//! @attention      Function is used for a window length of 2*32 samples.
// -----------------------------------------------------------------------------    

void txWindowing_w32( cfixed16_t *inp_out_p,
				  	  cfixed16_t *history_p,
				  	  float16_t  *real_window_p,
				  	  uint32_t	  CPlen);

// -----------------------------------------------------------------------------
//! @brief        Function perform a windowing algorithm to avoid discontinuities between 2 successive symbols in time 
//!				  domain and update history buffer for the following windowing instance.
//!	
//! @param[in/out]  inp_out_p   	pointer to the current symbol samples in time domain (dmem aligned) and also  to the output buffer (function operates in place)			 
//! @param[in]    	history_p		pointer to the �window length samples from the previous symbol (dmem aligned)
//! @param[in] 		real_window_p 	pointer to the real window (dmem aligned)
//! @param[in] 		CPlen			number of samples that constitutes cyclic prefix for current symbol
//! @return       	void
//! @cycle    	  	20 
//! @attention    	Function is used for a window length of 2*64 samples.
// -----------------------------------------------------------------------------

void txWindowing_w64( cfixed16_t *inp_out_p,
				  	  cfixed16_t *history_p,
				  	  float16_t  *real_window_p,
				  	  uint32_t	  CPlen);

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//! @brief        Function perform a windowing algorithm to avoid discontinuities between 2 successive symbols in time 
//!				  domain and update history buffer for the following windowing instance.
//!	
//! @param[in/out]  inp_out_p   	pointer to the current symbol samples in time domain (dmem aligned) and also  to the output buffer (function operates in place circular buffer)			 
//! @param[in]    	history_p		pointer to the window length with samples from the previous symbol (dmem aligned)
//! @param[in] 		real_window_p 	pointer to the real window (dmem aligned)
//! @param[in] 		CPlen			number of samples that constitutes cyclic prefix for current symbol
//! @param[in]      b   			Source base address (circular buffer).
//! @param[in]      nc  			Number of addressable units in circular buffer
//! @return         void
//! @cycle    	    18 
//! @attention    	Function is used for a window length of 2*16 samples.
// -----------------------------------------------------------------------------                
void txWindowing_circ_w16_vecaligned( cfixed16_t *inp_out_p,
				  	  cfixed16_t *history_p,
				  	  float16_t  *real_window_p,
				  	  uint32_t	  CPlen,
					  	 void *restrict b, 
					  	 size_t nc);
void txWindowing_circ_w16( cfixed16_t *inp_out_p,
				  	  cfixed16_t *history_p,
				  	  float16_t  *real_window_p,
				  	  uint32_t	  CPlen,
					  	 void *restrict b, 
					  	 size_t nc);

// -----------------------------------------------------------------------------
//! @brief        Function perform a windowing algorithm to avoid discontinuities between 2 successive symbols in time 
//!				  domain and update history buffer for the following windowing instance.
//!	
//! @param[in/out]  inp_out_p   	pointer to the current symbol samples in time domain (dmem aligned) and also  to the output buffer (function operates in place)			 
//! @param[in]    	history_p		pointer to the �window length samples from the previous symbol (dmem aligned)
//! @param[in] 		real_window_p 	pointer to the real window (dmem aligned)
//! @param[in] 		CPlen			number of samples that constitutes cyclic prefix for current symbol
//! @param[in]      b   			Source base address (circular buffer).
//! @param[in]      nc  			Number of addressable units in circular buffer
//! @return         void
//! @cycle    	    17 
//! @attention      Function is used for a window length of 2*32 samples.
// -----------------------------------------------------------------------------    

void txWindowing_circ_w32( cfixed16_t *inp_out_p,
				  	  cfixed16_t *history_p,
				  	  float16_t  *real_window_p,
				  	  uint32_t	  CPlen,
					  	 void *restrict b, 
					  	 size_t nc);

// -----------------------------------------------------------------------------
//! @brief        Function perform a windowing algorithm to avoid discontinuities between 2 successive symbols in time 
//!				  domain and update history buffer for the following windowing instance.
//!	
//! @param[in/out]  inp_out_p   	pointer to the current symbol samples in time domain (dmem aligned) and also  to the output buffer (function operates in place)			 
//! @param[in]    	history_p		pointer to the �window length samples from the previous symbol (dmem aligned)
//! @param[in] 		real_window_p 	pointer to the real window (dmem aligned)
//! @param[in] 		CPlen			number of samples that constitutes cyclic prefix for current symbol
//! @param[in]      b   			Source base address (circular buffer).
//! @param[in]      nc  			Number of addressable units in circular buffer
//! @return       	void
//! @cycle    	  	20 
//! @attention    	Function is used for a window length of 2*64 samples.
// -----------------------------------------------------------------------------

void txWindowing_circ_w64( cfixed16_t *inp_out_p,
				  	  cfixed16_t *history_p,
				  	  float16_t  *real_window_p,
				  	  uint32_t	  CPlen,
					  	 void *restrict b, 
					  	 size_t nc);
					  
						   
#endif // __txWindowing__
