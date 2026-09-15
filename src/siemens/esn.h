#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace siemens {

struct FullflashInfo;

std::string esnToHex(uint32_t esn);
bool readEsnCache(const std::string &fullflash, const std::vector<uint8_t> &hash, uint32_t &esn);
void writeEsnCache(const std::string &fullflash, const FullflashInfo &info, uint32_t esn);

}
