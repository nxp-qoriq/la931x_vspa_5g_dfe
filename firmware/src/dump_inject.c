// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#include "pair_config.h"
#include "dump_inject.h"
#include "global_var.h"

#if NUM_1T_IN_CORE
struct_dump_inject dpd_dump = {0,0,0,0, 0,0,0,SIZE_PER_SYSPERIOD(TX_DCS_SPS), 0};
struct_dump_inject ant_tx_dump = {0,0,0,0, 0,0,0,SIZE_PER_SYSPERIOD(TX_DCS_SPS), 0};
#endif

#if NUM_1R_IN_CORE
struct_dump_inject ant_rx_dump[MAX_NUM_1R_IN_CORE] = 
{0,0,0,0, 0,0,0,SIZE_PER_SYSPERIOD(RX_DCS_SPS), 0,
 0,0,0,0, 0,0,0,SIZE_PER_SYSPERIOD(RX_DCS_SPS), 0};
#endif

//return value: dest addr.  if return value is 0, no need to dump.
unsigned int get_dpdout_dest_addr(struct_dump_inject* dump_inject, unsigned int block_addr, unsigned int block_size, unsigned int offset_in_sframe)
{
	unsigned int dest_addr = 0;
	unsigned int dump_inject_flag = dump_inject->dump_inject_flag;
	unsigned int boundary_size = dump_inject->boundary_size;
	if(!dump_inject_flag)
		return 0;
	
	block_addr = UNIT2BYTE(block_addr);
	if(dump_inject_flag & DUMP_INJECT_WAIT_MASK)
	{
		if(dump_inject->offset_in_sysperiod_pre >= offset_in_sframe)
		{
			//this is a SFRAME wrap, dumping can start.
			dump_inject_flag >>= DUMP_INJECT_MODE_SHIFT;
		}
		else
		{
			dump_inject->offset_in_sysperiod_pre = offset_in_sframe;
			dump_inject->dump_inject_flag = dump_inject_flag;
			return 0;
		}
	}

	if(dump_inject_flag == DUMP_INJECT_ONCE)
	{
		unsigned int dump_inject_addr = dump_inject->dump_inject_addr;
		unsigned int dump_inject_size = dump_inject->dump_inject_size;
		unsigned int offset_size      = dump_inject->offset_size;
		unsigned int dump_offset_range_start = offset_size;
		unsigned int dump_offset_range_end   = dump_offset_range_start + dump_inject_size;
		unsigned int offset_block_start = offset_in_sframe;
		unsigned int offset_block_end   = offset_in_sframe + block_size;
		unsigned int offset_block_dump_start = __max(offset_block_start, dump_offset_range_start);
		unsigned int offset_block_dump_end   = __min(offset_block_end, dump_offset_range_end);
		if(offset_block_dump_start >= offset_block_dump_end)
		{
			if(dump_offset_range_start < offset_block_dump_start)
			{
				//current block is after dumping range, all required data have been dumped, stop dumping
				dump_inject_flag = DUMP_INJECT_STOPPED;
				dump_inject->offset_in_sysperiod_pre = 0;
				unsigned int last_dump_addr = dump_inject_addr + dump_inject_size - 4;
				if(dump_inject->next_addr_to_dump <= last_dump_addr)
				{
					axi_flag_msg_response[0]  = last_dump_addr; //if last addr was not written, write a flag to notify dumping done
					axi_flag_msg_response1[0] = 0;
					dmac_enable(DMAC_DI32 | DMA_CHAN_DUMP_ANT_DATA, 1, UNIT2BYTE((unsigned int)axi_flag_msg_response), UNIT2BYTE((unsigned int)axi_flag_msg_response1));
				}
			}
			else
			{
				//current block is before dumping range, do nothing but wait
			}
		}
		else
		{
			unsigned int dump_addr = dump_inject_addr + offset_block_dump_start - offset_size;
			unsigned int dump_size = offset_block_dump_end - offset_block_dump_start;
			unsigned int skip_size = offset_block_dump_start - offset_block_start;
			block_addr += skip_size;
			dest_addr = dump_addr; //dmac_enable(DMA_DMEM2AXI_CONVERT | DMA_CHAN_DUMP_ANT_DATA, dump_size, dump_addr, block_addr);
			dump_inject->next_addr_to_dump = dump_addr + dump_size;
		}
	}
	dump_inject->dump_inject_flag = dump_inject_flag;
	return dest_addr;
}

void dump_inject_data_1block(struct_dump_inject* dump_inject, unsigned int block_addr, unsigned int block_size, unsigned int offset_in_sframe)
{
	unsigned int dump_inject_flag = dump_inject->dump_inject_flag;
	unsigned int dump_inject_addr = dump_inject->dump_inject_addr;
	unsigned int next_addr_to_dump = dump_inject->next_addr_to_dump;
	unsigned int boundary_size = dump_inject->boundary_size;
	if(!dump_inject_flag)
		return;
	
//	offset_in_sframe = offset_in_sframe % boundary_size;
	block_addr = UNIT2BYTE(block_addr);
	if(dump_inject_flag & DUMP_INJECT_WAIT_MASK)
	{
		if(dump_inject->offset_in_sysperiod_pre >= offset_in_sframe)
		{
			//this is a SFRAME wrap, dumping can start.
			dump_inject_flag >>= DUMP_INJECT_MODE_SHIFT;
			next_addr_to_dump = dump_inject_addr;
		}
		else
		{
			dump_inject->offset_in_sysperiod_pre = offset_in_sframe;
			dump_inject->dump_inject_flag = dump_inject_flag;
			return;
		}
	}

	if(dump_inject_flag & (DUMP_INJECT_ONCE|DUMP_INJECT_FOREVER))
	{
		unsigned int dump_inject_size = dump_inject->dump_inject_size;
		unsigned int offset_size      = dump_inject->offset_size;
		unsigned int dump_offset_range_start = offset_size;
		unsigned int dump_offset_range_end   = dump_offset_range_start + dump_inject_size;
		unsigned int offset_block_start = offset_in_sframe;
		unsigned int offset_block_end   = offset_in_sframe + block_size;
		unsigned int offset_block_dump_start = __max(offset_block_start, dump_offset_range_start);
		unsigned int offset_block_dump_end   = __min(offset_block_end, dump_offset_range_end);
		if(offset_block_dump_start >= offset_block_dump_end)
		{
			if(dump_offset_range_start < offset_block_dump_start)
			{
				//current block is after dumping range, all required data have been dumped, stop dumping
				dump_inject_flag = DUMP_INJECT_STOPPED;
				dump_inject->dump_done_flag = DUMP_DONE_MASK;
				dump_inject->offset_in_sysperiod_pre = 0;
				unsigned int last_dump_addr = dump_inject_addr + dump_inject_size - 4;
				if(next_addr_to_dump <= last_dump_addr)
				{
					axi_flag_msg_response[0]  = last_dump_addr; //if last addr was not written, write a flag to notify dumping done
					axi_flag_msg_response1[0] = 0;
					dmac_enable(DMAC_DI32 | DMA_CHAN_DUMP_ANT_DATA, 1, UNIT2BYTE((unsigned int)axi_flag_msg_response), UNIT2BYTE((unsigned int)axi_flag_msg_response1));
				}
			}
			else
			{
				//current block is before dumping range, do nothing but wait
			}
		}
		else
		{
			unsigned int dump_addr = dump_inject_addr + offset_block_dump_start - offset_size;
			unsigned int dump_size1 = offset_block_dump_end - offset_block_dump_start;
			unsigned int dump_size2 = offset_block_end - offset_block_dump_end;
			unsigned int skip_size = offset_block_dump_start - offset_block_start;

			block_addr += skip_size;
			next_addr_to_dump = dump_addr + dump_size1;
			dmac_enable((DMAC_WRC | DMA_CHAN_INJECT_ANT_DATA), dump_size1, dump_addr, block_addr);
			if(dump_size2 || (next_addr_to_dump >= (dump_inject_addr+dump_inject_size)))
			{

				dump_inject_flag = DUMP_INJECT_STOPPED;   //last address has been dumped, stop
				dump_inject->dump_done_flag = DUMP_DONE_MASK;
				dump_inject->offset_in_sysperiod_pre = 0;
			}
		}
	}
	dump_inject->dump_inject_flag = dump_inject_flag;
	dump_inject->next_addr_to_dump = next_addr_to_dump;
}
