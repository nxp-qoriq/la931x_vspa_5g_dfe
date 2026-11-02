/* SPDX-License-Identifier: BSD-3-Clause
 * compute_ssb_demod.c — DMRS channel estimation + zero-forcing equalisation of
 * the PBCH, on the real library kernels.
 *
 * This is the demodulation half of gap 12. compute_ssb_chest.c already runs the
 * firmware's own cell-TRACKING chain (chEst against PSS, weights from PSS,
 * applied to SSS). This kernel answers the other question — *once the cell is
 * found, can the SSB actually be demodulated through a channel?* — and for that
 * the pilot has to be the PBCH DMRS, and the equaliser has to be applied to the
 * PBCH-carrying symbols (1, 2 and 3), not to the SSS.
 *
 *   chEst(rx_sym1, h1_raw, dmrs1, 8)            <- LS estimate, h = y*conj(d)
 *   chEst(rx_sym3, h3_raw, dmrs3, 8)
 *   csp_chf_rcpv_asm(eqwN, hN_in, 8)            <- ZF weights = 1/H
 *   chp_chf_csp_vmult_asm(eqN, rx_symN, eqwN, 8) <- equalise
 *
 * All four are the REAL routines from lib/lib_DFE_Ref_vspa2.elb — the same ones
 * the firmware calls in DFE_func_rx.c. There is no signal processing in this
 * file; it is wiring. In particular there is no equaliser "routine" in the
 * library and none is invented here: zero forcing is composed from rcpv and
 * vmult exactly the way DFE_func_rx.c composes it.
 *
 * WHY hN_in IS AN INPUT AND NOT JUST h1_raw
 *   The DMRS lives on every 4th RE (k = 4n + NcellID%4). chEst is elementwise, so
 *   h*_raw is only meaningful ON those REs — on the 3 REs in between it has
 *   multiplied the RX by the pilot's 1+0j filler and handed back the PBCH data
 *   itself. The estimate therefore has to be interpolated across the comb before
 *   it can be inverted, and there is NO interpolation or smoothing kernel in the
 *   library to do it with: `ssbSmooth` is declared in the NXP headers but is not
 *   exported by lib_DFE_Ref_vspa2.elb, and consistently its only call site in
 *   DFE_func_rx.c is commented out. So the comb interpolation is host-side and
 *   is labelled as such in cosim/scripts/ssb_cosim.py — it is the one modelled
 *   step in this chain. The host writes the interpolated estimate back into
 *   h1_in/h2_in/h3_in and the real rcpv inverts it.
 *
 *   Consequence for the caller: the kernel is run TWICE per SSB. Pass 1 reads
 *   h*_raw (h*_in is a don't-care — pass 1+0j, not 0, so rcpv is not asked to
 *   divide by zero). Pass 2 supplies the interpolated h*_in and reads eq1..eq3.
 *
 * UNITS AND TYPES — the two things that are silently wrong if you guess:
 *   N/L = 8 is a count of DMEM LINES (32 complex-16 elements each), not samples.
 *   rx/h  cfixed16_t — complex half-FIXED, sign-magnitude      (sm16_complex)
 *   dmrs  cfloat16_t — complex half-FLOAT, a pair of IEEE fp16 (chf_complex)
 *   eqw   cfloat32_t — TWO fp32 words per element              (csp_complex)
 *   eq    cfloat16_t
 *
 * chEst's doc comment describes 802.11ac VHT-LTF. Ignore it: the kernel is a
 * generic least-squares estimator, h = y * conj(pilot), elementwise — verified
 * here with a genuinely COMPLEX pilot, which the SSS/PSS references (real +/-1,
 * where conj() is a no-op) could never have told us apart from conj(y)*pilot.
 */
#include <stdint.h>

/* --- real routines from lib/lib_DFE_Ref_vspa2.elb ----------------------- */
extern void chEst(void const *pIn, void *pChEst, void const *pPilot,
                  unsigned int N);
extern void csp_chf_rcpv_asm(void *py, void *px, unsigned int L);
extern void chp_chf_csp_vmult_asm(void *py, void *px1, void *px2,
                                  unsigned int L);

#define NFFT     256          /* NR_SSB_NFFT — the 256-pt window round the SSB */
#define NLINES   (NFFT / 32)  /* = 8 DMEM lines */

/* ---- host-written inputs ---------------------------------------------- */
static uint32_t rx_sym1[NFFT] __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t rx_sym2[NFFT] __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t rx_sym3[NFFT] __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t dmrs1[NFFT]   __attribute__((aligned(128)));  /* cfloat16 */
static uint32_t dmrs3[NFFT]   __attribute__((aligned(128)));  /* cfloat16 */
static uint32_t h1_in[NFFT]   __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t h2_in[NFFT]   __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t h3_in[NFFT]   __attribute__((aligned(128)));  /* cfixed16 */

/* ---- outputs the host reads ------------------------------------------- */
static uint32_t h1_raw[NFFT]     __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t h3_raw[NFFT]     __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t eqw1[2 * NFFT]   __attribute__((aligned(128)));  /* cfloat32! */
static uint32_t eqw2[2 * NFFT]   __attribute__((aligned(128)));
static uint32_t eqw3[2 * NFFT]   __attribute__((aligned(128)));
static uint32_t eq1[NFFT]        __attribute__((aligned(128)));  /* cfloat16 */
static uint32_t eq2[NFFT]        __attribute__((aligned(128)));
static uint32_t eq3[NFFT]        __attribute__((aligned(128)));

int main(void)
{
    /* --- least-squares channel estimate on the two DMRS-bearing symbols --- */
    chEst(rx_sym1, h1_raw, dmrs1, NLINES);
    chEst(rx_sym3, h3_raw, dmrs3, NLINES);

    /* --- zero forcing: weights = 1/H, then apply ------------------------- */
    csp_chf_rcpv_asm(eqw1, h1_in, NLINES);
    csp_chf_rcpv_asm(eqw2, h2_in, NLINES);
    csp_chf_rcpv_asm(eqw3, h3_in, NLINES);

    chp_chf_csp_vmult_asm(eq1, rx_sym1, eqw1, NLINES);
    chp_chf_csp_vmult_asm(eq2, rx_sym2, eqw2, NLINES);
    chp_chf_csp_vmult_asm(eq3, rx_sym3, eqw3, NLINES);

    return 0;   /* `done`: the host reads h1_raw/h3_raw (pass 1), eq1..3 (pass 2) */
}
