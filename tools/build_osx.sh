#!/bin/bash
set -e

SRC=$(realpath $(dirname "$(realpath $0)")/..)

BUILD_DIR="$SRC/build/qemu-osx"

[[ -d "$BUILD_DIR" ]] || mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

[[ "$1" == "configure" ]] && rm -f "$BUILD_DIR/.configured"

[[ -e "$BUILD_DIR/.configured" ]] || {
	"$SRC/qemu/configure" \
		--target-list=arm-softmmu \
		--disable-werror \
		--disable-install-blobs \
		--enable-lto \
		--enable-strip
	touch "$BUILD_DIR/.configured"
}

make -j$((`nproc` + 1))
