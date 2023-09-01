#!/bin/bash
set -e

SRC=$(realpath $(dirname "$(realpath $0)")/..)

DIST_DIR="$SRC/build/dist-win"
BUILD_DIR="$SRC/build/qemu-win"
LIBS_DIR="$SRC/build/libs-win"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/bin"

find "$LIBS_DIR" -iname '*.dll' -exec cp -v {} "$DIST_DIR/bin" \;
cp -v "$BUILD_DIR/qemu-system-arm.exe" "$DIST_DIR/bin"
