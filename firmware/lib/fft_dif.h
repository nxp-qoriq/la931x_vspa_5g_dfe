// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef FFT_DIF_H_
#define FFT_DIF_H_


// ===========================================================================
//! @file            fft_dif.h
//! @brief           4096 FFT decimation in frequency algorithm
//! @copyright
//! DESCRIPTION:
//!		4096 all stages run in f24 precision. But VRA data are half fixed
//! @param[out]		pOut			output half fixed buffer 		(DMEM align)
//!	@param[in]		pIn				input half fixed buffer start 	(circular buffer start)
//!	@param[in]		pInBuffBase		input half fixed buffer Base  	(circular buffer base)
//!	@param[in]		inputBuffSize	input half fixed buffer size
//!	@param[in]		pScratch		scratch							(DMEM align)
//!
//! NOTE:
//! Due to hardware only support dual ld/st different memory bank in 1 cycle(others are 2 cycles).
//! For optimal performance input buffer, output buffer and scratch should be in different memory bank.
//! Stage count v = log2(FFT SIZE).
//! v belongs to even number
//! 	input buffer and scratch are in the same memory bank. output buffer in the other memory bank
//!		input( 0 ) - > output( 1 )->scratch( 0 )->output( 1 )-> .... -> scratch( 0 )->output( 1 )
//! v belongs to odd number
//!		input buffer and output buffer are in the same memory bank. scratch in other memory bank.
//! 	input( 0 ) - > scratch( 1 )-> output( 0 )->scratch( 1 )->output( 0 )-> ... -> scratch( 1 )->output( 1 )
//! However, it also support in-place algorithm but result in cycle penalty. It means output buffer and scratch buffer are the same.
// ===========================================================================

extern void fft_DIF_4096_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );

extern void ifft_DIF_4096_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );
extern void fft_DIF_2048_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );

extern void ifft_DIF_2048_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );
extern void fft_DIF_1024_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );

extern void ifft_DIF_1024_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );
extern void fft_DIF_512_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );

extern void ifft_DIF_512_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );

// ===========================================================================
//! @brief          256pt decimation-in-frequency FFT, f24 vector calc precision
//!
//! @param[out]     pOut            output half fixed buffer      (DMEM align, Bank "Y" optimal)
//! @param[in]      pIn             input half fixed data start   (Bank "X" optimal)
//! @param[in]      pInBuffBase     input circular buffer base
//! @param[in]      inputBuffSize   input circular buffer size
//! @param[in]      pScratch        scratch                       (DMEM align, Bank "X" optimal)
//!
//! Internal FFT stages are calculated using floating point 24bit precision (f24). In this mode VSPA
//! can take advantage of parallel load/store which takes only 1 cycle when performed from different
//! memory banks (otherwise 2 cycles = no benefit).
//!
//! 256 = 2^8 -> we have 8 FFT stages = even, so optimal performance when:
//! - input buffer =   Bank "X"
//! - scratch buffer = Bank "X"
//! - output buffer =  Bank "Y" (other bank)
//! input( 0 ) - > output( 1 )->scratch( 0 )->output( 1 )-> .... -> scratch( 0 )->output( 1 )
// ===========================================================================
extern void fft_DIF_256_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );

extern void ifft_DIF_256_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );
extern void fft_DIF_128_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );

extern void ifft_DIF_128_hfx(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );

extern void fft_DIF_4096_hfx_unit_gain(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch         //scratch is reusing input buffer.
    );

extern void ifft_DIF_4096_hfx_unit_gain(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch		//scratch is reusing input buffer.
    );

extern void ifft_DIF_4096_hfx_NoScale(
		cfixed16_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBuffBase,
		uint32_t			inputBuffSize,
		cfixed16_t*			pScratch
    );

// ===========================================================================
//! @file            fft_dif.h
//! @brief           4096 FFT decimation in frequency algorithm
//! @copyright
//! DESCRIPTION:
//!		4096 all stages run in f24 precision. But VRA data are half float
//! @param[out]		pOut			output half fixed buffer 		(DMEM align)
//!	@param[in]		pIn				input half fixed buffer start 	(circular buffer start)
//!	@param[in]		pInBuffBase		input half fixed buffer Base  	(circular buffer base)
//!	@param[in]		inputBuffSize	input half fixed buffer size
//!	@param[in]		pScratch		scratch							(DMEM align)

//! NOTE:
//! Due to hardware only support dual ld/st different memory bank in 1 cycle(others are 2 cycles).
//! For optimal performance input buffer, output buffer and scratch should be in different memory bank.
//! Stage count v = log2(FFT SIZE).
//! v belongs to even number
//! 	input buffer and scratch are in the same memory bank. output buffer in the other memory bank
//!		input( 0 ) - > output( 1 )->scratch( 0 )->output( 1 )-> .... -> scratch( 0 )->output( 1 )
//! v belongs to odd number
//!		input buffer and output buffer are in the same memory bank. scratch in other memory bank.
//! 	input( 0 ) - > scratch( 1 )-> output( 0 )->scratch( 1 )->output( 0 )-> ... -> scratch( 1 )->output( 1 )
//! However, it also support in-place algorithm but result in cycle penalty. It means output buffer and scratch buffer are the same.
// ===========================================================================

extern void fft_DIF_4096_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
extern void ifft_DIF_4096_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
extern void fft_DIF_2048_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
extern void ifft_DIF_2048_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
extern void fft_DIF_1024_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
extern void ifft_DIF_1024_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
extern void fft_DIF_512_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
extern void ifft_DIF_512_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
extern void fft_DIF_256_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
extern void ifft_DIF_256_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
extern void fft_DIF_128_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
extern void ifft_DIF_128_hflt(
		cfloat16_t*         pOut,
		cfloat16_t*         pIn,
		cfloat16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat16_t*			scratch
    );
// ===========================================================================
//! @file            fft_dif.h
//! @brief           4096 FFT decimation in frequency algorithm
//! @copyright
//! DESCRIPTION:
//!		4096 all stages run in single precision.
//! @param[out]		pOut			output half fixed buffer 		(DMEM align)
//!	@param[in]		pIn				input half fixed buffer start 	(circular buffer start)
//!	@param[in]		pInBuffBase		input half fixed buffer Base  	(circular buffer base)
//!	@param[in]		inputBuffSize	input half fixed buffer size
//!	@param[in]		pScratch		scratch							(DMEM align)

//! NOTE:
//! Due to hardware only support dual ld/st different memory bank in 1 cycle(others are 2 cycles).
//! For optimal performance input buffer, output buffer and scratch should be in different memory bank.
//! Stage count v = log2(FFT SIZE).
//! v belongs to odd number
//! 	input buffer and scratch are in the same memory bank. output buffer in the other memory bank
//!		input( 0 ) - > output( 1 )->scratch( 0 )->output( 1 )-> .... -> scratch( 0 )->output( 1 )
//! v belongs to even number
//!		input buffer and output buffer are in the same memory bank. scratch in other memory bank.
//! 	input( 0 ) - > scratch( 1 )-> output( 0 )->scratch( 1 )->output( 0 )-> ... -> scratch( 1 )->output( 1 )
//! However, it also support in-place algorithm but result in cycle penalty. It means output buffer and scratch buffer are the same.
// ===========================================================================

extern void fft_DIF_4096_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
extern void ifft_DIF_4096_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
extern void fft_DIF_2048_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
extern void ifft_DIF_2048_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
extern void fft_DIF_1024_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
extern void ifft_DIF_1024_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
extern void fft_DIF_512_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
extern void ifft_DIF_512_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
extern void fft_DIF_256_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
extern void ifft_DIF_256_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
extern void fft_DIF_128_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
extern void ifft_DIF_128_single(
		cfloat32_t*         pOut,
		cfloat32_t*         pIn,
		cfloat32_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );

// for PSS detection
// ===========================================================================
//! @file            fft_dif.h
//! @brief           FFT decimation in frequency algorithm
//! @copyright
//! DESCRIPTION:
//!		all stages run in single precision.
//! @param[out]		pOut			output single buffer 			(DMEM align)
//!	@param[in]		pIn				input half fixed buffer start 	(circular buffer start)
//!	@param[in]		pInBuffBase		input half fixed buffer Base  		(circular buffer base)
//!	@param[in]		inputBuffSize	input single buffer size
//!	@param[in]		pScratch		scratch							(DMEM align)

//! NOTE:
//! Due to hardware only support dual ld/st different memory bank in 1 cycle(others are 2 cycles).
//! For optimal performance input buffer, output buffer and scratch should be in different memory bank.
//! Stage count v = log2(FFT SIZE).
//! v belongs to odd number
//! 	input buffer and scratch are in the same memory bank. output buffer in the other memory bank
//!		input( 0 ) - > output( 1 )->scratch( 0 )->output( 1 )-> .... -> scratch( 0 )->output( 1 )
//! v belongs to even number
//!		input buffer and output buffer are in the same memory bank. scratch in other memory bank.
//! 	input( 0 ) - > scratch( 1 )-> output( 0 )->scratch( 1 )->output( 0 )-> ... -> scratch( 1 )->output( 1 )
//! However, it also support in-place algorithm but result in cycle penalty. It means output buffer and scratch buffer are the same.
// ===========================================================================
extern void fft_DIF_512_hfx_single(
		cfloat32_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );

extern void ifft_DIF_512_hfx_single(
		cfloat32_t*         pOut,
		cfixed16_t*         pIn,
		cfixed16_t*			pInBase,
		uint32_t			inputBuffSize,
		cfloat32_t*			scratch
    );
#endif /* FFT_DIF_H_ */
