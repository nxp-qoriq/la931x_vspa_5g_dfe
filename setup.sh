#!/bin/bash
# One-time setup: submodule + NXP VSPA toolchain + native VSPA simulator.
#
#   ./setup.sh
#
# Everything it fetches is a public NXP download; nothing proprietary is vendored
# into this repository. Re-running is safe — each step is skipped if already done.
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
TOOLCHAIN_VERSION="vbeta_14_00_781"

# NXP public SDK downloads (LA9310 SDK 10)
BASE="https://www.nxp.com/lgfiles/sdk/la1224/imx-la9310-sdk-10"
TOOLCHAIN_URL="$BASE/cwvspa.${TOOLCHAIN_VERSION}_vspa.linux.tgz"
SIM_URL="$BASE/ccssim2_linux_gcc4-64_lin_1_0_63_3.tar.gz"

TOOLCHAIN_DIR="$HERE/environment/cwvspa.${TOOLCHAIN_VERSION}_vspa.linux"
SIM_DIR="$HERE/environment/ccssim2"

echo "==> 1/3  la931x_vspa_common submodule (vspa-sdk headers + the bitRev IPPU kernel)"
if [ -f "$HERE/submodules/la931x_vspa_common/vspa-sdk/inc/vspa.h" ] \
   || [ -d "$HERE/submodules/la931x_vspa_common/vspa-sdk" ]; then
    echo "    already present"
else
    git -C "$HERE" submodule update --init --recursive
fi

echo "==> 2/3  VSPA toolchain ($TOOLCHAIN_VERSION)   [fsvspacc — builds the firmware]"
if [ -d "$TOOLCHAIN_DIR/VSPA_Tools" ]; then
    echo "    already installed: $TOOLCHAIN_DIR"
else
    mkdir -p "$TOOLCHAIN_DIR"
    echo "    downloading $TOOLCHAIN_URL"
    curl -fL --progress-bar -o "/tmp/vspa_toolchain.tgz" "$TOOLCHAIN_URL"
    tar -xf "/tmp/vspa_toolchain.tgz" -C "$TOOLCHAIN_DIR"
    rm -f "/tmp/vspa_toolchain.tgz"
    echo "    installed: $TOOLCHAIN_DIR"
fi

echo "==> 3/3  native VSPA simulator (ccssim2)       [libvspa2_16au.so — runs the firmware]"
if [ -f "$SIM_DIR/bin/libvspa2_16au.so" ]; then
    echo "    already installed: $SIM_DIR"
elif [ "$(uname -m)" != "x86_64" ]; then
    echo "    SKIPPED: the simulator ships as an x86_64 binary and this host is $(uname -m)."
    echo "    Point VSPA_MODEL at a libvspa2_16au.so from an x86_64 machine to run the tests."
else
    mkdir -p "$SIM_DIR"
    echo "    downloading $SIM_URL"
    curl -fL --progress-bar -o "/tmp/ccssim2.tar.gz" "$SIM_URL"
    tar -xf "/tmp/ccssim2.tar.gz" -C "$SIM_DIR" --strip-components=2
    rm -f "/tmp/ccssim2.tar.gz"
    echo "    installed: $SIM_DIR"
fi

cat <<EOF

Setup complete. Add these to your environment (or your shell profile):

    export VSPA_TOOLS=$TOOLCHAIN_DIR/VSPA_Tools
    export VSPA_MODEL=$SIM_DIR/bin/libvspa2_16au.so

Then:

    ./scripts/fetch_fr1fr2.sh    # NXP FR1 test vectors + prebuilt reference images
    make                         # -> eld/dfe-la9310-1t2r-25m30k.eld
    make verify                  # run the verification tiers (1-5)
    make verify-full             # + RX, downstream TX, silicon anchor (6-8)

EOF
