// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef __CHEST_H__
#define __CHEST_H__


// ---------------------------------------------------------------------------
//! @defgroup        GROUP_CHEST
//!
//! The CHEST module provides the following functions:
//!      - chEst():
//!      - nVarEst():
//!      - snrEst():
//!
//! @{
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
//! @brief           channel estimation function
//!
//! @param[in]	pIn   	  	Input buffer constaining RX samples. Must be vector aligned
//! @param[out] pChEst      Output buffer containing estimated channel vector; Must be vector aligned
//! @param[in]  pPilot      Input buffer containing pilot sequence; Must be vector aligned
//! @param[in]  N           Number of lines of input/output vector
//! @return     Void.
//! @cycle      18+3N    (N=2 for 20MHz, N=4 for 40MHz and N=8 for 80MHz channel)
//!
//! This function calculates the least square channel estimation using the VHT-LTF sequences defined in
//! sec 22.3.8.3.5 of Std 802.11ac. It handles all 3 channel bandwidth cases depending on the N value.
//!
//! @attention	The input RX vector contains 64, 128 and 256 complex HFixed samples for 20, 40 and 80MHz
//!             case, respectively. The DC sample (subcarrier) is always on the 1st of the second half
//!             of the input vector. See the implementation plan for subcarrier placement details.
//! @attention  The pilot buffer has exactly the same size and subcarrier placement as the input RX vector.
//!             It contains complex HFloat samples
// ---------------------------------------------------------------------------
extern void chEst(cfixed16_t const *pIn, cfixed16_t *pChEst,
    cfloat16_t const *pPilot, size_t N);

// ---------------------------------------------------------------------------
//! @brief           channel estimation function with accumulation
//!
//! @param[in]	pIn   	  	Input buffer containing RX samples. Must be vector aligned
//! @param[out] pChEst      Output buffer containing estimated channel vector; Must be vector aligned
//! @param[in]  pPilot      Input buffer containing pilot sequence; Must be vector aligned
//! @param[in]  N           Number of lines of input/output vector
//! @return     Void.
//! @cycle      19+4N    (N=2 for 20MHz, N=4 for 40MHz and N=8 for 80MHz channel)
//!
//! This function calculates the least square channel estimation using the VHT-LTF sequences defined in
//! sec 22.3.8.3.5 of Std 802.11ac. It handles all 3 channel bandwidth cases depending on the N value.
//!	It accumulates the results on top of the current contents of the output buffer pChEst
//!
//! @attention	The input RX vector contains 64, 128 and 256 complex HFixed samples for 20, 40 and 80MHz
//!             case, respectively. The DC sample (subcarrier) is always on the 1st of the second half
//!             of the input vector. See the implementation plan for subcarrier placement details.
//! @attention  The pilot buffer has exactly the same size and subcarrier placement as the input RX vector.
//!             It contains complex HFloat samples
// ---------------------------------------------------------------------------
extern void chEstAcc(cfixed16_t const *pIn, cfixed16_t *pChEst,
    cfloat16_t const *pPilot, size_t N);

// ---------------------------------------------------------------------------
//! @brief           noise variance estimation function
//!
//! @param[in]	pSym12      ptr to LTF sym1 and sym2, stored back to back in a circular buffer
//! @param[in]	pBase		ptr to the base address of circular buffer containing Sym1 and Sym2
//! @param[out] pSum        Output buffer containing the element-wise mean of sym1 and sym2
//! @param[in]	N           size of the circular input buffer in half-words (integer multiple of 32)
//! @return     estimated value of noise variance in SP
//! @cycle      57
//!
//! This function calculates the element-wise mean of sym1 and sym2. It also computes the estimated
//! value of noise variance by sum(abs(0.5*(sym1(i)-sym2(i)))^2)/(64^2) for i=1:64.
//!
//! @attention  The input buffer is a circular buffer of N cHFixed elements (vector aligned)
//!             The base address of this circular buffer is @pBase. This buffer contains LTF sym1 and
//!             sym2, stored back to back from address @pSym12, with a total of 128 cHFixed elements.
//!             Sym1 and sym2 may not be vector aligned.
//! @attention  The output buffer labeled @pSum has 64 cHFix elements and occupy 2 lines (vector aligned)
// ---------------------------------------------------------------------------
extern float nVarEst(cfixed16_t const *pSym12, cfixed16_t const *pBase,
  cfixed16_t *pSum, size_t N);


// ---------------------------------------------------------------------------
//! @brief          snr estimation function
//!
//! @param[in]	pChEst      ptr to input vector (estimated channel), 48 cHFixed elements
//! @param[in]	nVar        scaler of noise variance
//! @return     estimated SNR value in SP
//! @cycle      44
//!
//! This function calculates estimated SNR value, by SNR = ch*ch'/(48*nVar)
//! where ch is a 1x48 vector of complex elements.
//!
//! @attention  The input buffer labeled @pChEst contains 48 cHFixed elements with 16 zeros padded to
//!             the end. It is a buffer of 2 vector-aligned DMEM lines
// ---------------------------------------------------------------------------
extern float snrEst(cfixed16_t const *pChEst, float nVar);


#endif // __CHEST_H__

//! @} GROUP_CHEST
