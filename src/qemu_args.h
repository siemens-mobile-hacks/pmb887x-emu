#pragma once

#include <optional>
#include <string>
#include <vector>

std::vector<std::string> getQemuClockArguments(
	const std::optional<std::string> &accel,
	const std::optional<std::string> &icount
);
