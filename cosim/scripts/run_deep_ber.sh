#!/usr/bin/env bash
# Deep BER-vs-SNR run — one process per SNR point, merged at the end.
#
# A statistically meaningful curve needs ~1000 symbols at each of ~8 SNR points,
# and the chain is simulator-bound: every symbol is really executed by the ISS.
# Sequentially that is hours. This driver runs the grid points CONCURRENTLY, one
# process each, and merges their checkpoints into a single result.
#
#   cosim/scripts/run_deep_ber.sh                             # 1000 sym x 8 pts
#   SYMBOLS=200 cosim/scripts/run_deep_ber.sh                 # shallower
#   OUT=runs/qec cosim/scripts/run_deep_ber.sh --iq-imbalance 1,5 --qec on
#
# Re-running RESUMES: each shard picks up its own checkpoint, so an interrupted
# deep run costs at most one chunk per shard, and a shard that already finished
# is a no-op. Extra arguments are forwarded to EVERY shard, unchanged.
#
# ─────────────────────────────────────────────────────────────────────────────
# WHY SHARDING IS EXACTLY EQUIVALENT TO ONE SEQUENTIAL SWEEP
#
# This is the invariant the whole script rests on, and it is not self-evident —
# "run it 8 times in parallel and add up the answers" is only sound because of
# how dfe_cosim.py keys its RNG. It does NOT stream one Generator through the
# sweep (where a symbol's noise would depend on every draw before it, and a
# shard would therefore see a different channel than the sequential run). Every
# draw is instead a pure function of a GLOBAL coordinate and of nothing else:
#
#     bits  <- SeedSequence([seed, BITS_DOMAIN,  chunk])           # per chunk
#     noise <- SeedSequence([seed, NOISE_DOMAIN, point, chunk])    # per (point, chunk)
#
# `point` is the index into the FULL SNR grid, not into the shard's slice; the
# grid is built from --snr-min/--snr-max/--snr-points first and only then
# filtered by --only-points. So the shard that owns point 3 draws exactly the
# noise point 3 would have had inside one monolithic sweep — the arithmetic is
# element-wise identical, not merely "statistically equivalent". The same is
# true chunk-by-chunk, which is what makes --resume identical to never having
# been interrupted.
#
# (The bits are keyed WITHOUT the point index on purpose: the cosim transmits
# and interpolates each symbol once and reuses it across all SNR points — the
# noise goes in afterwards. A symbol therefore carries the same bits at every
# point, and only the noise is re-drawn.)
#
# Four rules preserve that invariant. Break one and shards stop being a sweep:
#
#   1. EVERY SHARD SEES THE SAME GRID AND THE SAME EXPERIMENT. A point index
#      only means something relative to the grid it indexes, so --symbols,
#      --snr-*, --chunk and every impairment flag must be identical across
#      shards. That is why "$@" is forwarded verbatim to all of them, and why
#      dfe_cosim.py records a config fingerprint and --merge refuses shards
#      whose fingerprints disagree rather than silently splicing two
#      experiments together.
#   2. ONE CHECKPOINT PER SHARD. Shards must not share a file: --merge detects a
#      grid point claimed by two checkpoints and refuses, because summing them
#      would double-count that point's bits and halve its BER with no other
#      symptom.
#   3. --chunk PARTITIONS THE RNG STREAMS. Changing it changes every number, so
#      it invalidates a resume (it is in the fingerprint). Pick it once.
#   4. SHARD BY PROCESS, NOT BY THREAD. cosim/dfe/kernel_runner.py holds a single
#      process-global NativeModel (the ISS keeps per-device state), so two sweeps
#      inside one interpreter would clobber each other's simulator.
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
cd "$REPO"

# The in-process ISS is libvspa2_16au.so; without this every shard dies on load.
export LD_LIBRARY_PATH="$REPO/environment/ccssim2/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

PYTHON=${PYTHON:-python3}
COSIM=${COSIM:-cosim/scripts/dfe_cosim.py}   # overridable only to test this driver
SYMBOLS=${SYMBOLS:-1000}
POINTS=${POINTS:-8}
OUT=${OUT:-runs/deep}
MODEL=${MODEL:-0}          # MODEL=1 to let each shard also compute the Python curve

mkdir -p "$(dirname "$OUT")"

# The Python-model curve needs no simulator and does not depend on the SNR
# sharding; computing it in all 8 shards would just burn 8x the CPU on the same
# numbers. Run it once, separately, when you want it.
COMMON=(--symbols "$SYMBOLS" --snr-points "$POINTS")
[ "$MODEL" = 1 ] || COMMON+=(--no-model)
COMMON+=("$@")

cores=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 0)
if [ "$cores" -gt 0 ] && [ "$POINTS" -gt "$cores" ]; then
    echo "[deep] note: $POINTS shards on $cores core(s) — they will time-share." >&2
fi

# ── build ONCE, before the fan-out ───────────────────────────────────────────
# NativeKernelRunner never builds anything (it loads the .eld and raises if it is
# missing), so nothing else will do this for us. Doing it here rather than in the
# shards is not an optimisation: N concurrent `make`s over the SAME targets race,
# each writing the same .eld while another links against it.
echo "[deep] building the kernels once (concurrent makes would race) …"
if ! make -C cosim/kernels > "${OUT}_build.log" 2>&1; then
    echo "[deep] kernel build FAILED — see ${OUT}_build.log" >&2
    tail -20 "${OUT}_build.log" >&2
    exit 1
fi

# ── fan out: one process per SNR point ───────────────────────────────────────
echo "[deep] $POINTS shard(s) x $SYMBOLS symbol(s) → ${OUT}_p*.json"
pids=()
for i in $(seq 0 $((POINTS - 1))); do
    # --resume is unconditional and is what makes this script idempotent: with no
    # checkpoint yet it starts fresh, with one it continues, and a finished shard
    # re-runs as a no-op. (Without it, an existing checkpoint is an error rather
    # than being silently overwritten — which is the behaviour we want.)
    $PYTHON -u "$COSIM" "${COMMON[@]}" --only-points "$i" \
        --checkpoint "${OUT}_p${i}.json" --resume \
        > "${OUT}_p${i}.log" 2>&1 &
    pids+=($!)
    echo "       point $i  pid ${pids[-1]}  → ${OUT}_p${i}.log"
done

failed=()
for i in "${!pids[@]}"; do
    wait "${pids[$i]}" || failed+=("$i")
done

if [ ${#failed[@]} -ne 0 ]; then
    # Do NOT merge. A merge over a failed shard's checkpoint is not "the curve
    # minus a point": --merge would report it as incomplete, but the far worse
    # case is a shard that died mid-chunk and looks merely short. Fix, then
    # re-run this script — every surviving shard resumes from its checkpoint.
    echo "[deep] shard(s) ${failed[*]} FAILED or were interrupted — NOT merging." >&2
    for i in "${failed[@]}"; do echo "       tail ${OUT}_p${i}.log:" >&2
        tail -5 "${OUT}_p${i}.log" >&2; done
    echo "[deep] re-run this script to resume from where each shard stopped." >&2
    exit 1
fi

# ── merge ────────────────────────────────────────────────────────────────────
# Exits non-zero if a grid point is missing chunks; the table it prints then says
# PARTIAL. Nothing here papers over that.
echo "[deep] all shards done — merging …"
# --figure: write the plot NEXT TO THE CHECKPOINTS, not into docs/figures/.
# The cosim's default figure path is a TRACKED deliverable that README.md and
# docs/TESTING.md embed, alongside a table quoting its depth. A deep run that
# silently overwrote it would leave the figure and the text next to it
# contradicting each other — and a smoke run would degrade it outright.
$PYTHON "$COSIM" --merge "${OUT}"_p*.json --checkpoint "${OUT}_merged.json" \
    --figure "${OUT}_ber.png"
