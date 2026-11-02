// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#include "pair_config.h"
#include "global_var.h"

#define NUM_TABLE_ENTRIES	UNIT2BYTE(AXI_BURST_SIZE)/4

unsigned int addr_table[NUM_TABLE_ENTRIES] __attribute__ ((aligned (AXI_BURST_SIZE)));	
unsigned int data_table[NUM_TABLE_ENTRIES] __attribute__ ((aligned (AXI_BURST_SIZE)));	
unsigned int addr_table_rx_block[NUM_TABLE_ENTRIES] __attribute__ ((aligned (AXI_BURST_SIZE)));	
unsigned int data_table_rx_block[NUM_TABLE_ENTRIES] __attribute__ ((aligned (AXI_BURST_SIZE)));	

void ext_word_write(unsigned int addr, unsigned int value)
{
	while(dmac_is_enabled(1<<DMA_CHAN_HIGH_PRIORITY)) {}
	addr_table[0] = addr;
	data_table[0] = value;
	dmac_enable(DMAC_DI32 | DMA_CHAN_HIGH_PRIORITY, 1, UNIT2BYTE((unsigned int)addr_table), UNIT2BYTE((unsigned int)data_table));
}
void ext_word_write_2int(unsigned int addr, unsigned int value, unsigned int addr1, unsigned int value1)
{
	while(dmac_is_enabled(1<<DMA_CHAN_HIGH_PRIORITY)) {}
	addr_table[0] = addr;
	data_table[0] = value;
	addr_table[1] = addr1;
	data_table[1] = value1;
	dmac_enable(DMAC_DI32 | DMA_CHAN_HIGH_PRIORITY, 2, UNIT2BYTE((unsigned int)addr_table), UNIT2BYTE((unsigned int)data_table));
}

void wait_for_cores(unsigned int core_id_bitmask)
{
	for(unsigned int i=0;i<8;i++)
	{
		//send ext_go message to all cores including self
		if(core_id_bitmask & (1<<i))
		{
			send_msg_ext_go(i, INTERCORE_SYNC_PATTERN);
		}
	}
	//wait until all cores have sent msg to me
	while(__ip_read(EXT_GO_STAT, core_id_bitmask) != core_id_bitmask)
	{}
	
	//check sync pattern
	for(unsigned int i=0;i<8;i++)
	{
		if(core_id_bitmask & (1<<i))
		{
			while(__ip_read(GPI(0) + i) != INTERCORE_SYNC_PATTERN) {}
		}
	}
	
	__ip_write(EXT_GO_STAT, core_id_bitmask); //clear ext go bit
	
	//wait for 100 cycles so that the slowest core can get out of the wait. otherwise the slowest core may wrongly clear
	//the ext go msg sent by other cores.
	delay_cycles(100);
}

#if (CORE_TYPE == TYPE_CORE_A || CORE_TYPE == TYPE_CORE_B) && (HIGHPHY_LOWPHY_IF_TYPE == IF_TYPE_VSPA_VSPA)

void inter_core_gpio_msg_processing()
{
	unsigned int msg_valid = __ip_read(EXT_GO_STAT, 0xFF);
	if(msg_valid)
	{
		for(int core_id=0; core_id<8; core_id++)
		{
			if(msg_valid & (1<<core_id))
			{
				unsigned int msg = __ip_read(GPI(0) + core_id);
				unsigned int msg_id = msg & IC_MSG_ID_MASK;
				if(msg_id == IC_MSG_ID_TX_SYM_BUF_CONFIG)
				{
					clr_msg_ext_go(core_id);
		            dfe_ctrl.tx_sym_buf_base = IC_MSG_GET_TX_BASE(msg);
		            dfe_ctrl.tx_num_sym_in_buf = IC_MSG_GET_NUM_SYM(msg);
		            dfe_ctrl.tx_sym_buff_size = SIZE_SYMBOL_DATA_TIME_DOMAIN_LCP;
		            send_msg_ext_go(core_id, IC_MSG_ID_TX_SYM_BUF_CONFIG_ACK|IC_MSG_SET_TR_ID(0));
				}
#if NUM_1R_IN_CORE
				else if(msg_id == IC_MSG_ID_RX_SYM_BUF_CONFIG)
				{
					clr_msg_ext_go(core_id);
		            dfe_ctrl.tx_sym_buf_base = IC_MSG_GET_RX_BASE(msg);
		            dfe_ctrl.tx_num_sym_in_buf = IC_MSG_GET_NUM_SYM(msg);
		            dfe_ctrl.tx_sym_buff_size = SIZE_SYMBOL_DATA_TIME_DOMAIN_LCP;
		            send_msg_ext_go(core_id, IC_MSG_ID_RX_SYM_BUF_CONFIG_ACK|IC_MSG_SET_TR_ID(0));
				}
#endif
			}
		}
	}
}
#endif
