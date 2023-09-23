#!/bin/bash
set -e

SRC=$(realpath $(dirname "$(realpath $0)")/..)

DIST_DIR="$SRC/build/dist-win"
BUILD_DIR="$SRC/build/qemu-win"
LIBS_DIR="$SRC/build/libs-win"
BOARDS_DIR="$SRC/bsp/lib/data/board"
DIST_ZIP="/tmp/pmb887x-emu-windows.zip"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/bin"

find /usr/x86_64-w64-mingw32/lib/ -iname '*.dll' -exec cp -v {} "$DIST_DIR/bin" \;
find /usr/lib/gcc/x86_64-w64-mingw32/12-win32 -iname '*.dll' -exec cp -v {} "$DIST_DIR/bin" \;

find "$LIBS_DIR" -iname '*.dll' -exec cp -v {} "$DIST_DIR/bin" \;
cp -v "$BUILD_DIR/qemu-system-arm.exe" "$DIST_DIR/bin"
rsync -av --delete "$BOARDS_DIR/" "$DIST_DIR/boards/"
cp -v "$SRC/emu" "$DIST_DIR/emu"
cd "$DIST_DIR"

[[ -f "$DIST_ZIP" ]] && rm "$DIST_ZIP"
zip "$DIST_ZIP" -r .
