#include "qemu_args.h"

std::vector<std::string> getQemuClockArguments(
	const std::optional<std::string> &accel,
	const std::optional<std::string> &icount
) {
	// Device timers run on the vCPU thread: a sleeping main loop cannot delay them.
	std::string defaultAccel = icount ? "tcg" : "tcg,vcpu-timers=on,cross-page-links=on";
	std::vector<std::string> arguments = { "-accel", accel.value_or(defaultAccel) };
	if (icount) {
		arguments.emplace_back("-icount");
		arguments.emplace_back(*icount);
	}
	return arguments;
}
