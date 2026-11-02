// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP

#ifndef __SSDET__
#define __SSDET__

#include "sss_msmts.h"

#define SSB_BLIND_MODE  -1
#define PSS_NUM_ID2     3


//! @brief      PSS detection parameters
typedef struct {
    float fir2h[3];             //!< 3-tap FIR2 filter coefficients
    float thre_pss;             //!< threshold for pss detection
    uint32_t nco5x1[5];         //!< negative of nco freqs of 5 freq hypothesis
} pssdet_param_T;

//! @brief      SSS detection parameters
typedef struct {
    float thre_sss;             //!< threshold for sss detection
    float scaling;        //!< scaling to compensate 1/8-scaling from ld.qam and 127-scaling from rssi_sss = 127 * [(1/8)^4] = 0.0310058593750
} sssdet_param_T;


//! @brief      PSS detection peak buffer
typedef struct {
    float peak_val[15];     //!< value of the peak (from pss). First 15 are valid; last entry must be zero to ensure correct peak search
    uint32_t rsvd0;         //!< reserved. Being non-zero won't affect 15-pt peak search.
    uint32_t peak_idx[15];  //!< index of the peak. 0-based.
    uint32_t rsvd1;         //!< reserved. Can be used as a counter: Reset to zero when a new batch of samples arrive. {0..14}
} pssdet_peaks_T;

//! @brief      SSB detection output struct: In the order of occurrence
typedef struct {

    // Below: PSS stuff
    float peak_val_pss;             //!< peak normalized xcorr_squared ("xcn" from pss)
    cfixed16_t* ptrPssData_1536;    //!< ptr to 1st sample of PSS data portion @15.36msps =
                                    //!< pss15[9] of total pss15[137] symbol
    uint32_t id2;                   //!< physical cell id: ID2 = {0,1,2}
    int nco_freqhypo;               //!< NCO value of CFO frequency hypothesis from cold PSS search
    cfloat32_t atan2_pss_in;        //!< input to PSS atan2
    cfixed16_t* ptrPssData_3072;    //!< ptr to <Ofst30>-th sample of PSS[0:273] symbol @30.72msps.
                                    //! "Ofst30" macro is defined in "pssdet_xcorr_fft_asm.sx":
                                    //! At OFST30=8, this points to pss30[8], i.e. middle of 18-pt CP.
                                    //! At OFST30=16, this points to pss30[16], i.e. first sample of data part
    int nco_psscfo;                 //!< NCO value of PSS-based coarse CFO estimate @30.72 MSPS.
    float rssi_pss;                 //!< Moving sum of squares of most recent 128 samples.

    // SSS stuff
    float rssi_sss;                 //!< Sum of squares of equalized SSS REs (127x1 vector)
    float peak_val_sss;             //!< value of the peak (from sss)
    uint32_t id1;                   //!< physical cell id: ID1 = {0,1,...,335}
    uint32_t cellid;                //!< physical cell id: ID1*336+ID2
    cfloat32_t atan2_sss_in;        //!< input to SSS atan2
    int nco_ssscfo;                 //!< NCO value = coarse PSS + fine PSS/SSS CFO estimates
    sss_msmt_T ss_msmt;             //!< SSS measurements = RSRP, RSRQ, RSSI, SINR
} ssdet_out_T;

/*
//! @brief      PSS initialization parameters.
typedef struct {
    cfixed16_t  *px30_cbuf_beg;       //!< 30.72 MSPS circular buffer base address.
    size_t      px30_cbuf_size;       //!< 30.72 MSPS circular buffer size.
    uint32_t    *pscr_buf;            //!< pointer to scratch buffer.
    uint32_t    isInitBuffer;         //!< flag for buffer initialized.
    ssdet_out_T *pOutStruct;          //!< SS detected parameters
    float       pssThresh;            //!< PSS thresholds, L1C/L1SP-Int can change
} pss_init_params_t;
*/
// =============================================== //
// Common buffers
//#define SZ_X15_WORD (512+32)
//#define SZ_X15_UNIT ( SZ_X15_WORD * 2 * UPHW )
//#define LENGTH_DECIM2X (32)

//extern cfixed16_t PSSDET_X15[SZ_X15_WORD] __align_vec;

//extern const union PSSDET_REF_XCORRFFT_U    PSSDET_REF_XCORRFFT __align_vec;
//extern const union PSSDET_REF_PSSSEQTIME_U  PSSDET_REF_PSSSEQTIME __align_vec;
//extern struct pss_sss_scratch_b0_t pss_sss_temp_b0  __align_vec ;
//__attribute__((aligned(2))) extern sssdet_param_T sssdet_param;

///////////////////////////////////////////////////////////////////////////////////////////////////
//! @brief      Targeted (NcellID-specific) SSS Detection for input 127x1 RE candidate
//!             This function is called after targeted PSS detection in the same SSB.
//!
//! @param[in]  px              Pointer to potential SSS: 127x1 vector (linear buffer, non-aligned)
//!                             "sssdet_targeted_hpgemv_v1": In complex 16-bit floating-pt half-prec.
//! @param[in]  pScrBuf         SSS detector scratch buffer. Size = 512 bytes = 256 half words.  Vector-aligned.
//! @param[out] pSeq            SSS sequence corresponding to targeted cell id is output from this kernel on to
//!                             buffer passed to the kernel. Size of buffer = 128 Half fixed words. 127 SSS half
//!                             fixed real sequence with one zero padding at the end is returned.  The buffer must be
//!                             vector-aligned.
//! @param[in]  id1             Cell ID1 reported by the previous Blind SSS detector. Range: {0,1,..335}, (id1 of Targeted cell ID)
//! @param[in]  id2             Cell ID2 reported by the PSS detector. Range: {0,1,2}, id2 of Targeted cell ID).
//! @return     is_sss_det      = 0: targeted NcellID SSS NOT detected
//!                             = 1: targeted NcellID SSS detected
//!                             (peak_val_sss, rssi_sss, are updated in *pOutStruct)
///////////////////////////////////////////////////////////////////////////////////////////////////
extern uint32_t sssdet_targeted_hpgemv_rc_v1( 	cfloat16_t *px, 
												uint32_t *pScrBuf, 
												fixed16_t *pSeq, 
												uint32_t id1, 
												uint32_t id2,
												uint32_t sssdet_param_addr,
												uint32_t SSSDET_REF_D0_addr,
												uint32_t SSSDET_REF_D1_addr);

#endif // __SSDET__



