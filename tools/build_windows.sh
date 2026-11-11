#!/bin/bash
set -e

ARCH=${1:-x86_64}
case "$ARCH" in
	x86_64)
		MSYS2_REPO=clang64
		;;
	aarch64)
		MSYS2_REPO=clangarm64
		;;
	*)
		echo "usage: $0 [x86_64|aarch64]" >&2
		exit 1
		;;
esac

TRIPLE=$ARCH-w64-mingw32
IMAGE=pmb887x-emu-windows-$ARCH
BUILD_DIR=build-windows-$ARCH
SRC=$(realpath "$(dirname "$(realpath "$0")")/..")

docker build \
	--build-arg MSYS2_REPO=$MSYS2_REPO \
	--build-arg MSYS2_PACKAGE_PREFIX=mingw-w64-clang-$ARCH \
	-t "$IMAGE" -f "$SRC/docker/Dockerfile.windows" "$SRC/docker"

docker run --rm -u "$(id -u):$(id -g)" -v "$SRC:/app:Z" -w /app "$IMAGE" bash -c "
	cmake -S . -B $BUILD_DIR \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_SYSTEM_NAME=Windows \
		-DCMAKE_SYSTEM_PROCESSOR=$ARCH \
		-DCMAKE_C_COMPILER=$TRIPLE-clang \
		-DCMAKE_CXX_COMPILER=$TRIPLE-clang++ \
		-DCMAKE_RC_COMPILER=$TRIPLE-windres \
		-DMINGW_DLL_DIR=/opt/msys2/$MSYS2_REPO/bin &&
	cmake --build $BUILD_DIR -j\$(nproc)
"
