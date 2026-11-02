# The DFE application — LA9310 / VSPA2  (1 core, 1T2R, 25 MHz / 30 kHz SCS)
# Configuration: BANDWIDTH_25M_30K_61_61 | ARCH_1T_1CORE | ARCH_1R_1CORE
#
#   make -C firmware   ->  firmware/eld/dfe-la9310-1t2r-25m30k.eld
#
# Builds natively with fsvspacc. No Docker, no Wine, no CodeWarrior.

APP_NAME   := dfe-la9310-1t2r-25m30k

REPO_ROOT  := $(abspath ..)
# vspa-sdk headers come from the la931x_vspa_common submodule (run ./setup.sh)
VSPA_SDK   := $(REPO_ROOT)/submodules/la931x_vspa_common/vspa-sdk

# The toolchain: $VSPA_TOOLS if you exported it, else the tree ./setup.sh installs.
VSPA_TOOLS    ?= $(REPO_ROOT)/environment/cwvspa.vbeta_14_00_781_vspa.linux/VSPA_Tools
VSPA_COMPILER := $(VSPA_TOOLS)/bin/fsvspacc

ARCH_FLAGS := -arch vspa2 -au_count 16 -core_type sp
DEFS       := -D__LA9310__ -DLA9310 -DVSPA2 \
              -DSELF_CORE_ID=0 \
              -DPROJ_CONFIG_OPTION=PROJ_1T1R_1CORE_LS_9310
OPT_FLAGS  := -O3 -Os -opt=noalias_by_type
MISC_FLAGS := -ansi off --keep -mvcpu -msgstyle gcc -env $(VSPA_TOOLS)

INCS := \
    -I include \
    -I lib \
    -I src \
    -I src/kernel_FFT \
    -I src/function_Ant_manage \
    -I $(VSPA_TOOLS)/include \
    -I $(VSPA_SDK)/inc

CFLAGS := $(ARCH_FLAGS) $(DEFS) $(OPT_FLAGS) $(MISC_FLAGS) $(INCS)

BUILD := build
ELD   := eld

# Source files compiled from src/
LOCAL_C_SRCS := \
    src/DFE_func_rx.c \
    src/DFE_func_tx.c \
    src/DFE_init.c \
    src/debug.c \
    src/dump_inject.c \
    src/global_var.c \
    src/host_vspa_msg.c \
    src/main_pair_coreA.c \
    src/pair_para.c \
    src/function_Ant_manage/Ant_Rx_Buffer_manager.c \
    src/function_Ant_manage/Ant_Tx_Buffer_manager.c

# Pre-built objects: cannot be recompiled on Linux due to known compiler bugs.
#   inter_core_msg.c — ICE in backend codegen (fscgg_vspa.cpp:282)
#   ifft_cp.c        — fsvspacc cannot resolve IPPU function addresses at compile time
PREBUILT_OBJS := \
    $(abspath prebuilt/inter_core_msg.eln) \
    $(abspath prebuilt/kernel_FFT/ifft_cp.eln)

# SDK boot source
SDK_C_SRCS := $(VSPA_SDK)/src/boot.c

LOCAL_S_SRCS := src/startup_vspa2_modified.s

LOCAL_OBJS := $(addprefix $(BUILD)/,$(notdir $(LOCAL_C_SRCS:.c=.eln)))
SDK_OBJS   := $(addprefix $(BUILD)/,$(notdir $(SDK_C_SRCS:.c=.eln)))
S_OBJS     := $(addprefix $(BUILD)/,$(notdir $(LOCAL_S_SRCS:.s=.eln)))
ALL_OBJS   := $(LOCAL_OBJS) $(SDK_OBJS) $(S_OBJS) $(PREBUILT_OBJS)

TARGET      := $(ELD)/$(APP_NAME).eld
TEST_TARGET := $(ELD)/$(APP_NAME)-test.eld

.PHONY: all test clean
.DEFAULT_GOAL := all

all: $(TARGET)

# ── loopback testbench ────────────────────────────────────────────────────
#
# The RX bit-reversal runs REAL NXP IPPU microcode (bitRev1024sEbyE, from the
# vspa-lib submodule) by default.  `make test SW_BITREV=1` swaps in the software
# permutation instead — the A/B reference: both builds must produce identical
# results, and they do.
#
# NOTE: append to CFLAGS, not DEFS.  CFLAGS is simply-expanded (:=) above, so a
# later `DEFS += …` would never reach the compiler — a trap that silently builds
# the wrong variant and then reports success.
SW_BITREV ?= 0
ifeq ($(SW_BITREV),1)
CFLAGS += -DUSE_IPPU_BITREV=0
endif

VSPA_LIB_BITREV := $(REPO_ROOT)/submodules/la931x_vspa_common/vspa-lib/bitRev/asm/bitRev1024sEbyE.sx

TEST_OBJS := $(BUILD)/main_test.eln $(BUILD)/boot.eln $(BUILD)/startup_vspa2_modified.eln
ifneq ($(SW_BITREV),1)
TEST_OBJS += $(VSPA_LIB_BITREV)
endif

test: $(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS) | $(ELD)
	$(VSPA_COMPILER) \
	    $(ARCH_FLAGS) $(DEFS) $(OPT_FLAGS) $(MISC_FLAGS) \
	    -Xlnk "-Map $(abspath $(ELD))/map_test.map" \
	    -mem lcf/LA9310_sp.lcf \
	    -no-startup-files \
	    -L lib \
	    $(INCS) \
	    $(TEST_OBJS) \
	    -l lib_DFE_Ref_vspa2.elb \
	    -o $@

# Compile rules for local C sources
$(BUILD)/%.eln: src/%.c | $(BUILD)
	$(VSPA_COMPILER) $(CFLAGS) -c $< -o $(BUILD)

$(BUILD)/%.eln: src/kernel_FFT/%.c | $(BUILD)
	$(VSPA_COMPILER) $(CFLAGS) -c $< -o $(BUILD)

$(BUILD)/%.eln: src/function_Ant_manage/%.c | $(BUILD)
	$(VSPA_COMPILER) $(CFLAGS) -c $< -o $(BUILD)

# SDK boot source
$(BUILD)/boot.eln: $(VSPA_SDK)/src/boot.c | $(BUILD)
	$(VSPA_COMPILER) $(CFLAGS) -c $< -o $(BUILD)

# Assembly startup
$(BUILD)/startup_vspa2_modified.eln: src/startup_vspa2_modified.s | $(BUILD)
	$(VSPA_COMPILER) $(CFLAGS) -c $< -o $(BUILD)

# Link step
$(TARGET): $(ALL_OBJS) | $(ELD)
	$(VSPA_COMPILER) \
	    $(ARCH_FLAGS) $(DEFS) $(OPT_FLAGS) $(MISC_FLAGS) \
	    -Xlnk "-Map $(abspath $(ELD))/map_core0.map" \
	    -mem lcf/LA9310_sp.lcf \
	    -no-startup-files \
	    -L lib \
	    $(INCS) \
	    $(ALL_OBJS) \
	    -l lib_DFE_Ref_vspa2.elb \
	    -o $@

$(BUILD):
	mkdir -p $@

$(ELD):
	mkdir -p $@

clean:
	rm -rf $(BUILD) $(ELD)
