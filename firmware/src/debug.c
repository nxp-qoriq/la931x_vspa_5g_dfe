// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#include "pair_config.h"
extern struct_dfe_ctrl dfe_ctrl;

#if LOG_BUFFER_BANK
unsigned int log_buffer[LOG_BUFFER_SIZE/4]__attribute__ ((section (".ibss")))__attribute__ ((aligned (LINE_SIZE)));
#else
unsigned int log_buffer[LOG_BUFFER_SIZE/4]__attribute__((section (".bss_line_aligned")))__attribute__ ((aligned (LINE_SIZE)));
#endif

void log_2int(unsigned int data1, unsigned int data2)
{
	unsigned int flag_log_stop_idx = dfe_ctrl.flag_log_stop_idx;
	unsigned int flag_log_stop = flag_log_stop_idx & LOG_STOP_MASK;
	if(flag_log_stop)
	{
		return;
	}
	flag_log_stop = ((data1 & LOG_TYPE_MASK) == LOG_TYPE_STOP);
	unsigned int log_idx = flag_log_stop_idx & LOG_IDX_MASK;
	log_buffer[log_idx++] = data1;
	log_buffer[log_idx++] = data2;
	unsigned int ext_log_buf_size = dfe_ctrl.ext_log_buf_size;
	if( ( ((log_idx == LOG_BUFFER_SIZE/4) || flag_log_stop) && ext_log_buf_size ) )
	{
		unsigned int ext_log_buf_offset = dfe_ctrl.ext_log_buf_offset;
		unsigned int ext_log_buf_addr = ext_log_buf_offset + dfe_ctrl.ext_log_buf_base;
		dmac_enable(DMAC_WR | DMA_CHAN_USED_FOR_LOG, log_idx*4, ext_log_buf_addr, UNIT2BYTE((unsigned int)log_buffer));
		ext_log_buf_offset += log_idx*4;
		if(ext_log_buf_offset >= ext_log_buf_size)
		{
			ext_log_buf_offset = ext_log_buf_size/2;
		}
		dfe_ctrl.ext_log_buf_offset = ext_log_buf_offset;
	}
	log_idx %= LOG_BUFFER_SIZE/4;
	
#if(CORE_TYPE != TYPE_CORE_A && CORE_TYPE != TYPE_CORE_B )
	if(log_idx == 0)
		log_idx = LOG_BUFFER_SIZE/4/2;
#endif
	dfe_ctrl.flag_log_stop_idx = log_idx|(flag_log_stop<<LOG_STOP_BITIDX);
}
