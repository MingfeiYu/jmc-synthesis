#!/usr/bin/env bash
# Copies the baseline and optimized Bristol circuits to MP-SPDZ/Programs/Circuits/.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ARTIFACT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MOCKTURTLE_DIR="$ARTIFACT_DIR/mockturtle"
CIRCUITS_DIR="$ARTIFACT_DIR/MP-SPDZ/Programs/Circuits"

mkdir -p "$CIRCUITS_DIR"
for suite in arith_cf crypto_gp; do
    for f in "$MOCKTURTLE_DIR/experiments/${suite}_benchmarks/"*.txt \
             "$MOCKTURTLE_DIR/experiments/${suite}_benchmarks/jmc_opt_bristol/"*.txt; do
        if [ -f "$f" ]; then
            cp "$f" "$CIRCUITS_DIR/"
        fi
    done
done

echo "[i] $(ls "$CIRCUITS_DIR"/*.txt | wc -l | tr -d ' ') circuits in $CIRCUITS_DIR"
