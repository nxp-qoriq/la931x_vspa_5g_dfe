/*
* Copyright 2022-2023 NXP
*
*/

%>cd C:/Freescale/CW_VSPA_v10.3.0/b200724/workspace_A\vspa_sim\
%>source ./script/libsim.tcl 
%>sim_dram_load "./tv/sym0_ed_div4_dc1639.hex" [evaluate #x decomp_out]

%>sim_dram_save "./tv/kernel_FFT/IFFT_out.hex"  0x5d80 4096*4            <-- [evaluate #x vcpu_sym_out]
%>sim_dram_save "./tv/kernel_FFT/IFFT_cp_out_tmp.hex"  [evaluate #x iFFTout_CFRinout] 20480

Dec3
int symbol_buffer[NUM_SYMBOLS][ALIGN64(SYMBOL_SIZE)]__attribute__ ((aligned (128)))__attribute__ ((section (".ibss")))={
#include "..\test\sym14s_div4_dc1639.hex"
};
14 symbols, 
3276 + 30_zeros each symbol
tm33
C:\Users\nxa16744\OneDrive - NXP\Documents\MATLAB\Examples\R2021a\5g\NRTestModelWaveformGenerationExample\tt.m
 
%>sim_dram_save "./tv/out.hex"  0x60000000 61440*16 p
## matlab ##
	sym14_tx_o = dmemReadHexFile( '../tv/out.hex', 0 );
	sym14_tx_o = dmemReadComplex(sym14_tx_o, 0, 61440*4, 'half_fixed' );
	sym14_tx_o_1x = resample(sym14_tx_o,1,4);
	XL_EVMMeasurementDFE
	
Dec7
Rx
sim_dram_load "./tv/RX_sym14s_tdd_td_2X.hex"  0x70000000 p
switchtarget
sim_dram_save "E:/E_work/AE/Chips/UA/5G/RU/RUref/DFE_ref_project/DFE_ref_project/test/Rx_sym14_out.hex"  [evaluate #x  symbol_buffer] 186368
