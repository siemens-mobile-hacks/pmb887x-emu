#!/bin/bash
set -e

SRC=$(realpath $(dirname "$(realpath $0)")/..)

DIST_DIR="$SRC/build/dist-osx"
BUILD_DIR="$SRC/build/qemu-osx"
LIBS_DIR="$SRC/build/libs-osx"
BOARDS_DIR="$SRC/bsp/lib/data/board"
DIST_OUT="/tmp/pmb887x-emu-osx-arm.tar.gz"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/bin"

cp -v "$BUILD_DIR/qemu-system-arm" "$DIST_DIR/bin"
rsync -av --delete "$BOARDS_DIR/" "$DIST_DIR/boards/"
cp -v "$SRC/emu" "$DIST_DIR/emu"
chmod +x "$DIST_DIR/emu"

[[ -f "$DIST_OUT" ]] && rm "$DIST_OUT"
tar -C "$DIST_DIR" -cpzvf "$DIST_OUT" -r .
