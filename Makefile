# DFE LA9310 — the firmware, and the co-simulation that tests it.
#
#   make                 build the firmware            -> firmware/eld/*.eld
#   make kernels         build the per-stage kernels   -> cosim/kernels/build/*.eld
#   make verify          tiers 1-5   (is the waveform right?)
#   make verify-full     tiers 1-9   (+ RX, downstream TX, silicon anchor, kernels)
#   make cosim           the link-level BER-vs-SNR sweep
#
# Prerequisites: ./setup.sh, then ./cosim/scripts/fetch_fr1fr2.sh for NXP's vectors.

APP_NAME := dfe-la9310-1t2r-25m30k
ELD_MAIN := firmware/eld/$(APP_NAME).eld

# NXP's own reference image and test vectors — fetched, never vendored.
NXP_REF  := cosim/nxp/fr1_fr2_test_tool/MEvspa_images_LS.1T2R_25M_30K_61_61_TDDFDD_LA93/geul-vspa0.eld
VEC      := cosim/nxp/fr1_fr2_test_tool/test_vectors/TM3.3_10MHz_15kHz_FDD.bin

.DEFAULT_GOAL := firmware
.PHONY: firmware kernels clean verify verify-full cosim

# ── the classical VSPA part ───────────────────────────────────────────────────
firmware:
	$(MAKE) -C firmware

# ── the simulation ────────────────────────────────────────────────────────────
# One ELF per DSP stage, each linking the same library the firmware links.
kernels:
	$(MAKE) -C cosim/kernels

clean:
	$(MAKE) -C firmware clean
	$(MAKE) -C cosim/kernels clean

# ── verification tiers (see docs/TESTING.md) ─────────────────────────────────
verify:            # tiers 1-5: vectors, IPPU, differential A/B, negative control, absolute
	python3 cosim/tests/test_fr1fr2_vectors.py
	python3 cosim/tests/test_ippu_native.py
	python3 cosim/scripts/dfe_ab_compare.py --old $(NXP_REF) --new $(ELD_MAIN) \
	        --vector $(VEC) --scs 15 --bw 10 --symbols 2
	python3 cosim/scripts/dfe_absolute_check.py --eld $(ELD_MAIN) --symbols 2

verify-full: verify   # + tiers 6-9: RX chain, downstream TX, silicon anchor, kernels
	python3 cosim/scripts/dfe_rx_probe.py
	python3 cosim/scripts/dfe_tx_downstream_probe.py
	python3 cosim/tools/dfe_md5_anchor/run_md5_anchor.py
	python3 cosim/scripts/dfe_ab_matrix.py --symbols 3
	$(MAKE) kernels
	python3 cosim/scripts/kernel_check.py

# The link-level co-simulation: TX -> AWGN -> RX, every DSP stage on the
# simulator, plotted as BER vs SNR. Separate from `verify` because it is a
# statistical measurement, not a pass/fail assertion (~2 min).
cosim: kernels
	python3 cosim/scripts/kernel_check.py
	python3 cosim/scripts/dfe_cosim.py --symbols 12
