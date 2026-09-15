#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace siemens {

/*
 * In-memory "recalculation" of a Siemens x65/x75/x85 fullflash for a given
 * IMEI / ESN / SKEY, equivalent to "Recalc Fullflash" in x65PapuaUtils.
 * See docs/recalc-algorithm.md for the description of the algorithm.
 */

struct Keys {
	std::string imei;
	uint32_t esn = 0;
	uint32_t skey = 0;
	std::array<uint32_t, 6> masterKeys{};
	uint8_t verDown = 1;
};

struct RecalcResult {
	bool complete = false;
	bool changed = false;
};

// Patches fullflash in place. Nothing is modified when the image already contains the right keys.
std::optional<RecalcResult> recalculateFullflash(std::vector<uint8_t> &fullflash, const Keys &keys);

}
