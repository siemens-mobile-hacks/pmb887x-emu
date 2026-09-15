#include "esn.h"

#include "fullflash.h"
#include "utils/string.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace siemens {

std::string esnToHex(uint32_t esn) {
	static const char HEX_DIGITS[] = "0123456789ABCDEF";
	std::string result;
	for (int shift = 28; shift >= 0; shift -= 4)
		result += HEX_DIGITS[((esn >> shift) & 0xF)];
	return result;
}

bool readEsnCache(const std::string &fullflash, const std::vector<uint8_t> &hash, uint32_t &esn) {
	if (hash.size() != 16)
		return false;

	try {
		auto cache = toml::parse_file(fullflash + ".esn");
		auto cachedHash = cache["HASH"].value<std::string>();
		auto value = cache["ESN"].value<std::string>();
		if (!cachedHash || !value)
			return false;
		if (*cachedHash != bytesToHex(hash.data(), hash.size()))
			return false;
		if (value->size() != 8)
			return false;
		if (!std::all_of(value->begin(), value->end(), [](uint8_t character) { return std::isxdigit(character); }))
			return false;

		esn = (uint32_t) std::stoul(*value, nullptr, 16);
		return true;
	} catch (const toml::parse_error &) {
		return false;
	}
}

void writeEsnCache(const std::string &fullflash, const FullflashInfo &info, uint32_t esn) {
	auto path = fullflash + ".esn";
	if (std::filesystem::exists(path))
		throw std::runtime_error("ESN cache already exists: " + path);

	std::ofstream output(path, std::ios::trunc);
	if (!output)
		throw std::runtime_error("Can't write ESN cache: " + path);

	toml::table cache {
		{ "ESN", esnToHex(esn) },
		{ "IMEI", info.imei },
		{ "HASH", bytesToHex(info.hash.data(), info.hash.size()) },
		{ "BKEY", bytesToHex(info.bkey.data(), info.bkey.size()) },
		{ "SKEY", bytesToHex(info.skey.data(), info.skey.size()) },
	};

	output << cache;

	if (!output)
		throw std::runtime_error("Can't write ESN cache: " + path);
}

}
