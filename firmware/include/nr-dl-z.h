// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __NR_DL_Z_H__
#define __NR_DL_Z_H__

//#include "cpe-csirs.h"
#include "vcpu.h"
#include "atan.h"
#include "ssdet.h"

#define UNIT_TEST    1
//#define DMA_ENABLED	 1

#define SLOTTED_PSS_SSS_OFST    64

// Convert SSB CFO from 7.68MSPS units to 30.72MSPS units
//#define CFO_7_68_TO_30_72(x)   ( (x)>>2 )
// Convert SSB CFO from 7.68MSPS units to 61.44MSPS units
#define CFO_7_68_TO_61_44(x)   ( (x)>>3 )

// Convert FFT phase slope into timing error in units of 61.44 samples
#define FD_PHASE_RAMP_TO_TD_61_44(x)   ( (x)<<11 )

#define DL_FFT_GAIN_PWR_DB_Q16      0x001E1A5E  // 20log10(DL_FFT_GAIN_FACTOR_FP)=30.1030 dB


//! @brief      Subcarrier spacing enumeration
//! @see        3GPP TS 38.101-2, table 5.3.2-1; 38.101-1, table 5.3.2-1
enum nr_scs_t {
    NR_SCS_15,      //!<  15 kHz subcarrier spacing (FR1)
    NR_SCS_30,      //!<  30 kHz subcarrier spacing (FR1)
    NR_SCS_60,      //!<  60 kHz subcarrier spacing (FR1/FR2)
    NR_SCS_120,     //!< 120 kHz subcarrier spacing (FR2)
    NR_SCS_240,     //!< 240 kHz subcarrier spacing (FR2)
    NR_SCS_CNT      //!< Total number of subcarrier spacings.
};

enum nr_pss_nid2_t {
    PSS_NID2_0      = 0,
    PSS_NID2_1      = 1,
    PSS_NID2_2      = 2,
    PSS_NUM_NID2    = 3
};

#define NR_CELLID(id1, id2)     ((3 * (id1)) + (id2))
#define NR_ID1(id)          (id/3)
#define NR_ID2(id)          (id%3)

#define NR_SSB_NFFT         256
#define NR_SSB_NRE          240
#define NR_SSB_KRE          ((NR_SSB_NFFT - NR_SSB_NRE) / 2)

#define NR_PSS_NFFT         256     //!< Number of points in FFT.
#define NR_PSS_NRE          127     //!< Number of PSS resource elements.
#define NR_PSS_KRE           56     //!< PSS resource element offset in frequency domain, from SSB RE 0.

#define NR_SSS_NFFT         NR_PSS_NFFT
#define NR_SSS_NRE          127
#define NR_SSS_KRE           56

 

//! @brief  Slotted SSB Measurement Structure,  messages from upper layer
struct slot_ssb_meas_t {
    //uint32_t report_id;     //!< SSB report-identifier loops back in report indication
    //uint32_t bitmap_td;     //!< time domain symbol bitmap of SSB start symbols (i.e. PSS symbols)
    uint32_t re_offset;     //!< offset to start of guard band adjacent to PSS/SSS

    int32_t Nid1;           //!< Targeted Cell ID for SSS detection = {0,...,335}
                            //!< or = -1 for blind exhaustive search of all 336 IDs
    int32_t Nid2;           //!< Targeted Cell ID for PSS detection = {0,1,2}
                            //!< or = -1 for blind exhaustive search of all 3 IDs
                            //!< cellId = 3*Nid1 + Nid2 = {0,...,1007}
    /*
    uint32_t ant_en_scs;    //!< ssb0_ant[4] | ssb1_ant[4] | ssb2_ant[4] | ssb3_ant[4] | rsvd[13] | scs[3]
                            //!< bit31:rx0_en = process/report RX ant0 for 1st SSB in bitmap (if==1)
                            //!< bit30:rx1_en = process/report RX ant1 for 1st SSB in bitmap (if==1)
                            //!< bit29:rx2_en = process/report RX ant1 for 1st SSB in bitmap (if==1)
                            //!< bit28:rx3_en = process/report RX ant3 for 1st SSB in bitmap (if==1)
                            //!< ssb1_ant[4] = bits[27:24] = rx ant enables for 2nd SSB in bitmap (@todo TBD TECH)
                            //!< ssb2_ant[4] = bits[23:20] = rx ant enables for 3rd SSB in bitmap (applicable only for FR2)
                            //!< ssb3_ant[4] = bits[19:16] = rx ant enables for 4th SSB in bitmap (applicable only for FR2)
                            //!< bits[15:3] = RESERVED
                            //!< bits[2:0] = sub-carrier spacing: {0: 15KHZ, 1: 30KHZ, 2: 60KHZ}
    */
    //uint32_t nsym_dl;                       //!< # symbols for cpe_dl_z_process()
    uint32_t scs_ssb;       //!< Sub-Carrier Spacing (SCS) for slotted SSB measurement.
    uint32_t ssb_sym;       //!< Slotted SSB: starting symbol of SSB(or PSS symbol id) , value 0~11
    //uint32_t ssb_map;
    uint32_t msmt_flag;
    //uint32_t fft_size;
    //uint32_t guard_size;    //!< Guard size in words(REs) - (fft_size - re_size)/2, floor aligned to AXI_BUS

} ;


// -----------------------------------------------------------------------------
//! @brief  L1SP->L1C: Slotted-SSB Detection Indication Definition
//! @note:  only L1SP DL-B core(s) send this
// -----------------------------------------------------------------------------
//! @brief  Slotted-SSB Indication Message Structure
struct slotted_ssb_det_t {
       //uint32_t msg_hdr;       //!< msgId[4] | seqId[8] | tStamp[20] (generic L1C<->L1SP msg header)
       //uint32_t report_id;     //!< SSB report-identifier looped back
       uint32_t cell_id;       //!< cell_id = detected Cell ID or L1C_V2A_CELL_ID_NO_SSB_DETECTED
       //uint32_t sym_cc_ant_scs; //!< sym_bit_map[16] | rsvd[6] | cc[3] | ant[4] | scs[3]
                                //!< bits[31:16] = sym_bit_map = TD bitmap of where SSB starts
                                //!< bits[15:10] = RESERVED
                                //!< bits[9:7] = cc = {0: cc0, ... 7: cc7}
                                //!< bit6 =ant[3]=1 if rx_ant0 used for this detect (echoed from L1C REQ)
                                //!< bit5 =ant[2]=1 if rx_ant1 used for this detect (echoed from L1C REQ)
                                //!< bit4 =ant[1]=1 if rx_ant2 used for this detect (echoed from L1C REQ)
                                //!< bit3 =ant[0]=1 if rx_ant3 used for this detect (echoed from L1C REQ)
                                //!< bits[2:0] = scs = {0: 15KHZ, 1: 30KHZ, 2: 60KHZ} (echoed from L1C REQ)
       int32_t time_ofst;      //!< RX sample counter value (61.44MSPS) of start of SSB
                               //!< a) cell search mode: relative to when RX_AXIQ_ALLOWED = 0->1
                               //!< b) slotted SSB mode: signed sample timing error (INT32_Q16_T)
       int32_t cfo;            //!< CFO = Carrier Frequency Offset estimate in NCO units interpreted
                               //!< @30.72MSPS: cfo (Hz) = (uint32_t cfo) * (245760000)/(2^32)
       int32_t ss_rssi_db;     //!< SS-RSSI in INT32_Q16_T;  // s iiiiiiiiiiiiiiii.fffffffffffffff
       int32_t ss_rsrp_db;     //!< SS-RSRP in INT32_Q16_T;  // s iiiiiiiiiiiiiiii.fffffffffffffff
       int32_t ss_rsrq_db;     //!< SS-RSRQ in INT32_Q16_T;  // s iiiiiiiiiiiiiiii.fffffffffffffff
       int32_t ss_sinr_db;     //!< SS-SINR in INT32_Q16_T;  // s iiiiiiiiiiiiiiii.fffffffffffffff
};
/*
struct nr_dl_z_scratch_b00_t {
    union{
    	cfloat32_t pss_eqw[NR_PSS_NFFT];					//2048B
        struct {        	
    		union{
				cfloat16_t pssRef256[NR_PSS_NFFT] ;			//1024B
				cfloat16_t sss_conv_out[NR_SSB_NFFT];		//1024B
    		};
    		union{
    			cfixed16_t symbol_aligned[NR_SSB_NFFT];		//1024B    			
				struct {
					
					//cfloat16_t pssRef256[NR_PSS_NFFT] ;
					//cfloat16_t cf16[NR_SSB_NFFT];
					fixed16_t sss_seq[128];
					char scratch_512B[512];
					//uint32_t u32[NR_SIZE_DFT_SCS30_CBW100];
					//cfixed16_t sss_chest[NR_SSS_NFFT];
					//cfloat32_t cf32[32];
				}buf_b;
    		};
        }buf_a;
    };

};
*/
//cfloat32_t tcm_pss_eqw[NR_PSS_NFFT] __align_vec __section(".tcm_data");					//2048B
struct tcm_ssb_buff_t {
	  	struct slotted_ssb_det_t ssb_det;
    	cfloat32_t pss_eqw[NR_PSS_NFFT] __align_vec;		//2048B
    	cfixed16_t pss_csi_out[NR_PSS_NFFT];				//1024B  
};

struct tcm_ssb_const_t {
		cfloat16_t pssRef256[NR_PSS_NFFT] ;			//1024B
	    uint16_t  SSSDET_REF_D1[ 18 ] __attribute__((aligned( 32 * UPHW )));    
	    uint16_t  SSSDET_REF_D0[ 72 ] __attribute__((aligned( 4 * UPHW )));  
	    uint32_t  ATAN2_COEFF_BUFF[2 * ATAN2_NUM_COEFF]; 
};


struct pss_sss_scratch_b0_t {
    union{
    	cfloat32_t pss_eqw[NR_PSS_NFFT];					//2048B

    	struct {
    			cfixed16_t pss_input_res[NR_SSB_NFFT];		//1024B  
    			cfloat16_t pssRef256[NR_PSS_NFFT] ;			//1024B
    		}pss_csi_in;
		struct {
    			cfloat16_t sss_conv_out[NR_SSB_NFFT];		//1024B	
    			fixed16_t scratch_512B[256];
				fixed16_t sss_seq[128];
			}sss_det_targeted;

    };  		
	struct slot_ssb_meas_t  ptcb_ssb __attribute__((aligned(2 * UPHW))) ;					//!< messages from upper layer  , 24B

};



struct pss_sss_scratch_b1_t {
    union{
        struct {
            cfixed16_t sss_input_res[NR_SSB_NFFT];		//1024B
            cfixed16_t pss_csi_out[NR_PSS_NFFT];		//1024B
			struct {
			    uint16_t  SSSDET_REF_D1[ 18 ] __attribute__((aligned( 32 * UPHW )));	// 36B    	
			    uint16_t  SSSDET_REF_D0[ 72 ] __attribute__((aligned( 4 * UPHW )));  	// 144B
			    uint32_t  ATAN2_COEFF_BUFF[2 * ATAN2_NUM_COEFF];						// 32B
				sssdet_param_T sssdet_param __attribute__((aligned(2 * UPHW))) ;
				}params;
        };
        struct slotted_ssb_det_t ssb_det;				//32B
    };
};


struct pss_sss_scratch_b2_t {
    union{
    	cfloat16_t sss_eq[NR_SSS_NFFT];
    	cfixed16_t sss_csi[NR_SSS_NFFT];
    };    
};

#if 0
/*
//! @brief  DL SCH RX Symbol Power Struct
struct rx_sym_pwr {
    uint32_t slot_num;
    uint32_t epre[CPE_MAX_NUM_CC][CPE_RX_PATH_CNT];
    uint8_t  pad[60];   // pad 
};
*/



/*
__attribute__((aligned(2 * UPHW))) sssdet_param_T sssdet_param = {
    .thre_sss = 0.05,
    .scaling = 0.0310058593750,
};


// SSB Detection Output Struct
//__attribute__((aligned(2 * UPHW))) ssdet_out_T ssdet_out;

// -----------------------------------------------------------------------------
//! @brief      SSS sequence in frequency domain, packed in bits. For SSS detector.
//!                 D0: 128x3x3 bits        = 144 bytes = 72 halfword buffer.
//!                 D1: (127+127+34) bits   =  36 bytes = 18 halfword buffer.
//!                 === Total size = 90 halfwords
//!                 D0 aligned to NAU*4/16 halfwords because 4hw=64bits are mapped to QPSK with ld.qam. D0 creates D0 sequences of SSS.
//!                 D1 aligned to 32 halfwords to ensure entire D1 is in one DMEM line. D1 creates D1 sequence of SSS.
// -----------------------------------------------------------------------------
__attribute__((aligned( 4 * UPHW )))
static volatile uint16_t  SSSDET_REF_D0[ 72 ] = {
    #include "SSS_sssseq_d0_128x3_bits.hex"
};

__attribute__((aligned( 32 * UPHW )))
static volatile uint16_t  SSSDET_REF_D1[ 18 ] = {
    #include "SSS_sssseq_d1_288_bits.hex"
};

// -----------------------------------------------------------------------------
//! @brief      Polynomial coefficient buffer used for the polynomial fitting 
//!             approximation of atan(x) in the interval x : [-1,1]. 
//! @attention  This buffer is NOT required to be vector aligned.
//! @note       Each coefficient value is duplicated in order to use the VSPA 
//!             S2mode = "S2i1r1i1r1" in order to load entire S2 register with 
//!             one coefficient.
// -----------------------------------------------------------------------------

const uint32_t ATAN2_COEFF_BUFF[2 * ATAN2_NUM_COEFF] = {
    #include "atan_coeff.txt"
};
#else
__attribute__((aligned(2 * UPHW))) sssdet_param_T sssdet_param ;

// -----------------------------------------------------------------------------
//! @brief      SSS sequence in frequency domain, packed in bits. For SSS detector.
//!                 D0: 128x3x3 bits        = 144 bytes = 72 halfword buffer.
//!                 D1: (127+127+34) bits   =  36 bytes = 18 halfword buffer.
//!                 === Total size = 90 halfwords
//!                 D0 aligned to NAU*4/16 halfwords because 4hw=64bits are mapped to QPSK with ld.qam. D0 creates D0 sequences of SSS.
//!                 D1 aligned to 32 halfwords to ensure entire D1 is in one DMEM line. D1 creates D1 sequence of SSS.
// -----------------------------------------------------------------------------
__attribute__((aligned( 4 * UPHW )))
uint16_t  SSSDET_REF_D0[ 72 ] ;

__attribute__((aligned( 32 * UPHW )))
uint16_t  SSSDET_REF_D1[ 18 ] ;

// -----------------------------------------------------------------------------
//! @brief      Polynomial coefficient buffer used for the polynomial fitting 
//!             approximation of atan(x) in the interval x : [-1,1]. 
//! @attention  This buffer is NOT required to be vector aligned.
//! @note       Each coefficient value is duplicated in order to use the VSPA 
//!             S2mode = "S2i1r1i1r1" in order to load entire S2 register with 
//!             one coefficient.
// -----------------------------------------------------------------------------

uint32_t ATAN2_COEFF_BUFF[2 * ATAN2_NUM_COEFF] ;

// SSB Detection Output Struct
//__attribute__((aligned(2 * UPHW))) ssdet_out_T ssdet_out;
*/
#endif

 
#endif
