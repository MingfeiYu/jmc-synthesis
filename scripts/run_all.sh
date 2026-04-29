#!/usr/bin/env bash
# End-to-end pipeline: synthesis → copy circuits → MPC evaluation → analysis.
#
# Usage: run_all.sh [REPETITIONS]
#   REPETITIONS  timed reps per circuit for MPC benchmarks (default: 5)
#                Set to 100 to reproduce the paper's full results.
#
# Prerequisites (native):
#   Synthesis: cmake >=3.14, ninja-build, g++ or clang++ with C++17 support
#   MPC eval:  clang++-11, libssl-dev, libgmp-dev, libboost-dev,
#              libboost-thread-dev, libsodium-dev, libntl-dev, Python 3.8+
#
# See README.md for Docker instructions (recommended).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPS="${1:-5}"

echo "================================================================"
echo " CCS 2026 Artifact: Boolean Circuit Synthesis for Garbled Circuits"
echo " Using $REPS repetition(s) per circuit"
echo "================================================================"
echo ""

bash "$SCRIPT_DIR/01_run_synthesis.sh"
echo ""
bash "$SCRIPT_DIR/02_copy_circuits.sh"
echo ""
bash "$SCRIPT_DIR/03_build_mpc.sh"
echo ""
bash "$SCRIPT_DIR/04_run_mpc_eval.sh" "$REPS"
echo ""
bash "$SCRIPT_DIR/05_analyze_results.sh"

echo ""
echo "================================================================"
echo " All done."
echo "================================================================"
