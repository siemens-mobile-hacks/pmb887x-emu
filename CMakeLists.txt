cmake_minimum_required(VERSION 3.16)

project(pmb887x-emu C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(ExternalProject)
include(CheckIPOSupported)
include(CTest)
include(GNUInstallDirs)

execute_process(
	COMMAND git describe --tags --dirty
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE PROJECT_VERSION
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file("src/config.h.in" "config.h" @ONLY)

add_subdirectory(third_party/argparse)

if (BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
	set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
	set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
	add_subdirectory(third_party/catch2 EXCLUDE_FROM_ALL)
	set(SIEMENSFW_BUILD_TESTS ON)
endif()

add_subdirectory(src/siemens)

add_executable(
	emu
	src/main.cpp
	src/utils/file.cpp
	src/utils/process.cpp
	src/utils/string.cpp
	src/utils/temp_file.cpp
)
set_target_properties(emu PROPERTIES OUTPUT_NAME "pmb887x-emu")
target_include_directories(emu PRIVATE "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/src")
install(TARGETS emu RUNTIME DESTINATION bin)

if (BUILD_STATIC)
	target_link_options(emu PUBLIC -static -static-libgcc -static-libstdc++)
endif()

if (CMAKE_BUILD_TYPE MATCHES "Release")
	check_ipo_supported(RESULT HAVE_IPO)
	if (HAVE_IPO)
		set_property(TARGET emu siemensfw PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()
	target_link_options(emu PUBLIC -s)
endif()

target_link_libraries(emu PUBLIC argparse SiemensFW::SiemensFW siemensfw-dependencies)
target_compile_options(emu PUBLIC -Wall -Wextra -Werror -O3)

if (BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
	add_subdirectory(tests)
endif()

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
	add_custom_command(TARGET emu POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/boards/include")

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
	file(GLOB BOARDS_LIST "${CMAKE_SOURCE_DIR}/bsp/lib/data/board/*.toml")
	foreach(board_file IN LISTS BOARDS_LIST)
		add_custom_command(TARGET emu POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${board_file}" "${DIST_DIR}/boards/"
			VERBATIM
		)
	endforeach()

	file(GLOB BOARDS_LIST "${CMAKE_SOURCE_DIR}/bsp/lib/data/board/include/*.toml")
	foreach(board_file IN LISTS BOARDS_LIST)
		add_custom_command(TARGET emu POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${board_file}" "${DIST_DIR}/boards/include/"
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
		--enable-iconv
		--enable-pixman
		--enable-tcg
		--enable-capstone
	)

	if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		SET(QEMU_CONFIGURE_OPTIONS
			${QEMU_CONFIGURE_OPTIONS}
			--enable-cocoa
			--enable-coreaudio
		)
	else()
		SET(QEMU_CONFIGURE_OPTIONS
			${QEMU_CONFIGURE_OPTIONS}
			--enable-smartcard
			--enable-gtk
			--enable-iconv
			--enable-libudev
			--enable-linux-aio
			--enable-linux-io-uring
			--enable-pa
			--enable-xkbcommon
			--enable-multiprocess
			--enable-malloc-trim
		)
	endif()

	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		SET(QEMU_CONFIGURE_OPTIONS ${QEMU_CONFIGURE_OPTIONS} --enable-lto --enable-strip)
	endif()

	if (BUILD_STATIC)
		SET(QEMU_CONFIGURE_OPTIONS ${QEMU_CONFIGURE_OPTIONS} --static)
	endif()

	install(
		DIRECTORY ${CMAKE_SOURCE_DIR}/bsp/lib/data/board/
		DESTINATION share/pmb887x-emu/boards
		FILES_MATCHING PATTERN "*.toml"
	)

	install(
		DIRECTORY ${CMAKE_BINARY_DIR}/qemu-install/
		DESTINATION share/pmb887x-emu/qemu
		USE_SOURCE_PERMISSIONS
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
