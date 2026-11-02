// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __MEM__
#define __MEM__

//The following APIS for VSPA3 and VSPA2

//memclr/memset/memcpy APIs
//! @param      dst   Destination address, line aligned.
//! @param      src   Source address, line aligned.
//! @param      nL    Number of vectors lines.
//! @param      circ_base   circular buffer base for src or dst.
//! @param      circ_size   circular buffer size in bytes
//! @param      num_words   number of 32-bit words
void memclr32(void *dst, unsigned int num_words);
void memclr32_circ(void *dst, unsigned int num_words, void *circ_base, unsigned int circ_size);
void veccpy(void *dst, void const *src, unsigned int nL);
void veccpy_dst_circ(void *dst, void const *src, unsigned int nL, void* circ_base, unsigned int circ_size);
void veccpy_src_circ(void *dst, void const *src, unsigned int nL, void* circ_base, unsigned int circ_size);
void veccpy_both_circ(void *dst, void const *src, unsigned int nL, void* circ_base, unsigned int circ_size);
void vecclr(void *dst, unsigned int nL);
void vecclr_circ(void *dst, unsigned int nL, void *circ_base, unsigned int circ_size);
void vecset(void *dst, unsigned int nL, unsigned int data);
void vecset_circ(void *dst, unsigned int nL, unsigned int data, void *circ_base, unsigned int circ_size);

//The following APIs for VSPA3 only
void memcpy32_circw2v_hfx2hfx(void *dst, void *src, unsigned int nL, void* circ_base, unsigned int circ_size);
//! @brief      Copy vectors to this same vector-aligned circular array, CP insert.
//! @param      dst   Destination address (circular buffer).
//!                 //Source address = dst + FFT_SIZE, and wrap automatically
void CPmemcpy32_circw2v_hfx2hfx(void *dst, unsigned int nL, void* circ_base, unsigned int circ_size);



//the following APIs for VSPA2 only


//  USE the following veccpy_dst_aligned APIs when dest address is line aligned and the copy size is multiple of lines.
//  Copy vectors in data memory, dst address must be line aligned and copy size is multiple of lines.
//			The 4 APIs proivdes different circular type: 1-no circular, 2-src circular, 3-dst circular, 4-src and dst circular
//  Parameters:
//      dst:  destination address. Line aligned
//      src:  source address. any address
//      nL:		        num of dest lines to copy
//		circ_base:		circular base
//		circ_size_byte: circular size in bytes.
void veccpy_dst_aligned_vspa2(void *dst, void *src, unsigned int nL);
void veccpy_dst_aligned_src_circ_vspa2(void *dst, void *src, unsigned int nL, void* circ_base, unsigned int circ_size_byte);
void veccpy_dst_aligned_dst_circ_vspa2(void *dst, void *src, unsigned int nL, void* circ_base, unsigned int circ_size_byte);
void veccpy_dst_aligned_both_circ_vspa2(void *dst, void *src, unsigned int nL, void* circ_base, unsigned int circ_size_byte);


//  USE the following memcpy APIs when dest address is not line aligned or the copy size is not multiple of lines, or in any case.
//  Copy data in data memory, src and dst address can be any address, size must be 256 bytes or more.
//			The 4 APIs proivdes different circular type: 1-no circular, 2-src circular, 3-dst circular, 4-src and dst circular
//  Parameters:
//      dst:  destination address. any address
//      src:  source address. any address
//      size_byte:		byte size to copy, size must be any size but no less than 256
//		circ_base:		circular base
//		circ_size_byte: circular size in bytes.
//	return value:
//		the next dst address after memcpy done
void* memcpy(void *dst, void *src, unsigned int size_byte);
void* memcpy_src_circ(void *dst, void *src, unsigned int size_byte, void* circ_base, unsigned int circ_size_byte);
void* memcpy_dst_circ(void *dst, void *src, unsigned int size_byte, void* circ_base, unsigned int circ_size_byte);
void* memcpy_both_circ_vspa2(void *dst, void *src, unsigned int size_byte, void* circ_base, unsigned int circ_size_byte);


//  USE the following memclr APIs when dest address is not line aligned or the clearing size is not multiple of lines.
//  clear data in data memory, dst address can be any address, size can be any size.
//  Applicable for both VSPA2 and VSPA3
//  Parameters:
//      dst:  			destination address. any address
//      size_byte:		byte size to clear, size can be any size including 0
//		circ_base:		circular base
//		circ_size_byte: circular size in bytes.
//  Return value:
//      return the address after the cleared data ret_addr=input_addr+size.

void* mem_clr(void *dst, unsigned int size_byte);
void* mem_clr_circ(void *dst, unsigned int size_byte, void* circ_base, unsigned int circ_size_byte);


#endif /* __QEC_OPT_H__ */
