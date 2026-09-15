#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "siemens/bruteforce.h"
#include "siemens/crypto.h"
#include "siemens/eeprom.h"
#include "siemens/fullflash.h"
#include "siemens/recalc.h"
#include "utils/binary.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <vector>

static const size_t X65_PARTITION_SIZE = 0x20000;
static const size_t X85_PARTITION_SIZE = 0x40000;
static const size_t X65_HEADER_SIZE = 16;
static const size_t X85_HEADER_SIZE = 32;
static const size_t X65_ENTRY_SIZE = 16;
static const size_t X85_ENTRY_SIZE = 32;
static const size_t X85_DATA_CHUNK_SIZE = 16;
static const size_t X85_EELITE_MAX_LINEAR_SIZE = 0x10;
static const size_t X85_EEFULL_MAX_LINEAR_SIZE = 0x200;
static const uint32_t EEFULL_ID_OFFSET = 5000;
static const uint32_t ENTRY_FREE = 0xFFFFFFFF;
static const uint32_t ENTRY_VALID = 0xFFFFFFC0;
static const uint32_t ENTRY_OBSOLETE = 0xFFFFFF00;

enum class TestEepromLayout {
	X65,
	X85,
};

struct TestEepromBlock {
	uint32_t id = 0;
	std::vector<uint8_t> data;
	uint32_t state = ENTRY_VALID;
	bool extendedMetadata = false;
};

static void writePartitionHeader(std::vector<uint8_t> &eeprom, size_t offset, std::string_view name) {
	std::copy(name.begin(), name.end(), eeprom.begin() + offset);
	eeprom[offset + 6] = 0;
	eeprom[offset + 7] = 0;
	eeprom[offset + 10] = 0;
	eeprom[offset + 11] = 0;
	eeprom[offset + 12] = 0xF0;
}

static std::vector<uint8_t> makeStoredBlock(TestEepromLayout layout, bool full, const TestEepromBlock &block) {
	auto stored = block.data;
	if (full) {
		stored.insert(stored.begin(), block.extendedMetadata ? 0x80 : 0);
		if (block.extendedMetadata)
			stored.insert(stored.end(), 5, 0xFF);
	} else if (layout == TestEepromLayout::X65) {
		stored.push_back(0xFF);
	}
	return stored;
}

static size_t alignDataOffset(size_t offset, size_t chunkSize) {
	if ((offset & chunkSize) == 0)
		return offset;

	size_t stride = chunkSize * 2;
	return offset + stride - (offset & (stride - 1));
}

static size_t getStridedByteOffset(size_t offset, size_t index, size_t chunkSize) {
	size_t chunkOffset = (offset & (chunkSize - 1));
	return offset + index + (((chunkOffset + index) / chunkSize) * chunkSize);
}

static void writeX65Partition(
	std::vector<uint8_t> &eeprom,
	size_t partitionOffset,
	bool full,
	const std::vector<TestEepromBlock> &blocks
) {
	writePartitionHeader(eeprom, partitionOffset, full ? "EEFULL" : "EELITE");

	size_t entryOffset = partitionOffset + X65_PARTITION_SIZE - X65_ENTRY_SIZE;
	size_t dataOffset = X65_HEADER_SIZE;
	for (const auto &block : blocks) {
		if (block.state != ENTRY_FREE) {
			auto stored = makeStoredBlock(TestEepromLayout::X65, full, block);
			writeUInt32LE(&eeprom[entryOffset], block.state);
			writeUInt32LE(&eeprom[entryOffset + 4], full ? block.id - EEFULL_ID_OFFSET : block.id);
			writeUInt32LE(&eeprom[entryOffset + 8], stored.size());
			writeUInt32LE(&eeprom[entryOffset + 12], dataOffset);
			std::copy(stored.begin(), stored.end(), eeprom.begin() + partitionOffset + dataOffset);
			dataOffset += stored.size();
		}
		entryOffset -= X65_ENTRY_SIZE;
	}
}

static void writeX85Partition(
	std::vector<uint8_t> &eeprom,
	size_t partitionOffset,
	bool full,
	const std::vector<TestEepromBlock> &blocks
) {
	writePartitionHeader(eeprom, partitionOffset + X85_PARTITION_SIZE - X85_HEADER_SIZE, full ? "EEFULL" : "EELITE");

	size_t entryOffset = partitionOffset + X85_PARTITION_SIZE - X85_HEADER_SIZE - X85_ENTRY_SIZE;
	size_t dataOffset = X85_HEADER_SIZE;
	for (const auto &block : blocks) {
		if (block.state == ENTRY_FREE) {
			entryOffset -= X85_ENTRY_SIZE;
			continue;
		}

		auto stored = makeStoredBlock(TestEepromLayout::X85, full, block);
		writeUInt32LE(&eeprom[entryOffset], block.state);
		if (full) {
			writeUInt32LE(&eeprom[entryOffset + 4], block.id - EEFULL_ID_OFFSET);
			writeUInt32LE(&eeprom[entryOffset + 8], stored.size());
		} else {
			writeUInt16LE(&eeprom[entryOffset + 4], block.id);
			writeUInt16LE(&eeprom[entryOffset + 8], stored.size());
		}

		size_t maxLinearSize = full ? X85_EEFULL_MAX_LINEAR_SIZE : X85_EELITE_MAX_LINEAR_SIZE;
		if (stored.size() <= maxLinearSize) {
			size_t reservedSize = X85_DATA_CHUNK_SIZE + stored.size() + (stored.size() & ~(X85_DATA_CHUNK_SIZE - 1));
			size_t storedOffset = alignDataOffset(entryOffset - reservedSize, X85_DATA_CHUNK_SIZE);
			for (size_t index = 0; index < stored.size(); index++)
				eeprom[getStridedByteOffset(storedOffset, index, X85_DATA_CHUNK_SIZE)] = stored[index];

			size_t stride = X85_DATA_CHUNK_SIZE * 2;
			entryOffset = (storedOffset & ~(stride - 1)) - X85_ENTRY_SIZE;
		} else {
			writeUInt32LE(&eeprom[entryOffset + 12], dataOffset);
			std::copy(stored.begin(), stored.end(), eeprom.begin() + partitionOffset + dataOffset);
			dataOffset += stored.size();
			entryOffset -= X85_ENTRY_SIZE;
		}
	}
}

static std::vector<uint8_t> makeEeprom(TestEepromLayout layout, std::initializer_list<TestEepromBlock> blocks) {
	std::vector<TestEepromBlock> liteBlocks;
	std::vector<TestEepromBlock> fullBlocks;
	for (const auto &block : blocks) {
		if (block.id < EEFULL_ID_OFFSET) {
			liteBlocks.push_back(block);
		} else {
			fullBlocks.push_back(block);
		}
	}

	size_t partitionSize = layout == TestEepromLayout::X65 ? X65_PARTITION_SIZE : X85_PARTITION_SIZE;
	size_t partitionCount = !liteBlocks.empty() + !fullBlocks.empty();
	std::vector<uint8_t> eeprom(partitionSize * partitionCount, 0xFF);
	size_t partitionOffset = 0;
	if (!liteBlocks.empty()) {
		if (layout == TestEepromLayout::X65) {
			writeX65Partition(eeprom, partitionOffset, false, liteBlocks);
		} else {
			writeX85Partition(eeprom, partitionOffset, false, liteBlocks);
		}
		partitionOffset += partitionSize;
	}
	if (!fullBlocks.empty()) {
		if (layout == TestEepromLayout::X65) {
			writeX65Partition(eeprom, partitionOffset, true, fullBlocks);
		} else {
			writeX85Partition(eeprom, partitionOffset, true, fullBlocks);
		}
	}
	return eeprom;
}

static std::vector<uint8_t> makeRecalcFullflash(std::initializer_list<TestEepromBlock> blocks, const siemens::Keys &keys) {
	auto partition = makeEeprom(TestEepromLayout::X65, blocks);
	std::vector<uint8_t> eeprom(0x100000, 0xFF);
	std::copy(partition.begin(), partition.end(), eeprom.begin() + 0x40000);
	writeUInt32LE(&eeprom[0x3C], 0x544B4A43);
	eeprom[0x200] = 0;
	eeprom[0x201] = 2;
	writeUInt16LE(&eeprom[0x202], 0x534C);
	eeprom[0x238] = 0;
	auto result = siemens::recalculateFullflash(eeprom, keys);
	REQUIRE(result);
	return eeprom;
}

static void writeChecksum(uint8_t *data, size_t size) {
	uint8_t sum = 0;
	uint8_t xorValue = 0;
	for (size_t index = 0; index < size; index++) {
		sum += data[index];
		xorValue ^= data[index];
	}
	data[size] = sum;
	data[size + 1] = xorValue;
}

TEST_CASE("X65 parsing stops at the first free EIT entry", "[eeprom]") {
	auto eepromData = makeEeprom(TestEepromLayout::X65, {
		{ 1, { 1, 2, 3 } },
		{ 0, {}, ENTRY_FREE },
		{ 2, { 4 } },
	});

	siemens::Eeprom eeprom(eepromData);
	CHECK_FALSE(eeprom.hasBlock(2));
	CHECK((eeprom.readBlock(1) == std::vector<uint8_t> { 1, 2, 3 }));
}

TEST_CASE("Unknown X65 EIT states are rejected", "[eeprom]") {
	auto eepromData = makeEeprom(TestEepromLayout::X65, {
		{ 1, { 0 }, 0xFFFFFFF0 },
	});

	CHECK_THROWS_WITH(siemens::Eeprom(eepromData), Catch::Matchers::ContainsSubstring("0001FFF0"));
}

TEST_CASE("EELITE block IDs cannot overlap the EEFULL range", "[eeprom]") {
	SECTION("X65") {
		auto eepromData = makeEeprom(TestEepromLayout::X65, {
			{ 4999, {} },
		});
		writeUInt32LE(&eepromData[X65_PARTITION_SIZE - X65_ENTRY_SIZE + 4], 5000);

		CHECK_THROWS_WITH(siemens::Eeprom(eepromData), Catch::Matchers::ContainsSubstring("Block ID out of range"));
	}

	SECTION("X85") {
		auto eepromData = makeEeprom(TestEepromLayout::X85, {
			{ 4999, { 0 } },
		});
		size_t entryOffset = X85_PARTITION_SIZE - X85_HEADER_SIZE - X85_ENTRY_SIZE;
		writeUInt16LE(&eepromData[entryOffset + 4], 5000);

		CHECK_THROWS_WITH(siemens::Eeprom(eepromData), Catch::Matchers::ContainsSubstring("Block ID out of range"));
	}
}

TEST_CASE("Recalculation stays incomplete when a mandatory block has the wrong size", "[eeprom][recalc]") {
	siemens::Keys keys;
	keys.imei = "123456789012345";
	keys.esn = 0x12345678;
	keys.skey = 12345678;
	keys.masterKeys.fill(12345678);

	auto partitions = makeEeprom(TestEepromLayout::X65, {
		{ 76, std::vector<uint8_t>(9) },
		{ 5008, std::vector<uint8_t>(0xE0) },
		{ 5009, std::vector<uint8_t>(10) },
		{ 5077, std::vector<uint8_t>(0xE8) },
		{ 5121, std::vector<uint8_t>(0x38) },
		{ 5122, std::vector<uint8_t>(6) },
		{ 5123, std::vector<uint8_t>(0xC) },
		{ 52, std::vector<uint8_t>(0x122) },
	});
	std::vector<uint8_t> fullflash(0x100000, 0xFF);
	std::copy(partitions.begin(), partitions.end(), fullflash.begin() + 0x40000);
	writeUInt32LE(&fullflash[0x3C], 0x544B4A43);
	fullflash[0x200] = 0;
	fullflash[0x201] = 2;
	writeUInt16LE(&fullflash[0x202], 0x534C);
	fullflash[0x238] = 0;

	auto result = siemens::recalculateFullflash(fullflash, keys);
	REQUIRE(result);
	CHECK_FALSE(result->complete);
}

TEST_CASE("X65 EEFULL extended metadata is excluded from block data", "[eeprom]") {
	auto eepromData = makeEeprom(TestEepromLayout::X65, {
		{ 5009, { 1, 2, 3, 4 }, ENTRY_VALID, true },
	});

	siemens::Eeprom eeprom(eepromData);
	CHECK((eeprom.readBlock(5009) == std::vector<uint8_t> { 1, 2, 3, 4 }));
}

TEST_CASE("X65 keeps the first valid EIT entry for duplicate block IDs", "[eeprom]") {
	auto eepromData = makeEeprom(TestEepromLayout::X65, {
		{ 1, { 0x11 } },
		{ 1, { 0x22 } },
	});

	siemens::Eeprom eeprom(eepromData);
	CHECK((eeprom.readBlock(1) == std::vector<uint8_t> { 0x11 }));
}

TEST_CASE("X85 reads strided inline data across obsolete and free entries", "[eeprom]") {
	auto eepromData = makeEeprom(TestEepromLayout::X85, {
		{ 10, { 0, 0, 0, 0 }, ENTRY_OBSOLETE },
		{ 76, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 } },
		{ 0, {}, ENTRY_FREE },
		{ 77, { 0x42 } },
	});

	siemens::Eeprom eeprom(eepromData);
	CHECK_FALSE(eeprom.hasBlock(10));
	CHECK((eeprom.readBlock(76) == std::vector<uint8_t> { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }));
	CHECK((eeprom.readBlock(77) == std::vector<uint8_t> { 0x42 }));
}

TEST_CASE("X85 EEFULL extended metadata is excluded from inline block data", "[eeprom]") {
	auto eepromData = makeEeprom(TestEepromLayout::X85, {
		{ 5009, { 1, 2, 3, 4 }, ENTRY_VALID, true },
	});

	siemens::Eeprom eeprom(eepromData);
	CHECK((eeprom.readBlock(5009) == std::vector<uint8_t> { 1, 2, 3, 4 }));
}

TEST_CASE("X85 keeps the last valid EIT entry for duplicate block IDs", "[eeprom]") {
	auto eepromData = makeEeprom(TestEepromLayout::X85, {
		{ 1, { 0x11, 0xFF } },
		{ 1, { 0x22, 0xFF } },
	});

	siemens::Eeprom eeprom(eepromData);
	CHECK((eeprom.readBlock(1) == std::vector<uint8_t> { 0x22, 0xFF }));
}

TEST_CASE("ESN can be recovered from every supported security record", "[eeprom][recalc]") {
	siemens::Keys keys;
	keys.imei = "123456789012345";
	keys.esn = 257;
	keys.skey = 12345678;
	keys.masterKeys.fill(87654321);

	SECTION("known HASH") {
		siemens::FullflashInfo info;
		info.hash = { 0x54, 0xF8, 0x0A, 0xC1, 0x2A, 0xCD, 0x94, 0xB2,
			0xF5, 0xCF, 0xFB, 0x9B, 0xF7, 0xE4, 0xD4, 0x93 };
		std::vector<uint8_t> eepromData;
		siemens::Eeprom eeprom(eepromData);
		std::vector<std::tuple<siemens::EsnRecoveryStage, uint32_t>> progress;
		auto [found, esn] = siemens::recoverEsn(eeprom, info, 1, [&](auto stage, uint32_t percent) {
			progress.emplace_back(stage, percent);
		});

		REQUIRE(found);
		CHECK(esn == 0x12345678);
		CHECK(progress == std::vector<std::tuple<siemens::EsnRecoveryStage, uint32_t>> {
			{ siemens::EsnRecoveryStage::KNOWN_HASH, 0 },
			{ siemens::EsnRecoveryStage::KNOWN_HASH, 100 },
		});
	}

	SECTION("BKEY without IMEI") {
		siemens::FullflashInfo info;
		std::array<uint8_t, 16> bkey;
		std::array<uint8_t, 16> hash;
		siemens::calculateBkeyAndHash(keys.esn, keys.skey, bkey, hash);
		info.bkey.assign(bkey.begin(), bkey.end());
		info.skey.resize(4);
		writeUInt32LE(info.skey.data(), keys.skey);
		std::vector<uint8_t> eepromData;
		siemens::Eeprom eeprom(eepromData);
		std::vector<std::tuple<siemens::EsnRecoveryStage, uint32_t>> progress;
		auto [found, esn] = siemens::recoverEsn(eeprom, info, 1, [&](auto stage, uint32_t percent) {
			progress.emplace_back(stage, percent);
		});

		REQUIRE(found);
		CHECK(esn == keys.esn);
		CHECK(progress == std::vector<std::tuple<siemens::EsnRecoveryStage, uint32_t>> {
			{ siemens::EsnRecoveryStage::BKEY, 0 },
			{ siemens::EsnRecoveryStage::BKEY, 100 },
		});
	}

	SECTION("HASH without IMEI") {
		siemens::FullflashInfo info;
		std::array<uint8_t, 16> bkey;
		std::array<uint8_t, 16> hash;
		siemens::calculateBkeyAndHash(keys.esn, keys.skey, bkey, hash);
		info.hash.assign(hash.begin(), hash.end());
		info.skey.resize(4);
		writeUInt32LE(info.skey.data(), keys.skey);
		std::vector<uint8_t> eepromData;
		siemens::Eeprom eeprom(eepromData);
		auto [found, esn] = siemens::recoverEsn(eeprom, info, 1);

		REQUIRE(found);
		CHECK(esn == keys.esn);
	}

	SECTION("block 5468") {
		auto eepromData = makeRecalcFullflash({ { 5468, std::vector<uint8_t>(0x31) } }, keys);
		siemens::Eeprom eeprom(eepromData);
		siemens::FullflashInfo info;
		auto [found, esn] = siemens::recoverEsn(eeprom, info, 1);

		REQUIRE(found);
		CHECK(esn == keys.esn);
	}

	SECTION("block 5121") {
		auto eepromData = makeRecalcFullflash({ { 5121, std::vector<uint8_t>(0x38) } }, keys);
		siemens::Eeprom eeprom(eepromData);
		siemens::FullflashInfo info;
		info.imei = keys.imei;
		info.skey.resize(4);
		writeUInt32LE(info.skey.data(), keys.skey);
		auto [found, esn] = siemens::recoverEsn(eeprom, info, 1);

		REQUIRE(found);
		CHECK(esn == keys.esn);
	}

	SECTION("block 5123") {
		auto eepromData = makeRecalcFullflash({ { 5123, std::vector<uint8_t>(0xC) } }, keys);
		siemens::Eeprom eeprom(eepromData);
		siemens::FullflashInfo info;
		info.imei = keys.imei;
		info.skey.resize(4);
		writeUInt32LE(info.skey.data(), keys.skey);
		auto [found, esn] = siemens::recoverEsn(eeprom, info, 1);

		REQUIRE(found);
		CHECK(esn == keys.esn);
	}

	SECTION("block 5008") {
		auto eepromData = makeRecalcFullflash({
			{ 5008, std::vector<uint8_t>(0xE0) },
			{ 5077, std::vector<uint8_t>(0xE8) },
		}, keys);
		siemens::Eeprom eeprom(eepromData);
		auto key = siemens::buildEepromKey(keys.esn, keys.imei);

		auto block5008 = eeprom.readBlock(5008);
		siemens::cipherDecrypt(&block5008[0], 0x20, key.data());
		siemens::cipherDecrypt(&block5008[0x20], 0xC0, key.data());
		block5008[0xD0] = 0x42;
		writeChecksum(&block5008[0x28], 0xB0);
		siemens::cipherEncrypt(&block5008[0], 0x20, key.data());
		siemens::cipherEncrypt(&block5008[0x20], 0xC0, key.data());
		eeprom.writeBlock(5008, block5008);

		auto block5077 = eeprom.readBlock(5077);
		siemens::cipherDecrypt(block5077.data(), block5077.size(), key.data());
		std::fill(block5077.begin() + 0xE2, block5077.end(), 0x42);
		siemens::cipherEncrypt(block5077.data(), block5077.size(), key.data());
		eeprom.writeBlock(5077, block5077);

		siemens::FullflashInfo info;
		info.imei = keys.imei;
		auto [found, esn] = siemens::recoverEsn(eeprom, info, 1);

		REQUIRE(found);
		CHECK(esn == keys.esn);
	}

	SECTION("block 5077") {
		auto eepromData = makeRecalcFullflash({ { 5077, std::vector<uint8_t>(0xE8) } }, keys);
		siemens::Eeprom eeprom(eepromData);
		siemens::FullflashInfo info;
		info.imei = keys.imei;
		auto [found, esn] = siemens::recoverEsn(eeprom, info, 1);

		REQUIRE(found);
		CHECK(esn == keys.esn);
	}

	SECTION("erased block 5008") {
		auto eepromData = makeEeprom(TestEepromLayout::X85, {
			{ 5008, std::vector<uint8_t>(0xE0, 0xFF) },
		});
		siemens::Eeprom eeprom(eepromData);
		siemens::FullflashInfo info;
		info.imei = keys.imei;

		CHECK_FALSE(std::get<0>(siemens::recoverEsn(eeprom, info, 1)));
	}
}
