#pragma once

#include <cstddef>
#include <cstdint>
#include <tuple>

namespace siemens {

class Eeprom;
struct FullflashInfo;

// Recovers the ESN from the available fullflash identity and EEPROM security blocks.
// Brute-force methods sweep the whole 32-bit space. threadCount = 0 uses every core.
std::tuple<bool, uint32_t> recoverEsn(const Eeprom &eeprom, const FullflashInfo &info, size_t threadCount = 0);

}
