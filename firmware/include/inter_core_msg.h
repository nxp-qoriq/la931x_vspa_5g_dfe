// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP


#ifndef INTER_CORE_MSG_H_
#define INTER_CORE_MSG_H_

#define VSPA_DMEM_BASE	0x20400000 
#define VCPU_DMEM_SIZE	0x4000
#define VSPA_IP_BASE	0x41000000

#define ADDR_AXI_VCPU_DMEM_CORE(core)		(VSPA_DMEM_BASE + 0x400000*(core))
#define ADDR_AXI_IPPU_DMEM_CORE(core)		(ADDR_AXI_VCPU_DMEM_CORE(core)+0x100000)
#define ADDR_AXI_FLAG_CORE(core)			(ADDR_AXI_IPPU_DMEM_CORE(core)+0x100000)
#define ADDR_AXI_VCPU_IP_CORE(core,ipaddr)	(VSPA_IP_BASE + 0x4000*(core) + (ipaddr))

#define IP_IDX_FAST_FLAGS		IPPUSWVER


//#define IP_IDX_RX_POWER			LD_RF_TB_REAL_4
//#define IP_IDX_RX_POWER_SAMPLES	LD_RF_TB_IMAG_4
//#define IP_IDX_TX_POWER			LD_RF_TB_REAL_5
//#define IP_IDX_TX_POWER_SAMPLES	LD_RF_TB_IMAG_5
//#define IP_IDX_TRX_NUM_SYMS		LD_RF_TB_REAL_6
#define IP_IDX_NSC				LD_RF_TB_IMAG_6
#define IP_IDX_DFE_MODE_HI		LD_RF_TB_REAL_7
#define IP_IDX_DFE_MODE_LO		LD_RF_TB_IMAG_7

#define SET_DFE_MODE_HI_BITS(mask, value)	__ip_write(IP_IDX_DFE_MODE_HI,mask,value)
#define GET_DFE_MODE_HI_BITS(mask)			__ip_read(IP_IDX_DFE_MODE_HI,mask)

#define GET_NUM_SC				__ip_read(IP_IDX_NSC)

#define FAST_FLAG_RX_TDDSTOP_IDX		31
#define FAST_FLAG_TX_TDDSTOP_IDX		30
//#define FAST_FLAG_TX_FDDSTART_IDX		29
//#define FAST_FLAG_TX_FDDSTARTED_IDX		28
//#define FAST_FLAG_TX_FDDSTOP_IDX		27
#define FAST_FLAG_RX_SYM_DMA_INP_IDX	26
#define FAST_FLAG_TX_SYM_DMA_INP_IDX	25
#define FAST_FLAG_2RX_IDX				0		//must be 0, this bit is used as number of RX, 0->1RX, 1->2RX
#define FAST_FLAG_RX_TDDSTOP_MASK		(1<<FAST_FLAG_RX_TDDSTOP_IDX)
#define FAST_FLAG_TX_TDDSTOP_MASK		(1<<FAST_FLAG_TX_TDDSTOP_IDX)
//#define FAST_FLAG_TX_FDDSTART_MASK		(1<<FAST_FLAG_TX_FDDSTART_IDX)
//#define FAST_FLAG_TX_FDDSTARTED_MASK	(1<<FAST_FLAG_TX_FDDSTARTED_IDX)
//#define FAST_FLAG_TX_FDDSTOP_MASK		(1<<FAST_FLAG_TX_FDDSTOP_IDX)
#define FAST_FLAG_TX_SYM_DMA_INP_MASK	(1<<FAST_FLAG_TX_SYM_DMA_INP_IDX)
#define FAST_FLAG_RX_SYM_DMA_INP_MASK	(1<<FAST_FLAG_RX_SYM_DMA_INP_IDX)
#define FAST_FLAG_2RX_MASK				(1<<0)

#define CLR_ALL_FAST_FLAGS		__ip_write(IP_IDX_FAST_FLAGS, 0)
#define CLR_FAST_FLAGS(flags)	__ip_write(IP_IDX_FAST_FLAGS, flags, 0)
#define SET_FAST_FLAGS(flags)	__ip_write(IP_IDX_FAST_FLAGS, flags, flags)
#define GET_FAST_FLAGS(flags)	__ip_read(IP_IDX_FAST_FLAGS, flags)


__inline unsigned int get_vspa_dmem_addr_axi32(unsigned int core_id, unsigned int addr19bit)
{
	unsigned addr = ADDR_AXI_VCPU_DMEM_CORE(core_id) + addr19bit;
	if(addr19bit >= VCPU_DMEM_SIZE)
		addr = addr - VCPU_DMEM_SIZE + 0x100000;
	return addr;
}

__inline unsigned int is_vspa_dmem(unsigned int addr)
{
	if((addr >= ADDR_AXI_VCPU_DMEM_CORE(0)) && (addr < ADDR_AXI_VCPU_DMEM_CORE(8)))
		return 1;
	else
		return 0;
}

//Inter-Core msg defines
#define IC_MSG_ID_MASK					0xF0000000
#define IC_MSG_DATA_MASK				0x0FFFFFFF
#define IC_MSG_ID_INTERCORE_SYNC		0x00000000
#define IC_MSG_ID_SLOT_PATTERN_REQUEST	0x10000000
#define IC_MSG_ID_TX_BLOCK_REQUEST		0x20000000
#define IC_MSG_ID_TX_BLOCK_RELEASE		0x80000000
#define IC_MSG_ID_RX_SYMBOL_REQUEST		0x90000000
#define IC_MSG_ID_OBS_DUMP				0xA0000000 //EXT GO MSG sent to OBS DUMP core for sync dump of DPD and OBS

#define IC_MSG_ID_TX_SYM_BUF_CONFIG		0xC0000000
#define IC_MSG_ID_TX_SYM_BUF_CONFIG_ACK	0xD0000000
#define IC_MSG_ID_RX_SYM_BUF_CONFIG		0xE0000000
#define IC_MSG_ID_RX_SYM_BUF_CONFIG_ACK	0xF0000000
#define IC_MSG_SET_TR_ID(tr_id)			(((tr_id)&1)<<27)
#define IC_MSG_GET_TR_ID(msg)			(((msg)>>27)&1)
#define IC_MSG_SET_TX_BASE(base)		((base)>>7)
#define IC_MSG_GET_TX_BASE(msg)			((msg)<<7)
#define IC_MSG_SET_RX_BASE				IC_MSG_SET_TX_BASE
#define IC_MSG_GET_RX_BASE				IC_MSG_GET_TX_BASE
#define IC_MSG_SET_NUM_SYM(num)			((((num)/2)&3)<<25)
#define IC_MSG_GET_NUM_SYM(msg)			__max(1,((((msg)>>25)&3)*2))


__inline void send_msg_ext_go(unsigned int dest_core_id, unsigned int msg)
{
	__ip_write(GPO(dest_core_id), msg);
}

//return value: 0-no msg,  none 0 - msg ready
__inline unsigned int check_msg_ext_go(unsigned int src_core_id)
{
	return __ip_read(EXT_GO_STAT, 1<<src_core_id);
}
__inline unsigned int read_msg_ext_go(unsigned int src_core_id)
{
	return __ip_read(GPI(src_core_id));
}
__inline void clr_msg_ext_go(unsigned int src_core_id)
{
	__ip_write(EXT_GO_STAT, 1<<src_core_id); //clear ext go bit
}
//after poll_msg_ext_go() returns none 0, call this API to get the msg
__inline unsigned int readclr_msg_ext_go(unsigned int src_core_id)
{
	__ip_write(EXT_GO_STAT, 1<<src_core_id); //clear ext go bit
	return __ip_read(GPI(0) + src_core_id);
}

__inline unsigned int check_axi_slave(unsigned int flag_mask)
{
	return __ip_read(AXISLV_FLAGS0, flag_mask);
}

__inline void clr_axi_slave(unsigned int flag_mask)
{
	__ip_write(AXISLV_FLAGS0, flag_mask);
}

__inline unsigned int host_sym_rdy(unsigned int sym_idx)
{
	return __ip_read(HOST_VCPU_FLAGS1, 1<<sym_idx);
}
__inline unsigned int host_sym_rdy_bitloc(unsigned int bitloc)
{
	return __ip_read(HOST_VCPU_FLAGS1, bitloc);
}
__inline void host_sym_rdy_clr(unsigned int sym_idx)
{
	__ip_write(HOST_VCPU_FLAGS1, 1<<sym_idx);
}
__inline void host_sym_rdy_clr_bitloc(unsigned int bitloc)
{
	__ip_write(HOST_VCPU_FLAGS1, bitloc);
}
__inline unsigned int host_sym_tdflag(unsigned int sym_idx)
{
	return __ip_read(HOST_VCPU_FLAGS0, 1<<sym_idx);
}
__inline void host_sym_tdflag_clr(unsigned int sym_idx)
{
	__ip_write(HOST_VCPU_FLAGS0, 1<<sym_idx);
}

__inline unsigned int vspa_sym_buf_full(unsigned int sym_idx)
{
	return __ip_read(VCPU_HOST_FLAGS1, 1<<sym_idx);
}
__inline void vspa_sym_rdy_set(unsigned int sym_idx)
{
	__ip_write(VCPU_HOST_FLAGS1, 1<<sym_idx);
}

//__inline void set_tx_slot_idx_to_host(unsigned int slot_idx)
//{
//	__ip_write(VCPU_HOST_FLAGS0, 0xFFFF, slot_idx);
//}
//__inline void set_rx_slot_idx_to_host(unsigned int slot_idx)
//{
//	__ip_write(VCPU_HOST_FLAGS0, 0xFFFF0000, slot_idx<<16);
//}


#if (CORE_TYPE == TYPE_CORE_A || CORE_TYPE == TYPE_CORE_B) && (HIGHPHY_LOWPHY_IF_TYPE == IF_TYPE_VSPA_VSPA)
void inter_core_gpio_msg_processing();
#endif



//this function is to wait for all cores to reach to the same point before continuing
#define CORE_BITMASK(core_id)			(1<<(core_id))
//core_id_bitmask is combination of multiple CORE_BITMASK(id)
#define INTERCORE_SYNC_PATTERN	(IC_MSG_ID_INTERCORE_SYNC|0x00005A5A)

void wait_for_cores(unsigned int core_id_bitmask);

#endif /* INTER_CORE_MSG_H_ */
