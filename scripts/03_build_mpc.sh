#!/usr/bin/env bash
# Builds the Yao garbled-circuit binary of MP-SPDZ. See README.md for the
# required packages.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MP_SPDZ_DIR="$(cd "$SCRIPT_DIR/../MP-SPDZ" && pwd)"

cd "$MP_SPDZ_DIR"
if [ ! -f CONFIG.mine ]; then
    if command -v clang++-11 &>/dev/null; then
        echo "CXX = clang++-11" > CONFIG.mine
    fi
    echo "MY_CFLAGS += -I/usr/local/include" >> CONFIG.mine
    echo "MY_LDLIBS += -Wl,-rpath -Wl,/usr/local/lib -L/usr/local/lib" >> CONFIG.mine
fi

make yao-party.x
