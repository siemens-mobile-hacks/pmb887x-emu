#include <catch2/catch_test_macros.hpp>

#include "qemu_args.h"

#include <string>
#include <vector>

TEST_CASE("QEMU clock arguments run device timers on the vCPU") {
	CHECK(getQemuClockArguments() == std::vector<std::string>{ "-accel", "tcg,vcpu-timers=on" });
}
