#!/usr/bin/env bash
# Summarize benchmark results and print a comparison table.
# Reads MP-SPDZ/results/raw_results.csv and produces:
#   - MP-SPDZ/results/summary_results.csv (per-metric statistics)
#   - A printed comparison table (baseline vs. optimized)
#   - MP-SPDZ/results/results_table.tex  (LaTeX table, with --tex)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ARTIFACT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MP_SPDZ_DIR="$ARTIFACT_DIR/MP-SPDZ"
RAW_CSV="$MP_SPDZ_DIR/results/raw_results.csv"

if [ ! -f "$RAW_CSV" ]; then
    echo "[error] $RAW_CSV not found. Run 04_run_mpc_eval.sh first."
    exit 1
fi

echo "============================================================"
echo " Summarizing results"
echo "============================================================"
python3 "$MP_SPDZ_DIR/summarize_mp_spdz_results.py" \
    --raw-csv "$RAW_CSV" \
    --summary-csv "$MP_SPDZ_DIR/results/summary_results.csv"

echo ""
echo "============================================================"
echo " Garbling communication & timing comparison (baseline vs. opt)"
echo "============================================================"
python3 "$MP_SPDZ_DIR/analyze_benchmarks.py" \
    --log-dir "$MP_SPDZ_DIR/results/logs" \
    --tex

echo ""
echo "[ok] Summary CSV: $MP_SPDZ_DIR/results/summary_results.csv"
echo "[ok] LaTeX table: $MP_SPDZ_DIR/results/results_table.tex"
