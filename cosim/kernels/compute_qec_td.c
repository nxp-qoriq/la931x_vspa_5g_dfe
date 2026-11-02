/* SPDX-License-Identifier: BSD-3-Clause
 * compute_qec_td.c — time-domain I/Q-imbalance correction (QEC), as a kernel.
 *
 * The receive-side quadrature-error corrector: it undoes the gain/phase mismatch
 * between the I and Q branches of an analogue downconverter. Runs on the 1x-rate
 * payload, after CP removal and before the RX FFT.
 *
 * The arithmetic is not ours. This is a harness around the real NXP vspa-lib
 * assembly, assembled straight out of the (read-only) submodule:
 *
 *     submodules/la931x_vspa_common/vspa-lib/qec/src/qec_opt.sx
 *
 * with -DVSPA2, which selects the "2nd round optimization" body — the last of
 * the four #if-guarded variants in that file.
 *
 *   qec_opt_asm(output, input, num_samples, qec_para)
 *     output/input  cfixed16_t*, 128-byte aligned, may alias
 *     num_samples   rounded up to a multiple of 32 internally — see N_SAMPLES
 *     qec_para      qec_params_opt_t* — ONE 128-byte vector line, 32 float32
 *
 * Per complex sample z = I + jQ, with pad0 = 0, the kernel computes
 *
 *     out_I = f1 * I            + dcoff_I
 *     out_Q = f2 * I + f4 * Q   + dcoff_Q
 *
 * PARAMETER LINE (qec/inc/qec_opt.h, float32 word index):
 *
 *     0  f1                     6  f1_backup        12..31  pad — keep 0
 *     1  f4                     7  f4_backup
 *     2  pad0  — MUST be 0      8  pad1  — MUST be 0
 *     3  f2                     9  f2_backup
 *     4  dcoff_I               10  dcoff_I_backup
 *     5  dcoff_Q               11  dcoff_Q_backup
 *
 * The samples are half-fixed (SM16), the coefficients float32, and the DC terms
 * are float32 *in the half-fixed domain*: a DC of d SM16 counts is d / 32768.
 *
 * WHY THE COEFFICIENTS ARE A PORT AND NOT A CONSTANT
 *     The correction coefficients depend on the imbalance being corrected, so in
 *     a sweep they change from run to run. The old co-simulation #included them
 *     from a generated params.hex, which meant every parameter change was a
 *     REBUILD of this ELF — and, because that file was shared with the standalone
 *     regression, a build-order hazard as well.
 *
 *     Here they are just another input buffer: the host writes qec_params at the
 *     breakpoint on main(), exactly as it writes the samples. One ELF, built once,
 *     serves every (f1, f4, f2) the sweep asks for. Do NOT re-add a static
 *     initialiser copy into qec_params — that would overwrite what the host wrote.
 *
 * The host writes input_td + qec_params and reads output_td directly out of DMEM,
 * by symbol.
 */
#include <stdint.h>

extern void qec_opt_asm(void *output, void *input, unsigned int num_samples,
                        void *qec_para);

/* N_SAMPLES must be a multiple of 32: the asm does (num_samples + 31) >> 5 to get
 * a line count and then writes THAT many lines — a non-multiple makes it run past
 * the end of output_td. 1024 = 32 * 32. */
#define N_SAMPLES    1024            /* one OFDM symbol, 1x rate, CP removed */
#define N_PARAM      32              /* qec_params_opt_t = 32 x float32 = 1 line */

/* All three are .bss: crt0 zeroes them before main, then the host pokes the two
 * inputs at the main breakpoint. A zeroed qec_params is the null correction's
 * degenerate case (f1 = f4 = 0), NOT the identity — so if the host forgets to
 * write them the output is zeros, which is loud rather than plausible. */
static uint32_t input_td[N_SAMPLES]   __attribute__((aligned(128)));
static uint32_t output_td[N_SAMPLES]  __attribute__((aligned(128)));
static uint32_t qec_params[N_PARAM]   __attribute__((aligned(128)));

int main(void)
{
    qec_opt_asm(output_td, input_td, (unsigned int)N_SAMPLES, qec_params);

    return 0;                    /* `done`: the host reads output_td */
}
