#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <tuple>

namespace SiemensFW {

class Eeprom;
struct FullflashInfo;

enum class EsnRecoveryMethod {
	KNOWN_HASH,
	BLOCK_5468,
	BKEY,
	HASH,
	BLOCK_5008,
	BLOCK_5121,
	BLOCK_5123,
	BLOCK_5077,
};

using EsnProgressCallback = std::function<void(EsnRecoveryMethod method, uint32_t percent)>;

// Recovers the ESN from the available fullflash identity and EEPROM security blocks.
// Brute-force methods sweep the whole 32-bit space. threadCount = 0 uses every core.
// Progress is reported on the calling thread and runs from 0 to 100 for each attempted method.
std::tuple<bool, uint32_t> recoverEsn(
	const Eeprom &eeprom,
	const FullflashInfo &info,
	size_t threadCount = 0,
	const EsnProgressCallback &progress = {}
);

}
