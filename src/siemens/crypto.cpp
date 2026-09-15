#include "crypto.h"

#include "crypto/md4.h"
#include "crypto/md5.h"
#include "utils/binary.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace siemens {

static const uint8_t IMEI_SBOX_HIGH[16] = { 0x03, 0x0F, 0x01, 0x0D, 0x05, 0x0B, 0x0D, 0x09, 0x0D, 0x07, 0x06, 0x00, 0x0E, 0x06, 0x0B, 0x08 };
static const uint8_t IMEI_SBOX_LOW[16] = { 0x0A, 0x0E, 0x01, 0x05, 0x03, 0x06, 0x02, 0x0F, 0x0B, 0x0A, 0x03, 0x05, 0x06, 0x05, 0x04, 0x02 };
static const uint8_t IMEI_ROUND_KEYS[8] = { 0xE3, 0xB7, 0x5C, 0x13, 0xB0, 0xD2, 0xC4, 0x19 };

// 64-byte cipher key templates embedded in PapuaUtils. Selected words are replaced at runtime.
static const uint8_t KEY_EEP[64] = {
	0x14, 0x0a, 0xee, 0xe5, 0xd7, 0xfb, 0xff, 0xf8, 0x76, 0x4d, 0xc6, 0x03, 0xfd, 0xb2, 0xf9, 0xb2,
	0xb4, 0xcd, 0x66, 0xa2, 0x26, 0xf7, 0x10, 0xa4, 0x3e, 0x11, 0xc2, 0x43, 0xf4, 0x51, 0xf6, 0xaf,
	0xdc, 0x6d, 0x98, 0x8c, 0x08, 0x8f, 0x58, 0x8e, 0xdc, 0x4f, 0xb8, 0x8e, 0x08, 0xe2, 0x28, 0x41,
	0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t KEY_MK1[64] = {
	0x00, 0x00, 0x00, 0x00, 0xee, 0xee, 0xee, 0xee, 0x0a, 0xd0, 0xdc, 0x9c, 0xaf, 0xec, 0xaa, 0xb9,
	0x51, 0xa5, 0x7b, 0x85, 0xe0, 0x80, 0xe9, 0xb3, 0xf6, 0x7f, 0x01, 0xf8, 0x9d, 0x71, 0x8f, 0x42,
	0x4d, 0xf5, 0xa0, 0xe1, 0xd2, 0x1e, 0xd2, 0x4f, 0xdc, 0x6d, 0x98, 0x8c, 0x08, 0x8f, 0x58, 0x8e,
	0xdc, 0x4f, 0xb8, 0x8e, 0x08, 0xe2, 0x28, 0x08, 0x1a, 0x32, 0x54, 0x76, 0x98, 0x10, 0x32, 0x54,
};
static const uint8_t KEY_MK2[64] = {
	0x08, 0x1a, 0x32, 0x54, 0x76, 0x98, 0x10, 0x32, 0xd8, 0xc5, 0x72, 0x7c, 0x25, 0xf8, 0xc8, 0x59,
	0xdb, 0xee, 0x22, 0xca, 0xb5, 0x7d, 0xb4, 0xf3, 0xa5, 0x5e, 0x6e, 0xb1, 0x0d, 0xb1, 0xa5, 0x67,
	0xdc, 0x6d, 0x98, 0x8c, 0x08, 0x8f, 0x58, 0x8e, 0xdc, 0x4f, 0xb8, 0x8e, 0x08, 0xe2, 0x28, 0x41,
	0x16, 0xea, 0xfa, 0xf7, 0xd7, 0xc4, 0xd9, 0x2a, 0x3b, 0xf2, 0x0b, 0x7f, 0xee, 0xee, 0xee, 0xee,
};

std::array<uint8_t, 8> packImei(const std::string &imei) {
	std::array<uint8_t, 8> packed{};
	auto digit = [&](size_t index) { return (uint8_t) (imei[index] - '0'); };
	packed[0] = (0x0A | (digit(0) << 4));
	for (size_t index = 1; index < packed.size(); index++)
		packed[index] = (digit(2 * index - 1) | (index < 7 ? (digit(2 * index) << 4) : 0));
	return packed;
}

uint8_t calculateImeiCheckDigit(const std::string &imei) {
	uint32_t sum = 0;
	for (size_t index = 0; index < 14; index++) {
		uint32_t digit = ((uint32_t) (imei[index] - '0') << (index & 1));
		sum += digit / 10 + digit % 10;
	}
	return (10 - sum % 10) % 10;
}

std::array<uint8_t, 64> buildEepromKey(uint32_t esn, const std::string &imei) {
	std::array<uint8_t, 64> key;
	memcpy(key.data(), KEY_EEP, sizeof(KEY_EEP));
	writeUInt32LE(&key[0x30], esn);
	auto packedImei = packImei(imei);
	memcpy(&key[0x38], packedImei.data(), packedImei.size());
	return key;
}

std::array<uint8_t, 64> buildCipherKey1(uint32_t code, uint32_t esn, const std::array<uint8_t, 8> &imei) {
	std::array<uint8_t, 64> key;
	memcpy(key.data(), KEY_MK1, sizeof(KEY_MK1));
	writeUInt32LE(&key[0], (code ^ 0x7F0BF23B));
	writeUInt32LE(&key[4], esn);
	memcpy(&key[0x38], imei.data(), imei.size());
	return key;
}

std::array<uint8_t, 64> buildCipherKey2(uint32_t code, uint32_t esn, const std::array<uint8_t, 8> &imei) {
	std::array<uint8_t, 64> key;
	memcpy(key.data(), KEY_MK2, sizeof(KEY_MK2));
	uint32_t firstWord = 8;
	firstWord += ((uint32_t) imei[0] << 8);
	firstWord += ((uint32_t) imei[1] << 16);
	firstWord += ((uint32_t) imei[2] << 24);
	writeUInt32LE(&key[0], firstWord);
	memcpy(&key[4], &imei[3], 4);
	writeUInt32LE(&key[0x38], (code ^ 0x7F0BF23B));
	writeUInt32LE(&key[0x3C], esn);
	return key;
}

static void transformImeiRound(uint8_t *data, uint8_t roundKey, bool encrypt) {
	uint8_t mask = 0;
	for (size_t i = 0; i < 5; i++) {
		if (encrypt) {
			uint8_t oldValue = data[i];
			uint8_t substituted = (IMEI_SBOX_HIGH[(oldValue >> 4)] << 4) + IMEI_SBOX_LOW[(oldValue & 0xF)];
			data[i] = (substituted ^ roundKey ^ data[i + 5] ^ mask);
			data[i + 5] = oldValue;
		} else {
			uint8_t oldValue = data[i + 5];
			uint8_t substituted = (IMEI_SBOX_HIGH[(oldValue >> 4)] << 4) + IMEI_SBOX_LOW[(oldValue & 0xF)];
			data[i + 5] = (data[i] ^ substituted ^ roundKey ^ mask);
			data[i] = oldValue;
		}
		mask ^= 0xFF;
	}
}

// RC4 key schedule followed by a modified PRGA with ciphertext feedback.
template<size_t Count, bool Encrypt>
static void transformCipher(
	uint8_t *data,
	size_t size,
	size_t stride,
	const std::array<std::array<uint8_t, 16>, Count> &keys
) {
	std::array<std::array<uint8_t, 256>, Count> states;
	for (size_t lane = 0; lane < Count; lane++)
		for (size_t index = 0; index < 256; index++)
			states[lane][index] = (uint8_t) index;

	std::array<uint8_t, Count> keyScheduleIndexes{};
	for (size_t index = 0; index < 256; index++) {
		for (size_t lane = 0; lane < Count; lane++) {
			keyScheduleIndexes[lane] += keys[lane][(index & 0xF)] + states[lane][index];
			std::swap(states[lane][index], states[lane][keyScheduleIndexes[lane]]);
		}
	}

	std::array<uint8_t, Count> stateIndexes{};
	std::array<uint8_t, Count> streamIndexes{};
	std::array<uint8_t, Count> previousBytes{};
	for (size_t offset = 0; offset < size; offset++) {
		for (size_t lane = 0; lane < Count; lane++) {
			uint8_t *value = data + lane * stride + offset;
			stateIndexes[lane]++;
			previousBytes[lane] += states[lane][stateIndexes[lane]];
			streamIndexes[lane] += previousBytes[lane];
			uint8_t outputIndex = states[lane][streamIndexes[lane]] + previousBytes[lane];
			states[lane][stateIndexes[lane]] = states[lane][streamIndexes[lane]];
			states[lane][streamIndexes[lane]] = previousBytes[lane];
			if constexpr (Encrypt) {
				previousBytes[lane] = (states[lane][outputIndex] ^ *value);
				*value = previousBytes[lane];
			} else {
				previousBytes[lane] = *value;
				*value = (states[lane][outputIndex] ^ previousBytes[lane]);
			}
		}
	}
}

void cipherEncrypt(uint8_t *data, size_t size, const uint8_t *key) {
	std::array<std::array<uint8_t, 16>, 1> keys = { md4(key) };
	transformCipher<1, true>(data, size, size, keys);
}

void cipherDecrypt(uint8_t *data, size_t size, const uint8_t *key) {
	std::array<std::array<uint8_t, 16>, 1> keys = { md4(key) };
	transformCipher<1, false>(data, size, size, keys);
}

void cipherDecryptBatch(uint8_t *data, size_t size, const CipherKeyBatch &keys) {
	std::array<std::array<uint8_t, 16>, CIPHER_BATCH_SIZE> digests;
	for (size_t lane = 0; lane < CIPHER_BATCH_SIZE; lane++)
		digests[lane] = md4(keys[lane].data());
	transformCipher<CIPHER_BATCH_SIZE, false>(data, size, size, digests);
}

void imeiCipherEncrypt(uint8_t *data, bool fullBlock) {
	for (size_t round = fullBlock ? 0 : 1; round < 8; round += 2)
		transformImeiRound(data, IMEI_ROUND_KEYS[round], true);
}

void imeiCipherDecrypt(uint8_t *data, bool fullBlock) {
	for (int round = fullBlock ? 6 : 7; round >= 0; round -= 2)
		transformImeiRound(data, IMEI_ROUND_KEYS[round], false);
}

void calculateBkeyAndHash(uint32_t esn, uint32_t skey, std::array<uint8_t, 16> &bkey, std::array<uint8_t, 16> &hash) {
	uint8_t block[16];
	writeUInt32LE(&block[0], esn);
	writeUInt32LE(&block[4], skey);
	for (size_t i = 0; i < 8; i++)
		block[8 + i] = (block[i] ^ block[i + 3]);
	bkey = md5(block, 16);
	hash = md5(bkey.data(), 16);
}

}
