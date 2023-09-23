#!/bin/bash
set -e

SRC=$(realpath $(dirname "$(realpath $0)")/..)

BUILD_DIR="$SRC/build/qemu"

[[ -d "$BUILD_DIR" ]] || mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

[[ "$1" == "configure" ]] && rm -f "$BUILD_DIR/.configured"

# --enable-lto
# --enable-strip
# --enable-blkio
# --enable-pipewire
# --enable-vte
# --enable-libdw

[[ -e "$BUILD_DIR/.configured" ]] || {
	"$SRC/qemu/configure" \
		--target-list=arm-softmmu \
		--disable-werror \
		--disable-install-blobs \
		--without-default-features \
		--enable-avx2 \
		--enable-avx512bw \
		--enable-avx512f \
		--enable-dbus-display \
		--enable-gettext \
		--enable-gio \
		--enable-gtk \
		--enable-gtk-clipboard \
		--enable-iconv \
		--enable-libpmem \
		--enable-libudev \
		--enable-linux-aio \
		--enable-linux-io-uring \
		--enable-malloc-trim \
		--enable-multiprocess \
		--enable-opengl \
		--enable-pa \
		--enable-sdl \
		--enable-spice \
		--enable-spice-protocol \
		--enable-tcg \
		--enable-virglrenderer \
		--enable-vnc \
		--enable-vnc-jpeg \
		--enable-vnc-sasl \
		--enable-xkbcommon \
		--enable-pie \
		--enable-stack-protector
	touch "$BUILD_DIR/.configured"
}

make -j$((`nproc` + 1))
