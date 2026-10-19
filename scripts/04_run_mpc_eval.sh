#!/usr/bin/env bash
# Runs all circuits in MP-SPDZ/Programs/Circuits/ under Yao's protocol (Table 4).
#
# Usage: 04_run_mpc_eval.sh [REPETITIONS]   (default: 100)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ARTIFACT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MP_SPDZ_DIR="$ARTIFACT_DIR/MP-SPDZ"
CIRCUITS_DIR="$MP_SPDZ_DIR/Programs/Circuits"
REPS="${1:-100}"

if [ ! -f "$MP_SPDZ_DIR/yao-party.x" ]; then
    echo "[e] yao-party.x not found, run 03_build_mpc.sh first"
    exit 1
fi

cd "$ARTIFACT_DIR"
python3 MP-SPDZ/run_mp_spdz_benchmarks.py \
    --mp-spdz "$MP_SPDZ_DIR" \
    --circuits-dir "$CIRCUITS_DIR" \
    --repetitions "$REPS" \
    --verbose
