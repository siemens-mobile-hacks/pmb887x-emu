#include "qemu_args.h"

std::vector<std::string> getQemuClockArguments() {
	// Device timers run on the vCPU thread: a sleeping main loop cannot delay them.
	return { "-accel", "tcg,vcpu-timers=on" };
}
