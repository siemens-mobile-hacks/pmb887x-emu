cmake_minimum_required(VERSION 3.10)

project(pmb887x-emu)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(ExternalProject)
include(GNUInstallDirs)

add_subdirectory(third_party/argparse)

add_executable(emu src/main.cpp src/utils.cpp)
set_target_properties(emu PROPERTIES OUTPUT_NAME "pmb887x-emu")
install(TARGETS emu RUNTIME DESTINATION bin)

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
		--prefix=${CMAKE_BINARY_DIR}/dist/qemu
		--cross-prefix=x86_64-w64-mingw32-
		--enable-werror
		--disable-user
		--target-list=arm-softmmu
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
	)

	set(DIST_DIR "${CMAKE_BINARY_DIR}/dist")
	add_custom_command(TARGET emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}")
	add_custom_command(TARGET emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/qemu")
	add_custom_command(TARGET emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/boards")

	# Copy executables
	add_custom_command(TARGET emu POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/pmb887x-emu.exe" "${DIST_DIR}/pmb887x-emu.exe"
		VERBATIM
	)

	# Copy DLL's
	file(GLOB SYSROOT_DLLS "/usr/x86_64-w64-mingw32/sys-root/mingw/bin/*.dll")
	foreach(dll_file IN LISTS SYSROOT_DLLS)
		add_custom_command(TARGET emu POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll_file}" "${DIST_DIR}/qemu"
			VERBATIM
		)
	endforeach()

	# Copy boards
	file(GLOB SYSROOT_DLLS "${CMAKE_SOURCE_DIR}/bsp/lib/data/board/*.toml")
	foreach(board_file IN LISTS SYSROOT_DLLS)
		add_custom_command(TARGET emu POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${board_file}" "${DIST_DIR}/boards"
			VERBATIM
		)
	endforeach()

	ExternalProject_Add(qemu
		PREFIX ${CMAKE_BINARY_DIR}/dist/qemu
		SOURCE_DIR ${CMAKE_SOURCE_DIR}/qemu
		BINARY_DIR ${CMAKE_BINARY_DIR}/qemu-build
		INSTALL_DIR ${CMAKE_BINARY_DIR}/dist/qemu

		CONFIGURE_COMMAND ${CMAKE_SOURCE_DIR}/qemu/configure ${QEMU_CONFIGURE_OPTIONS}
		BUILD_COMMAND $(MAKE)
		INSTALL_COMMAND $(MAKE) install

		BUILD_ALWAYS TRUE
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
	)

	install(
		DIRECTORY ${CMAKE_SOURCE_DIR}/bsp/lib/data/board/
		DESTINATION share/pmb887x-emu/boards
		FILES_MATCHING PATTERN "*.toml"
	)

	install(
		DIRECTORY ${CMAKE_BINARY_DIR}/qemu-install/
		DESTINATION share/pmb887x-emu/qemu
	)

	ExternalProject_Add(qemu
		PREFIX ${CMAKE_BINARY_DIR}/qemu-prefix
		SOURCE_DIR ${CMAKE_SOURCE_DIR}/qemu
		BINARY_DIR ${CMAKE_BINARY_DIR}/qemu-build
		INSTALL_DIR ${CMAKE_BINARY_DIR}/qemu-install

		CONFIGURE_COMMAND ${CMAKE_SOURCE_DIR}/qemu/configure ${QEMU_CONFIGURE_OPTIONS}
		BUILD_COMMAND $(MAKE)
		INSTALL_COMMAND $(MAKE) install > /dev/null

		BUILD_ALWAYS TRUE
	)
endif()
