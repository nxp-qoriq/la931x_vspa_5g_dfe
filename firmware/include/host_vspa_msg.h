// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef HOST_INTERF_H_
#define HOST_INTERF_H_

#include "pair_config.h"
#include "global_var.h"

#define MAX_NUM_TDD_PATTERNS					4	//total num of patterns is usually no more than 3, total num of slots in all patterns is 5 or 10 slots

typedef struct struct_tdd_pattern_s
{
	unsigned char num_dl_slots;
	unsigned char num_gp_slots_after_dl;
	unsigned char num_dl_symbols;
	unsigned char num_ul_slots;
	unsigned char num_gp_slots_after_ul;
	unsigned char num_ul_symbols;
} struct_tdd_pattern;

typedef struct struct_static_tdd_patterns_s
{
	unsigned short num_patterns;
	unsigned short num_onetime_patterns;
	struct_tdd_pattern pattern[MAX_NUM_TDD_PATTERNS];
} struct_static_tdd_patterns;


//typedef struct DFE_mode_config_struct_s
//{
//#define OPTION8_TX	(1<<0)
//#define OPTION8_RX	(1<<1)
//	unsigned char 	option8;					//1:option8
//	unsigned char 	eCPRI_compression_disable;	//1:disable
//	unsigned char 	eCPRI_compression_ratio; 	//1:9bit
//	unsigned char 	FFT_size;					//12:4096
//} struct_DFE_mode_config;

/**********************************************************************************
 *   Host side API
 ***********************************************************************************/




/**********************************************************************************
 *   VSPA-Host Antenna ID and VSPA core mapping
 ***********************************************************************************/
#define MAX_NUM_ANTENNAS	6

/**********************************************************************************
 *   VSPA-Host mailbox message general defines
 ***********************************************************************************/
#define MSG_ID_MASK								0xFF000000
#define MSG_ID_TEST_MSG							0x0A000000
#define MSG_ID_DFE_MODE_CONFIG					0x0E000000
#define MSG_ID_SYMBOL_BUFFER_STRUCT				0x10000000
#define MSG_ID_DFE_MODE_CONFIG_ACK				0x0F000000
#define MSG_ID_SYMBOL_BUFFER_STRUCT_ACK			0x11000000
#define MSG_ID_SYMBOL_BUFFER_STRUCT2			0x20000000
#define MSG_ID_SYMBOL_BUFFER_STRUCT2_ACK		0x21000000
#define MSG_ID_STATIC_SLOT_FORMAT				0x12000000
#define MSG_ID_STATIC_SLOT_FORMAT_ACK			0x13000000
#define MSG_ID_FLEXIBLE_SLOT_FORMAT_TX			0x03000000
#define MSG_ID_FLEXIBLE_SLOT_FORMAT_RX			0x04000000
#define MSG_ID_FLEXIBLE_SLOT_FORMAT				0x03000000
#define MSG_ID_TIMING_OFFSET					0x08000000
#define MSG_ID_FDD_START						0x02000000
#define MSG_ID_FDD_STOP							0x00000000
#define MSG_ID_FINE_CFO_NCO_FREQ_UPDATE_TX		0x14000000
#define MSG_ID_FINE_CFO_NCO_FREQ_UPDATE_RX		0x15000000
#define MSG_ID_STAT_REQ_TX_POWER				0x16000000
#define MSG_ID_UPDATE_PHCOM_COEFF				0x18000000
#define MSG_ID_CELLTRACK_REQUEST				0x19000000

#define MSG_TRID(x)								(((x)>>23)&1)
#define MSG_TXRX_IDX							22


#define MSG_ID_UNEXPECTED_MSG					0x40000000
#define MSG_ID_ERROR_MSG						0x42000000
#define MSG_ID_WRONG_PATTERN_CONFIG				0x43000000	//total num of slots in the pattern config is not 5/10/20, or MAX_NUM_PATTERN_ENTRIES is too small
#define MSG_ID_ERROR_REPORT_MSG					0x44000000
#define MSG_ID_STAT_REQ_TXRX_SYM				0x46000000
#define MSG_ID_STAT_REQ_TXRX_SYM_ACK			0x47000000
#define MSG_ID_STAT_REQ_DFE_CAPABILITY			0x4C000000
#define MSG_ID_STAT_REQ_DFE_CAPABILITY_ACK		0x4D000000
#define MSG_ID_STAT_REQ_ANT_CORE_MAPPING		0x4E000000
#define MSG_ID_STAT_REQ_ANT_CORE_MAPPING_ACK	0x4F000000

#define MSG_ID_STANDALONE_MEM_LATENCY			0x60000000
#define MSG_ID_STANDALONE_MEM_LATENCY_ACK		0x61000000

#define MSG_ID_DCS_ENABLE						0x6F000000

/**********************************************************************************
 *   VSPA-Host mailbox message defines for MSG_ID_TEST_MSG
 ***********************************************************************************/
//For mailbox 32MSB
#define COMMAND_INVALID									0x0
#define COMMAND_UPDATE_DPD_COEF							0x1
#define COMMAND_UPDATE_QEC_COEF							0x2
#define COMMAND_OUTPUT_SCALING							0x3
#define COMMAND_KERNELS_DISABLE							0x4
#define COMMAND_UPDATE_CFR_PARA							0x6
#define COMMAND_UPDATE_FILTER_COEF						0x7
#define COMMAND_SEND_FREQDOMAIN_SINGLE_TONE				0x0D
#define COMMAND_SEND_TX_TIMEDOMAIN_SINGLE_TONE			0x0E
#define COMMAND_SEND_RX_TIMEDOMAIN_SINGLE_TONE			0x1E
#define COMMAND_DUMP_FREQ_DOMAIN_TX_SYMBOLS				0x0F
#define COMMAND_DUMP_TIME_DOMAIN_TX_ANT_DATA			0x10
#define COMMAND_INJECT_FREQ_DOMAIN_TX_SYMBOLS			0x11
#define COMMAND_DUMP_TIME_DOMAIN_RX_ANT_DATA			0x18
#define COMMAND_DUMP_FREQ_DOMAIN_RX_SYMBOLS				0x1C
#define COMMAND_INJECT_TIME_DOMAIN_RX_ANT_DATA			0x28
#define COMMAND_SINAD_TEST								0x30
//#define COMMAND_DUMP_HANDSHAKE_TIMESTAMP				0x20
//#define COMMAND_DUMP_TX_ALLOWED_TIMESTAMP				0x21
//#define COMMAND_DUMP_EVENT_TIMESTAMP_CORE_B				0x22
//#define COMMAND_STOP_INJECT_DL_SYMBLOS					0x12
//#define COMMAND_STOP_INJECT_UL_ANT_DATA					0x29
#define SET_COMMAND(cmd)								(((cmd)&0x3F)<<16)
#define GET_COMMAND(msg32msb)							(((msg32msb)>>16)&0x3F)
#define SET_TR_ID(id)									(((id)&0x1)<<15)
#define GET_TR_ID(msg32msb)								(((msg32msb)>>15)&0x1)
#define SET_CONTROL1(id)								(((id)&0x1)<<14)
#define SET_CONTROL2(id)								(((id)&0x1)<<13)
#define SET_CONTROL3(id)								(((id)&0x1)<<12)
#define GET_CONTROL0(msg32msb)							(((msg32msb)>>22)&0x3)
#define GET_CONTROL1(msg32msb)							(((msg32msb)>>14)&0x1)
#define GET_CONTROL2(msg32msb)							(((msg32msb)>>13)&0x1)
#define GET_CONTROL3(msg32msb)							(((msg32msb)>>12)&0x1)
#define GET_CONTROL(msg32msb)							(((msg32msb)>>12)&0x7)
#define GET_CONTROL23(msg32msb)							(((msg32msb)>>12)&0x3)
#define SET_TEST_PARA_HI(phi)							((phi)&0xFFF)
#define GET_TEST_PARA_HI(msg32msb)						((msg32msb)&0xFFF)
#define SET_INJECT_SYM_BUF_SIZE(size)					(((size)/128)&0xFF)
#define GET_INJECT_SYM_BUF_SIZE(msg32msb)				(((msg32msb)&0xFF)*128)

//For mailbox 32LSB
#define SET_UPDATE_DPD_COEF_ADDR(addr)					(((addr)>>7)&0x01FFFFFF)
#define GET_UPDATE_DPD_COEF_ADDR(msg32lsb)				((msg32lsb)<<7)
#define SET_UPDATE_TXQEC_COEF_ADDR(addr)				(((addr)>>7)&0x01FFFFFF)
#define GET_UPDATE_QEC_COEF_ADDR(msg32lsb)				((msg32lsb)<<7)
#define GET_UPDATE_FILTER_COEF_ADDR(msg32lsb)			((msg32lsb)<<7)
#define SET_DUMP_ADDR(addr)								(((addr)>>12)&0xFFFFF)
#define GET_DUMP_ADDR(msg32lsb)							(((msg32lsb)&0xFFFFF)<<12)
//#define SET_SIZE_NUM32KB_TO_DUMP(size)					(((size)&0xFFF)<<20)
#define GET_SIZE_NUM32KB_TO_DUMP(msg32lsb)				(((msg32lsb)>>20)&0xFFF)
#define SET_SCALING_FACTOR(factor)						((factor)&0xFFFF)
#define GET_SCALING_FACTOR(msg32lsb)					((msg32lsb)&0xFFFF)
#define SET_INJECT_ADDR(addr)							(((addr)>>12)&0xFFFFF)
#define GET_INJECT_ADDR(msg32lsb)						(((msg32lsb)&0xFFFFF)<<12)
#define SET_INJECT_NUM_SYMBOLS(num)						(((num)&0xFFF)<<20)
#define GET_INJECT_NUM_SYMBOLS(msg32lsb)				(((msg32lsb)>>20)&0xFFF)

#define GET_START_FREQ_INDEX_SIG(msg32msb)				(((msg32msb)>>0)&0x1FFF)
#define GET_NUM_FREQ_INDEX_SIG(msg32lsb)				(((msg32lsb)>>26)&0x3F)
#define GET_NUM_FREQ_INDEX_DC(msg32lsb)					(((msg32lsb)>>20)&0x3F)
#define GET_SINAD_OUTPUT_ADDR(msg32lsb)					(((msg32lsb)&0xFFFFF)<<12)

/**********************************************************************************
 *   VSPA-Host mailbox message defines for MSG_ID_DFE_MODE_CONFIG
 ***********************************************************************************/
//For mailbox 32MSB
#define SET_OPTION8(flag)								(((flag)&1)<<8)
#define GET_OPTION8(msg32msb)							(((msg32msb)>>8)&1)
#define GET_SYM_DATA_FORMAT(msg32msb)					(((msg32msb)>>9)&1)
#define GET_FLAG_CPE(msg32msb)							(((msg32msb)>>10)&1)
#define GET_CHAN_DIS_TX(msg32msb)						(((msg32msb)>>22)&1)
#define GET_CHAN_DIS_RX(msg32msb)						(((msg32msb)>>23)&1)
#define GET_BYPASS_HANDSHAKE_TX(msg32msb)				(((msg32msb)>>14)&1)
#define GET_BYPASS_HANDSHAKE_RX(msg32msb)				(((msg32msb)>>15)&1)
#define GET_CHAN_RESTART_TX(msg32msb)					(((msg32msb)>>20)&1)
#define GET_CHAN_RESTART_RX(msg32msb)					(((msg32msb)>>21)&1)
#define GET_TX_SYM_BUF_IN_DFE(msg32msb)					(((msg32msb)>>18)&1)
#define GET_RX_SYM_BUF_IN_DFE(msg32msb)					(((msg32msb)>>19)&1)
#define GET_TX_NUM_SYM_IN_DFE(msg32msb)					((((msg32msb)>>16)&1)+1)
#define GET_RX_NUM_SYM_IN_DFE(msg32msb)					((((msg32msb)>>17)&1)+1)
#define SET_ECPRI_COMPRESSION_DISABLE(flag)				(((flag)&1)<<7)
#define GET_ECPRI_COMPRESSION_DISABLE(msg32msb)			(((msg32msb)>>7)&1)
#define SET_ECPRI_COMPRESSION_RATIO(ratio)				(((ratio)&7)<<4)
#define GET_ECPRI_COMPRESSION_RATIO(msg32msb)			(((msg32msb)>>4)&7)
#define SET_FFT_SIZE(size)								((size)&0xF)
#define GET_FFT_SIZE(msg32msb)							((msg32msb)&0xF)
#define GET_INVERSE_POLARITY_TX(msg32lsb)				(((msg32lsb)>>0)&3)
#define GET_INVERSE_POLARITY_RX(msg32lsb)				(((msg32lsb)>>2)&3)
#define GET_DFE_MODE_RX_IQSWAP(msg32lsb)				(((msg32lsb)>>23)&1)
#define GET_DFE_MODE_TX_IQSWAP(msg32lsb)				(((msg32lsb)>>19)&1)
#define GET_DFE_MODE_ANT_MAP_RX(msg32lsb)				(((msg32lsb)>>20)&0x7)
#define GET_DFE_MODE_ANT_MAP_TX(msg32lsb)				(((msg32lsb)>>16)&0x7)
#define GET_DFE_MODE_RX2_IQSWAP(msg32lsb)				(((msg32lsb)>>7)&1)
#define GET_DFE_MODE_TX2_IQSWAP(msg32lsb)				(((msg32lsb)>>3)&1)
#define GET_DFE_MODE_ANT_MAP_RX2(msg32lsb)				(((msg32lsb)>>4)&0x7)
#define GET_DFE_MODE_ANT_MAP_TX2(msg32lsb)				(((msg32lsb)>>0)&0x7)
#define GET_DFE_MODE_CFR_CTRRL(msg32lsb)				(((msg32lsb)>>8)&0xFF)
#define GET_DFE_MODE_RXEXT(msg32lsb)					(((msg32lsb)>>12)&0xF)
#define GET_DFE_MODE_TXEXT(msg32lsb)					(((msg32lsb)>>8)&0xF)

#define GET_DFE_MODE_NUM_RB(msg32lsb)					(((msg32lsb)>>24)&0xFF)
#define GET_DFE_MODE_LOWPHY_ONLY(msg32lsb)				(((msg32lsb)>>9)&1)

/**********************************************************************************
 *   VSPA-Host mailbox message defines for MSG_ID_SYMBOL_BUFFER_STRUCT
 ***********************************************************************************/
//For mailbox 32MSB
#define SET_SYMBOL_BUFFER_BASE_UL(addr32bit)			(((addr32bit)>>12)&0xFFFFF)
#define GET_SYMBOL_BUFFER_BASE_UL(msg32msb)				(((msg32msb)&0xFFFFF)<<12)
#define SET_SYMBOL_BUFFER2_BASE_UL(addr32bit)			(((addr32bit)>>7)&0xFFFFF)
#define GET_SYMBOL_BUFFER2_BASE_UL(msg32msb)			((((msg32msb)&0xFFFFF)<<7)|0xE0000000)
#define SET_BUFFER_SIZE_UL_NUM_SYMBOLS(size)			((((size)/2)&0xF)<<20)
#define GET_BUFFER_SIZE_UL_NUM_SYMBOLS(msg32msb)		((((msg32msb)>>20)&0xF)*2)
//For mailbox 32LSB
#define SET_SYMBOL_BUFFER_BASE_DL(addr32bit)			(((addr32bit)>>12)&0xFFFFF)
#define GET_SYMBOL_BUFFER_BASE_DL(msg32lsb)				(((msg32lsb)&0xFFFFF)<<12)
#define SET_SYMBOL_BUFFER2_BASE_DL(addr32bit)			(((addr32bit)>>7)&0xFFFFF)
#define ON_CHIP_MEM_START_ADDR							0xE0000000
#define GET_SYMBOL_BUFFER2_BASE_DL(msg32lsb)			((((msg32lsb)&0xFFFFF)<<7)|ON_CHIP_MEM_START_ADDR)
#define SET_BUFFER_SIZE_DL_NUM_SYMBOLS(size)			((((size)/2)&0xF)<<20)
#define GET_BUFFER_SIZE_DL_NUM_SYMBOLS(msg32lsb)		((((msg32lsb)>>20)&0xF)*2)
#define SET_SYMBOL_SIZE(size)							((((size)/128)&0xFF)<<24)
#define GET_SYMBOL_SIZE(msg32lsb)						((((msg32lsb)>>24)&0xFF)*128)

/**********************************************************************************
 *   VSPA-Host mailbox message defines for MSG_ID_STATIC_SLOT_FORMAT
 ***********************************************************************************/
//For mailbox 32MSB
#define MAX_NUM_PATTERNS_IN_ONE_MSG						2
#define GET_NUM_ONETIME_PATTERN(msg32lsb)				(((msg32lsb)>>30))		//change for special pattern
#define GET_SEQ_ID_MASK(msg32lsb)						((msg32lsb)&(3<<28))
#define GET_SEQ_ID(msg32lsb)							(((msg32lsb)>>28)&3)
#define GET_TOTAL_NUM_PATTERNS(msg32lsb)				(((msg32lsb)>>24)&0xF)
#define GET_PATTERN(msg32bit)							((msg32bit)&0xFFFFFF)	

#define GET_FLEX_SF_RX_SYMBOL_START(x)					(((x)>>20)&0xF)
#define GET_FLEX_SF_RX_NUM_SYMBOLS(x)					(((x)>>16)&0xF)
#define GET_FLEX_SF_TX_SYMBOL_START(x)					(((x)>>12)&0xF)
#define GET_FLEX_SF_TX_NUM_SYMBOLS(x)					(((x)>>8)&0xF)
#define GET_FLEX_SF_STOP(x)								(((x)>>7)&0x1)
#define GET_FLEX_SF_SLOT_INDEX(x)						(((x)>>0)&0x1F)


/**********************************************************************************
 *   VSPA-Host mailbox message defines for MSG_ID_ERROR_REPORT_MSG
 ***********************************************************************************/
#define ERROR_REPORT_MSG_ERROR_TYPE_TXAXIQ_ENABLING_ERROR	0xFF		
#define ERROR_REPORT_MSG_ERROR_TYPE_RXAXIQ_ENABLING_ERROR	0xFE		
#define ERROR_REPORT_MSG_ERROR_TYPE_TXAXIQ_INIT_STATE_ERROR	0xFD		
#define ERROR_REPORT_MSG_ERROR_TYPE_RXAXIQ_INIT_STATE_ERROR	0xFC		
#define ERROR_REPORT_MSG_ERROR_TYPE_TX_UNDERFLOW		1		
#define ERROR_REPORT_MSG_ERROR_TYPE_RX_OVERFLOW			2	
#define ERROR_REPORT_MSG_ERROR_TYPE_BOTH_UDF_OVF		3	
#define ERROR_REPORT_MSG_ERROR_TYPE_DMA_CONFIG_ERR		4	
#define ERROR_REPORT_MSG_ERROR_TYPE_DMA_TRANSFER_ERR	5	
#define ERROR_REPORT_MSG_ERROR_TYPE_TX_DEADLINE_MISS	0x11
#define ERROR_REPORT_MSG_ERROR_TYPE_RX_DEADLINE_MISS	0x12
#define ERROR_REPORT_MSG_ERROR_LATE_TX_ANT_BLOCK		0x21
#define ERROR_REPORT_MSG_ERROR_LATE_RX_ANT_BLOCK		0x22
#define ERROR_REPORT_MSG_ERROR_UNEXPECTED_TX_ALLOWED_FALLING		0x31
#define ERROR_REPORT_MSG_ERROR_UNEXPECTED_RX_ALLOWED_FALLING		0x32
#define ERROR_REPORT_MSG_ERROR_SAMPLE_VALUE_CHECK_ERROR				0x34


#define SET_ERROR_TYPE(type)							(MSG_ID_ERROR_REPORT_MSG | (((type)&0xFF)<<16))
#define SET_ERROR_TR_ID(id)								(((id)&0x1)<<15)
#define SET_ERROR_ANT_ID(id)							(((id)&0xF)<<12)
#define SET_ERROR_DATA_HI(data)							__min((data)&0xFFF, 0xFFF)

/**********************************************************************************
 *   VSPA-Host mailbox message defines for MSG_ID_STAT_QUERY
 ***********************************************************************************/
#define TIMESTAMP_TYPE_FIRST_AXIQDMA_COMPLETE			0
#define TIMESTAMP_TYPE_TX_ALLOWED_FIRST_ENABLED			1
#define TIMESTAMP_TYPE_FIRST_SYMBOL_ARRIVING			4

#define SET_NUM_TX_SYM(num)								(num)		
#define GET_NUM_TX_SYM(msg32lsb)						(msg32lsb)		
#define SET_NUM_RX_SYM(num)								((num)&0xFFFFFF)		
#define GET_NUM_RX_SYM(msg32msb)						((msg32msb)&0xFFFFFF)	
#define GET_LATENCY_TYPE(msg32msb)						(((msg32msb)>>20)&0xF)
#define GET_TIMESTAMP_TYPE(msg32msb)					(((msg32msb)>>20)&0xF)
#define SET_CYCLE_DMA_TX_SYM(num)						(num)		
#define GET_CYCLE_DMA_TX_SYM(msg32lsb)					(msg32lsb)		
#define SET_CYCLE_DMA_RX_SYM(num)						((num)&0xFFFFFF)		
#define GET_CYCLE_DMA_RX_SYM(msg32msb)					((msg32msb)&0xFFFFFF)	
#define SET_CORE_LOAD_TX(num)							((num)&0xFF)		
#define GET_CORE_LOAD_TX(msg32b)						((msg32b)&0xFF)	
#define SET_CORE_LOAD_RX(num)							(((num)&0xFF)<<8)		
#define GET_CORE_LOAD_RX(msg32b)						(((msg32b)&0xFF)>>8)	
#define SET_CORE_LOAD_TOTAL(num)						(((num)&0xFF)<<16)		
#define GET_CORE_LOAD_TOTAL(msg32b)						(((msg32b)&0xFF)>>16)	
#define SET_CORE_LOAD_SINGLETONE_FLAG(flag)				(((flag)&0xFF)<<24)		
#define SET_KERNEL_CORE_LOAD_IFFT(num)					(((num)&0xFF)<<0)		
#define SET_KERNEL_CORE_LOAD_CFR(num)					(((num)&0xFF)<<8)		
#define SET_KERNEL_CORE_LOAD_2XUP1(num)					(((num)&0xFF)<<16)		
#define SET_KERNEL_CORE_LOAD_DPD(num)					(((num)&0xFF)<<24)		
#define SET_KERNEL_CORE_LOAD_2XUP2(num)					(((num)&0xFF)<<0)		
#define SET_KERNEL_CORE_LOAD_TXQEC(num)					(((num)&0xFF)<<8)		
#define SET_KERNEL_CORE_LOAD_RXQEC(num)					(((num)&0xFF)<<16)		

/**********************************************************************************
 *   VSPA-Host mailbox message defines for MSG_ID_STANDALONE_MEM_LATENCY
 ***********************************************************************************/
#define GET_STANDALONE_MEM_LATENCY_ADDR(msg32b)			(((msg32b)&0xFFFFF)<<12)
#define GET_STANDALONE_MEM_LATENCY_SIZE(msg32b)			(((msg32b)&0xFFFF)&0xFFFFFFFE) //size should be even
#define GET_STANDALONE_MEM_LATENCY_WRITE(msg32b)		(((msg32b)>>16)&0x1)
#define SET_STANDALONE_MEM_LATENCY_CYCLECOUNT(cycle)	((cycle)&0xFFFFFF)

/**********************************************************************************
 *   VSPA-Host mailbox message defines for MSG_ID_CELLTRACK_REQUEST
 ***********************************************************************************/
#define GET_CELLTRACK_EXTBUF_ADDR(x)					((x)<<12)
#define GET_CELLTRACK_SSB_RE_OFFSET(x)					((x)>>20)
#define GET_CELLTRACK_SSB_SYM_ID(x)						((x)&0x7FF)
#define GET_CELLTRACK_NID1(x)							(((x)>>11)&0x1FF)
#define GET_CELLTRACK_NID2(x)							(((x)>>20)&0x3)
#define GET_CELLTRACK_SSB_PERIOD(x)						(((x)>>22)&0x3)

#define GET_CFO_TRID(x)									(((x)>>23)&1)

void host_msg_handling();
void core_A_msg_parse(unsigned int test_cmd_MSB, unsigned int test_cmd_LSB);
unsigned int parse_static_tdd_pattern(struct_static_tdd_patterns* slot_patterns, struct_tdd_pattern_entry* tdd_pattern_entry);

#define PATTERN_IDX_WRAP	0

/* extern struct_dfe_ctrl dfe_ctrl; moved to global_var.h */

__inline__ unsigned int get_tx_pattern_size()
{
	return dfe_ctrl.tdd_pattern_entry.tx_entry[dfe_ctrl.tx_pattern_entry_idx];
}
__inline__ unsigned int get_ts_txallowed()
{
	return dfe_ctrl.tdd_pattern_entry.ts_txallowed[dfe_ctrl.tx_pattern_entry_idx];
//	return dfe_ctrl.tdd_pattern_entry.ts_txallowed[dfe_ctrl.tx_allowed_entry_idx];
}
//__inline__ unsigned int advance_to_next_txallowed()
//{
//	dfe_ctrl.tx_allowed_entry_idx++;
//	if(dfe_ctrl.tx_allowed_entry_idx == MAX_NUM_PATTERN_ENTRIES)
//		dfe_ctrl.tx_allowed_entry_idx = 0;
//}

__inline__ unsigned int get_next_tx_pattern_size()
{
	unsigned int tx_pattern_entry_idx = dfe_ctrl.tx_pattern_entry_idx + 1;
	if(tx_pattern_entry_idx == dfe_ctrl.tdd_pattern_entry.num_patterns)
		tx_pattern_entry_idx = PATTERN_IDX_WRAP;
	return dfe_ctrl.tdd_pattern_entry.tx_entry[tx_pattern_entry_idx];
}
__inline__ void advance_to_next_tx_pattern_size()
{
	dfe_ctrl.tx_pattern_entry_idx++;
	if(dfe_ctrl.tx_pattern_entry_idx == dfe_ctrl.tdd_pattern_entry.num_patterns)
		dfe_ctrl.tx_pattern_entry_idx = PATTERN_IDX_WRAP;
}



__inline__ unsigned int get_rx_pattern_size()
{
	return dfe_ctrl.tdd_pattern_entry.rx_entry[dfe_ctrl.rx_pattern_entry_idx];
}
__inline__ void advance_to_next_rx_pattern_size()
{
	dfe_ctrl.rx_pattern_entry_idx++;
	if(dfe_ctrl.rx_pattern_entry_idx == dfe_ctrl.tdd_pattern_entry.num_patterns)
		dfe_ctrl.rx_pattern_entry_idx = PATTERN_IDX_WRAP;
}

#endif /* HOST_INTERF_H_ */
