#include <catch2/catch_test_macros.hpp>

#include "qemu_args.h"

#include <string>
#include <vector>

TEST_CASE("QEMU clock arguments select instruction-counted or realtime mode") {
	CHECK(getQemuClockArguments(false) == std::vector<std::string>{ "-icount", "precise-clocks=on" });
	CHECK(getQemuClockArguments(true).empty());
}
