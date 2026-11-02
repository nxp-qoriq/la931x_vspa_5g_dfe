/* SPDX-License-Identifier: BSD-3-Clause
 * compute_sss_det.c — blind SSS / N_id_1 search, on the real detector.
 *
 * Gap 11, SSS half. Drives sssdet_targeted_hpgemv_rc_v1() and sssConvert(),
 * both from lib/lib_DFE_Ref_vspa2.elb. No signal processing lives here.
 *
 * WHY A SWEEP, AND WHY THAT IS NOT CHEATING
 * -----------------------------------------
 * The library's only detector is TARGETED: id1 and id2 are INPUTS, and the
 * return value is 0/1 — a yes/no verdict on a cell ID you already guessed. The
 * disassembled tail says so plainly:
 *
 *     cmp g2, g4;  jmp.lt (NO_SSS);  mvS.z g0,+$0;
 *     YES_SSS:     mvS.z g0,+$1;
 *     NO_SSS:      fnop; rts;
 *
 * i.e. g0 = (metric >= thre_sss). It returns no peak value, no index, no N_id.
 * That is all the shipping firmware ever needs, because it does cell TRACKING:
 * DFE_func_rx.c reads Nid1/Nid2 straight out of the host's config word
 * (GET_CELLTRACK_NID1/NID2) and only asks the kernel to confirm them.
 *
 * A blind search is still reachable with the REAL kernel, by using it as the
 * scoring function it already is: call it once per id1 hypothesis, 336 times,
 * and see which hypotheses it accepts. Every correlation is done by the shipped
 * assembly; the only thing this file adds is the loop.
 *
 * RECOVERING A SCORE FROM A KERNEL THAT ONLY RETURNS A BIT
 * -------------------------------------------------------
 * det(t) == 1  <=>  metric >= t, and t (thre_sss) is an input. The metric is
 * therefore  sup{ t : det(t) == 1 }, and a bisection on t recovers it to any
 * precision wanted — without reimplementing one line of the correlation. That
 * is what score[] holds. It turns a verifier into a ranker, so the sweep yields
 * an argmax rather than merely a set of accepted IDs.
 *
 * THE SEQUENCE OUTPUT IS THE STRONGER PROOF
 * -----------------------------------------
 * The detector also *emits* pSeq: the 127 ±1 SSS chips of the (id1,id2) it was
 * asked about, decoded out of the D0/D1 bit tables with ld.qam. That is a much
 * sharper test than the 0/1 flag — it must match 3GPP 38.211 §7.4.2.3 for every
 * one of the 336 id1, and it cannot come out right if the tables, the packing,
 * or the kernel are wrong. seq_sign32[] captures the first 32 chips' signs for
 * every id1 in the sweep, so one run checks all 336 sequences.
 *
 * BUFFERS (ssdet.h; alignments are hardware requirements, not style)
 *   px       127 candidate SSS REs, cfloat16_t, linear, NOT required aligned.
 *            The app passes sss_eq + SLOTTED_PSS_SSS_OFST, and 64 is where the
 *            SSS starts in the 256-pt grid: NR_SSB_KRE(8) + NR_SSS_KRE(56).
 *   pScrBuf  512 bytes = 256 halfwords = 128 words. VECTOR-ALIGNED. (The kernel
 *            expands D1 into it with stA.laddr and re-reads it cyclically.)
 *   pSeq     128 half-fixed words = 256 B = 64 words. VECTOR-ALIGNED.
 *   D0       72 uint16, aligned 4 halfwords (ld.qam eats 4hw = 64 bits at once)
 *   D1       18 uint16, aligned 32 halfwords so the whole table is in ONE line
 * D0/D1/param are all over-aligned to 128 here, which satisfies every one of
 * those and costs nothing.
 *
 * The D0/D1 tables are NOT in the firmware or the library — the app DMAs them
 * from a host buffer (celltrack_extbuf) and the .hex files the headers refer to
 * do not exist in this repo. The host generates them; see cosim/vspa_model/nr5g.py.
 */
#include <stdint.h>

/* --- real routines from lib/lib_DFE_Ref_vspa2.elb ----------------------- */
extern uint32_t sssdet_targeted_hpgemv_rc_v1(void *px, uint32_t *pScrBuf,
                                             void *pSeq,
                                             uint32_t id1, uint32_t id2,
                                             uint32_t sssdet_param_addr,
                                             uint32_t d0_addr, uint32_t d1_addr);
extern void sssConvert(void *py, void *px);

#define NFFT        256
#define PSS_SSS_OFS 64     /* SLOTTED_PSS_SSS_OFST */
#define N_ID1       336
#define MAX_BISECT  30

/* ---- host-written inputs ---------------------------------------------- */
static uint32_t sss_eq[NFFT]   __attribute__((aligned(128)));  /* cfloat16 */
static uint32_t d0_tab[36]     __attribute__((aligned(128)));  /* 72 x uint16 */
static uint32_t d1_tab[9]      __attribute__((aligned(128)));  /* 18 x uint16 */
static uint32_t param[2]       __attribute__((aligned(128)));  /* {float thre_sss; float scaling;} */
static uint32_t ctrl[4]        __attribute__((aligned(128)));

/*  ctrl[0] = id2         — the N_id_2 hypothesis (from the PSS stage)
 *  ctrl[1] = id1_probe   — whose full sequence is captured into seq_out
 *  ctrl[2] = n_bisect    — bisection steps for score[]; 0 disables (fast path)
 *  ctrl[3] = unused
 */

/* ---- scratch the kernel owns ------------------------------------------ */
static uint32_t scratch512[128] __attribute__((aligned(128)));  /* 512 B */
static uint32_t sss_seq[64]     __attribute__((aligned(128)));  /* 128 fixed16 */

/* ---- outputs the host reads ------------------------------------------- */
static uint32_t det_flag[N_ID1];                 /* 0/1 at the host's thre_sss */
static uint32_t score[N_ID1];                    /* metric, float32 bits        */
static uint32_t seq_sign32[N_ID1];               /* first 32 chips' signs       */
static uint32_t seq_out[64]      __attribute__((aligned(128)));  /* pSeq @id1_probe  */
static uint32_t sss_conv_out[NFFT] __attribute__((aligned(128)));/* cfloat16 pilot   */

static uint32_t f2b(float f) { union { uint32_t u; float f; } c; c.f = f; return c.u; }
static float    b2f(uint32_t u) { union { uint32_t u; float f; } c; c.u = u; return c.f; }

/* One detector call at threshold t. Everything else is held fixed. */
static uint32_t det_at(uint32_t id1, uint32_t id2, float t)
{
    param[0] = f2b(t);
    return sssdet_targeted_hpgemv_rc_v1(&sss_eq[PSS_SSS_OFS], scratch512,
                                        sss_seq, id1, id2,
                                        (uint32_t)param,
                                        (uint32_t)d0_tab,
                                        (uint32_t)d1_tab);
}

int main(void)
{
    uint32_t id2      = ctrl[0];
    uint32_t id1_prb  = ctrl[1];
    uint32_t n_bisect = ctrl[2];
    float    thre     = b2f(param[0]);
    uint32_t id1, n;
    int b;

    if (n_bisect > MAX_BISECT)
        n_bisect = MAX_BISECT;

    for (id1 = 0; id1 < N_ID1; id1++) {
        /* (a) the verdict, at the threshold the host asked for */
        det_flag[id1] = det_at(id1, id2, thre);

        /* (b) the emitted SSS sequence — signs of the first 32 chips.
         * sss_seq is 128 half-fixed words: chip n is the low halfword of
         * word n/2 when n is even, the high halfword when odd. Sign-magnitude,
         * so the sign IS bit 15 (this is not two's complement). */
        {
            uint32_t m = 0;
            for (n = 0; n < 32; n++) {
                uint32_t w  = sss_seq[n >> 1];
                uint32_t hw = (n & 1u) ? (w >> 16) : (w & 0xFFFFu);
                if (hw & 0x8000u)
                    m |= (1u << n);
            }
            seq_sign32[id1] = m;
        }

        /* (c) the metric, by bisecting the threshold this kernel compares against */
        if (n_bisect) {
            float lo = 0.0f, hi = 1.0f;
            /* bracket first: grow hi until the kernel stops accepting */
            for (b = 0; b < 24 && det_at(id1, id2, hi); b++) {
                lo = hi;
                hi *= 2.0f;
            }
            for (b = 0; b < (int)n_bisect; b++) {
                float mid = 0.5f * (lo + hi);
                if (det_at(id1, id2, mid))
                    lo = mid;
                else
                    hi = mid;
            }
            score[id1] = f2b(lo);
        } else {
            score[id1] = 0;
        }
    }

    /* Capture the full sequence for one chosen id1, and run it through the real
     * sssConvert() — the padded cfloat16 reference that chEst() then uses as its
     * pilot on the SSS symbol. Per its header: out[0:63] = 1+j0,
     * out[64:190] = 64 * SSS, out[191:255] = 1+j0. */
    det_at(id1_prb, id2, thre);
    for (n = 0; n < 64; n++)
        seq_out[n] = sss_seq[n];

    sssConvert(sss_conv_out, sss_seq);

    return 0;
}
