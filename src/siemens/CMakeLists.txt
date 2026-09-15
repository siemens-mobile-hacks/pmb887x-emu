option(SIEMENSFW_BUILD_TESTS "Build Siemens firmware library tests" OFF)

add_library(siemensfw-dependencies INTERFACE)
target_include_directories(
	siemensfw-dependencies INTERFACE
	"${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/spdlog/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/tomlplusplus/include"
)

add_library(
	siemensfw STATIC
	bruteforce.cpp
	crypto.cpp
	eeprom.cpp
	esn.cpp
	fullflash.cpp
	otp.cpp
	recalc.cpp
	../crypto/md4.cpp
	../crypto/md5.cpp
	../utils/file.cpp
	../utils/string.cpp
)
add_library(SiemensFW::SiemensFW ALIAS siemensfw)
target_include_directories(siemensfw PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(siemensfw PRIVATE siemensfw-dependencies)
target_compile_features(siemensfw PUBLIC cxx_std_20)
target_compile_options(siemensfw PRIVATE -Wall -Wextra -Werror -O3)

if (EMSCRIPTEN)
	target_compile_options(siemensfw PUBLIC -msimd128 -pthread)
	target_link_options(siemensfw INTERFACE -msimd128 -pthread)
endif()

if (SIEMENSFW_BUILD_TESTS)
	add_subdirectory(tests)
endif()
