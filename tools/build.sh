#!/bin/bash
set -e

SRC=$(realpath $(dirname "$(realpath $0)")/..)

cd "$SRC"

if [[ "$(uname)" == "Darwin" ]]; then
    CPU_CORES=$(sysctl -n hw.logicalcpu)
else
    CPU_CORES=$(nproc)
fi

[[ -f build/Makefile ]] || cmake -B build
cmake --build build -j$CPU_CORES
