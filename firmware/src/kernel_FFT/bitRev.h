// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __BITREV_H__
#define __BITREV_H__


#define BANK0_2_BANK0	0
#define BANK0_2_BANK1	1
#define BANK1_2_BANK0	2
#define BANK1_2_BANK1	3


//! @brief          bitRev_fft_size_bX_bY ():
//!
//! @param[In]      Arg0                Pointer to output buffer (can be either memory bank). Vector-aligned. size: (NFFT*4) bytes
//!                                     The value of output pointer depends on the memory bank.
//!                                     Output bank0 (vcpu):  p_out = p_out /2 ;
//!                                     Output bank1 (ippu):  p_out = (p_out -ippu_start_address)/2 ;
//!                                     (ippu_start_addr = 0x20000).
//!
//! @param[In]      Arg1                Pointer to input buffer (can be either memory bank). Vector-aligned. size: (NFFT*4) bytes
//!                                     The value of output pointer depends on the memory bank.
//!                                     Output bank0 (vcpu): p_in = p_in /2 ;
//!                                     Output bank1 (ippu): p_in = (p_in -ippu_start_address)/2 ;
//!                                     (ippu_start_addr = 0x20000).


extern void bitRev_fft_128_b0_b0( void );
extern void bitRev_fft_128_b0_b1( void );

extern void bitRev_fft_128_b1_b0( void );
extern void bitRev_fft_128_b1_b1( void );

extern void bitRev_fft_256_b0_b0( void );
extern void bitRev_fft_256_b0_b1( void );
extern void bitRev_fft_256_b1_b0( void );
extern void bitRev_fft_256_b1_b1( void );

extern void bitRev_fft_512_b0_b0( void );
extern void bitRev_fft_512_b0_b1( void );
extern void bitRev_fft_512_b1_b0( void );
extern void bitRev_fft_512_b1_b1( void );

extern void bitRev_fft_1024_b0_b0( void );
extern void bitRev_fft_1024_b0_b1( void );
extern void bitRev_fft_1024_b1_b0( void );
extern void bitRev_fft_1024_b1_b1( void );

extern void bitRev_fft_2048_b0_b0( void );
extern void bitRev_fft_2048_b0_b1( void );
extern void bitRev_fft_2048_b1_b0( void );
extern void bitRev_fft_2048_b1_b1( void );

extern void bitRev_fft_4096_b0_b0( void );
extern void bitRev_fft_4096_b0_b1( void );
extern void bitRev_fft_4096_b1_b0( void );
extern void bitRev_fft_4096_b1_b1( void );


//! @brief          bitRev_fft_size_bX_bY ():
//!
//! @param[In]      Arg0                Pointer to output buffer (can be either memory bank). Vector-aligned. size: (NFFT*4) bytes
//!                                     The value of output pointer depends on the memory bank.
//!                                     Output bank0 (vcpu):  p_out = p_out /2 ;
//!                                     Output bank1 (ippu):  p_out = (p_out -ippu_start_address)/2 ;
//!                                     (ippu_start_addr = 0x20000).
//!
//! @param[In]      Arg1                Pointer to input buffer (can be either memory bank). Vector-aligned. size: (NFFT*4) bytes
//!                                     The value of output pointer depends on the memory bank.
//!                                     Output bank0 (vcpu): p_in = p_in /2 ;
//!                                     Output bank1 (ippu): p_in = (p_in -ippu_start_address)/2 ;
//!                                     (ippu_start_addr = 0x20000).
//! @param[In]      Arg2                Pointer to output circular buffer base(can be either memory bank). Vector-aligned. size: (NFFT*4) bytes
//!                                     The value of output buffer base depends on the memory bank.
//!                                     Output bank0 (vcpu):  p_out = p_out /2 ;
//!                                     Output bank1 (ippu):  p_out = (p_out -ippu_start_address)/2 ;
//!                                     (ippu_start_addr = 0x20000).
//! @param[In]      Arg3                circular buffer size in unit of 16bit
extern void bitRev_fft_4096_b0_b0_wrap( void );




//! @brief          bitRev_fft_size_single_bX_bY ():
//!
//! @param[In]      Arg0                Pointer to output buffer (can be either memory bank). Vector-aligned. size: (NFFT*8) bytes
//!                                     The value of output pointer depends on the memory bank.
//!                                     Output bank0 (vcpu):  p_out = p_out /2 ;
//!                                     Output bank1 (ippu):  p_out = (p_out -ippu_start_address)/2 ;
//!                                     (ippu_start_addr = 0x20000).
//!
//! @param[In]      Arg1                Pointer to input buffer (can be either memory bank). Vector-aligned. size: (NFFT*8) bytes
//!                                     The value of output pointer depends on the memory bank.
//!                                     Output bank0 (vcpu): p_in = p_in /2 ;
//!                                     Output bank1 (ippu): p_in = (p_in -ippu_start_address)/2 ;
//!                                     (ippu_start_addr = 0x20000).


extern void bitRev_fft_128_single_b0_b0( void );
extern void bitRev_fft_128_single_b0_b1( void );
extern void bitRev_fft_128_single_b1_b0( void );
extern void bitRev_fft_128_single_b1_b1( void );

extern void bitRev_fft_256_single_b0_b0( void );
extern void bitRev_fft_256_single_b0_b1( void );
extern void bitRev_fft_256_single_b1_b0( void );
extern void bitRev_fft_256_single_b1_b1( void );

extern void bitRev_fft_512_single_b0_b0( void );
extern void bitRev_fft_512_single_b0_b1( void );
extern void bitRev_fft_512_single_b1_b0( void );
extern void bitRev_fft_512_single_b1_b1( void );

extern void bitRev_fft_1024_single_b0_b0( void );
extern void bitRev_fft_1024_single_b0_b1( void );
extern void bitRev_fft_1024_single_b1_b0( void );
extern void bitRev_fft_1024_single_b1_b1( void );

extern void bitRev_fft_2048_single_b0_b0( void );
extern void bitRev_fft_2048_single_b0_b1( void );
extern void bitRev_fft_2048_single_b1_b0( void );
extern void bitRev_fft_2048_single_b1_b1( void );

extern void bitRev_fft_4096_single_b0_b0( void );
extern void bitRev_fft_4096_single_b0_b1( void );
extern void bitRev_fft_4096_single_b1_b0( void );
extern void bitRev_fft_4096_single_b1_b1( void );


// ---------------------------------------------------------------------------
//! @defgroup        GROUP_BITREV bit-reversal Library
//!
//! The bit-reversal Library provides the following functions:
//!    - bitRev():  performs shifting and bit-reversal on a input of various length
//!    - bitRev64()  : performs shifting and bit-reversal on a 64 sub-carriers input
//!    - bitRev256() : performs shifting and bit-reversal on a 256 sub-carriers input
//!    - bitRev256sEbyE() : performs shifting and bit-reversal on a 256 sub-carriers input
//!                         (outputs element by element)
//!    - bitRev256sEbyEippu() : performs shifting and bit-reversal on a 256 sub-carriers input,
//!                             output buffer in ippu dmem(outputs element by element)
//!    - bitRev1024sEbyE() : performs shifting and bit-reversal on a 1024 sub-carriers input
//!                         (outputs element by element)
//!
//! @{
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//! @brief        Performs shifting and bit-reversal on a 64 sub-carriers input
//!
//! @return       Void.
//! @cycle        fft64:  104
//!               fft128: 173
//!               fft256: 306
//!               fft1204:1119
//!
//! @see          bitRevInvoke()
// ---------------------------------------------------------------------------
extern void bitRev( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRev().
//!
//! @param[out]  pOut      pointer to output(linear ordered sub-carriers) buffer
//!						   in IPPU DMEM.(32 bit aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in bit-reversed order. (dmem aligned)
//! @param[in]   mask	   pointer to a mask of integer value, must be one of the
//!                        following value:
//!                        0b0001   64-point fft
//!                        0b0010   128-point fft
//!                        0b0100   256-point fft
//!                        0b1000   1024-point fft
//!                        NOTE: THIS MASK IS A 32-bit WORD, WHICH MUST BE STORED
//!                        AT THE BEGINING OF A DMEM LINE.
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRev()
//!
//! This function invokes the IPPU to execute the procedure bitRev().
//!
// ---------------------------------------------------------------------------
extern bool bitRevInvoke(
	vspa_complex_float16 *pOut,
	vspa_complex_float16 const *pIn,
	unsigned int         *mask
    );

// ---------------------------------------------------------------------------
//! @brief        Performs shifting and bit-reversal on a sub-carriers input
//!
//! @return       Void.
//! @cycle        91.
//!
//! @see          bitRev64Invoke()
// ---------------------------------------------------------------------------
extern void bitRev64( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRev64().
//!
//! @param[in]   pOut      pointer to output(linear ordered sub-carriers) buffer
//!						   in VCPU.(dmem aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in bit-reversed order. (32 bit aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRev64()
//!
//! This function invokes the IPPU to execute the procedure bitRev64().
//!
// ---------------------------------------------------------------------------
extern bool bitRev64Invoke(
	vspa_complex_float16 *pOut,
	vspa_complex_float16 const *pIn
    );
// ---------------------------------------------------------------------------
//! @brief        Performs shifting and bit-reversal on a 64 sub-carriers input
//!
//! @return       Void.
//! @cycle
//!
//! @see          bitRev128Invoke()
// ---------------------------------------------------------------------------
extern void bitRev128( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRev128().
//!
//! @param[in]   pOut      pointer to output(linear ordered sub-carriers) buffer
//!						   in VCPU.(dmem aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in bit-reversed order. (32 bit aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRev128()
//!
//! This function invokes the IPPU to execute the procedure bitRev64().
//!
// ---------------------------------------------------------------------------
extern bool bitRev128Invoke(
	vspa_complex_float16 *pOut,
	vspa_complex_float16 const *pIn
    );

// ---------------------------------------------------------------------------
//! @brief        Performs shifting and bit-reversal on a 256 sub-carriers input
//!
//! @return       Void.
//! @cycle        308.
//!
//! @see          bitRev256Invoke()
// ---------------------------------------------------------------------------
extern void bitRev256( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRev256().
//!
//! @param[in]   pOut      pointer to output(linear ordered sub-carriers) buffer
//!                        in VCPU.(dmem aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in bit-reversed order. (32 bit aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRev256()
//!
//! This function invokes the IPPU to execute the procedure bitRev256().
//!
// ---------------------------------------------------------------------------
extern bool bitRev256Invoke(
	vspa_complex_float16 *pOut,
	vspa_complex_float16 const *pIn
    );

// ---------------------------------------------------------------------------
extern void bitRevMirr1024_vcpu(
		cfixed16_t const *pIn,
		cfixed16_t *pOut

    );

// ---------------------------------------------------------------------------
//! @brief        VCPU function for bit reversal for 1024 fft in addition to image mirrored around DC
//!
//! @param[in]    pIn     pointer to the input buffer in VCPU dmem containing
//!                       sub-carriers in bit-reversed order. (1024 word aligned)
//! @param[out]   pData   pointer to output buffer containing sub-carriers in linear order but image reversed around DC (dmem aligned)
//!
//! @return       Void.
//! @cycle        .
//!
//!
// ---------------------------------------------------------------------------
extern void bitRevMirr2048_vcpu(
		cfixed16_t const *pIn,
		cfixed16_t *pOut

    );
// ---------------------------------------------------------------------------
//! @brief        VCPU function for bit reversal for 2048 fft in addition to image mirrored around DC
//!
//! @param[in]    pIn     pointer to the input buffer in VCPU dmem containing
//!                       sub-carriers in bit-reversed order. (1024 word aligned)
//! @param[out]   pData   pointer to output buffer containing sub-carriers in linear order but image reversed around DC (dmem aligned)
//!
//! @return       Void.
//! @cycle        .
//!
//!
// ---------------------------------------------------------------------------
//! @brief        Performs shifting and bit-reversal on a 1024 sub-carriers input
//!
//! @return       Void.
//! @cycle        .
//!
//! @see          bitRev1024sEbyEIPPUInvoke()
// ---------------------------------------------------------------------------
extern void bitRev1024sEbyEIPPU( void );
// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRev1024sEbyEIPPU().
//!
//! @param[in]   pOut      pointer to output(linear ordered sub-carriers) buffer
//!                        in IPPU.(32-bit aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in bit-reversed order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRev1024sEbyEIPPU()
//!
//! This function invokes the IPPU to execute the procedure bitRev1024sEbyEIPPU().
//!
// ---------------------------------------------------------------------------
extern bool bitRev1024sEbyEIPPUInvoke(
    cfixed16_t *pIPPUbitRevOut,
    cfixed16_t const *pIn
    );
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//! @brief        Performs shifting and bit-reversal on a 2048 sub-carriers input
//!
//! @return       Void.
//! @cycle        .
//!
//! @see          bitRev2048sEbyEIPPUInvoke()
// ---------------------------------------------------------------------------
extern void bitRev2048sEbyEIPPU( void );
// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRev2048sEbyEIPPU().
//!
//! @param[in]   pOut      pointer to output(linear ordered sub-carriers) buffer
//!                        in IPPU.(32-bit aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in bit-reversed order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRev2048sEbyEIPPU()
//!
//! This function invokes the IPPU to execute the procedure bitRev2048sEbyEIPPU().
//!
// ---------------------------------------------------------------------------
extern bool bitRev2048sEbyEIPPUInvoke(
    cfixed16_t *pIPPUbitRevOut,
    cfixed16_t const *pIn
    );
// ---------------------------------------------------------------------------
//! @brief        Performs shifting and bit-reversal on a 256 sub-carriers input
//!
//! @return       Void.
//! @cycle        315.
//!
//! @see          bitRev256sEbyEInvoke()
// ---------------------------------------------------------------------------
extern void bitRev256sEbyE( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRev256sEbyE().
//!
//! @param[in]   pOut      pointer to output(linear ordered sub-carriers) buffer
//!                        in VCPU.(32-bit aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in bit-reversed order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRev256sEbyE()
//!
//! This function invokes the IPPU to execute the procedure bitRev256sEbyE().
//!
// ---------------------------------------------------------------------------
extern bool bitRev256sEbyEInvoke(
    vspa_complex_float16 *pOut,
    vspa_complex_float16 const *pIn
    );

// ---------------------------------------------------------------------------
//! @brief        Performs shifting and bit-reversal on a 1024 sub-carriers input
//!
//! @return       Void.
//! @cycle        1108.
//!
//! @see          bitRev1024sEbyEInvoke()
// ---------------------------------------------------------------------------
extern void bitRev1024sEbyE( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRev1024sEbyE().
//!
//! @param[in]   pOut      pointer to output(linear ordered sub-carriers) buffer
//!                        in VCPU.(32-bit aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in bit-reversed order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRev1024sEbyE()
//!
//! This function invokes the IPPU to execute the procedure bitRev1024sEbyE().
//!
// ---------------------------------------------------------------------------
extern bool bitRev1024sEbyEInvoke(
    vspa_complex_float16 *pOut,
    vspa_complex_float16 const *pIn
    );

//! @brief        VCPU proxy for bitRevTX11ac_sippu().
//!
//! @param[in]   pOut      pointer to output(bit-reverse ordered sub-carriers) buffer
//!                        in VCPU.(32 bit aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing

//! @param[in]   mask	   pointer to a mask of integer value, must be one of the
//!                        following value:
//!                        0b0001   20MHz
//!                        0b0010   40MHz
//!                        0b0100   80MHz
//!                        NOTE: THIS MASK IS A 32-bit WORD, WHICH MUST BE STORED
//!                        AT THE BEGINING OF A DMEM LINE.
//!
//!         sub-carriers in linear order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.

//! This function invokes the IPPU to execute the procedure bitRevTX11ac().
//!
// ---------------------------------------------------------------------------
extern bool bitRevTX11ac_sippuInvoke(
	vspa_complex_float16 *pOut,
	vspa_complex_float16 const *pIn,
	unsigned int         *mask
    );
// ---------------------------------------------------------------------------
//! @brief        perfroms shiftting, zero padding and bit-reversal on 64 input SCs.
//!               output buffer in IPPU
//!
//! @return       Void.
//! @cycle        .
//!
//! @see          bitRevTX11ac_sippu()
// ---------------------------------------------------------------------------
extern void bitRevTX11ac_sippu( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU function for bit reversal of 11ac 20M, 40M and 80M cases
//!
//! @param[in]    pIn     pointer to the input buffer in VCPU dmem containing
//!                       sub-carriers in linear order. (vector line aligned)
//! @param[out]   pOut    pointer to output (bit-reverse ordered sub-carriers) buffer
//!					      (aligned to buffer size of 512 words)
//! @param[in]    FLAG    Flag indicating input data BW:
//!                       0 20M
//!                       1 40M
//!                       2 80M
//!
//! @return       Void.
//! @cycle        77  20M
//!               137 40M
//!               272 80M
//!
//!
//! This function gives the bit reversal of 11ac 20M, 40M and 80M cases
//!
// ---------------------------------------------------------------------------
extern void bitRevTX11ac_vcpu(
		vspa_complex_float16 const *pIn,
		vspa_complex_float16 	   *pOut,
		unsigned int		        FLAG
    );

// ---------------------------------------------------------------------------
//! @brief        perfroms shiftting, zero padding and bit-reversal of 64 input SCs
//!
//! @return       Void.
//! @cycle        ???.
//!
//! @see          bitRevTX11ac20MInvoke()
// ---------------------------------------------------------------------------
extern void bitRevTX11ac20M( void );
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRevTX11ac20M().
//!
//! @param[in]   pOut      pointer to output(bit-reverse ordered sub-carriers) buffer
//!                        in VCPU.(32 bit aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in linear order. (dmem aligned)
//!
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRevTX11ac20M()
//!
//! This function invokes the IPPU to execute the procedure bitRevTX11ac20M().
//!
// ---------------------------------------------------------------------------
extern bool bitRevTX11ac20MInvoke(
	vspa_complex_float16 *pOut,
	vspa_complex_float16 const *pIn
    );
// ---------------------------------------------------------------------------
//! @brief        perfroms shiftting, zero padding and bit-reversal on 64 input SCs.
//!               output buffer in IPPU
//!
//! @return       Void.
//! @cycle        .
//!
//! @see          bitRevTX11ac20MsippuInvoke()
// ---------------------------------------------------------------------------
extern void bitRevTX11ac20Msippu( void );
// ---------------------------------------------------------------------------
//! @brief        perfroms shiftting, zero padding and bit-reversal of 128 input SCs
//!
//! @return       Void.
//! @cycle        ???.
//!
//! @see          bitRevTX11ac80MInvoke()
// ---------------------------------------------------------------------------
extern void bitRevTX11ac40M( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRevTX11ac40M().
//!
//! @param[in]   pOut      pointer to output(bit-reverse ordered sub-carriers) buffer
//!                        in VCPU.(32 bit aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in linear order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRevTX11ac40M()
//!
//! This function invokes the IPPU to execute the procedure bitRevTX11ac40M().
//!
// ---------------------------------------------------------------------------
extern bool bitRevTX11ac40MInvoke(
	vspa_complex_float16 *pOut,
	vspa_complex_float16 const *pIn
    );
// ---------------------------------------------------------------------------
//! @brief        perfroms shiftting, zero padding and bit-reversal on 128 input SCs.
//!               output buffer in IPPU
//!
//! @return       Void.
//! @cycle
//!
//! @see          bitRevTX11ac40MsippuInvoke()
// ---------------------------------------------------------------------------
extern void bitRevTX11ac40Msippu( void );
// ---------------------------------------------------------------------------
//! @brief        perfroms shiftting, zero padding and bit-reversal of 256 input SCs
//!
//! @return       Void.
//! @cycle        ???.
//!
//! @see          bitRevTX11ac80MInvoke()
// ---------------------------------------------------------------------------
extern void bitRevTX11ac80M( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRevTX11ac80M().
//!
//! @param[in]   pOut      pointer to output(bit-reverse ordered sub-carriers) buffer
//!                        in VCPU.(32 bit aligned)
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in linear order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRevTX11ac80M()
//!
//! This function invokes the IPPU to execute the procedure bitRevTX11ac80M().
//!
// ---------------------------------------------------------------------------
extern bool bitRevTX11ac80MInvoke(
	vspa_complex_float16 *pOut,
	vspa_complex_float16 const *pIn
    );

// ---------------------------------------------------------------------------
//! @brief        perfroms shiftting, zero padding and bit-reversal on 256 input SCs.
//!               output buffer in IPPU
//!
//! @return       Void.
//! @cycle        286.
//!
//! @see          bitRevTX11ac80MsippuInvoke()
// ---------------------------------------------------------------------------
extern void bitRevTX11ac80Msippu( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRevTX11ac80Msippu().
//!
//! @param[in]   pOut      pointer to output(bit-reverse ordered sub-carriers) buffer
//!                        in IPPU.(32 bit aligned)
//!                        pointer is an offset from base of IPPU dmem.
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in linear order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRevTX11ac80Msippu()
//!
//! This function invokes the IPPU to execute the procedure bitRevTX11ac80Msippu().
//!
// ---------------------------------------------------------------------------
extern bool bitRevTX11ac80MsippuInvoke(
    vspa_complex_float16 *pOut,
    vspa_complex_float16 const *pIn
    );
// ---------------------------------------------------------------------------
//! @brief        Performs shifting and bit-reversal on a 64 sub-carriers input,
//!               the ouput buffer is in IPPU dmem.
//!
//! @return       Void.
//! @cycle
//!
//! @see          bitRev64sEbyEippuInvoke()
// ---------------------------------------------------------------------------
extern void bitRev64sEbyEippu( void );
// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRev64sEbyEippu().
//!
//! @param[in]   pOut      pointer to output(linear ordered sub-carriers) buffer
//!                        in IPPU.(32-bit aligned)
//!                        offset from base of IPPU dmem.
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in bit-reversed order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRev64sEbyEippu()
//!
//! This function invokes the IPPU to execute the procedure bitRev64sEbyEippu().
//!
// ---------------------------------------------------------------------------
extern bool bitRev64sEbyEippuInvoke(
    vspa_complex_float16 *pOut,
    vspa_complex_float16 const *pIn
    );
// ---------------------------------------------------------------------------
//! @brief        Performs shifting and bit-reversal on a 128 sub-carriers input,
//!               the ouput buffer is in IPPU dmem.
//!
//! @return       Void.
//! @cycle
//!
//! @see          bitRev128sEbyEippuInvoke()
// ---------------------------------------------------------------------------
extern void bitRev128sEbyEippu( void );
// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRev128sEbyEippu().
//!
//! @param[in]   pOut      pointer to output(linear ordered sub-carriers) buffer
//!                        in IPPU.(32-bit aligned)
//!                        offset from base of IPPU dmem.
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in bit-reversed order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRev128sEbyEippu()
//!
//! This function invokes the IPPU to execute the procedure bitRev128sEbyEippu().
//!
// ---------------------------------------------------------------------------
extern bool bitRev128sEbyEippuInvoke(
    vspa_complex_float16 *pOut,
    vspa_complex_float16 const *pIn
    );
// ---------------------------------------------------------------------------
//! @brief        Performs shifting and bit-reversal on a 256 sub-carriers input,
//!               the ouput buffer is in IPPU dmem.
//!
//! @return       Void.
//! @cycle        315.
//!
//! @see          bitRev256sEbyEippuInvoke()
// ---------------------------------------------------------------------------
extern void bitRev256sEbyEippu( void );
// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRev256sEbyEippu().
//!
//! @param[in]   pOut      pointer to output(linear ordered sub-carriers) buffer
//!                        in IPPU.(32-bit aligned)
//!                        offset from base of IPPU dmem.
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in bit-reversed order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRev256sEbyEippu()
//!
//! This function invokes the IPPU to execute the procedure bitRev256sEbyEippu().
//!
// ---------------------------------------------------------------------------
extern bool bitRev256sEbyEippuInvoke(
    vspa_complex_float16 *pOut,
    vspa_complex_float16 const *pIn
    );

// ---------------------------------------------------------------------------
//! @brief        VCPU function for bit reversal of 11ax 80M
//!
//! @param[in]    pIn     pointer to the input buffer in VCPU dmem containing
//!                       sub-carriers in linear order. (dmem aligned)
//! @param[out]   pData   pointer to output (bit-reverse ordered sub-carriers) buffer
//!					      (aligned to buffer size of 2048 words)
//!
//! @return       Void.
//! @cycle        1085.
//!
//!
//! This function gives the bit reversal of 11ax 80M use case
//!
// ---------------------------------------------------------------------------
extern void bitRevTX11ax80Mvcpu(
		vspa_complex_float16 const *pIn,
		vspa_complex_float16 *pOut

    );

// ---------------------------------------------------------------------------
//! @brief        perfroms shiftting, zero padding and bit-reversal on 256 input SCs.
//!               output buffer in IPPU
//!
//! @return       Void.
//! @cycle        ~1024.
//!
//! @see          bitRevTX11ax80MippuInvoke()
// ---------------------------------------------------------------------------
extern void bitRevTX11ax80Mippu( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRevTX11ax80Mippu().
//!
//! @param[in]   pOut      pointer to output(bit-reverse ordered sub-carriers) buffer
//!                        in IPPU.(32 bit aligned)
//!                        pointer is an offset from base of IPPU dmem.
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in linear order. (dmem aligned)
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRevTX11ax80Mippu()
//!
//! This function invokes the IPPU to execute the procedure bitRevTX11ax80Mippu().
//!
// ---------------------------------------------------------------------------
extern bool bitRevTX11ax80MippuInvoke(
    vspa_complex_float16 *pOut,
    vspa_complex_float16 const *pIn
    );


// ---------------------------------------------------------------------------
//! @brief        perfroms shiftting, zero padding and bit-reversal on 256/1024 input SCs.
//!               output buffer in IPPU
//!
//! @return       Void.
//! @cycle        337 for 256-point fft
//!               1226 for 1024-point fft
//!
//! @see          bitRevTX11axInvoke()
// ---------------------------------------------------------------------------
extern void bitRevTX11ax( void );

// ---------------------------------------------------------------------------
//! @brief        VCPU proxy for bitRevTX11ax().
//!
//! @param[in]   pOut      pointer to output(bit-reverse ordered sub-carriers) buffer
//!                        in IPPU.(32 bit aligned)
//!                        pointer is an offset from base of IPPU dmem.
//! @param[in]   pData     pointer to the input buffer in VCPU dmem containing
//!                        sub-carriers in linear order. (dmem aligned)
//! @param[in]   mask	   pointer to a mask of integer value, must be one of the
//!                        following value:
//!                        0b0001   1024-point fft
//!                        0b0010   256-point fft
//!
//! @retval       true if the IPPU procedure is started.
//! @retval       false if the IPPU procedure could not be started.
//!
//! @see          bitRevTX11ax()
//!
//! This function invokes the IPPU to execute the procedure bitRevTX11ax().
//!
// ---------------------------------------------------------------------------
extern bool bitRevTX11axInvoke(
    vspa_complex_float16 *pOut,
    vspa_complex_float16 const *pIn,
    unsigned int         *mask
    );

#endif // __BITREV_H__

//! @} GROUP_BITREV
