/* SPDX-License-Identifier: BSD-3-Clause
 * compute_ssb_fft.c — stage 1 of the cell-search correlator: window the raw IQ
 * stream and FFT each window, on the real library FFT.
 *
 * Part of gap 11 (cell search). See compute_ssb_corr.c for stage 2 and
 * cosim/scripts/cell_search_cosim.py for the host that drives both.
 *
 * WHY A SEPARATE ELF FROM THE CORRELATOR
 * --------------------------------------
 * The cross-correlation theorem gives the whole PSS search in three transforms:
 *
 *     corr = IDFT( DFT(rx_window) . conj(PSS_spectrum) )
 *
 * and every one of those three operations is a real kernel in the DFE library
 * (fftDIF1024 / chEst / ifftDIF1024). But the two FFTs are DECIMATION IN
 * FREQUENCY: natural order in, BIT-REVERSED order out. So chaining them needs a
 * bit-reversal in between, and the reversal is the problem:
 *
 *   - on the VCPU it is a 1024-iteration scalar loop. Measured cost of the ISS
 *     is ~50 us/phase, so a scalar pass over 1024 elements is ~10k phases ~ 0.5 s,
 *     PER window PER hypothesis. It would dominate everything.
 *   - the library's bitRev_* kernels are IPPU microcode, and the IPPU is a slave
 *     engine that only advances when explicitly stepped — the plain run-to-exit
 *     the kernel runner uses would never execute it.
 *
 * A bit-reversal is a pure index permutation: no arithmetic, no signal
 * processing. So it is done on the HOST, in numpy, between this ELF and
 * compute_ssb_corr.c — which costs nothing and keeps every arithmetic operation
 * on the real assembly. (The firmware does the same reordering with the IPPU;
 * main_test.c does it with a software sw_bitrev() and says so.)
 *
 * The FFT reads its window straight out of the stream through the circular
 * buffer arguments (pBuff / cbuffSize) rather than memcpy-ing it first. That is
 * exactly how DFE_func_rx.c drives it — it points the FFT at an offset into the
 * ADC's circular capture buffer — and it means no scalar copy loop here either.
 */
#include <stdint.h>

/* --- the real routine from lib/lib_DFE_Ref_vspa2.elb -------------------- */
extern void fftDIF1024_sc128_hfx_hfx_vspa2(void const *pIn, void *pOut,
                                           void const *pBuff,
                                           unsigned int cbuffSize);

#define NFFT        1024              /* OFDM FFT size: 1024 x 15 kHz = 15.36 MHz,
                                       * 1024 x 30 kHz = 30.72 MHz. Both vectors. */
#define STREAM_MAX  16384             /* complex samples; power of 2 for the
                                       * circular buffer */
#define NWIN_MAX    24                /* windows per call; the host chunks */

/* Circular buffer size is in HALF-WORDS, and one complex sample is two of them
 * (16-bit I + 16-bit Q). The app writes BYTE2UNIT(POINTS_FFT*4) = 2048 for a
 * 1024-point buffer; the same arithmetic gives 2*STREAM_MAX here. */
#define STREAM_HW   (2 * STREAM_MAX)

/* ---- host-written inputs ---------------------------------------------- */
static uint32_t stream[STREAM_MAX] __attribute__((aligned(128)));  /* cfixed16 */
static uint32_t ctrl[4]            __attribute__((aligned(128)));
/* ctrl[0] = n_win, ctrl[1] = step (samples), ctrl[2] = win0 (samples) */

/* ---- output the host reads -------------------------------------------- */
/* Bit-reversed frequency-domain windows. The host un-permutes them. */
static uint32_t fd_out[NWIN_MAX][NFFT] __attribute__((aligned(128)));

int main(void)
{
    uint32_t n_win = ctrl[0];
    uint32_t step  = ctrl[1];
    uint32_t win0  = ctrl[2];
    uint32_t w;

    if (n_win > NWIN_MAX)
        n_win = NWIN_MAX;

    for (w = 0; w < n_win; w++) {
        uint32_t start = win0 + w * step;
        /* REAL kernel. Out-of-place: reads the window from the stream (with
         * circular wraparound), writes the bit-reversed spectrum to fd_out[w],
         * leaving the stream intact for the next window. */
        fftDIF1024_sc128_hfx_hfx_vspa2(&stream[start], fd_out[w],
                                       stream, STREAM_HW);
    }

    return 0;
}
