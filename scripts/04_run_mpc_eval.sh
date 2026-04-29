#!/usr/bin/env bash
# Run the two-party Yao garbled-circuit benchmarks for all circuits in
# MP-SPDZ/Programs/Circuits/ and write results to MP-SPDZ/results/.
#
# Usage: 04_run_mpc_eval.sh [REPETITIONS]
#   REPETITIONS  number of timed repetitions per circuit (default: 100)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ARTIFACT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MP_SPDZ_DIR="$ARTIFACT_DIR/MP-SPDZ"
CIRCUITS_DIR="$MP_SPDZ_DIR/Programs/Circuits"

REPS="${1:-100}"

echo "============================================================"
echo " Running MPC benchmarks  (repetitions=$REPS)"
echo "============================================================"
echo "[i] Results will be written to: $MP_SPDZ_DIR/results/"
echo ""

if [ ! -f "$MP_SPDZ_DIR/yao-party.x" ]; then
    echo "[error] yao-party.x not found. Run 03_build_mpc.sh first."
    exit 1
fi

if [ -z "$(ls "$CIRCUITS_DIR"/*.txt 2>/dev/null)" ]; then
    echo "[error] No .txt circuits found in $CIRCUITS_DIR. Run 01 and 02 first."
    exit 1
fi

cd "$ARTIFACT_DIR"
python3 MP-SPDZ/run_mp_spdz_benchmarks.py \
    --mp-spdz     "$MP_SPDZ_DIR" \
    --circuits-dir "$CIRCUITS_DIR" \
    --repetitions  "$REPS" \
    --verbose

echo ""
echo "[ok] Raw results: $MP_SPDZ_DIR/results/raw_results.csv"
