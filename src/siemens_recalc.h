#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/*
 * In-memory "recalculation" of a Siemens x65/x75/x85 fullflash for a given
 * IMEI / ESN / SKEY, equivalent to "Recalc Fullflash" in x65PapuaUtils.
 * See docs/recalc-alogrithm.md for the description of the algorithm.
 */

struct SiemensKeys {
	std::string imei;                       // 15 decimal digits
	uint32_t esn = 0;                       // NOR flash ESN
	uint32_t skey = 0;                      // service key (decimal in the UI)
	std::array<uint32_t, 6> masterKeys{};   // master codes stored in EEPROM block 121
	uint8_t verDown = 1;                    // PapuaUtils "VerDown" value stored in EEPROM block 52
};

struct SiemensRecalcResult {
	bool structureOk = false;   // fullflash layout was recognized
	bool complete = false;      // every mandatory EEPROM block was found
	size_t replaced = 0;        // number of items that had to be rewritten
	std::vector<std::string> log;
};

// Identity a fullflash carries: everything the key check needs except the ESN.
struct SiemensIdentity {
	bool ok = false;                      // IMEI and usable keys were found
	std::string imei;                     // bootcore
	uint32_t skey = 0;                    // EEPROM FULL block 122
	bool hasBootKey = false;
	std::array<uint8_t, 16> bootKey{};    // EEPROM LITE block 52
	std::array<uint8_t, 16> hash{};       // bootcore
};

// Reads the IMEI, SKEY and keys stored in the fullflash.
SiemensIdentity siemensReadIdentity(const std::vector<uint8_t> &fullflash);

// Brute forces the ESN the stored keys were derived from. Sweeps the whole 32-bit
// space, so it takes a while. threads = 0 uses every core.
bool siemensRecoverEsn(const SiemensIdentity &identity, uint32_t &esn, unsigned threads = 0);

// BootKey (BKEY) and HASH derived from ESN and SKEY.
void siemensCalcKeys(uint32_t esn, uint32_t skey, std::array<uint8_t, 16> &bootKey, std::array<uint8_t, 16> &hash);

// Patches fullflash in place. Nothing is modified when the image already contains the right keys.
SiemensRecalcResult siemensRecalcFullflash(std::vector<uint8_t> &fullflash, const SiemensKeys &keys);

std::string toHex(const uint8_t *data, size_t size);
