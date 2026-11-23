add_executable(
	unit-tests
	binary.cpp
	board_config.cpp
	qemu_args.cpp
	../src/board_config.cpp
	../src/qemu_args.cpp
	../src/utils/file.cpp
)
target_include_directories(unit-tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_definitions(unit-tests PRIVATE BOARD_DIR="${CMAKE_SOURCE_DIR}/bsp/lib/data/board")
target_link_libraries(unit-tests PRIVATE Catch2::Catch2WithMain ptr89)
target_compile_options(unit-tests PRIVATE -Wall -Wextra -Werror)

include(Catch)
catch_discover_tests(unit-tests)
