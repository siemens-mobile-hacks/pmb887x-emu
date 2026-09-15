#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace siemens {

class Eeprom;

struct BootcoreVersion {
	uint8_t major = 0;
	uint8_t minor = 0;
};

struct BootcoreLayout {
	BootcoreVersion version;
	size_t hashOffset = 0;
	size_t imeiOffset = 0;
};

struct FullflashInfo {
	std::string device;
	std::string vendor;
	std::string model;
	BootcoreVersion bootcoreVersion;
	std::string imei;
	std::vector<uint8_t> bkey;
	std::vector<uint8_t> skey;
	std::vector<uint8_t> hash;
};

struct Otp {
	std::string otp0;
	std::string otp1;
};

std::optional<FullflashInfo> probeFullflash(const std::string &path);
std::optional<BootcoreLayout> getBootcoreLayout(const std::vector<uint8_t> &fullflash);
std::optional<FullflashInfo> getFullflashInfo(const Eeprom &eeprom, const std::vector<uint8_t> &fullflash);
bool isRecalculated(const std::string &path);
std::optional<Otp> recoverOtp(const std::string &path);

}
