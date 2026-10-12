#include "qemu_args.h"

std::vector<std::string> getQemuClockArguments(bool realtime) {
	if (realtime)
		return {};

	return { "-icount", "precise-clocks=on" };
}
