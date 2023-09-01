#!/bin/bash
set -e
set -x

SRC=$(realpath $(dirname "$(realpath $0)")/..)

[[ -d "$SRC/build/qemu" ]] || mkdir -p "$SRC/build/qemu"
cd "$SRC/build/qemu"

[[ -e "$SRC/build/qemu/Makefile.ninja" ]] || "$SRC/qemu/configure" --disable-user --target-list=arm-softmmu --disable-werror \
	--disable-install-blobs --enable-strip --disable-docs --disable-tools --enable-lto

make -j$((`nproc` + 1))
# DESTDIR="$SRC/build/qemu-bin" make install
