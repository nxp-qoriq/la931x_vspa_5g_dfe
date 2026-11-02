// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#include "pair_config.h"
#include "fft_dif.h"
#include "bitRev.h"
#include "fft_cp.h"
#include "txWindowing.h"
#include "mem.h"
#include "global_var.h"

#if (( !BYPASS_IFFT && (NUM_1T_IN_CORE) ) || ( !BYPASS_FFT && (NUM_1R_IN_CORE) ))

#define UNIT_GAIN_RX
extern void bitRev_fft_4096_b1_b1_wrap( void );
extern void bitRev_fft_4096_b0_b0_wrap_msi( void );
extern void bitRev_fft_4096_b0_b1_wrap_msi( void );
extern void bitRev_fft_4096_b0_b0_wrap_cp_insert( void );
extern void bitRev_fft_4096_b0_b1_wrap_cp_insert( void );

#define WORD32_TO_BYTE(p)   			 	((unsigned long int)p << 2)

extern volatile uint32_t tx_bitrev_arg[MAX_NUM_IPPU_ARG];
extern volatile uint32_t rx_bitrev_arg[MAX_NUM_IPPU_ARG];


void start_ippu_ifft_bitrev(unsigned int ipOut, unsigned int vpIn, unsigned int ipOutbase, unsigned int circ_size, unsigned int vpFlag)
{
	unsigned int ippu_func;
	if(ipOut>=VCPU_SIZE_DRAM)
	{
		tx_bitrev_arg[0] = (ipOut-VCPU_SIZE_DRAM)/IPPU_ADDR_RATIO;
		tx_bitrev_arg[2] = (ipOutbase-VCPU_SIZE_DRAM)/IPPU_ADDR_RATIO;
		ippu_func = (unsigned int)bitRev_fft_4096_b0_b1_wrap_msi;
	}
	else
	{
		tx_bitrev_arg[0] = (ipOut)/IPPU_ADDR_RATIO;
		tx_bitrev_arg[2] = (ipOutbase)/IPPU_ADDR_RATIO;
		ippu_func = (unsigned int)bitRev_fft_4096_b0_b0_wrap_msi;
	}
	tx_bitrev_arg[1] = (vpIn)/IPPU_ADDR_RATIO;
	tx_bitrev_arg[3] = BYTE2UNIT(circ_size)/IPPU_ADDR_RATIO;		// i/p circular buffer size
	tx_bitrev_arg[4] = (vpFlag)/IPPU_ADDR_RATIO;
	__ip_write( IPPUARGBASEADDR, ( (uint32_t)tx_bitrev_arg - IPPU_ARG_OFFSET)/IPPU_ADDR_RATIO);
	__ip_write( IPPUCONTROL, 0xFFFFFFFF, ippu_func | (1 << 31) | (1 << 24) );   
	__nop(); //avoid issue in case the next instruction is checking IPPU status register
}
void start_ippu_tx_bitrev_cp_insert(unsigned int ipOut, unsigned int vpIn, unsigned int ipOutbase, unsigned int circ_size, unsigned int nL_cp)
{
	
#if TX_BITREV_BANK != 0
#error EEOR Bank
#endif
	
	unsigned int ippu_func;
#if TX_CIRC_BANK
	tx_bitrev_arg[0] = (ipOut-VCPU_SIZE_DRAM)/IPPU_ADDR_RATIO;
	tx_bitrev_arg[2] = (ipOutbase-VCPU_SIZE_DRAM)/IPPU_ADDR_RATIO;
	ippu_func = (unsigned int)bitRev_fft_4096_b0_b1_wrap_cp_insert;
#else
	tx_bitrev_arg[0] = (ipOut)/IPPU_ADDR_RATIO;
	tx_bitrev_arg[2] = (ipOutbase)/IPPU_ADDR_RATIO;
	ippu_func = (unsigned int)bitRev_fft_4096_b0_b0_wrap_cp_insert;
#endif
	tx_bitrev_arg[1] = (vpIn)/IPPU_ADDR_RATIO;
	tx_bitrev_arg[3] = BYTE2UNIT(circ_size)/IPPU_ADDR_RATIO;		// i/p circular buffer size
	tx_bitrev_arg[4] = nL_cp;
	__ip_write( IPPUARGBASEADDR, ( (uint32_t)tx_bitrev_arg - IPPU_ARG_OFFSET)/IPPU_ADDR_RATIO);
	__ip_write( IPPUCONTROL, 0xFFFFFFFF, ippu_func | (1 << 31) | (1 << 24) );   
	__nop(); //avoid issue in case the next instruction is checking IPPU status register
}

void fft_cp_remove( uint32_t *ippu_sym_out, 
					uint32_t *vcpu_sym_in, 
					uint32_t fft_size,	
					uint32_t cp_samples,
					uint32_t circ_base,
					uint32_t circ_size,
					cfixed16_t *vcpu_scratch_buff)
{	
	cfixed16_t *ippu_fft_out = (cfixed16_t *)ippu_sym_out;
	cfixed16_t *vcpu_acc;

	vcpu_acc = (cfixed16_t *)&vcpu_sym_in[cp_samples];
	if(vcpu_acc >= (cfixed16_t *)(circ_base + BYTE2UNIT(circ_size)))
		vcpu_acc -= (circ_size/4);

#if POINTS_FFT == 4096
	fft_DIF_4096_hfx_unit_gain(ippu_fft_out, vcpu_acc, (cfixed16_t *)circ_base, circ_size, (cfixed16_t *) vcpu_scratch_buff);//, (cfixed16_t *) vcpu_scratch_buff);
#endif

//	while (ippu_is_active()) {}
	
	rx_bitrev_arg[0] = ((uint32_t)vcpu_scratch_buff)/IPPU_ADDR_RATIO;                               // o/p address
    rx_bitrev_arg[1] = ((uint32_t)ippu_fft_out - VCPU_SIZE_DRAM)/IPPU_ADDR_RATIO;                 // i/p address
    rx_bitrev_arg[2] = ((uint32_t)vcpu_scratch_buff)/IPPU_ADDR_RATIO;
    rx_bitrev_arg[3] = BYTE2UNIT(4096*4)/IPPU_ADDR_RATIO;
#if RX_SYM_QUEUE_BANK != 1
#error ERROR banks
#endif
    
    rx_bitrev_progress_ippu[0] = BITREV_STARTED;
    rx_bitrev_arg[4] = ((unsigned int)(&rx_bitrev_progress_ippu[0])-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;
    rx_bitrev_arg[5] = ((unsigned int)(&rx_bitrev_done[0])-BYTE2UNIT(VCPU_SIZE_DRAM))/IPPU_ADDR_RATIO;

    __ip_write( IPPUARGBASEADDR, ( (uint32_t)rx_bitrev_arg - IPPU_ARG_OFFSET)/IPPU_ADDR_RATIO);
    __ip_write( IPPUCONTROL, 0xFFFFFFFF, (unsigned int) bitRev_fft_4096_b1_b0 | (1 << 31) | (1 << 24) );   // Start IPPU
	__nop(); //avoid issue in case the next instruction is checking IPPU status register

//	while (ippu_is_active())
//	{
//
//	}

}

#endif

