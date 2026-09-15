#include <catch2/catch_test_macros.hpp>

#include "crypto/md5.h"
#include "siemens/eeprom.h"
#include "siemens/esn.h"
#include "siemens/fullflash.h"
#include "siemens/recalc.h"
#include "utils/binary.h"
#include "utils/file.h"
#include "utils/temp_file.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

static const size_t MODEL_OFFSET = 0x8FC70;
static const size_t VENDOR_OFFSET = 0x8FC80;
static const size_t VENDOR_OFFSETS[] = { 0x220, 0x880, 0xC80, VENDOR_OFFSET };
static const size_t X65_PARTITION_SIZE = 0x20000;
static const size_t X65_ENTRY_SIZE = 16;
static const uint32_t ENTRY_VALID = 0xFFFFFFC0;

static std::vector<uint8_t> makeFullflash(
	uint8_t bootcoreMajor,
	uint8_t bootcoreMinor,
	const std::string &vendor,
	const std::string &model,
	const std::string &imei,
	const std::array<uint8_t, 16> &hash
) {
	std::vector<uint8_t> fullflash(VENDOR_OFFSET + 16, 0xFF);
	writeUInt32LE(&fullflash[0x3C], 0x544B4A43);

	size_t interfaceOffset = bootcoreMajor == 2 ? 0x200 : 0x1200;
	fullflash[interfaceOffset] = bootcoreMinor;
	fullflash[interfaceOffset + 1] = bootcoreMajor;
	writeUInt16LE(&fullflash[interfaceOffset + 2], 0x534C);

	size_t hashOffset;
	size_t imeiOffset;
	if (bootcoreMajor == 3) {
		hashOffset = 0x3E400;
		imeiOffset = 0x3E410;
	} else if (bootcoreMinor == 2) {
		hashOffset = 0x23C;
		imeiOffset = 0x660;
	} else {
		hashOffset = 0x238;
		imeiOffset = 0x65C;
	}

	std::copy(hash.begin(), hash.end(), fullflash.begin() + hashOffset);
	std::copy(imei.begin(), imei.end(), fullflash.begin() + imeiOffset);
	std::copy(model.begin(), model.end(), fullflash.begin() + MODEL_OFFSET);
	fullflash[MODEL_OFFSET + model.size()] = 0;
	std::copy(vendor.begin(), vendor.end(), fullflash.begin() + VENDOR_OFFSET);
	fullflash[VENDOR_OFFSET + vendor.size()] = 0;
	return fullflash;
}

static std::vector<uint8_t> makeX65EelitePartition(uint32_t id, const std::vector<uint8_t> &data) {
	std::vector<uint8_t> eeprom(X65_PARTITION_SIZE, 0xFF);
	std::copy_n("EELITE", 6, eeprom.begin());
	eeprom[6] = 0;
	eeprom[7] = 0;
	eeprom[10] = 0;
	eeprom[11] = 0;
	eeprom[12] = 0xF0;

	size_t entryOffset = X65_PARTITION_SIZE - X65_ENTRY_SIZE;
	writeUInt32LE(&eeprom[entryOffset], ENTRY_VALID);
	writeUInt32LE(&eeprom[entryOffset + 4], id);
	writeUInt32LE(&eeprom[entryOffset + 8], data.size() + 1);
	writeUInt32LE(&eeprom[entryOffset + 12], X65_ENTRY_SIZE);
	std::copy(data.begin(), data.end(), eeprom.begin() + X65_ENTRY_SIZE);
	return eeprom;
}

static std::filesystem::path getFullflashDirectory() {
	const char *configuredDirectory = getenv("PMB887X_FULLFLASH_DIR");
	if (configuredDirectory != nullptr)
		return configuredDirectory;

	const char *homeDirectory = getenv("HOME");
	if (homeDirectory == nullptr)
		return {};

	return std::filesystem::path(homeDirectory) / "Documents/ff";
}

static std::vector<std::filesystem::path> getFullflashPaths(const std::filesystem::path &directory) {
	std::vector<std::filesystem::path> paths;
	for (const auto &entry : std::filesystem::recursive_directory_iterator(directory)) {
		if (entry.is_regular_file() && entry.path().extension() == ".bin")
			paths.push_back(entry.path());
	}
	std::sort(paths.begin(), paths.end());
	return paths;
}

static bool isImei(const std::string &value) {
	if (value.size() != 15)
		return false;

	return std::all_of(value.begin(), value.end(), [](char digit) { return digit >= '0' && digit <= '9'; });
}

static std::string getExpectedModel(const std::filesystem::path &path) {
	auto stem = path.stem().string();
	size_t versionOffset = stem.rfind('v');
	return versionOffset == std::string::npos ? stem : stem.substr(0, versionOffset);
}

static std::string getExpectedDevice(const std::filesystem::path &path) {
	auto device = "siemens-" + getExpectedModel(path);
	std::transform(device.begin(), device.end(), device.begin(), [](uint8_t value) { return std::tolower(value); });
	return device;
}

TEST_CASE("Bootcore information can be read from a fullflash", "[fullflash]") {
	std::array<uint8_t, 16> hash = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

	for (auto version : { std::array<uint8_t, 2> { 2, 0 }, { 2, 2 }, { 3, 0 } }) {
		INFO("Bootcore version: " << (uint32_t) version[0] << "." << (uint32_t) version[1]);

		auto data = makeFullflash(version[0], version[1], "SIEMENS", "CX75", "123456789012345", hash);
		std::vector<uint8_t> eepromData;
		siemens::Eeprom eeprom(eepromData);
		auto info = siemens::getFullflashInfo(eeprom, data);
		REQUIRE(info);
		CHECK(info->device == "siemens-cx75");
		CHECK(info->vendor == "SIEMENS");
		CHECK(info->model == "CX75");
		CHECK(info->bootcoreVersion.major == version[0]);
		CHECK(info->bootcoreVersion.minor == version[1]);
		CHECK(info->imei == "123456789012345");
		CHECK(info->hash == std::vector<uint8_t>(hash.begin(), hash.end()));
	}
}

TEST_CASE("Siemens fullflash can be identified without reading the whole file", "[fullflash]") {
	for (size_t offset : VENDOR_OFFSETS) {
		for (const auto &vendor : { std::string("SIEMENS"), std::string("BENQ-SIEMENS") }) {
			std::vector<uint8_t> fullflash(offset + 16);
			std::copy_n("CX75", 4, fullflash.begin() + offset - 16);
			std::copy(vendor.begin(), vendor.end(), fullflash.begin() + offset);
			TempFileCopy file;
			auto path = file.create(fullflash);

			auto info = siemens::probeFullflash(path);
			REQUIRE(info);
			CHECK(info->device == "siemens-cx75");
			CHECK(info->vendor == vendor);
			CHECK(info->model == "CX75");
		}
	}

	std::vector<uint8_t> fullflash(VENDOR_OFFSET + 16);
	std::copy_n("SIEMENS", 7, fullflash.begin() + VENDOR_OFFSET);
	fullflash[VENDOR_OFFSET + 7] = 1;
	TempFileCopy file;
	auto path = file.create(fullflash);
	CHECK_FALSE(siemens::probeFullflash(path));
}

TEST_CASE("Siemens bootcore identity is recalculated without changing the source file", "[fullflash][recalc]") {
	static const std::array<uint8_t, 16> EXPECTED_HASH = { 0x54, 0xF8, 0x0A, 0xC1, 0x2A, 0xCD, 0x94, 0xB2,
		0xF5, 0xCF, 0xFB, 0x9B, 0xF7, 0xE4, 0xD4, 0x93 };
	std::array<uint8_t, 16> hash{};
	std::fill_n(hash.begin(), 4, 0xFF);
	auto fullflash = makeFullflash(3, 0, "SIEMENS", "CX75", "123456789012345", hash);
	fullflash.resize(0x90000, 0xFF);

	auto recalculated = fullflash;
	siemens::Keys keys;
	keys.imei = "490154203237518";
	keys.esn = 0x12345678;
	keys.skey = 12345678;
	keys.masterKeys.fill(12345678);
	REQUIRE(siemens::recalculateFullflash(recalculated, keys));
	auto layout = siemens::getBootcoreLayout(recalculated);
	REQUIRE(layout);
	CHECK(std::equal(EXPECTED_HASH.begin(), EXPECTED_HASH.end(), recalculated.begin() + layout->hashOffset));
	auto imei = std::string(recalculated.begin() + layout->imeiOffset, recalculated.begin() + layout->imeiOffset + 15);
	CHECK(imei == "490154203237518");
}

TEST_CASE("Siemens OTP recovery is skipped when the bootcore hash is erased", "[fullflash][recover]") {
	std::array<uint8_t, 16> hash;
	hash.fill(0xFF);
	auto fullflash = makeFullflash(3, 0, "SIEMENS", "CX75", "123456789012345", hash);
	TempFileCopy file;
	auto path = file.create(fullflash);

	CHECK_FALSE(siemens::recoverOtp(path));
}

TEST_CASE("Siemens ESN cache stores full identity and is validated by HASH", "[fullflash][esn]") {
	auto fullflash = (std::filesystem::temp_directory_path() / "pmb887x-emu-esn-cache-test.bin").string();
	std::filesystem::remove(fullflash + ".esn");
	siemens::FullflashInfo info;
	info.imei = "490154203237518";
	info.hash = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
		0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE };
	info.bkey = { 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67,
		0x98, 0xBA, 0xDC, 0xFE, 0x10, 0x32, 0x54, 0x76 };
	info.skey = { 0x10, 0x32, 0x54, 0x76 };
	siemens::writeEsnCache(fullflash, info, 0x12345678);
	CHECK_THROWS_AS(siemens::writeEsnCache(fullflash, info, 0x12345678), std::runtime_error);

	auto cache = toml::parse_file(fullflash + ".esn");
	CHECK(cache["ESN"].value_or(std::string()) == "12345678");
	CHECK(cache["IMEI"].value_or(std::string()) == info.imei);
	CHECK(cache["HASH"].value_or(std::string()) == "0123456789ABCDEF1032547698BADCFE");
	CHECK(cache["BKEY"].value_or(std::string()) == "89ABCDEF0123456798BADCFE10325476");
	CHECK(cache["SKEY"].value_or(std::string()) == "10325476");

	uint32_t esn = 0;
	REQUIRE(siemens::readEsnCache(fullflash, info.hash, esn));
	CHECK(esn == 0x12345678);
	info.hash[0] ^= 0xFF;
	CHECK_FALSE(siemens::readEsnCache(fullflash, info.hash, esn));

	std::filesystem::remove(fullflash + ".esn");
}

TEST_CASE("Bootcore layout rejects truncated data", "[fullflash]") {
	std::vector<uint8_t> fullflash(0x1204, 0xFF);
	writeUInt32LE(&fullflash[0x3C], 0x544B4A43);
	fullflash[0x1200] = 0;
	fullflash[0x1201] = 3;
	writeUInt16LE(&fullflash[0x1202], 0x534C);

	CHECK_FALSE(siemens::getBootcoreLayout(fullflash));
}

TEST_CASE("EELITE block 320 provides BKEY without a bootcore hash", "[fullflash]") {
	std::vector<uint8_t> bkey = { 0x42, 0x54, 0x53, 0x42, 0x01, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	auto eepromData = makeX65EelitePartition(320, bkey);
	siemens::Eeprom eeprom(eepromData);
	std::array<uint8_t, 16> hash;
	hash.fill(0xFF);
	auto fullflash = makeFullflash(3, 0, "SIEMENS", "S68", "358439003424460", hash);
	auto expectedHash = md5(bkey.data(), bkey.size());

	auto info = siemens::getFullflashInfo(eeprom, fullflash);
	REQUIRE(info);
	CHECK(info->bkey == bkey);
	CHECK(info->hash == std::vector<uint8_t>(expectedHash.begin(), expectedHash.end()));
}

TEST_CASE("Fullflash corpus preserves device and available identity invariants", "[fullflash]") {
	auto directory = getFullflashDirectory();
	if (directory.empty() || !std::filesystem::is_directory(directory))
		SKIP("Fullflash corpus is not available: " << directory);

	auto paths = getFullflashPaths(directory);
	REQUIRE_FALSE(paths.empty());
	size_t imeiCount = 0;
	size_t bkeyCount = 0;
	size_t skeyCount = 0;
	size_t hashCount = 0;

	for (const auto &path : paths) {
		INFO("Fullflash: " << std::filesystem::relative(path, directory).string());
		auto probe = siemens::probeFullflash(path.string());
		REQUIRE(probe);
		CHECK(probe->device == getExpectedDevice(path));
		CHECK(probe->model == getExpectedModel(path));

		std::vector<uint8_t> fullflash;
		REQUIRE(readFile(path.string(), fullflash));
		siemens::Eeprom eeprom(fullflash);
		auto info = siemens::getFullflashInfo(eeprom, fullflash);
		REQUIRE(info);
		CHECK(info->device == getExpectedDevice(path));
		CHECK(info->model == getExpectedModel(path));
		if (!info->imei.empty()) {
			CHECK(isImei(info->imei));
			imeiCount++;
		}
		if (!info->bkey.empty())
			bkeyCount++;
		if (!info->skey.empty())
			skeyCount++;
		if (!info->hash.empty())
			hashCount++;
		if (!info->bkey.empty() && !info->hash.empty()) {
			auto calculatedHash = md5(info->bkey.data(), info->bkey.size());
			CHECK(info->hash == std::vector<uint8_t>(calculatedHash.begin(), calculatedHash.end()));
		}
	}

	CHECK(imeiCount > 0);
	CHECK(bkeyCount > 0);
	CHECK(skeyCount > 0);
	CHECK(hashCount > 0);
}

TEST_CASE("EEPROM blocks can be read and written in the fullflash corpus", "[fullflash]") {
	auto directory = getFullflashDirectory();
	if (directory.empty() || !std::filesystem::is_directory(directory))
		SKIP("Fullflash corpus is not available: " << directory);

	auto paths = getFullflashPaths(directory);
	REQUIRE_FALSE(paths.empty());

	for (const auto &path : paths) {
		INFO("Fullflash: " << std::filesystem::relative(path, directory).string());

		std::vector<uint8_t> fullflash;
		REQUIRE(readFile(path.string(), fullflash));

		siemens::Eeprom eeprom(fullflash);
		auto block = eeprom.readBlock(76);
		REQUIRE_FALSE(block.empty());
		block[0] ^= 0xFF;
		CHECK(eeprom.writeBlock(76, block));

		auto updatedBlock = eeprom.readBlock(76);
		CHECK(updatedBlock == block);

		updatedBlock.push_back(0);
		CHECK_THROWS_AS(eeprom.writeBlock(76, updatedBlock), siemens::EepromError);
		CHECK_THROWS_AS(eeprom.readBlock(0xFFFFFFFF), siemens::EepromError);
	}
}
