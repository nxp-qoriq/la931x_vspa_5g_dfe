// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __VECTOR_H__
#define __VECTOR_H__


//! @brief      vector addition with scalar: y = x + alpha.
//! @param[Out] py      Pointer to y.                   Vector-aligned.     In real 16-bit half-precision fixed-pt.
//! @param[in]  px      Pointer to x.                   Vector-aligned.     In real 16-bit half-precision fixed-pt.
//! @param[in]  palpha  Pointer to the scalar alpha.    Half-word-aligned.  In real 16-bit half-precision fixed-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @attention  This function may operate in-place.
//! @CycleCount Assembly version:   18 + 2*L cycles.
//! @PMEMsize   See "PMEMsize" of "vAddSclr" family.
extern void rhf_rhf_rhf_vAddSclr_asm( __fx16* py, __fx16* px, __fx16* px2, size_t L );
//! @brief      vector addition with scalar: y = x + alpha.
//! @param[Out] py      Pointer to y.                   Vector-aligned.     In real 16-bit half-precision floating-pt.
//! @param[in]  px      Pointer to x.                   Vector-aligned.     In real 16-bit half-precision floating-pt.
//! @param[in]  palpha  Pointer to the scalar alpha.    Half-word-aligned.  In real 16-bit half-precision floating-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @attention  This function may operate in-place.
//! @CycleCount Assembly version:   18 + 2*L cycles.
//! @PMEMsize   See "PMEMsize" of "vAddSclr" family.
extern void rhp_rhp_rhp_vAddSclr_asm( __fp16* py, __fp16* px, __fp16* px2, size_t L );
//! @brief      vector addition with scalar: y = x + alpha.
//! @param[Out] py      Pointer to y.                   Vector-aligned.     In real 32-bit single-precision floating-pt.
//! @param[in]  px      Pointer to x.                   Vector-aligned.     In real 32-bit single-precision floating-pt.
//! @param[in]  palpha  Pointer to the scalar alpha.    Half-word-aligned.  In real 32-bit single-precision floating-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @attention  This function may operate in-place.
//! @CycleCount Assembly version:   18 + 2*L cycles.
//! @PMEMsize   See "PMEMsize" of "vAddSclr" family.
extern void rsp_rsp_rsp_vAddSclr_asm( float* py, float* px, float* px2, size_t L );


//! @brief      vector addition with scalar: y = x + alpha.
//! @param[Out] py      Pointer to y.                   Vector-aligned.     In complex 16-bit half-precision fixed-pt.
//! @param[in]  px      Pointer to x.                   Vector-aligned.     In complex 16-bit half-precision fixed-pt.
//! @param[in]  palpha  Pointer to the scalar alpha.    Half-word-aligned.  In complex 16-bit half-precision fixed-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @attention  This function may operate in-place.
//! @CycleCount Assembly version:   18 + 2*L cycles.
//! @PMEMsize   See "PMEMsize" of "vAddSclr" family.
extern void chf_chf_chf_vAddSclr_asm( cfixed16_t* py, cfixed16_t* px, cfixed16_t* px2, size_t L );
//! @brief      vector addition with scalar: y = x + alpha.
//! @param[Out] py      Pointer to y.                   Vector-aligned.     In complex 16-bit half-precision floating-pt. 
//! @param[in]  px      Pointer to x.                   Vector-aligned.     In complex 16-bit half-precision floating-pt.
//! @param[in]  palpha  Pointer to the scalar alpha.    Half-word-aligned.  In complex 16-bit half-precision floating-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @attention  This function may operate in-place.
//! @CycleCount Assembly version:   18 + 2*L cycles.
//! @PMEMsize   See "PMEMsize" of "vAddSclr" family.
extern void chp_chp_chp_vAddSclr_asm( cfloat16_t* py, cfloat16_t* px, cfloat16_t* px2, size_t L );
//! @brief      vector addition with scalar: y = x + alpha.
//! @param[Out] py      Pointer to y.                   Vector-aligned.     In complex 32-bit single-precision floating-pt. 
//! @param[in]  px      Pointer to x.                   Vector-aligned.     In complex 32-bit single-precision floating-pt.
//! @param[in]  palpha  Pointer to the scalar alpha.    Half-word-aligned.  In complex 32-bit single-precision floating-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @attention  This function may operate in-place.
//! @CycleCount Assembly version:   18 + 2*L cycles.
//! @PMEMsize   See "PMEMsize" of "vAddSclr" family.
extern void csp_csp_csp_vAddSclr_asm( vspa_complex_float32* py, vspa_complex_float32* px, vspa_complex_float32* px2, size_t L );

// ================================ y = x * alpha ============================================== //

//! @brief      vector multiplication with scalar: y = x * alpha.
//! @param[Out] py      Pointer to y.                   Vector-aligned.     In real 16-bit half-precision fixed-pt.
//! @param[in]  px      Pointer to x.                   Vector-aligned.     In real 16-bit half-precision fixed-pt.
//! @param[in]  palpha  Pointer to the scalar alpha.    Half-word-aligned.  In real 32-bit single-precision floating-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @attention  This function may operate in-place.
//! @CycleCount Assembly version:   18 + 2*L cycles.
//! @PMEMsize   See "PMEMsize" of "vMultiSclr" family.
extern void rhf_rhf_rsp_vMultiSclr_asm( __fx16* py, __fx16* px, float* px2, size_t L );
//! @brief      vector multiplication with scalar: y = x * alpha.
//! @param[Out] py      Pointer to y.                   Vector-aligned.     In real 16-bit half-precision floating-pt. 
//! @param[in]  px      Pointer to x.                   Vector-aligned.     In real 16-bit half-precision floating-pt. 
//! @param[in]  palpha  Pointer to the scalar alpha.    Half-word-aligned.  In real 32-bit single-precision floating-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @attention  This function may operate in-place.
//! @CycleCount Assembly version:   18 + 2*L cycles.
//! @PMEMsize   See "PMEMsize" of "vMultiSclr" family.
extern void rhp_rhp_rsp_vMultiSclr_asm( __fp16* py, __fp16* px, float* px2, size_t L );
//! @brief      vector multiplication with scalar: y = x * alpha.
//! @param[Out] py      Pointer to y.                   Vector-aligned.     In real 32-bit single-precision floating-pt.
//! @param[in]  px      Pointer to x.                   Vector-aligned.     In real 32-bit single-precision floating-pt.
//! @param[in]  palpha  Pointer to the scalar alpha.    Half-word-aligned.  In real 32-bit single-precision floating-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @attention  This function may operate in-place.
//! @CycleCount Assembly version:   18 + 2*L cycles.
//! @PMEMsize   See "PMEMsize" of "vMultiSclr" family.
extern void rsp_rsp_rsp_vMultiSclr_asm( float* py, float* px, float* px2, size_t L );

// ---------------------------------------------------------------------------
//! @brief      vector addition with scalar: y = x + alpha.
//!             vector multiplication with scalar: y = x * alpha.
//!             "vAddSclr" contains a family of vector addition with scalar functions for different input/output data types.
//!             "vMultiSclr" contains a family of vector addition with scalar functions for different input/output data types.
//!
//! @param[Out] py      Pointer to y.       Vector-aligned.
//! @param[in]  px      Pointer to x.       Vector-aligned.
//! @param[in]  palpha  Pointer to alpha.   Half-word-aligned.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @attention  This function may operate in-place.
//! @CycleCount Assembly version:   18 + 2*L cycles.
//! @PMEMsize   352 Bytes.  The family of functions shares a loop body that has 136 bytes. 
//!                         Each function uses its own 24 bytes to set up VALU with suitable precision and Smode.
// ---------------------------------------------------------------------------
#pragma cplusplus on
// y = x + alpha
// real
static inline void vAddSclr(__fx16* py, __fx16* px, __fx16* palpha, size_t L) { rhf_rhf_rhf_vAddSclr_asm(py, px, palpha, L); }
static inline void vAddSclr(__fp16* py, __fp16* px, __fp16* palpha, size_t L) { rhp_rhp_rhp_vAddSclr_asm(py, px, palpha, L); }
static inline void vAddSclr(float*  py, float*  px, float*  palpha, size_t L) { rsp_rsp_rsp_vAddSclr_asm(py, px, palpha, L); }
// complex
static inline void vAddSclr(cfixed16_t* py, cfixed16_t* px, cfixed16_t* palpha, size_t L) { chf_chf_chf_vAddSclr_asm(py, px, palpha, L); }
static inline void vAddSclr(cfloat16_t* py, cfloat16_t* px, cfloat16_t* palpha, size_t L) { chp_chp_chp_vAddSclr_asm(py, px, palpha, L); }
static inline void vAddSclr(vspa_complex_float32*  py, vspa_complex_float32*  px, vspa_complex_float32*  palpha, size_t L) { csp_csp_csp_vAddSclr_asm(py, px, palpha, L); }

// y = x * alpha, alpha is a real, 32-bit single-precision floating-pt scalar
// real
static inline void vMultiSclr(__fx16* py, __fx16* px, float* palpha, size_t L) { rhf_rhf_rsp_vMultiSclr_asm(py, px, palpha, L); }
static inline void vMultiSclr(__fp16* py, __fp16* px, float* palpha, size_t L) { rhp_rhp_rsp_vMultiSclr_asm(py, px, palpha, L); }
static inline void vMultiSclr(float* py,  float* px,  float* palpha, size_t L) { rsp_rsp_rsp_vMultiSclr_asm(py, px, palpha, L); }
// complex
static inline void vMultiSclr(cfixed16_t* py, cfixed16_t* px, float* palpha, size_t L) { rhf_rhf_rsp_vMultiSclr_asm((__fx16*)py, (__fx16*)px, palpha, L); }
static inline void vMultiSclr(cfloat16_t* py, cfloat16_t* px, float* palpha, size_t L) { rhp_rhp_rsp_vMultiSclr_asm((__fp16*)py, (__fp16*)px, palpha, L); }
static inline void vMultiSclr(vspa_complex_float32*  py, vspa_complex_float32*  px, float* palpha, size_t L) { rsp_rsp_rsp_vMultiSclr_asm((float*)py, (float*)px, palpha, L); }

// ... more functions can be added with different in/out data types.
#pragma cplusplus off


//! @brief      Elementwise vector multiplication: y = x1.*x2
//!
//! @param[Out] py      Pointer to y.  Vector-aligned. In complex 16-bit half-floating.
//! @param[in]  px1     Pointer to x1. Vector-aligned. In complex 16-bit half-fixed.
//! @param[in]  px2     Pointer to x2. Vector-aligned. In complex 32-bit single-precision floating-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @CycleCount Assembly version:   15+4*L cycles (VALU eff = 2L/(15+4L). 34% at L=8; 45% at L=31. 50% is upperlimit of AUeff.)
//!                                 [23 31 47] cycles at L=[2 4 8]
//! @PMEMsize   Assembly version:   152 bytes (152 = 19 x 8)
extern void chp_chf_csp_vmult_asm( cfloat16_t* py, cfixed16_t* px1, vspa_complex_float32* px2, size_t L );
extern void chp_chf_csp_vmult_nonalgn_asm( cfloat16_t* py, cfixed16_t* px1, vspa_complex_float32* px2, size_t L );

//! @brief      Elementwise vector multiplication: y = x1.*x2
//!
//! @param[Out] py      Pointer to y.  Vector-aligned. In complex 16-bit half-floating.
//! @param[in]  px1     Pointer to x1. Vector-aligned. In complex 32-bit single-precision floating-pt.
//! @param[in]  px2     Pointer to x2. Vector-aligned. In complex 32-bit single-precision floating-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//!
//! @CycleCount Assembly version:   ? cycles (VALU eff = ?/? = ?%)
//! @PMEMsize   Assembly version:   ? bytes (? = ? x 8)
extern void chp_csp_csp_vmult_asm( cfloat16_t* py, vspa_complex_float32* px1, vspa_complex_float32* px2, size_t L ); 

// ---------------------------------------------------------------------------
//! @brief          Elementwise vector multiplication: y = x1.*x2. 
//!                 搗mult�contains a family of element-wise vector multiplication functions for different input/output data types.
//!
//! @param[out]     py      Pointer to y.   Vector-aligned.
//! @param[in]      px1     Pointer to x1.  Vector-aligned.
//! @param[in]      px2     Pointer to x2.  Vector-aligned.
//! @param[in]      L       The number of input DMEM lines.
//! @return                 none.
//! @stack                  none.
//!
//! @attention      In SISO equalizer application: This function is called after FFT of SIG or DATA.
//!                     y is equalized output; 
//!                     x1 is the FFT output of an OFDM symbol; 
//!                     x2 is 1/h, which is the output of "rcpv". The "rcpv" computes 1/h where h is the channel state information estimated from L-LTF symbol.
//!
//! @CycleCount     Depends on input/output precision.
//! @PMEMsize       Depends on input/output precision.
// ---------------------------------------------------------------------------
#pragma cplusplus on
static inline void vmult(cfloat16_t* py, cfixed16_t* px1, vspa_complex_float32* px2, size_t L) { chp_chf_csp_vmult_asm(py, px1, px2, L); } 
static inline void vmult(cfloat16_t* py, vspa_complex_float32* px1,  vspa_complex_float32* px2, size_t L) { chp_csp_csp_vmult_asm(py, px1, px2, L); }
// ... more functions can be added with different in/out data types.
#pragma cplusplus off


// ---------------------------------------------------------------------------
//! @brief      Elementwise vector-reciprocal: y = 1./x
//!
//! @param[Out] py      Pointer to y.  Vector-aligned. In complex 32-bit single-precision floating-pt.
//! @param[in]  px      Pointer to x.  Vector-aligned. In complex 16-bit half-fixed.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @CycleCount Assembly version:   29+4*L cycles (VALU eff = 3L/(29+4L). 39% at L=8; 61% at L=31. 75% is upperlimit of AUeff.)
//!                                 [37 45 61] cycles at L=[2 4 8]                        
//! @PMEMsize   Assembly version:   264 bytes (264 = 33 x 8)
// ---------------------------------------------------------------------------
extern void csp_chf_rcpv_asm(vspa_complex_float32* py, cfixed16_t* px, size_t L);

// ---------------------------------------------------------------------------
//! @brief      Elementwise vector-reciprocal: y = 1./x
//!
//! @param[Out] py      Pointer to y.  Vector-aligned. In complex 32-bit single-precision floating-pt.
//! @param[in]  px      Pointer to x.  Vector-aligned. In complex 32-bit single-precision floating-pt.
//! @param[in]  L       The number of input DMEM lines.
//! @return     none
//! @stack      none
//! @CycleCount Assembly version:   ? cycles (VALU eff = ?/? = ?%)
//! @PMEMsize   Assembly version:   ? bytes (? = ? x 8)
// ---------------------------------------------------------------------------
extern void csp_csp_rcpv_asm(vspa_complex_float32* py, vspa_complex_float32* px, size_t L); 

// ---------------------------------------------------------------------------
//! @brief          Elementwise vector-reciprocal: y = 1./x
//!                 "rcpv" contains a family of elementwise vector-reciprocal functions for different input/output data types. 
//!
//! @param[out]     py      Pointer to y.  Vector-aligned.
//! @param[in]      px      Pointer to x.  Vector-aligned.
//! @param[in]      L       The number of input DMEM lines.
//! @return                 none.
//! @stack                  none.
//!
//! @attention      In SISO equalizer application: This function is called after channel estimation.
//!                     y is hinv, which will be used by "vmult" to compute hinv .* FFT(DATA) for each DATA symbol; 
//!                     x is h, the channel estimate;
//!
//! @CycleCount     Depends on input/output precision.
//! @PMEMsize       Depends on input/output precision.
// ---------------------------------------------------------------------------
#pragma cplusplus on
static inline void rcpv(vspa_complex_float32* py, cfixed16_t* px, size_t L) { csp_chf_rcpv_asm(py, px, L); }
static inline void rcpv(vspa_complex_float32* py,  vspa_complex_float32* px, size_t L) { csp_csp_rcpv_asm(py, px, L); }
// ... more functions can be added with different in/out data types.
#pragma cplusplus off

// ---------------------------------------------------------------------------
//! @brief        This kernel implements a dot product between 2 complex half-fixed buffers: sum(conj(inp1) .* inp2). 
//!               Both buffers are allocated and managed inside the same circular buffer and are NOT required to be vector aligned.
//!               Dot product is performed using a multiple of lines (dot product window is a multiple of 32 samples).
//!
//! @param[in]    inp1_p         Pointer for 1st input buffer (16-bit complex half-fixed, non vector aligned).
//! @param[in]    inp2_p         Pointer for 2nd input buffer (16-bit complex half-fixed, non vector aligned). 
//! @param[out]   dot_prod_p     Pointer for output dot product (32-bit complex single-precision, non vector aligned). 
//! @param[in]    inp_circ_p     Input circular buffer start address (16-bit complex half-fixed, vector aligned).
//! @param[in]    inp_circ_size  Input circular buffer size (in half-words).
//! @return       void
//!
//! @attention    The 1st input buffer is complex conjugated in the dot product.
//! @attention    Both input buffers are allocated and managed within the same 
//!               circular buffer and  are NOT required to be vector aligned.
//! @attention    For the proper management of the input circular buffer:
//!               - the circular buffer start address must be vector aligned
//!               - the circular buffer size must be a multiple of lines
//!
//! @note         This kernel does NOT use scratch or persistent memory.
// ---------------------------------------------------------------------------                
void dot_prod_circ_x32chf_csp_asm( cfixed16_t* inp1_p,            
                                   cfixed16_t* inp2_p,
                                   vspa_complex_float32*  dot_prod_p,
                                   cfixed16_t* inp_circ_p,
                                   size_t                 inp_circ_size,
                                   uint32_t               num_lines);

// ---------------------------------------------------------------------------
//! @brief        This kernel implements a dot product between 2 complex half-fixed buffers: sum(conj(inp1) .* inp2). 
//!               Both buffers are linearly allocated and MUST be vector aligned.
//!               Dot product is performed using a multiple of lines (dot product window is a multiple of 32 samples).
//!
//! @param[in]    inp1_p         Pointer for 1st input buffer (16-bit complex half-fixed, vector aligned).
//! @param[in]    inp2_p         Pointer for 2nd input buffer (16-bit complex half-fixed, vector aligned). 
//! @param[out]   dot_prod_p     Pointer for output dot product (32-bit complex single-precision, non vector aligned). 
//! @param[in]    num_lines      Number of input lines.
//! @return       void
//!
//! @attention    The 1st input buffer is complex conjugated in the dot product.
//! @attention    Both input buffers are linearly allocated and MUST be vector aligned.
//!
//! @note         This kernel does NOT use scratch or persistent memory.
// ---------------------------------------------------------------------------
void dot_prod_line_x32chf_csp_asm( cfixed16_t* inp1_p,            
                                   cfixed16_t* inp2_p,
                                   vspa_complex_float32*  dot_prod_p,
                                   uint32_t               num_lines);

#pragma cplusplus on
static inline void dot_prod_circ_asm(cfixed16_t* inp1_p, cfixed16_t* inp2_p, vspa_complex_float32* dot_prod_p, cfixed16_t* inp_circ_p, size_t inp_circ_size, uint32_t num_lines) { dot_prod_circ_x32chf_csp_asm(inp1_p, inp2_p, dot_prod_p, inp_circ_p, inp_circ_size, num_lines); }
static inline void dot_prod_line_asm(cfixed16_t* inp1_p, cfixed16_t* inp2_p, vspa_complex_float32*  dot_prod_p, uint32_t num_lines) { dot_prod_line_x32chf_csp_asm(inp1_p, inp2_p, dot_prod_p, num_lines); }
// ... more functions can be added with different in/out data types.
#pragma cplusplus off


// ================================ y = sum( abs( x ).^2 ) ============================================== //
//! @brief      Sum of squared: y = sum( abs( x ).^2 )
//!
//! @param[in]  px          Pointer to x. Non-vector-aligned. In complex 16-bit half-fixed. "x" is in circular buffer.
//! @param[in]  L           The number of input DMEM lines. L >= 1.
//! @return     y           The sum of squared. In 32-bit single-precision floating-pt.
//! @stack      none
//!
//! @CycleCount Assembly version:   36+L cycles
//! @PMEMsize   Assembly version:   224 bytes
extern float rsp_chf_sumsq_line_asm( cfixed16_t* px, size_t L ); 

// ================================ y = sum( abs( x ).^2 ) ============================================== //
//! @brief      Sum of squared: y = sum( abs( x ).^2 )
//!
//! @param[in]  px          Pointer to x. Non-vector-aligned. In complex 16-bit half-fixed. "x" is in circular buffer.
//! @param[in]  px_cbuf_beg Input circular buffer beginning address.
//! @param[in]  L           The number of input DMEM lines. L >= 1.
//! @param[in]  sz_cbuf     Input circular buffer size (in half-words).
//! @return     y           The sum of squared. In 32-bit single-precision floating-pt.
//! @stack      none
//!
//! @CycleCount Assembly version:   37+L cycles
//! @PMEMsize   Assembly version:   232 bytes
extern float rsp_chf_sumsq_circ_asm( cfixed16_t* px, cfixed16_t* px_cbuf_beg, size_t L, size_t in_circ_size );

// ---------------------------------------------------------------------------
//! @brief          Sum of squared: y = sum( abs( x ).^2 )
//!                 搗mult�contains a family of element-wise vector multiplication functions for different input/output data types.
//! @param[in]      px      Pointer to x1.  Non-vector-aligned.
//! @param[in]      L       The number of input DMEM lines. L >= 1.
//! @return         y       The sum of squared.
//! @stack                  none.
//! @attention      Application: power meter.
//!
//! @CycleCount     Depends on input/output precision.
//! @PMEMsize       232 bytes for both "rsp_chf_sumsq_line_asm" and "rsp_chf_sumsq_circ_asm".
// ---------------------------------------------------------------------------
#pragma cplusplus on
static inline float sumsq(cfixed16_t* px, size_t L) { return rsp_chf_sumsq_line_asm(px, L); }
static inline float sumsq(cfixed16_t* px, cfixed16_t* px_cbuf_beg, size_t L, size_t in_circ_size) { return rsp_chf_sumsq_circ_asm(px, px_cbuf_beg, L, in_circ_size); } 
// ... more functions can be added with different in/out data types.
#pragma cplusplus off


#endif // __VECTOR_H__


