#!/usr/bin/env bash
# Runs the whole pipeline: synthesis, MPC evaluation and analysis.
#
# Usage: run_all.sh [REPETITIONS]   (default: 5; the paper uses 100)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPS="${1:-5}"

bash "$SCRIPT_DIR/01_run_synthesis.sh"
bash "$SCRIPT_DIR/02_copy_circuits.sh"
bash "$SCRIPT_DIR/03_build_mpc.sh"
bash "$SCRIPT_DIR/04_run_mpc_eval.sh" "$REPS"
bash "$SCRIPT_DIR/05_analyze_results.sh"
