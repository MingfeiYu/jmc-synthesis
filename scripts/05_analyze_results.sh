#!/usr/bin/env bash
# Summarizes the MPC measurements and writes MP-SPDZ/results/summary_results.csv
# and MP-SPDZ/results/results_table.tex (Table 4).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MP_SPDZ_DIR="$(cd "$SCRIPT_DIR/../MP-SPDZ" && pwd)"
RAW_CSV="$MP_SPDZ_DIR/results/raw_results.csv"

if [ ! -f "$RAW_CSV" ]; then
    echo "[e] $RAW_CSV not found, run 04_run_mpc_eval.sh first"
    exit 1
fi

python3 "$MP_SPDZ_DIR/summarize_mp_spdz_results.py" \
    --raw-csv "$RAW_CSV" \
    --summary-csv "$MP_SPDZ_DIR/results/summary_results.csv"
python3 "$MP_SPDZ_DIR/analyze_benchmarks.py" \
    --log-dir "$MP_SPDZ_DIR/results/logs" \
    --tex
