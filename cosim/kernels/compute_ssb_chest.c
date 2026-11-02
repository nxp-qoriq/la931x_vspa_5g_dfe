/* SPDX-License-Identifier: BSD-3-Clause
 * compute_ssb_chest.c — SSB channel estimation + equalisation, as one kernel.
 *
 * Gap 12. This is the cell-tracking DSP chain of firmware/src/DFE_func_rx.c
 * (rx_sym_ready_processing(), the #if CELLTRACK_ENABLE block, lines ~610-735)
 * lifted out of its DMA/mailbox context and driven directly by the host:
 *
 *   PSS symbol:  chEst(pss_in, pss_csi, pss_ref, 8)          <- LS channel est
 *                rcpv (pss_eqw, pss_csi, 8)                  <- weights = 1/H
 *   SSS symbol:  vmult(sss_eq, sss_in, pss_eqw, 8)           <- equalise SSS
 *                chEst(sss_in, sss_csi, sss_ref, 8)          <- LS est on SSS
 *                sss_msmts(pss_csi, sss_csi, msmt)           <- RSSI/RSRP/RSRQ/SINR
 *                tim_off_est_sp_asm_rc(...)                  <- FD phase ramp
 *                fineCFO_rc(...)                             <- residual CFO
 *
 * Every one of those is the REAL routine from lib/lib_DFE_Ref_vspa2.elb. This
 * file contains no signal processing of its own — it is wiring.
 *
 * UNITS, because they are the easy thing to get wrong (all three N/L args):
 *   N is a count of DMEM LINES, not samples and not halfwords. One line is
 *   NLANE32 = 2*__AU_COUNT__ = 32 complex-16 elements = 128 bytes. The 256-point
 *   SSB grid is therefore N = 8, which is what the firmware passes both as
 *   NVEC32(NR_SSB_NFFT) and, at the SSS call site, as the literal NR_SSB_NFFT/32.
 *
 * TYPES, likewise (chEst is deliberately asymmetric and the header does not
 * shout about it):
 *   pIn / pChEst  cfixed16_t — complex half-FIXED, sign-magnitude    (sm16_complex)
 *   pPilot        cfloat16_t — complex half-FLOAT, a pair of IEEE fp16 (chf_complex)
 *   rcpv output   cfloat32_t — TWO fp32 words per element             (csp_complex)
 * Buffers are declared uint32_t here for exactly the reason compute_dfe_rx.c
 * does it: the host writes DMEM words, and the VSPA complex types are not
 * self-contained outside the firmware's own include set.
 *
 * chEst's doc comment describes 802.11ac VHT-LTF. Ignore it — the kernel is a
 * generic least-squares estimator (elementwise conj-multiply by the reference)
 * and NXP reuses it verbatim for the NR SSB, with PSS as the pilot on symbol 0
 * and the reconstructed SSS as the pilot on symbol 2. There is no DMRS here.
 */
#include <stdint.h>

/* --- real routines from lib/lib_DFE_Ref_vspa2.elb ----------------------- */
extern void chEst(void const *pIn, void *pChEst, void const *pPilot,
                  unsigned int N);
extern void csp_chf_rcpv_asm(void *py, void *px, unsigned int L);
extern void chp_chf_csp_vmult_asm(void *py, void *px1, void *px2,
                                  unsigned int L);
extern void sss_msmts(void const *h_pss, void const *h_sss, void *pOut);
extern int tim_off_est_sp_asm_rc(int32_t *null, void *in_p, void *scratch_p,
                                 uint32_t atan2_coeff_addr);
extern int32_t fineCFO_rc(void *hPSS, void *hSSS, void *cfoScratchBuff,
                          uint32_t atan2_coeff_addr);

#define NFFT     256          /* NR_SSB_NFFT */
#define NLINES   (NFFT / 32)  /* = 8 DMEM lines */

/* ---- host-written inputs ---------------------------------------------- */
static uint32_t pss_in[NFFT]   __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t pss_ref[NFFT]  __attribute__((aligned(128)));  /* cfloat16 */
static uint32_t sss_in[NFFT]   __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t sss_ref[NFFT]  __attribute__((aligned(128)));  /* cfloat16 */

/* ATAN2_COEFF_BUFF: 2*ATAN2_NUM_COEFF words, each coefficient DUPLICATED so the
 * AU can load a whole S2 register with one coeff (S2mode = S2i1r1i1r1). The app
 * DMAs this in from the host; so do we. Values in atan/src/atan_coeff.txt. */
static uint32_t atan2_coeff[8] __attribute__((aligned(128)));

/* ---- outputs the host reads ------------------------------------------- */
static uint32_t pss_csi[NFFT]     __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t pss_eqw[2 * NFFT] __attribute__((aligned(128)));  /* cfloat32! */
static uint32_t sss_eq[NFFT]      __attribute__((aligned(128)));  /* cfloat16 */
static uint32_t sss_csi[NFFT]     __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t msmt[4]           __attribute__((aligned(128)));  /* sss_msmt_T */
static uint32_t est[4]            __attribute__((aligned(128)));  /* see below  */

/* tim_off_est wants 2 DMEM lines of scratch, fineCFO_rc wants 1. 512 B covers
 * both, and the app shares one buffer between them for the same reason. */
static uint32_t scratch[128] __attribute__((aligned(128)));

int main(void)
{
    /* --- PSS symbol ---------------------------------------------------- */
    chEst(pss_in, pss_csi, pss_ref, NLINES);
    csp_chf_rcpv_asm(pss_eqw, pss_csi, NLINES);

    /* --- SSS symbol ---------------------------------------------------- */
    /* Note the cross-symbol structure: the weights come from PSS and are
     * applied to SSS. Not self-equalisation. */
    chp_chf_csp_vmult_asm(sss_eq, sss_in, pss_eqw, NLINES);
    chEst(sss_in, sss_csi, sss_ref, NLINES);

    /* --- measurements --------------------------------------------------- */
    sss_msmts(pss_csi, sss_csi, msmt);

    est[0] = (uint32_t)tim_off_est_sp_asm_rc(0, sss_csi, scratch,
                                             (uint32_t)atan2_coeff);
    est[1] = (uint32_t)fineCFO_rc(pss_csi, sss_csi, scratch,
                                  (uint32_t)atan2_coeff);
    est[2] = 0;
    est[3] = 0;

    return 0;   /* `done`: the host reads pss_csi / pss_eqw / sss_eq / sss_csi */
}
