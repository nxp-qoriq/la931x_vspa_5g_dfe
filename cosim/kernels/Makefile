# The DFE chain, one stage per ELF — the pieces the link-level co-simulation
# (cosim/scripts/dfe_cosim.py) runs one after another with a channel in between.
#
#   make -C cosim/kernels      -> cosim/kernels/build/compute_*.eld
#
# Every stage links against firmware/lib/lib_DFE_Ref_vspa2.elb, the SAME library
# the firmware links, so the co-simulation exercises the real iFFT, FFT, filters
# and windowing rather than a private copy of them. Nothing is vendored here:
# these .c files are only harnesses that call into that library.

REPO_ROOT     := $(abspath ../..)
VSPA_SDK      := $(REPO_ROOT)/submodules/la931x_vspa_common/vspa-sdk
VSPA_TOOLS    ?= $(REPO_ROOT)/environment/cwvspa.vbeta_14_00_781_vspa.linux/VSPA_Tools
VSPA_CC        = $(VSPA_TOOLS)/bin/fsvspacc

AU        ?= 16
BUILD      = $(CURDIR)/build
DFE_LIB    = $(REPO_ROOT)/firmware/lib/lib_DFE_Ref_vspa2.elb

# The QEC stage is the one kernel the DFE firmware library does not settle by
# itself: lib_DFE_Ref_vspa2.elb happens to carry a qec_opt.eln, but which of the
# .sx's four #if-selected bodies that archive member was assembled from is not
# something we can read off a binary. So compute_qec_td links the submodule
# ASSEMBLY SOURCE directly, with our own -DVSPA2 — the provenance is then on the
# link line. The submodule is NXP-authored and read-only; it is assembled where
# it lies, never copied.
QEC_SX     = $(REPO_ROOT)/submodules/la931x_vspa_common/vspa-lib/qec/src/qec_opt.sx

ARCH_FLAGS := -arch vspa2 -au_count $(AU) -core_type sp
DEFS       := -DVSPA2
OPT_FLAGS  := -g -O3 -Os -opt=noalias_by_type
MISC_FLAGS := -ansi off -mvcpu -msgstyle gcc -env $(VSPA_TOOLS)
INCS       := -isystem $(VSPA_TOOLS)/include -I include -I $(VSPA_SDK)/inc \
              -I $(REPO_ROOT)/firmware/lib
CFLAGS     := $(ARCH_FLAGS) $(DEFS) $(OPT_FLAGS) $(MISC_FLAGS) $(INCS)

LCF        := $(VSPA_TOOLS)/etc/vspa2_$(AU)au/lcf/vspa2_$(AU)au_sp.lcf
STARTUP    := $(VSPA_TOOLS)/lib/vspa2/startup_au$(AU)_sp.eln
LIBS       := $(VSPA_TOOLS)/lib/vspa2/libc_au$(AU)_sp.elb \
              $(VSPA_TOOLS)/lib/vspa2/librt_au$(AU)_sp.elb

# The BER co-simulation chain (+ qec_td, whose coefficients are a runtime port,
# not a compile-time constant — see compute_qec_td.c).
KERNELS   := dfe_tx dfe_rx x2_interp decim_2x_8tap qec_td

# Cell search / SSB (gaps 11 + 12). Same rule, same library — these link the
# real chEst / rcpv / vmult / sssdet / sssConvert / sss_msmts out of the DFE
# library rather than modelling them.
KERNELS   += ssb_chest sss_det pss_search

# The cell-search correlator (gap 11): the PSS search built as a real
# frequency-domain cross-correlation — fftDIF1024 -> chEst -> ifftDIF1024, all
# three from the same DFE library. ssb_fft windows and transforms the raw IQ
# stream; ssb_corr does the conjugate multiply and the inverse transform.
KERNELS   += ssb_fft ssb_corr

# SSB co-simulation (cosim/scripts/ssb_cosim.py): the same real iFFT/FFT the
# 61-subcarrier dfe_tx/dfe_rx stages use, on a 624-RE carrier wide enough to hold
# a 240-RE SSB, plus DMRS channel estimation and ZF equalisation of the PBCH on
# the real chEst / rcpv / vmult kernels.
KERNELS   += ssb_tx ssb_rx ssb_demod

TARGETS   := $(patsubst %,$(BUILD)/compute_%_vspa2_$(AU)au.eld,$(KERNELS))

all: $(TARGETS)

$(BUILD):
	mkdir -p $@

$(BUILD)/compute_%_vspa2_$(AU)au.eld: compute_%.c $(DFE_LIB) | $(BUILD)
	$(VSPA_CC) $(CFLAGS) -mem $(LCF) $(STARTUP) $(DFE_LIB) $< $(LIBS) -o $@

# QEC: the submodule .sx instead of the library (see QEC_SX above). Explicit rule,
# so it wins over the pattern rule.
$(BUILD)/compute_qec_td_vspa2_$(AU)au.eld: compute_qec_td.c $(QEC_SX) | $(BUILD)
	$(VSPA_CC) $(CFLAGS) -mem $(LCF) $(STARTUP) $< $(QEC_SX) $(LIBS) -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
