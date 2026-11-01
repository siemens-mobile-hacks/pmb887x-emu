#include <catch2/catch_test_macros.hpp>

#include "qemu_args.h"

#include <string>
#include <vector>

TEST_CASE("QEMU clock arguments run device timers on the vCPU by default") {
	CHECK(getQemuClockArguments(std::nullopt, std::nullopt) == std::vector<std::string>{ "-accel", "tcg,vcpu-timers=on,cross-page-links=on" });
}

TEST_CASE("QEMU clock arguments pass a custom accelerator unchanged") {
	CHECK(getQemuClockArguments("tcg,thread=single", std::nullopt) == std::vector<std::string>{ "-accel", "tcg,thread=single" });
}

TEST_CASE("QEMU clock arguments with icount drop vCPU timers") {
	std::vector<std::string> expected = { "-accel", "tcg", "-icount", "shift=3,sleep=off" };
	CHECK(getQemuClockArguments(std::nullopt, "shift=3,sleep=off") == expected);
}
