#!/bin/bash
# Fetch the LA9310-relevant subset of nxp-qoriq/fr1_fr2_test_tool (branch oss).
#
# Pulls into nxp/fr1_fr2_test_tool/ (untracked, see .gitignore):
#   - test_vectors/  : only 10/20 MHz FR1 waveforms (LA9310 bandwidths)
#   - MEvspa_images_*_LA93/ : NXP prebuilt LA9310 DFE images (geul-vspa0.eld + map)
#   - utils/, doc/, top-level control scripts (format + protocol reference)
#
# The clone is blobless (--filter=blob:none): only the sparse-selected files
# are downloaded. Re-running updates to the latest oss branch.
set -euo pipefail

REPO_URL=https://github.com/nxp-qoriq/fr1_fr2_test_tool.git
BRANCH=oss
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/nxp/fr1_fr2_test_tool"

if [ ! -d "$DEST/.git" ]; then
    git clone --filter=blob:none --no-checkout --branch "$BRANCH" --depth 1 \
        "$REPO_URL" "$DEST"
fi

cd "$DEST"
git sparse-checkout set --no-cone \
    '/*.sh' '/*.txt' '/*.dat' '/*.cfg' \
    '/doc/' \
    '/utils/' \
    '/test_vectors/*10MHz*' \
    '/test_vectors/*20MHz*' \
    '/test_vectors/*25MHz*' \
    '/test_vectors/*50MHz*' \
    '/MEvspa_images_LS.1T1R.T4x_25M_60K_122_122_TDDFDD_LA93/' \
    '/MEvspa_images_LS.1T2R_10M_15K_61_61_TDDFDD_LA93/' \
    '/MEvspa_images_LS.1T2R_25M_30K_61_61_TDDFDD_LA93/' \
    '/MEvspa_images_LS.1T2R_25M_60K_61_61_TDDFDD_LA93/'
git checkout "$BRANCH"

echo
echo "Fetched into $DEST:"
du -sh test_vectors MEvspa_images_*_LA93 utils doc 2>/dev/null
