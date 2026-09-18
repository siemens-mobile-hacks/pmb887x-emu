add_executable(
	unit-tests
	binary.cpp
	../src/utils/file.cpp
)
target_include_directories(unit-tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(unit-tests PRIVATE Catch2::Catch2WithMain)
target_compile_options(unit-tests PRIVATE -Wall -Wextra -Werror)

include(Catch)
catch_discover_tests(unit-tests)
