#!/usr/bin/env bash
# Builds the synthesis experiments and runs the optimization of both benchmark
# suites (Tables 1 and 2). Optimized circuits are written to
# mockturtle/experiments/{arith_cf,crypto_gp}_benchmarks/jmc_opt_bristol/.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ARTIFACT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MOCKTURTLE_DIR="$ARTIFACT_DIR/mockturtle"
BUILD_DIR="$MOCKTURTLE_DIR/build"

# Tables 1 and 2, then Table 3 and Section 5.1.3.
TARGETS="xag_decomp_max_local xag_decomp_kappa_sweep xag_kappa4 xag_kappa6 xag_kappa10 multi_output_mc_check"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
if command -v ninja &>/dev/null; then
    cmake -G Ninja -DMOCKTURTLE_BUILD_EXPERIMENTS=ON -DCMAKE_BUILD_TYPE=Release .. | tail -5
    ninja $TARGETS
else
    cmake -DMOCKTURTLE_BUILD_EXPERIMENTS=ON -DCMAKE_BUILD_TYPE=Release .. | tail -5
    make $TARGETS -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"
fi

# the experiments look for the database in the working directory
cp "$MOCKTURTLE_DIR/experiments/databases/xag_mc_min_6_db" "$BUILD_DIR/"
mkdir -p "$MOCKTURTLE_DIR/experiments/arith_cf_benchmarks/jmc_opt_bristol"
mkdir -p "$MOCKTURTLE_DIR/experiments/crypto_gp_benchmarks/jmc_opt_bristol"

echo "[i] running xag_decomp_max_local"
./experiments/xag_decomp_max_local

python3 "$SCRIPT_DIR/gen_synthesis_tables.py"
