#!/bin/bash
set -e

SRC=$(realpath $(dirname "$(realpath $0)")/..)

cd "$SRC"
[[ -f build/Makefile ]] || cmake -B build
cmake --build build -j$(nproc)
