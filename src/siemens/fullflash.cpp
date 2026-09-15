#include "fullflash.h"

#include "crypto/md5.h"
#include "siemens/bruteforce.h"
#include "siemens/crypto.h"
#include "siemens/eeprom.h"
#include "siemens/esn.h"
#include "siemens/otp.h"
#include "utils/binary.h"
#include "utils/file.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <stdexcept>

namespace siemens {

static const size_t BOOTCORE_MAGIC_OFFSET = 0x3C;
static const size_t INTERFACE_OFFSETS[] = { 0x200, 0x1200 };
static const size_t MODEL_OFFSET = 0x8FC70;
static const size_t VENDOR_OFFSET = 0x8FC80;
static const size_t VENDOR_OFFSETS[] = { VENDOR_OFFSET, 0x220, 0x880, 0xC80 };
static const size_t BCORE_HASH_OFFSETS[] = { 0x238, 0x23C, 0x3E400 };
static const uint32_t BOOTCORE_MAGIC = 0x544B4A43;
static const uint16_t INTERFACE_MAGIC = 0x534C;
static const std::array<uint8_t, 16> RECALCULATED_BCORE_HASH = {
	0x54, 0xF8, 0x0A, 0xC1, 0x2A, 0xCD, 0x94, 0xB2, 0xF5, 0xCF, 0xFB, 0x9B, 0xF7, 0xE4, 0xD4, 0x93,
};

struct Identity {
	std::string esn;
	std::string imei;
};

static std::string buildDeviceName(const std::string &model) {
	auto device = "siemens-" + model;
	std::transform(device.begin(), device.end(), device.begin(), [](uint8_t value) { return std::tolower(value); });
	return device;
}

static bool readString(std::ifstream &input, size_t offset, size_t length, std::string &value) {
	std::vector<char> data(length);
	input.clear();
	input.seekg((std::streamoff) offset);
	if (!input.read(data.data(), (std::streamsize) data.size()))
		return false;

	auto end = std::find(data.begin(), data.end(), 0);
	if (end == data.end())
		return false;
	if (!std::all_of(end, data.end(), [](char character) { return character == 0; }))
		return false;

	value.assign(data.begin(), end);
	return !value.empty();
}

std::optional<FullflashInfo> probeFullflash(const std::string &path) {
	std::ifstream input(path, std::ios::binary);
	if (!input)
		return std::nullopt;

	for (size_t offset : VENDOR_OFFSETS) {
		FullflashInfo info;
		if (!readString(input, offset, 16, info.vendor))
			continue;
		if (info.vendor != "SIEMENS" && info.vendor != "BENQ-SIEMENS")
			continue;
		if (!readString(input, offset - 16, 16, info.model))
			continue;

		info.device = buildDeviceName(info.model);
		return info;
	}
	return std::nullopt;
}

static bool readString(const std::vector<uint8_t> &data, size_t offset, size_t length, std::string &value) {
	if (offset > data.size())
		return false;
	if (length > data.size() - offset)
		return false;

	value.clear();
	for (size_t index = 0; index < length; index++) {
		uint8_t character = data[offset + index];
		if (character == 0)
			return true;
		value.push_back((char) character);
	}

	return false;
}

static std::optional<BootcoreVersion> readBootcoreVersion(const std::vector<uint8_t> &fullflash) {
	for (size_t offset : INTERFACE_OFFSETS) {
		if (fullflash.size() < offset + 4)
			continue;
		if (readUInt16LE(&fullflash[offset + 2]) != INTERFACE_MAGIC)
			continue;

		return BootcoreVersion { fullflash[offset + 1], fullflash[offset] };
	}
	return std::nullopt;
}

std::optional<BootcoreLayout> getBootcoreLayout(const std::vector<uint8_t> &fullflash) {
	if (fullflash.size() < BOOTCORE_MAGIC_OFFSET + 4)
		return std::nullopt;
	if (readUInt32LE(&fullflash[BOOTCORE_MAGIC_OFFSET]) != BOOTCORE_MAGIC)
		return std::nullopt;

	auto version = readBootcoreVersion(fullflash);
	if (!version)
		return std::nullopt;

	uint16_t versionNumber = ((uint16_t) version->major << 8) | version->minor;
	BootcoreLayout layout;
	switch (versionNumber) {
		case 0x0200:
			layout = { *version, 0x238, 0x65C };
			break;
		case 0x0202:
			layout = { *version, 0x23C, 0x660 };
			break;
		case 0x0300:
			layout = { *version, 0x3E400, 0x3E410 };
			break;
		default:
			return std::nullopt;
	}

	size_t requiredSize = std::max(layout.hashOffset + 16, layout.imeiOffset + 15);
	if (fullflash.size() < requiredSize)
		return std::nullopt;
	return layout;
}

static bool decodeImeiBlock(std::vector<uint8_t> data, bool fullBlock, std::string &imei) {
	if (data.size() != 10)
		return false;

	imeiCipherDecrypt(data.data(), fullBlock);

	uint8_t xorEven = 0;
	uint8_t xorOdd = 0;
	for (size_t index = 0; index < 8; index++) {
		if ((index & 1) != 0) {
			xorOdd ^= data[index];
		} else {
			xorEven ^= data[index];
		}
	}
	if (data[8] != (fullBlock ? xorOdd : xorEven))
		return false;
	if (data[9] != (uint8_t) (xorOdd ^ xorEven ^ 0xFF))
		return false;

	std::string value;
	value.reserve(15);
	for (size_t index = 0; index < 7; index++) {
		uint8_t low = (data[index] & 0xF);
		uint8_t high = (data[index] >> 4);
		if (low > 9)
			return false;
		if (high > 9)
			return false;
		value += (char) ('0' + low);
		value += (char) ('0' + high);
	}
	uint8_t checkDigit = (data[7] >> 4);
	if ((data[7] & 0xF) != 0)
		return false;
	if (calculateImeiCheckDigit(value) != checkDigit)
		return false;
	value += (char) ('0' + checkDigit);

	imei = value;
	return true;
}

std::optional<FullflashInfo> getFullflashInfo(const Eeprom &eeprom, const std::vector<uint8_t> &fullflash) {
	auto bootcore = getBootcoreLayout(fullflash);
	if (!bootcore)
		return std::nullopt;

	FullflashInfo info;
	if (!readString(fullflash, VENDOR_OFFSET, 16, info.vendor))
		return std::nullopt;
	if (info.vendor != "SIEMENS" && info.vendor != "BENQ-SIEMENS")
		return std::nullopt;
	if (!readString(fullflash, MODEL_OFFSET, 16, info.model))
		return std::nullopt;
	if (info.model.empty())
		return std::nullopt;

	const char *imei = (const char *) &fullflash[bootcore->imeiOffset];
	bool hasImei = std::all_of(imei, imei + 15, [](char digit) { return digit >= '0' && digit <= '9'; });
	if (hasImei)
		info.imei.assign(imei, 15);
	if (!isErasedData(&fullflash[bootcore->hashOffset], 16))
		info.hash.assign(fullflash.begin() + bootcore->hashOffset, fullflash.begin() + bootcore->hashOffset + 16);

	std::vector<uint8_t> block;
	if (info.imei.empty() && eeprom.hasBlock(76)) {
		block = eeprom.readBlock(76);
		decodeImeiBlock(block, false, info.imei);
	}
	if (info.imei.empty() && eeprom.hasBlock(5009)) {
		block = eeprom.readBlock(5009);
		decodeImeiBlock(block, true, info.imei);
	}

	if (eeprom.hasBlock(5122)) {
		block = eeprom.readBlock(5122);
		if (block.size() >= 4)
			info.skey.assign(block.begin(), block.begin() + 4);
	}

	for (uint32_t id : { 52, 320 }) {
		if (!eeprom.hasBlock(id))
			continue;

		block = eeprom.readBlock(id);
		if (block.size() < 16)
			continue;

		std::array<uint8_t, 16> bkey;
		std::copy_n(block.begin(), bkey.size(), bkey.begin());
		if (isErasedData(bkey.data(), bkey.size()))
			continue;

		auto calculatedHash = md5(bkey.data(), bkey.size());
		if (!info.hash.empty() && !std::equal(calculatedHash.begin(), calculatedHash.end(), info.hash.begin()))
			continue;

		info.bkey.assign(bkey.begin(), bkey.end());
		if (info.hash.empty())
			info.hash.assign(calculatedHash.begin(), calculatedHash.end());
		break;
	}

	info.device = buildDeviceName(info.model);
	info.bootcoreVersion = bootcore->version;
	return info;
}

static std::vector<uint8_t> readFullflash(const std::string &path) {
	std::vector<uint8_t> data;
	if (!readFile(path, data))
		throw std::runtime_error("Can't read fullflash: " + path);
	return data;
}

bool isRecalculated(const std::string &path) {
	std::ifstream input(path, std::ios::binary);
	if (!input)
		return false;

	std::array<uint8_t, 16> hash;
	for (size_t offset : BCORE_HASH_OFFSETS) {
		input.clear();
		input.seekg((std::streamoff) offset);
		if (!input.read((char *) hash.data(), (std::streamsize) hash.size()))
			continue;
		if (hash == RECALCULATED_BCORE_HASH)
			return true;
	}
	return false;
}

static Identity recoverIdentity(const std::string &path, std::vector<uint8_t> &data) {
	Eeprom eeprom(data);
	auto info = getFullflashInfo(eeprom, data);
	if (!info)
		throw std::runtime_error("Can't recover ESN: unknown fullflash");

	bool canUseCache = !info->hash.empty();

	uint32_t esn = 0;
	if (canUseCache && readEsnCache(path, info->hash, esn)) {
		spdlog::info("[otp] Using cached ESN {}", esnToHex(esn));
		return Identity { esnToHex(esn), info->imei };
	}

	auto started = std::chrono::steady_clock::now();
	auto [found, recoveredEsn] = recoverEsn(eeprom, *info);
	if (!found)
		throw std::runtime_error("ESN not found: inconsistent fullflash keys");

	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started);
	spdlog::info("[otp] Recovered ESN {} for IMEI {} in {} s", esnToHex(recoveredEsn), info->imei, elapsed.count());
	if (canUseCache) {
		try {
			writeEsnCache(path, *info, recoveredEsn);
		} catch (const std::exception &error) {
			spdlog::warn("{}", error.what());
		}
	}
	return Identity { esnToHex(recoveredEsn), info->imei };
}

std::optional<Otp> recoverOtp(const std::string &path) {
	auto data = readFullflash(path);
	auto bootcore = getBootcoreLayout(data);
	if (!bootcore)
		throw std::runtime_error("Can't recover OTP: unknown bootcore");
	if (isErasedData(&data[bootcore->hashOffset], 16)) {
		spdlog::info("[otp] BCORE HASH is erased, recovery is not needed");
		return std::nullopt;
	}

	auto identity = recoverIdentity(path, data);
	return Otp { esnToOtp(identity.esn), imeiToOtp(identity.imei) };
}

}
