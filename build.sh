#!/bin/bash
set -e
set -x

SRC=$(dirname $(realpath $0))

[[ -d "$SRC/build/qemu" ]] || mkdir -p "$SRC/build/qemu"
cd "$SRC/build/qemu"

[[ -e "$SRC/build/qemu/Makefile" ]] || "$SRC/qemu/configure" --disable-user --target-list=arm-softmmu --disable-werror

make -j$((`nproc` + 1))
DESTDIR="$SRC/build/qemu-bin" make install
