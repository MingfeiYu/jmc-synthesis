#!/usr/bin/env bash
# Copy all Bristol-format circuits (baseline + optimized) from the mockturtle
# experiments directories into MP-SPDZ/Programs/Circuits/ so that
# run_mp_spdz_benchmarks.py can find them.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ARTIFACT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MOCKTURTLE_DIR="$ARTIFACT_DIR/mockturtle"
CIRCUITS_DIR="$ARTIFACT_DIR/MP-SPDZ/Programs/Circuits"

mkdir -p "$CIRCUITS_DIR"

echo "Copying circuits to $CIRCUITS_DIR ..."

# Baseline circuits (original, unoptimized)
for f in \
    "$MOCKTURTLE_DIR/experiments/arith_cf_benchmarks/"*.txt \
    "$MOCKTURTLE_DIR/experiments/crypto_gp_benchmarks/"*.txt; do
    [ -f "$f" ] && cp "$f" "$CIRCUITS_DIR/" && echo "  [base] $(basename "$f")"
done

# Optimized circuits (output of step 01)
for f in \
    "$MOCKTURTLE_DIR/experiments/arith_cf_benchmarks/jmc_opt_bristol/"*.txt \
    "$MOCKTURTLE_DIR/experiments/crypto_gp_benchmarks/jmc_opt_bristol/"*.txt; do
    [ -f "$f" ] && cp "$f" "$CIRCUITS_DIR/" && echo "  [opt]  $(basename "$f")"
done

echo ""
echo "[ok] $(ls "$CIRCUITS_DIR"/*.txt 2>/dev/null | wc -l | tr -d ' ') circuits in $CIRCUITS_DIR"
