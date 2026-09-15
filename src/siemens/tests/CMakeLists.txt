add_executable(
	siemensfw-tests
	crypto.cpp
	eeprom.cpp
	fullflash.cpp
	../../utils/temp_file.cpp
)
target_link_libraries(siemensfw-tests PRIVATE Catch2::Catch2WithMain siemens::fw siemensfw-dependencies)
target_compile_options(siemensfw-tests PRIVATE -Wall -Wextra -Werror)

include(Catch)
catch_discover_tests(siemensfw-tests)
