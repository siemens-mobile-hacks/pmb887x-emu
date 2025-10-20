#!/bin/bash
set -e

SRC=$(realpath $(dirname "$(realpath $0)")/..)

cd "$SRC"

[[ -d build ]] || cmake -B build
cmake --build build -j$(nproc)
