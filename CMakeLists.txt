cmake_minimum_required(VERSION 3.10)

project(pmb887x-emu)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(ExternalProject)

add_subdirectory(third_party/argparse)

add_executable(emu src/main.cpp src/utils.cpp)
install(TARGETS emu RENAME pmb887x-emu)

if (BUILD_STATIC)
	target_link_options(emu PUBLIC -static -static-libgcc -static-libstdc++)
endif()

if (CMAKE_BUILD_TYPE MATCHES "Release")
	target_link_options(emu PUBLIC -s)
endif()

target_link_libraries(emu PUBLIC argparse)
target_compile_options(emu PUBLIC -Wall -Wextra -Werror -O3)

execute_process(COMMAND nproc OUTPUT_VARIABLE BUILD_THREADS)
string(REGEX REPLACE "\n$" "" BUILD_THREADS "${BUILD_THREADS}")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_link_options(emu PUBLIC -static -static-libgcc -static-libstdc++)

	SET(QEMU_CONFIGURE_OPTIONS
		--prefix=${CMAKE_BINARY_DIR}/qemu-install
		--cross-prefix=x86_64-w64-mingw32-
		--disable-user
		--target-list=arm-softmmu
		--disable-werror
		--disable-docs
		--disable-install-blobs
		--without-default-features
		--enable-gtk
		--enable-pixman
		--enable-iconv
		--enable-opengl
		--enable-dsound
		--enable-sdl
		--enable-tcg
		--enable-lto
		--enable-strip
		--enable-avx2
		--enable-avx512bw
	)
else()
	SET(QEMU_CONFIGURE_OPTIONS
		--prefix=${CMAKE_BINARY_DIR}/qemu-install
		--target-list=arm-softmmu
		--disable-werror
		--disable-docs
		--disable-install-blobs
		--without-default-features
		--enable-gettext
		--enable-gio
		--enable-gtk
		--enable-pixman
		--enable-iconv
		--enable-libudev
		--enable-linux-aio
		--enable-linux-io-uring
		--enable-malloc-trim
		--enable-multiprocess
		--enable-opengl
		--enable-pa
		--enable-sdl
		--enable-tcg
		--enable-xkbcommon
		--enable-lto
		--enable-strip
		--enable-avx2
		--enable-avx512bw
	)
endif()

ExternalProject_Add(qemu
	PREFIX ${CMAKE_BINARY_DIR}/qemu-prefix
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/qemu
	BINARY_DIR ${CMAKE_BINARY_DIR}/qemu-build
	INSTALL_DIR ${CMAKE_BINARY_DIR}/qemu-install

	CONFIGURE_COMMAND ${CMAKE_SOURCE_DIR}/qemu/configure ${QEMU_CONFIGURE_OPTIONS}
	BUILD_COMMAND $(MAKE)
	INSTALL_COMMAND ""

	BUILD_ALWAYS TRUE
)
