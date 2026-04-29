#!/usr/bin/env bash
# Build the mockturtle-based synthesis tool and run the circuit optimization
# experiments that generate optimized Bristol-format circuits.
# Outputs land in:
#   mockturtle/experiments/arith_cf_benchmarks/jmc_opt_bristol/
#   mockturtle/experiments/crypto_gp_benchmarks/jmc_opt_bristol/
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ARTIFACT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MOCKTURTLE_DIR="$ARTIFACT_DIR/mockturtle"
BUILD_DIR="$MOCKTURTLE_DIR/build"

echo "============================================================"
echo " Step 1: Build circuit synthesis tool"
echo "============================================================"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if command -v ninja &>/dev/null; then
    cmake -G Ninja \
          -DMOCKTURTLE_BUILD_EXPERIMENTS=ON \
          -DCMAKE_BUILD_TYPE=Release \
          .. 2>&1 | tail -5
    ninja xag_decomp_max_local
else
    cmake -DMOCKTURTLE_BUILD_EXPERIMENTS=ON \
          -DCMAKE_BUILD_TYPE=Release \
          .. 2>&1 | tail -5
    make xag_decomp_max_local -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"
fi
echo "[ok] Binary: $BUILD_DIR/experiments/xag_decomp_max_local"

# The binary resolves xag_mc_min_6_db relative to its working directory.
cp "$MOCKTURTLE_DIR/experiments/databases/xag_mc_min_6_db" "$BUILD_DIR/"

# Ensure output directories exist.
mkdir -p "$MOCKTURTLE_DIR/experiments/arith_cf_benchmarks/jmc_opt_bristol"
mkdir -p "$MOCKTURTLE_DIR/experiments/crypto_gp_benchmarks/jmc_opt_bristol"

echo ""
echo "============================================================"
echo " Step 2: Run circuit synthesis (arith_cf and crypto_gp suites)"
echo "============================================================"
cd "$BUILD_DIR"
./experiments/xag_decomp_max_local

echo ""
echo "[ok] Synthesis complete. Generated circuits:"
echo "  arith_cf:"
ls "$MOCKTURTLE_DIR/experiments/arith_cf_benchmarks/jmc_opt_bristol/"
echo "  crypto_gp:"
ls "$MOCKTURTLE_DIR/experiments/crypto_gp_benchmarks/jmc_opt_bristol/"

echo ""
echo "============================================================"
echo " Step 3: Generate result tables"
echo "============================================================"
python3 "$SCRIPT_DIR/gen_synthesis_tables.py"
echo "[ok] results_arith_cp.tex and results_crypto_gp.tex written to"
echo "     $MOCKTURTLE_DIR/experiments/"
