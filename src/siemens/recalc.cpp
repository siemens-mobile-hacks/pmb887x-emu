#include "recalc.h"

#include "crypto/md5.h"
#include "siemens/crypto.h"
#include "siemens/eeprom.h"
#include "siemens/fullflash.h"
#include "utils/binary.h"
#include "utils/string.h"

#include <spdlog/spdlog.h>

#include <cstring>

namespace siemens {

// Constant tail of EEPROM block 52 (after BKEY and the VerDown byte); the rest is 0xFF.
static const uint8_t BLOCK52_TAIL[145] = {
	0xff, 0x01, 0x00, 0x00, 0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0x00, 0x54, 0x00, 0x55, 0x00, 0x80,
	0x00, 0xd9, 0xd9, 0xf3, 0xbb, 0x3f, 0x70, 0x85, 0x83, 0x86, 0x91, 0xd8, 0xa0, 0xda, 0xc3, 0x95,
	0xa5, 0xa3, 0x4d, 0xd0, 0x4f, 0xcf, 0xe3, 0x50, 0x4a, 0x54, 0xa6, 0x34, 0x52, 0x60, 0x34, 0xd2,
	0xfb, 0x76, 0x69, 0x5d, 0xef, 0x36, 0x96, 0x56, 0x0f, 0xc0, 0x6d, 0xef, 0x38, 0x12, 0xb7, 0xc7,
	0x2f, 0xbf, 0xd6, 0xa0, 0xd5, 0x15, 0xe7, 0x13, 0x24, 0x14, 0x45, 0x40, 0x40, 0xf9, 0x69, 0x49,
	0x08, 0xb1, 0x3b, 0xed, 0x97, 0x9a, 0xef, 0x9f, 0x20, 0x63, 0xda, 0x07, 0xf8, 0x40, 0x3c, 0xeb,
	0x30, 0x32, 0x68, 0x08, 0x49, 0x86, 0xa2, 0x3b, 0x3e, 0x61, 0x21, 0xfe, 0x7b, 0xce, 0xd2, 0xad,
	0x3b, 0x02, 0x00, 0x5c, 0x41, 0x30, 0xa3, 0x8d, 0x52, 0x79, 0xf0, 0x97, 0x28, 0x4e, 0xfc, 0x18,
	0x6a, 0xe9, 0x5e, 0x0a, 0xb2, 0xbe, 0x4b, 0xa7, 0xa8, 0x58, 0x44, 0x28, 0x91, 0xf4, 0xbe, 0xec,
	0xef,
};

struct GeneratedBlock {
	uint32_t id;
	std::vector<uint8_t> data;
	bool required;
};

// EEPROM LITE block 76 (variant 0) and FULL block 9 (variant 1).
static std::vector<uint8_t> buildImeiBlock(const std::string &imei, bool fullBlock) {
	std::vector<uint8_t> block(10, 0);
	for (size_t index = 0; index < 14; index++) {
		uint8_t digit = imei[index] - '0';
		if ((index & 1) != 0) {
			block[index / 2] |= (digit << 4);
		} else {
			block[index / 2] = digit;
		}
	}
	block[7] = (calculateImeiCheckDigit(imei) << 4);

	uint8_t xorEven = 0;
	uint8_t xorOdd = 0;
	for (size_t index = 0; index < 8; index++) {
		if ((index & 1) != 0) {
			xorOdd ^= block[index];
		} else {
			xorEven ^= block[index];
		}
	}
	block[8] = fullBlock ? xorOdd : xorEven;
	block[9] = (xorOdd ^ xorEven ^ 0xFF);

	imeiCipherEncrypt(block.data(), fullBlock);
	return block;
}

static void writeChecksum(uint8_t *data, size_t offset, size_t size) {
	uint8_t sum = 0;
	uint8_t xorValue = 0;
	for (size_t index = 0; index < size; index++) {
		sum += data[offset + index];
		xorValue ^= data[offset + index];
	}
	data[offset + size] = sum;
	data[offset + size + 1] = xorValue;
}

// EEPROM FULL block 8
static std::vector<uint8_t> buildBlock5008(const std::string &imei, uint32_t esn) {
	static const uint32_t HEADER[10] = { 0x56605650, 0, 0x300, 0x670000, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0x6462FF00, 0x56805670, 0 };
	std::vector<uint8_t> block(0xE0, 0);
	for (size_t index = 0; index < 10; index++)
		writeUInt32LE(&block[index * 4], HEADER[index]);
	memset(&block[0x28], 0xFF, 0xB8);
	writeUInt32LE(&block[0xD8], 0x50);
	writeUInt32LE(&block[0xDC], 0);
	writeChecksum(block.data(), 8, 0x16);
	writeChecksum(block.data(), 0x28, 0xB0);
	writeUInt32LE(&block[0x00], (readUInt32LE(&block[0x00]) ^ 0xBA1FE5D7));
	writeUInt32LE(&block[0x04], (readUInt32LE(&block[0x04]) ^ 0xD95D2DFD));
	writeUInt32LE(&block[0x20], (readUInt32LE(&block[0x20]) ^ 0xBA1FE5D7));
	writeUInt32LE(&block[0x24], (readUInt32LE(&block[0x24]) ^ 0xD95D2DFD));
	auto key = buildEepromKey(esn, imei);
	cipherEncrypt(&block[0], 0x20, key.data());
	cipherEncrypt(&block[0x20], 0xC0, key.data());
	return block;
}

// EEPROM FULL block 77
static std::vector<uint8_t> buildBlock5077(const std::string &imei, uint32_t esn) {
	std::vector<uint8_t> block(0xE8, 0xFF);
	writeUInt32LE(&block[0], 0x56A05690);
	writeUInt32LE(&block[4], 0);
	block[0xB] = 0;
	writeUInt32LE(&block[0xE0], 0x063DFF29);
	writeUInt32LE(&block[0xE4], 0xF3800B06);
	writeChecksum(block.data(), 8, 0xD8);
	writeUInt32LE(&block[0], (readUInt32LE(&block[0]) ^ 0xBA1FE5D7));
	writeUInt32LE(&block[4], (readUInt32LE(&block[4]) ^ 0xD95D2DFD));
	auto key = buildEepromKey(esn, imei);
	cipherEncrypt(block.data(), block.size(), key.data());
	return block;
}

// EEPROM FULL block 121: encrypted SKEY marker + 6 encrypted master codes
static std::vector<uint8_t> buildBlock5121(const Keys &keys) {
	auto packedImei = packImei(keys.imei);

	std::vector<uint8_t> block(8 + 6 * 8);
	writeUInt32LE(&block[0], 0x77C5742D);
	writeUInt32LE(&block[4], 0xF49A4ADA);
	cipherEncrypt(&block[0], 8, buildCipherKey1(keys.skey, keys.esn, packedImei).data());

	for (size_t index = 0; index < keys.masterKeys.size(); index++) {
		uint8_t *entry = &block[8 + index * 8];
		writeUInt32LE(entry, ((uint32_t) index ^ 0x77C5742D));
		writeUInt32LE(entry + 4, 0xF49A4ADA);
		cipherEncrypt(entry, 8, buildCipherKey2(keys.masterKeys[index], keys.esn, packedImei).data());
		cipherEncrypt(entry, 8, buildCipherKey1(keys.masterKeys[index], keys.esn, packedImei).data());
	}
	return block;
}

// EEPROM FULL block 122
static std::vector<uint8_t> buildBlock5122(uint32_t skey) {
	std::vector<uint8_t> block(6);
	writeUInt32LE(&block[0], skey);
	block[4] = 0x58;
	block[5] = 0;
	return block;
}

// EEPROM FULL block 123 (short 12-byte variant used by x75/x85 firmware)
static std::vector<uint8_t> buildBlock5123(const std::vector<uint8_t> &block5121, bool shortVariant) {
	std::vector<uint8_t> block(shortVariant ? 0xC : 0x20, 0);
	writeUInt32LE(&block[0], shortVariant ? 0xFFFFFF3F : 0xFFFF1F07);
	memcpy(&block[4], block5121.data(), 8);
	return block;
}

// EEPROM LITE block 52
static std::vector<uint8_t> buildBlock52(const std::array<uint8_t, 16> &bkey, uint8_t verDown) {
	std::vector<uint8_t> block(0x122, 0xFF);
	memcpy(&block[0], bkey.data(), 16);
	block[16] = verDown;
	memcpy(&block[17], BLOCK52_TAIL, sizeof(BLOCK52_TAIL));
	return block;
}

// EEPROM FULL block 468 (x85 only)
static std::vector<uint8_t> buildBlock5468(const Keys &keys, const std::array<uint8_t, 16> &bkey) {
	std::vector<uint8_t> block(0x31);
	memcpy(&block[0], bkey.data(), 16);
	block[16] = 0x58;
	uint8_t packedEsn[4];
	writeUInt32LE(packedEsn, keys.esn);
	auto esnHash = md5(packedEsn, 4);
	memcpy(&block[0x11], esnHash.data(), 16);
	auto imeiHash = md5((const uint8_t *) keys.imei.data(), keys.imei.size());
	memcpy(&block[0x21], imeiHash.data(), 8);
	auto totalHash = md5(block.data(), 0x29);
	memcpy(&block[0x29], totalHash.data(), 8);
	return block;
}

std::optional<RecalcResult> recalculateFullflash(std::vector<uint8_t> &fullflash, const Keys &keys) {
	auto layout = getBootcoreLayout(fullflash);
	if (!layout) {
		spdlog::warn("[recalc] Bootcore was not recognized");
		return std::nullopt;
	}

	std::array<uint8_t, 16> bkey, hash;
	calculateBkeyAndHash(keys.esn, keys.skey, bkey, hash);

	// Bootcore: HASH + IMEI string
	size_t hashOffset = layout->hashOffset;
	size_t imeiOffset = layout->imeiOffset;

	if (isErasedData(&fullflash[hashOffset], 16)) {
		spdlog::info("[recalc] BCORE is erased, recalculation is not needed");
		return std::nullopt;
	}

	RecalcResult result;
	result.complete = true;

	if (memcmp(&fullflash[hashOffset], hash.data(), 16) != 0) {
		memcpy(&fullflash[hashOffset], hash.data(), 16);
		spdlog::info("[recalc] BCORE: HASH replaced with {}", bytesToHex(hash.data(), 16));
		result.changed = true;
	}

	if (memcmp(&fullflash[imeiOffset], keys.imei.data(), 15) != 0) {
		memcpy(&fullflash[imeiOffset], keys.imei.data(), 15);
		spdlog::info("[recalc] BCORE: IMEI replaced with {}", keys.imei);
		result.changed = true;
	}

	Eeprom eeprom(fullflash);
	auto block5121 = buildBlock5121(keys);
	size_t block5123Size = eeprom.hasBlock(5123) ? eeprom.getBlockSize(5123) : 0;
	std::vector<GeneratedBlock> blocks = {
		{ 52, buildBlock52(bkey, keys.verDown), true },
		{ 76, buildImeiBlock(keys.imei, false), true },
		{ 320, std::vector<uint8_t>(bkey.begin(), bkey.end()), false },
		{ 5008, buildBlock5008(keys.imei, keys.esn), true },
		{ 5009, buildImeiBlock(keys.imei, true), true },
		{ 5077, buildBlock5077(keys.imei, keys.esn), true },
		{ 5121, block5121, true },
		{ 5122, buildBlock5122(keys.skey), true },
		{ 5123, buildBlock5123(block5121, block5123Size == 0xC), true },
		{ 5468, buildBlock5468(keys, bkey), false },
	};

	for (const auto &[id, value, required] : blocks) {
		if (!eeprom.hasBlock(id)) {
			if (required)
				result.complete = false;
			continue;
		}

		size_t blockSize = eeprom.getBlockSize(id);
		if (blockSize != value.size()) {
			spdlog::warn("[recalc] EEPROM: block {} has unexpected size {}, not replaced", id, blockSize);
			if (required)
				result.complete = false;
			continue;
		}

		if (eeprom.writeBlock(id, value)) {
			spdlog::info("[recalc] EEPROM: block {} replaced", id);
			result.changed = true;
		} else {
			spdlog::info("[recalc] EEPROM: block {} already matches", id);
		}
	}

	if (!result.complete)
		spdlog::warn("[recalc] EEPROM: not all required blocks were found");
	if (result.changed) {
		spdlog::info("[recalc] Fullflash keys recalculated for IMEI {} and ESN {:08X}", keys.imei, keys.esn);
	} else {
		spdlog::info("[recalc] Fullflash keys already match IMEI {} and ESN {:08X}", keys.imei, keys.esn);
	}
	return result;
}

}
