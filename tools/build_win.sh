#!/bin/bash
set -e

SRC=$(realpath $(dirname "$(realpath $0)")/..)

BUILD_DIR="$SRC/build/qemu-win"
LIBS_DIR="$SRC/build/libs-win"

[[ -d "$BUILD_DIR" ]] || mkdir -p "$BUILD_DIR"
[[ -d "$LIBS_DIR" ]] || mkdir -p "$LIBS_DIR"

function download_pkg {
	url=$1
	hash=$2
	
	filename=$(basename "$url")
	
	[[ -f "$LIBS_DIR/$filename.ok" ]] || {
		wget "$url" -O "$LIBS_DIR/$filename"
		tar -C "$LIBS_DIR" -xf "$LIBS_DIR/$filename"
		touch "$LIBS_DIR/$filename.ok"
	}
	
	echo "$hash $LIBS_DIR/$filename" > "$LIBS_DIR/$filename.sha256sum"
	sha256sum --quiet -c "$LIBS_DIR/$filename.sha256sum" || {
		real_hash=$(sha256sum "$LIBS_DIR/$filename" | awk '{print $1}')
		echo "$hash != $real_hash"
		exit 1
	}
}

export PKG_CONFIG_SYSROOT_DIR="$LIBS_DIR"
export PKG_CONFIG_LIBDIR="$LIBS_DIR/mingw64/lib/pkgconfig/:$LIBS_DIR/usr/lib/pkgconfig/"

export CFLAGS=" -I$LIBS_DIR/mingw64/include/ -I$LIBS_DIR/usr/include/ "
export LDFLAGS=" -Wl,-L$LIBS_DIR/mingw64/lib/ -Wl,-L$LIBS_DIR/usr/lib/ "

download_pkg "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-glib2-2.76.4-1-any.pkg.tar.zst" "44b5793003516deec9dcbbe928f0658e03e8051272ddddabcd41aa8384250c9b"
download_pkg "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-SDL2-2.28.2-1-any.pkg.tar.zst" "1f38e0b90c303dfa735ae1631c73ea3a8f24bb75546a25c2d68c4c07a8851a16"
download_pkg "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-pcre2-10.42-1-any.pkg.tar.zst" "03633abbee1e0502c27ca02331e7544601853cb8c368af5fe7f2b5d57b6b10d1"
download_pkg "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-pixman-0.42.2-1-any.pkg.tar.zst" "d8448d55fef62494818cad70753d781942ce82f4d5c0e9e87bcfc1bebf5d0187"
download_pkg "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-zlib-1.3-1-any.pkg.tar.zst" "254a6c5a8a27d1b787377a3e70a39cceb200b47c5f15f4ab5bfa1431b718ef98"
download_pkg "https://mirror.msys2.org/msys/x86_64/gettext-devel-0.22-1-x86_64.pkg.tar.zst" "5407413a7ae3f4b8d7b03554df111694adc59f28bbb85699ffca1b0e3f506582"
download_pkg "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-gettext-0.21.1-2-any.pkg.tar.zst" "3dd1de072c05b79b12ddb2bc3adfdf20a94862674ac6e4470f1c2778fbc3493e"
download_pkg "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-libiconv-1.17-3-any.pkg.tar.zst" "7047b4f4324ab2682ddc72d8de21c3cdd42390f2502fd50b1002de64dea52d18"
echo "PKG_CONFIG_LIBDIR=$PKG_CONFIG_LIBDIR"

cd "$BUILD_DIR"

[[ "$1" == "configure" ]] && rm -f "$BUILD_DIR/.configured"

# --without-default-devices

[[ -e "$BUILD_DIR/.configured" ]] || {
	"$SRC/qemu/configure" \
		--cross-prefix=x86_64-w64-mingw32- \
		--disable-user \
		--extra-ldflags="$LDFLAGS" \
		--extra-cflags="$CFLAGS" \
		--extra-cxxflags="$CFLAGS" \
		--target-list=arm-softmmu \
		--disable-werror \
		--disable-install-blobs \
		--without-default-features \
		--enable-avx2 \
		--enable-avx512bw \
		--enable-avx512f \
		--enable-dsound \
		--enable-iconv \
		--enable-gettext \
		--enable-membarrier \
		--enable-sdl \
		--enable-tcg \
		--enable-stack-protector \
		--enable-strip
	touch "$BUILD_DIR/.configured"
}

make -j$((`nproc` + 1))
