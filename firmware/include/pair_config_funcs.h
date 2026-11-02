// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef PAIR_CONFIG_FUNCS_H
#define PAIR_CONFIG_FUNCS_H

/* ASM/C function declarations, inline helpers, and a couple of mode-derivation
 * #defines that depend on dfe_ctrl_s flags. Included via pair_config.h after
 * pair_config_types.h.
 */

void IQ_duplicate_x2_asm(void* out, void* in, unsigned int num_line_in);
void IQ_duplicate_x4_asm(void* out, void* in, unsigned int num_line_in);
void IQ_duplicate_x6_asm(void* out, void* in, unsigned int num_line_in);
void IQ_decim_x2_asm(void* out, void* in, unsigned int num_line_out);


void boot(void);
convert_half_to_hfix(void* out, void* in, unsigned int nL);
convert_hfix_to_half(void* out, void* in, unsigned int nL);

void host_msg_handling();
void pipeline_reset_tx();


void dpd_hist_calc_lp(void* output_abs_buf, void* input_hist_buf);
void dpd_hist_calc_hp(void* output_abs_buf, void* input_hist_buf);

unsigned int run_ifft();
unsigned int run_cfr();
unsigned int get_available_size_for_ifft_out();
unsigned int get_valid_size_for_fft_in();
unsigned int dma_tx_1block_to_coreA();
void start_ippu_bitrev_tx();
void check_ippu_bitrev_done_tx_windowing();
unsigned int dma_tx_blocks_to_coreA();
unsigned int get_tx_1block_from_cfr_out();
void ecpri_decomp_init();
void ecpri_comp_init();
unsigned int circ_addr_advance(unsigned int addr, int offset);
unsigned int run_fft();
unsigned int PHY_DFE_interf_tx_check_dma_start();
unsigned int PHY_DFE_interf_tx_check_dma_finish();
unsigned int PHY_DFE_interf_rx_check_dma_start();
unsigned int PHY_DFE_interf_rx_check_dma_finish();
void copy_needed_var_to_core_A();
void copy_needed_var_to_core_B();
void copy_needed_var_from_core_A();
void copy_needed_var_to_core_B();
void init_common();
unsigned int tx_processing_symbol_level();
unsigned int rx_processing_symbol_level();
unsigned int dma_start(unsigned int control, unsigned int size, unsigned int axi_addr, unsigned int vspa_addr);
unsigned int convert_sym_idx_to_sample_idx(unsigned int sym_idx);
unsigned int get_num_samples_in_symbol_idx_range(unsigned int start_idx, unsigned int num_symbols);
void ext_word_write(unsigned int addr, unsigned int value);
void ext_word_write_2int(unsigned int addr, unsigned int value, unsigned int addr2, unsigned int value2);
void set_rx_slot_sym_idx(unsigned int idx);
void pipeline_reset_rx();
void check_msg();

__inline__ void core_reset()
{
	__ip_write(SWVERSION, CURRENT_CORE_VERSION);
	__asm volatile("jmp 0");
	__asm volatile("fnop .asmvol");
	__asm volatile("fnop .asmvol");
}

void delay_cycles(unsigned int cycles);

//__inline unsigned char advance_pattern_size_idx(struct_tdd_pattern_entry* p, unsigned char pattern_size_idx)
//{
//	return (pattern_size_idx + 1) < p->num_patterns ? (pattern_size_idx + 1) : p->num_patterns_onetime; //change for special pattern
//}

__inline unsigned int get_cp_size(unsigned int sym_idx_per_cp_period)
{
	if (sym_idx_per_cp_period == 0)
		return NUM_SAMPLES_LCP*4;
	else
		return NUM_SAMPLES_SCP*4;
}

#if TDD_ONLY
#define FDD_MODE_TX		0
#define FDD_MODE_RX		0
#elif FDD_ONLY
#define FDD_MODE_TX		1
#define FDD_MODE_RX		1
#else
#define FDD_MODE_TX		GET_DFE_MODE_HI_BITS(DFE_MODE_TX_FDD)
#define FDD_MODE_RX		GET_DFE_MODE_HI_BITS(DFE_MODE_RX_FDD)
#endif

#if RX_OPTION8_DISABLE
#define RX_OPTION8_CFG	0
#else
#define RX_OPTION8_CFG	GET_DFE_MODE_HI_BITS(DFE_MODE_OPTION8)
#endif

#endif /* PAIR_CONFIG_FUNCS_H */
