// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#include "pair_config.h"
#include "cfr.h"
#include "Ant_TxRx_Buffer_manager.h"
#include "single_tone.h"
#include "ecpri_compression_decompression.h"
#include "fft_cp.h"
#include "dump_inject.h"
#include "host_vspa_msg.h"
#include "qec.h"
#include "qec_opt.h"
#include "global_var.h"

unsigned int ddr_addr=0;  //used to remember DDR address in DMA latency test.
extern struct_dfe_ctrl dfe_ctrl;
unsigned int convert_2implement(unsigned int data);

float mpy_f32_f16(float f32, short f16);
float mpy_f32_f32(float f32_1, float f32_2);
//vector multiplied by scalar
//num must be between 1 to 32.
void mpy_f32_f16_vs(float* dest_f32, float* src_f32, short f16, unsigned int num); 

#if NUM_1T_IN_CORE
#define TEMP_BUFFER		iFFTout_CFRinout
#elif NUM_1R_IN_CORE
#define TEMP_BUFFER		FFTin
#endif

#if (CORE_TYPE == TYPE_CORE_A || CORE_TYPE == TYPE_CORE_B)
extern struct_antman_ctrl_tx antman_ctrl_tx;
extern struct_antman_ctrl_rx antman_ctrl_rx;
extern short cfr_cpulse[SIZEOF8(struct_cpulse)/2*8];
extern int cfr_para[MAX_NUM_CFR_PASS][CFR_PARA_SIZE_BYTES/4];

#if(!BYPASS_IFFT)
#if(NUM_1T_IN_CORE)
extern cfixed16_t sym_queue_tx[NUM_IFFT_IN_BUFFERS][TX_SYM_SIZE/4] __align_vec __attribute__ ((section (".ibss"))) ;
#endif
#endif

#if(!BYPASS_FFT)
#if(NUM_1R_IN_CORE)
extern cfixed16_t sym_queue_rx[RX_SYM_SIZE/4] __align_vec __attribute__ ((section (".ibss"))) ;
#endif
#endif

unsigned int axi_flag_msg_response[4]__attribute__ ((aligned (AXI_BURST_SIZE)));
unsigned int axi_flag_msg_response1[4]__attribute__ ((aligned (AXI_BURST_SIZE)));

void channel_restart()
{
	SET_DFE_MODE_HI_BITS(DFE_MODE_RESTART_TX, 0);   //clear restart bit
			
#if CORE_TYPE == TYPE_CORE_A
#endif
	//sync and reset so that coreA will not write to coreB while coreB is reseting.
	core_reset();
}

int mailbox_send_msg(unsigned int mailbox_id, unsigned int msb32, unsigned int lsb32)
{
	if(!__ip_read(VCPU_MBOX_STATUS, 1<<mailbox_id))
	{
		#ifdef LOG_TYPE_MBOX_MSG_SENT
		log_2int(TYPE_STAMPED(LOG_TYPE_MBOX_MSG_SENT), msb32);
		//log_2int(msb32, lsb32);
		#endif
		__ip_write(VCPU_OUT_0_MSB+(VCPU_OUT_1_MSB-VCPU_OUT_0_MSB)*mailbox_id, msb32);
		__ip_write(VCPU_OUT_0_LSB+(VCPU_OUT_1_LSB-VCPU_OUT_0_LSB)*mailbox_id, lsb32);
//		__ip_write(VCPU_HOST_FLAGS1, 1<<(30+mailbox_id));  //set also this flag as mbox flag sometimes is cleared by unknown host
		return 0;
	}
	else
		return 1;
}

void add_a_pattern(unsigned int pattern_word)
{
	unsigned int n_dl_slots          = pattern_word & 0xF;
	unsigned int n_gp_slots_after_dl = (pattern_word>>4) & 0xF;
	unsigned int n_dl_symbols        = (pattern_word>>8) & 0xF;
	unsigned int n_ul_slots          = (pattern_word>>12) & 0xF;
	unsigned int n_gp_slots_after_ul = (pattern_word>>16) & 0xF;
	unsigned int n_ul_symbols        = (pattern_word>>20) & 0xF;
	
	unsigned int num_dl_symbols          = n_dl_slots*NUM_SYM_PER_SLOT + n_dl_symbols;
	unsigned int num_gp_symbols_after_dl = ((NUM_SYM_PER_SLOT-n_dl_symbols-n_ul_symbols)%NUM_SYM_PER_SLOT) + n_gp_slots_after_dl*NUM_SYM_PER_SLOT;
	unsigned int num_ul_symbols          = n_ul_slots*NUM_SYM_PER_SLOT + n_ul_symbols;
	unsigned int num_gp_symbols_after_ul = n_gp_slots_after_ul*NUM_SYM_PER_SLOT;
	
	unsigned int pattern_idx = dfe_ctrl.tdd_pattern_entry.num_patterns;
	if(!__ip_read(IP_IDX_DFE_MODE_HI, DFE_MODE_CPE))
	{
		dfe_ctrl.tdd_pattern_entry.tx_entry[pattern_idx] = (num_dl_symbols<<PATTERN_SIZE_SYMBOL_BITIDX)|((num_ul_symbols+num_gp_symbols_after_dl+num_gp_symbols_after_ul)<<PATTERN_SIZE_SYMBOL_SKIP1_BITIDX)|(1<<PATTERN_SIZE_VALID_BITIDX);
		dfe_ctrl.tdd_pattern_entry.rx_entry[pattern_idx] = (num_ul_symbols<<PATTERN_SIZE_SYMBOL_BITIDX)|((num_dl_symbols+num_gp_symbols_after_dl)<<PATTERN_SIZE_SYMBOL_SKIP0_BITIDX)|((num_gp_symbols_after_ul)<<PATTERN_SIZE_SYMBOL_SKIP1_BITIDX)|(1<<PATTERN_SIZE_VALID_BITIDX);
	}
	else
	{
		dfe_ctrl.tdd_pattern_entry.rx_entry[pattern_idx] = (num_dl_symbols<<PATTERN_SIZE_SYMBOL_BITIDX)|((num_ul_symbols+num_gp_symbols_after_dl+num_gp_symbols_after_ul)<<PATTERN_SIZE_SYMBOL_SKIP1_BITIDX)|(1<<PATTERN_SIZE_VALID_BITIDX);
		dfe_ctrl.tdd_pattern_entry.tx_entry[pattern_idx] = (num_ul_symbols<<PATTERN_SIZE_SYMBOL_BITIDX)|((num_dl_symbols+num_gp_symbols_after_dl)<<PATTERN_SIZE_SYMBOL_SKIP0_BITIDX)|((num_gp_symbols_after_ul)<<PATTERN_SIZE_SYMBOL_SKIP1_BITIDX)|(1<<PATTERN_SIZE_VALID_BITIDX);
	}
	dfe_ctrl.tdd_pattern_entry.num_patterns = pattern_idx+1;
}

unsigned int convert_sym_idx_to_sample_idx(unsigned int sym_idx)
{
	unsigned num_cp_period = sym_idx/NUM_SYM_PER_CP_PERIOD;
	unsigned num_sym_remain = sym_idx % NUM_SYM_PER_CP_PERIOD;
	unsigned num_sample_remain = num_sym_remain * NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP + __min(1,num_sym_remain)*(NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_LCP-NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP);
	return (num_cp_period*NUM_SAMPLE_PER_CP_PERIOD+num_sample_remain);
}

unsigned int get_num_samples_in_symbol_idx_range(unsigned int start_idx, unsigned int num_symbols)
{
#if 0
	if (!num_symbols)	return 0;
	unsigned int num_cp_period = num_symbols/NUM_SYM_PER_CP_PERIOD;
	unsigned int num_samples   = num_symbols*NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP + num_cp_period*(NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_LCP-NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP);
	unsigned int num_sym_remain = num_symbols%NUM_SYM_PER_CP_PERIOD;
	if(num_sym_remain == 0)
	{
		return num_samples;
	}
	unsigned int start_idx_in_cp_period = start_idx%NUM_SYM_PER_CP_PERIOD;
	unsigned int end_idx = start_idx_in_cp_period + num_sym_remain;
	if((start_idx_in_cp_period == 0) || (end_idx > NUM_SYM_PER_CP_PERIOD))
		num_samples += (NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_LCP-NUM_SAMPLE_PER_SYMBOL_TIME_DOMAIN_SCP);
	
	return num_samples;
#else
	return convert_sym_idx_to_sample_idx(start_idx+num_symbols)-convert_sym_idx_to_sample_idx(start_idx);
#endif
}


#if (CORE_TYPE == TYPE_CORE_A)
void core_A_msg_parse(unsigned int test_cmd_MSB, unsigned int test_cmd_LSB)
{
	unsigned int cmd = GET_COMMAND(test_cmd_MSB);
	unsigned int para_hi = GET_TEST_PARA_HI(test_cmd_MSB);
	unsigned int para_lo = test_cmd_LSB;

	//parse test command
		
	
		
		
		
#if (NUM_1T_IN_CORE)
		
#endif
#if (NUM_1R_IN_CORE)
		
	#if SINAD_TEST_ENABLE
	if(cmd == COMMAND_SINAD_TEST)
	{
		dfe_ctrl.sinad_start_idx_sig = GET_START_FREQ_INDEX_SIG(test_cmd_MSB);
		dfe_ctrl.sinad_num_idx_sig   = GET_NUM_FREQ_INDEX_SIG(test_cmd_LSB);
		dfe_ctrl.sinad_num_idx_dc    = GET_NUM_FREQ_INDEX_DC(test_cmd_LSB);
		dfe_ctrl.sinad_output_addr   = GET_SINAD_OUTPUT_ADDR(test_cmd_LSB);
	}
	else
	#endif
#endif
	{
		//undefined cmd
	}
}
#endif

char seq_id=0;

void host_msg_handling()
{
	unsigned int box_id = 0;
	unsigned long long msg64;
	unsigned int msg_hi, msg_lo;
	
	if (host_mbox0_event())
	{
		// Read message from host:
		msg64 = host_mbox0_read();
		msg_hi = HIWORD(msg64);
		msg_lo = LOWORD(msg64);
	}
	else if(host_mbox1_event())
	{
		// Read message from host:
		msg64 = host_mbox1_read();
		msg_hi = HIWORD(msg64);
		msg_lo = LOWORD(msg64);
		box_id = 1;
	}
	else
	{
		return;
	}
	
	// Check message from host:
		#ifdef LOG_TYPE_MBOX_MSG_RECEIVED
		log_2int(TYPE_STAMPED(LOG_TYPE_MBOX_MSG_RECEIVED), msg_hi);
		#ifdef LOG_TYPE_MBOX_MSG_RECEIVED_LOW
		log_2int(box_id, msg_lo);
		#endif
		#endif

		uint32_t cid = msg_hi & MSG_ID_MASK;

    	
    	
    	if(cid == MSG_ID_FDD_START)
        {
        }
    	else if(cid == MSG_ID_FDD_STOP)
        {
        }
    	
#if FLEXIBLE_SLOT_FORMAT_ENABLE
    	
#if TXDCS_FDD == 0
    	else if(cid == MSG_ID_FLEXIBLE_SLOT_FORMAT)
        {	
    		
#if 1  //use m4 03 msg
    		unsigned int start_idx = GET_FLEX_SF_TX_SYMBOL_START(msg_hi);
        	unsigned int num_symbols = GET_FLEX_SF_TX_NUM_SYMBOLS(msg_hi);
        	unsigned int num_symbols_skip0 = start_idx;
        	unsigned int num_symbols_skip1 = NUM_SYM_PER_SLOT-start_idx-num_symbols;
        	unsigned int stop = GET_FLEX_SF_STOP(msg_hi);
        	SET_FAST_FLAGS((stop<<FAST_FLAG_TX_TDDSTOP_IDX)|(stop<<FAST_FLAG_RX_TDDSTOP_IDX));
        	unsigned int num_tx_symbol_word = (num_symbols<<PATTERN_SIZE_SYMBOL_BITIDX)|(num_symbols_skip1<<PATTERN_SIZE_SYMBOL_SKIP1_BITIDX)|(num_symbols_skip0<<PATTERN_SIZE_SYMBOL_SKIP0_BITIDX)|PATTERN_SIZE_VALID_MASK;


       		start_idx = GET_FLEX_SF_RX_SYMBOL_START(msg_hi);
       		num_symbols = GET_FLEX_SF_RX_NUM_SYMBOLS(msg_hi);
       		num_symbols_skip0 = start_idx;
       		num_symbols_skip1 = NUM_SYM_PER_SLOT-start_idx-num_symbols;
        	unsigned int num_rx_symbol_word = (num_symbols<<PATTERN_SIZE_SYMBOL_BITIDX)|(num_symbols_skip1<<PATTERN_SIZE_SYMBOL_SKIP1_BITIDX)|(num_symbols_skip0<<PATTERN_SIZE_SYMBOL_SKIP0_BITIDX)|PATTERN_SIZE_VALID_MASK;

        	struct_tdd_pattern_entry* p = &dfe_ctrl.tdd_pattern_entry;
        	p->tx_entry[dfe_ctrl.flex_sf_pattern_entry_wr_idx] = num_tx_symbol_word;
        	p->rx_entry[dfe_ctrl.flex_sf_pattern_entry_wr_idx] = num_rx_symbol_word;
        	p->ts_txallowed[dfe_ctrl.flex_sf_pattern_entry_wr_idx] = msg_lo + get_timestamp32();

        	dfe_ctrl.flex_sf_pattern_entry_wr_idx = (dfe_ctrl.flex_sf_pattern_entry_wr_idx+1)%MAX_NUM_PATTERN_ENTRIES;
     		p->tx_entry[dfe_ctrl.flex_sf_pattern_entry_wr_idx] = 0;
     		p->rx_entry[dfe_ctrl.flex_sf_pattern_entry_wr_idx] = 0;

     		
#else   //use static msg. not tested.
        	unsigned int stop = GET_FLEX_SF_STOP(msg_hi);
        	SET_FAST_FLAGS((stop<<FAST_FLAG_TX_TDDSTOP_IDX)|(stop<<FAST_FLAG_RX_TDDSTOP_IDX));
        	if(msg_lo)
        	{
            	struct_tdd_pattern_entry* p = &dfe_ctrl.tdd_pattern_entry;
            	p->ts_txallowed[dfe_ctrl.flex_sf_pattern_entry_wr_idx] = msg_lo + get_timestamp32();
            	dfe_ctrl.flex_sf_pattern_entry_wr_idx = (dfe_ctrl.flex_sf_pattern_entry_wr_idx+1)%MAX_NUM_PATTERN_ENTRIES;
        	}
     		
#endif
      }
#endif
    	
    	
#else
		else if(cid == MSG_ID_STATIC_SLOT_FORMAT)
        {
           	unsigned int seq_id_mask = GET_SEQ_ID_MASK(msg_lo);
			mailbox_send_msg(box_id, MSG_ID_STATIC_SLOT_FORMAT_ACK, seq_id_mask);
			if(seq_id_mask == 0)
				 dfe_ctrl.tdd_pattern_entry.num_patterns = 0;  //clear num of entry if this is the first msg
			
           	add_a_pattern(msg_lo);
           	
           	if(msg_hi&0xFFFFFF)
           		add_a_pattern(msg_hi);

           	dfe_ctrl.tdd_pattern_entry.num_patterns_onetime = GET_NUM_ONETIME_PATTERN(msg_lo);
        }
#endif

    	else 
    		
        
        	

#if DMA_MEASUREMENT_ENABLE
        if(cid == MSG_ID_STANDALONE_MEM_LATENCY)
        {
        	unsigned int addr = GET_STANDALONE_MEM_LATENCY_ADDR(msg_lo);
        	if(addr<0xe0000000)
        		ddr_addr = addr;
        	unsigned int size = GET_STANDALONE_MEM_LATENCY_SIZE(msg_hi);
        	if(size==0)
        		size = 64*1024;
        	unsigned int write = GET_STANDALONE_MEM_LATENCY_WRITE(msg_hi);
        	unsigned int vspa_internal_addr = (unsigned int)TEMP_BUFFER;
        	unsigned int vspa_internal_addr_r = vspa_internal_addr; //addr for reverse direction.
        	unsigned int background_dma_size = __min(65536,SIZEOF8(TEMP_BUFFER));
        	
        	unsigned int dma_mode = DMAC_RD | DMAC_MBRE;
        	unsigned int dma_mode_r = DMAC_WR;   //r is the reverse direction
        	if(write)
        	{
        		dma_mode = DMAC_WR; 
        		dma_mode_r = DMAC_RD | DMAC_MBRE;
        		*(unsigned int*)vspa_internal_addr = 0x1234ABCD;					//fill the first data with special value
        		*(unsigned int*)(vspa_internal_addr + size - 4) =0x1A2B3C4D;		//fill the last data with special value
        	}
        	
        	//type1 testing:  single DMA channel, no background DMA.        	
        	dmac_enable(dma_mode | DMA_CHAN_DUMP_ANT_DATA, size, addr, UNIT2BYTE(vspa_internal_addr));
        	unsigned int cycle_1_chan = get_timestamp32();
        	while(dmac_is_enabled(1<<DMA_CHAN_DUMP_ANT_DATA)){} //wait DMA completes
        	cycle_1_chan = get_timestamp32() - cycle_1_chan;
        	
        	//type2 testing:  2 DMA channels, no background DMA.
        	unsigned int size1 = size/2;
        	dmac_enable(dma_mode | DMA_CHAN_DUMP_ANT_DATA, size, addr, UNIT2BYTE(vspa_internal_addr));
			__asm volatile("fnop .asmvol");
        	dmac_enable(dma_mode | DMA_CHAN_INJECT_ANT_DATA, size1, addr, UNIT2BYTE(vspa_internal_addr));
        	unsigned int cycle_2_chan = get_timestamp32();
        	while(dmac_is_enabled(1<<DMA_CHAN_INJECT_ANT_DATA)){} //wait DMA completes
        	cycle_2_chan = get_timestamp32() - cycle_2_chan;
        	mailbox_send_msg(box_id, MSG_ID_STANDALONE_MEM_LATENCY_ACK|SET_STANDALONE_MEM_LATENCY_CYCLECOUNT(cycle_2_chan), 
        			SET_STANDALONE_MEM_LATENCY_CYCLECOUNT(cycle_1_chan));
        	while(dmac_is_enabled(1<<DMA_CHAN_DUMP_ANT_DATA)){}
        	
        	//type 3 bi-direction:
        	unsigned int cycle_3_chan;
        	if(write ==0)  //1 READ channels, with background 1 WRITE channel.  
        	{
				dmac_enable(dma_mode_r | DMA_CHAN_INJECT_ANT_DATA, background_dma_size, addr, UNIT2BYTE(vspa_internal_addr_r));
				__asm volatile("fnop .asmvol");
				dmac_enable(dma_mode | DMA_CHAN_FETCH_DU_TX_SYMBOLS(0), size, addr, UNIT2BYTE(vspa_internal_addr));
				cycle_3_chan = get_timestamp32();
				while(dmac_is_enabled((1<<DMA_CHAN_FETCH_DU_TX_SYMBOLS(0))))
				{
					if((__ip_read( DMA_FIFO_STAT, 1<<DMA_CHAN_INJECT_ANT_DATA)))
						dmac_enable(dma_mode_r | DMA_CHAN_INJECT_ANT_DATA, background_dma_size, addr, UNIT2BYTE(vspa_internal_addr_r));
				} //wait DMA completes
				cycle_3_chan = get_timestamp32() - cycle_3_chan;
        	}
        	else // 1 WRITE channel, with background 1 READ channels
        	{
				dmac_enable(dma_mode_r | DMA_CHAN_INJECT_ANT_DATA, background_dma_size, addr, UNIT2BYTE(vspa_internal_addr_r));
				__asm volatile("fnop .asmvol");
				dmac_enable(dma_mode | DMA_CHAN_SEND_DU_RX_SYMBOLS(0), size, addr, UNIT2BYTE(vspa_internal_addr));
				cycle_3_chan = get_timestamp32();
				while(dmac_is_enabled((1<<DMA_CHAN_SEND_DU_RX_SYMBOLS(0))))
				{
					if((__ip_read( DMA_FIFO_STAT, 1<<DMA_CHAN_INJECT_ANT_DATA)))
						dmac_enable(dma_mode_r | DMA_CHAN_INJECT_ANT_DATA, background_dma_size, addr, UNIT2BYTE(vspa_internal_addr_r));
				} //wait DMA completes
				cycle_3_chan = get_timestamp32() - cycle_3_chan;
        	}
			while(dmac_is_enabled((1<<DMA_CHAN_INJECT_ANT_DATA))) {}
        		
        	//type 4: same direction testing: 
        	unsigned int cycle_4_chan;
        	if(write ==0)  //1 READ channels, with background 1 READ channel on DDR.  
        	{
				dmac_enable(dma_mode | DMA_CHAN_INJECT_ANT_DATA, background_dma_size, ddr_addr, UNIT2BYTE(vspa_internal_addr_r));
				__asm volatile("fnop .asmvol");
				dmac_enable(dma_mode | DMA_CHAN_SEND_DU_RX_SYMBOLS(0), size, addr, UNIT2BYTE(vspa_internal_addr));
				cycle_4_chan = get_timestamp32();
				while(dmac_is_enabled((1<<DMA_CHAN_SEND_DU_RX_SYMBOLS(0))))
				{
					if((__ip_read( DMA_FIFO_STAT, 1<<DMA_CHAN_INJECT_ANT_DATA)))
						dmac_enable(dma_mode | DMA_CHAN_INJECT_ANT_DATA, background_dma_size, ddr_addr, UNIT2BYTE(vspa_internal_addr_r));
				} //wait DMA completes
				cycle_4_chan = get_timestamp32() - cycle_4_chan;
        	}
        	else // 1 WRITE channel, with background 1 WRITE channel on DDR
        	{
				dmac_enable(dma_mode | DMA_CHAN_INJECT_ANT_DATA, background_dma_size, ddr_addr, UNIT2BYTE(vspa_internal_addr_r));
				__asm volatile("fnop .asmvol");
				dmac_enable(dma_mode | DMA_CHAN_SEND_DU_RX_SYMBOLS(0), size, addr, UNIT2BYTE(vspa_internal_addr));
				cycle_4_chan = get_timestamp32();
				while(dmac_is_enabled((1<<DMA_CHAN_SEND_DU_RX_SYMBOLS(0))))
				{
					if((__ip_read( DMA_FIFO_STAT, 1<<DMA_CHAN_INJECT_ANT_DATA)))
						dmac_enable(dma_mode | DMA_CHAN_INJECT_ANT_DATA, background_dma_size, ddr_addr, UNIT2BYTE(vspa_internal_addr_r));
				} //wait DMA completes
				cycle_4_chan = get_timestamp32() - cycle_4_chan;
        	}
			while(dmac_is_enabled((1<<DMA_CHAN_INJECT_ANT_DATA))) {}
        
        	while(__ip_read(VCPU_MBOX_STATUS, 1<<box_id)){}  //wait until msg reg availabe.
        	mailbox_send_msg(box_id, MSG_ID_STANDALONE_MEM_LATENCY_ACK|SET_STANDALONE_MEM_LATENCY_CYCLECOUNT(cycle_4_chan), 
        			SET_STANDALONE_MEM_LATENCY_CYCLECOUNT(cycle_3_chan));
        }
    	else
#endif
        {
            // Reply to host:
        	//mailbox_send_msg(box_id, MSG_ID_UNEXPECTED_MSG, msg_hi);
        }
}

void check_msg()
{
	#if (CORE_TYPE == TYPE_CORE_B) || ((CORE_TYPE == TYPE_CORE_A) && (CORE_A_ID == CORE_B_ID))
	host_msg_handling();
	#else
	if(__ip_read(IP_IDX_DFE_MODE_HI, (DFE_MODE_RESTART_TX|DFE_MODE_RESTART_RX)))
	{
		channel_restart();
	}
	#endif
	
	if(__ip_read(IP_IDX_DFE_MODE_HI, (DFE_MODE_RESTART_TX|DFE_MODE_RESTART_RX)))
	{
		channel_restart();
	}
}


#endif
