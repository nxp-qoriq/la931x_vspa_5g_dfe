// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef DUMP_INJECT_H_
#define DUMP_INJECT_H_

#include "pair_config.h"

/***********************************************************************************************************
 * dumping state machine:
 * 
 * when host sends dumping msg, 
 * 		set DUMP_INJECT_WAIT_MASK and DUMP_INJECT_MODE_ONCE or DUMP_INJECT_MODE_FOREVER or DUMP_INJECT_MODE_COMPACT
 * at the time of starting point of 40ms(SFRAME) if DUMP_INJECT_WAIT_MASK is set, flag is right shifted DUMP_INJECT_MODE_SHIFT
 * DUMP_INJECT_WAIT_MASK is cleared, DUMP_INJECT_ONCE or DUMP_INJECT_FOREVER or DUMP_INJECT_COMPACT is set.
 * 
 **********************************************************************************************************/

#define DUMP_INJECT_STOPPED						0x0000
#define DUMP_INJECT_WAIT_MASK					0x0001
#define DUMP_DONE_MASK							0x0001
#define DUMP_INJECT_ONCE						0x0002
#define DUMP_INJECT_FOREVER						0x0004
//#define DUMP_INJECT_COMPACT						0x0008
#define DUMP_INJECT_MODE_ONCE					0x0020
#define DUMP_INJECT_MODE_FOREVER				0x0040
//#define DUMP_INJECT_MODE_COMPACT				0x0080
#define DUMP_INJECT_MODE_SHIFT					4
#define DUMP_INJECT_DMA_CTRL_DUMP				(DMAC_WRC | DMA_CHAN_INJECT_ANT_DATA)
//#define DUMP_INJECT_DMA_CTRL_INJECT				(DMAC_RDC | DMAC_MBRE | DMA_CHAN_INJECT_ANT_DATA)

typedef struct dump_inject_struct_s
{
	unsigned short	dump_inject_flag;
	unsigned short	dump_done_flag;
	unsigned int	offset_in_sysperiod_pre;
	unsigned int	offset_size;				//byte size offset from 40ms boundary to start dumping
	unsigned int	dump_inject_addr;
	unsigned int	dump_inject_size;
	unsigned int	dump_inject_mode_direct;	//this is to contorl the DMA direction for dumping or injecting
	unsigned int	next_addr_to_dump;
	unsigned int	boundary_size;		        //size in byte of a sys period
	unsigned int	special_control;
} struct_dump_inject;

/* extern dpd_dump, ant_tx_dump, ant_rx_dump moved to global_var.h */

unsigned int get_dpdout_dest_addr(struct_dump_inject* dump_inject, unsigned int block_addr, unsigned int block_size, unsigned int offset_in_sframe);
void dump_inject_data_1block(struct_dump_inject* dump_inject, unsigned int block_addr, unsigned int block_size, unsigned int offset_in_sframe);

#endif /* DUMP_INJECT_H_ */
