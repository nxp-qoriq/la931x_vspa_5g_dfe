// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __ATAN__
#define __ATAN__

// -----------------------------------------------------------------------------
//! @brief      Number of polynomial coefficients used for atan2() approximation: minimum 3, maximum 10.
//! FORINT:     Change to the desired value which offer the desired approximation error. 
//!             Tale a look in atan_coeff.h for details.
// -----------------------------------------------------------------------------
#define ATAN2_NUM_COEFF    4

#if !defined( __ASSEMBLER__ )
// ---------------------------------------------------------------------------
//! @brief        This kernel performs FULL CIRCLE PHASE EXTRACTION of a complex 
//!               input buffer consisting of N x 64 complex Half Fixed values.
//!               The output phase is Half Fixed and normalized to PI.
//!               The implemented function:
//!                                   
//!                       phase(real + i * imag) = atan2(imag, real) / PI
//! 
//! @param[in]    inp_p   Pointer of input complex buffer (16-bit complex half-fixed, vector non-aligned).
//! @param[out]   out_p   Pointer of output phase buffer  (16-bit real half-fixed, vector aligned).
//! @param[in]    count   Number of iterations (N), each processing 64 input samples (2 DMEM lines) and producing 64 output phases (1 line).
//! @return       void
//!
//! @CycleCount   (17 + 53 * count) cycles when using 4 coefficients. Each extra coefficient takes 4 cycles per iteration.
//!
//! @attention    The input buffer is NOT required to be vector aligned.
//! @attention    The output buffer MUST be vector aligned.
//! @note         This kernel does NOT use scratch memory.
// ---------------------------------------------------------------------------
void atan2_x64_chf_hf_asm( cfixed16_t*  inp_p,            
                           fixed16_t*   out_p,
                           uint32_t     count);

// ---------------------------------------------------------------------------
//! @brief        This kernel performs FULL CIRCLE PHASE EXTRACTION of a complex 
//!               input buffer consisting of N x 64 complex Half Fixed values.
//!               The output phase is in Single Precision and normalized to PI.
//!               The implemented function:
//!                                   
//!                       phase(real + i * imag) = atan2(imag, real) / PI
//! 
//! @param[in]    inp_p   Pointer of input complex buffer (16-bit complex half-fixed, vector non-aligned).
//! @param[out]   out_p   Pointer of output phase buffer  (32-bit real single-precision, vector aligned).
//! @param[in]    count   Number of iterations (N), each processing 64 input samples (2 DMEM lines) and producing 64 output phases (2 lines).
//! @return       void
//!
//! @CycleCount   (18 + 53 * count) cycles when using 4 coefficients. Each extra coefficient takes 4 cycles per iteration.
//!
//! @attention    The input buffer is NOT required to be vector aligned.
//! @attention    The output buffer MUST be vector aligned.
//! @note         This kernel does NOT use scratch memory.
// ---------------------------------------------------------------------------
void atan2_x64_chf_sp_asm( cfixed16_t*  inp_p,            
                           float32_t*   out_p,
                           uint32_t     count);

// ---------------------------------------------------------------------------
//! @brief        This kernel performs FULL CIRCLE PHASE EXTRACTION of a complex 
//!               input buffer consisting of N x 64 complex Single Precision values.
//!               The output phase is Half Fixed and is normalized to PI.
//!               The implemented function:
//!                                   
//!                       phase(real + i * imag) = atan2(imag, real) / PI
//! 
//! @param[in]    inp_p   Pointer of input complex buffer (32-bit complex single-precision, vector non-aligned).
//! @param[out]   out_p   Pointer of output phase buffer  (16-bit real half-fixed, vector aligned).
//! @param[in]    count   Number of iterations (N), each processing 64 input samples (4 DMEM lines) and producing 64 output phases (1 line).
//! @return       void
//!
//! @CycleCount   (17 + 54 * count) cycles when using 4 coefficients. Each extra coefficient takes 4 cycles per iteration.
//!
//! @attention    The input buffer is NOT required to be vector aligned.
//! @attention    The output buffer MUST be vector aligned.
//! @note         This kernel does NOT use scratch memory.
// ---------------------------------------------------------------------------
void atan2_x64_csp_hf_asm( cfloat32_t*  inp_p,            
                           fixed16_t*   out_p,
                           uint32_t     count);

// ---------------------------------------------------------------------------
//! @brief        This kernel performs FULL CIRCLE PHASE EXTRACTION of a complex 
//!               input buffer consisting of N x 64 complex Single Precision values.
//!               The output phase is in Single Precision and normalized to PI.
//!               The implemented function:
//!                                   
//!                       phase(real + i * imag) = atan2(imag, real) / PI
//! 
//! @param[in]    inp_p   Pointer of input complex buffer (32-bit complex single-precision, vector non-aligned).
//! @param[out]   out_p   Pointer of output phase buffer  (32-bit real single-precision, vector aligned).
//! @param[in]    count   Number of iterations (N), each processing 64 input samples (4 DMEM lines) and producing 64 output phases (2 lines).
//! @return       void
//!
//! @CycleCount   (18 + 54 * count) cycles when using 4 coefficients. Each extra coefficient takes 4 cycles per iteration.
//!
//! @attention    The input buffer is NOT required to be vector aligned.
//! @attention    The output buffer MUST be vector aligned.
//! @note         This kernel does NOT use scratch memory.
// ---------------------------------------------------------------------------
void atan2_x64_csp_sp_asm( cfloat32_t*  inp_p,            
                           float32_t *  out_p,
                           uint32_t     count);

//! @param[in]    inp_p   Pointer of input complex buffer (32-bit complex single-precision, vector non-aligned).
//! @param[out]   out_p   Pointer of output phase buffer  (32-bit real single-precision, vector aligned).
//! @param[in]    count   Number of iterations (N), each processing 64 input samples (4 DMEM lines) and producing 64 output phases (2 lines).
//! @param[in]    ATAN2_COEFF_addr   The address of ATAN2_COEFF_BUFF.
//! @return       void
void atan2_x64_csp_sp_asm_rc( 	cfloat32_t*  inp_p,            
                           	   float32_t *  out_p,
                           	   uint32_t     count,
                           	   uint32_t  	ATAN2_COEFF_addr);


// ---------------------------------------------------------------------------
//! @brief        This kernel performs FULL CIRCLE PHASE EXTRACTION of a complex 
//!               input buffer consisting of N x 64 complex Half Precision values.
//!               The output phase is Half Precision and normalized to PI.
//!               The implemented function:
//!                                   
//!                       phase(real + i * imag) = atan2(imag, real) / PI
//! 
//! @param[in]    inp_p   Pointer of input complex buffer (16-bit complex half-precision, vector non-aligned).
//! @param[out]   out_p   Pointer of output phase buffer  (16-bit real half-precision, vector aligned).
//! @param[in]    count   Number of iterations (N), each processing 64 input samples (2 DMEM lines) and producing 64 output phases (1 line).
//! @return       void
//!
//! @CycleCount   (17 + 53 * count) cycles when using 4 coefficients. Each extra coefficient takes 4 cycles per iteration.
//!
//! @attention    The input buffer is NOT required to be vector aligned.
//! @attention    The output buffer MUST be vector aligned.
//! @note         This kernel does NOT use scratch memory.
// ---------------------------------------------------------------------------
void atan2_x64_chp_hp_asm( cfloat16_t*  inp_p,            
                           float16_t*   out_p,
                           uint32_t     count);

// ---------------------------------------------------------------------------
//! @brief        This kernel performs FULL CIRCLE PHASE EXTRACTION of a complex 
//!               input buffer consisting of N x 64 complex Half Precision values.
//!               The output phase is in Single Precision and normalized to PI.
//!               The implemented function:
//!                                   
//!                       phase(real + i * imag) = atan2(imag, real) / PI
//! 
//! @param[in]    inp_p   Pointer of input complex buffer (16-bit complex half-precision, vector non-aligned).
//! @param[out]   out_p   Pointer of output phase buffer  (32-bit real single-precision, vector aligned).
//! @param[in]    count   Number of iterations (N), each processing 64 input samples (2 DMEM lines) and producing 64 output phases (2 lines).
//! @return       void
//!
//! @CycleCount   (18 + 53 * count) cycles when using 4 coefficients. Each extra coefficient takes 4 cycles per iteration.
//!
//! @attention    The input buffer is NOT required to be vector aligned.
//! @attention    The output buffer MUST be vector aligned.
//! @note         This kernel does NOT use scratch memory.
// ---------------------------------------------------------------------------
void atan2_x64_chp_sp_asm( cfloat16_t*  inp_p,            
                           float32_t*   out_p,
                           uint32_t     count);

// ---------------------------------------------------------------------------
//! @brief        This kernel performs FULL CIRCLE PHASE EXTRACTION of a complex 
//!               input buffer consisting of N x 64 complex Single Precision values.
//!               The output phase is Half Precision and is normalized to PI.
//!               The implemented function:
//!                                   
//!                       phase(real + i * imag) = atan2(imag, real) / PI
//! 
//! @param[in]    inp_p   Pointer of input complex buffer (32-bit complex single-precision, vector non-aligned).
//! @param[out]   out_p   Pointer of output phase buffer  (16-bit real half-precision, vector aligned).
//! @param[in]    count   Number of iterations (N), each processing 64 input samples (4 DMEM lines) and producing 64 output phases (1 line).
//! @return       void
//!
//! @CycleCount   (17 + 54 * count) cycles when using 4 coefficients. Each extra coefficient takes 4 cycles per iteration.
//!
//! @attention    The input buffer is NOT required to be vector aligned.
//! @attention    The output buffer MUST be vector aligned.
//! @note         This kernel does NOT use scratch memory.
// ---------------------------------------------------------------------------
void atan2_x64_csp_hp_asm( cfloat32_t*  inp_p,            
                           float16_t*   out_p,
                           uint32_t     count);

#pragma cplusplus on
static inline void atan2_x64(cfixed16_t* inp_p, fixed16_t* out_p, uint32_t count) { atan2_x64_chf_hf_asm(inp_p, out_p, count); }
static inline void atan2_x64(cfixed16_t* inp_p, float32_t* out_p, uint32_t count) { atan2_x64_chf_sp_asm(inp_p, out_p, count); }
static inline void atan2_x64(cfloat32_t* inp_p, fixed16_t* out_p, uint32_t count) { atan2_x64_csp_hf_asm(inp_p, out_p, count); }
static inline void atan2_x64(cfloat32_t* inp_p, float32_t* out_p, uint32_t count) { atan2_x64_csp_sp_asm(inp_p, out_p, count); }
static inline void atan2_x64(cfloat16_t* inp_p, float16_t* out_p, uint32_t count) { atan2_x64_chp_hp_asm(inp_p, out_p, count); }
static inline void atan2_x64(cfloat16_t* inp_p, float32_t* out_p, uint32_t count) { atan2_x64_chp_sp_asm(inp_p, out_p, count); }
static inline void atan2_x64(cfloat32_t* inp_p, float16_t* out_p, uint32_t count) { atan2_x64_csp_hp_asm(inp_p, out_p, count); }
// ... more functions can be added with different in/out data types.
#pragma cplusplus off

#endif  // !defined( __ASSEMBLER__ )

// -----------------------------------------------------------------------------
//! @brief   Section to overwrite the ATAN2_NUM_COEFF macro for TESTING ONLY!
//! FORINT   Ignore this section and make sure is NOT activated during integration!
// -----------------------------------------------------------------------------
#ifdef TEST_ATAN2_NUM_COEFF
#undef  ATAN2_NUM_COEFF
#define ATAN2_NUM_COEFF TEST_ATAN2_NUM_COEFF
#endif

#endif // __ATAN__
