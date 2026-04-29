#!/usr/bin/env bash
# Build the MP-SPDZ Yao (garbled-circuit) party binary.
# Requires: clang++-11 (or g++), libssl-dev, libgmp-dev, libboost-dev,
#           libboost-thread-dev, libsodium-dev, libntl-dev, automake, libtool.
# On Ubuntu/Debian: apt-get install -y build-essential clang-11 libssl-dev
#   libgmp-dev libboost-dev libboost-thread-dev libsodium-dev libntl-dev
#   automake libtool libclang-dev
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MP_SPDZ_DIR="$(cd "$SCRIPT_DIR/../MP-SPDZ" && pwd)"

echo "============================================================"
echo " Building MP-SPDZ yao-party.x"
echo "============================================================"

cd "$MP_SPDZ_DIR"

# Write a minimal CONFIG.mine if one does not already exist.
if [ ! -f CONFIG.mine ]; then
    if command -v clang++-11 &>/dev/null; then
        echo "CXX = clang++-11" > CONFIG.mine
    fi
    echo "MY_CFLAGS += -I/usr/local/include" >> CONFIG.mine
    echo "MY_LDLIBS += -Wl,-rpath -Wl,/usr/local/lib -L/usr/local/lib" >> CONFIG.mine
fi

make yao-party.x

echo ""
echo "[ok] MP-SPDZ Yao binary built: $MP_SPDZ_DIR/yao-party.x"
