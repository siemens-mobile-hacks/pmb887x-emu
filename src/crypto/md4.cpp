#include "md4.h"

#include "utils/binary.h"

#include <bit>

static void compressBlock(uint32_t state[4], const uint8_t *block) {
	uint32_t words[16];
	for (size_t index = 0; index < 16; index++)
		words[index] = readUInt32LE(block + index * 4);

	uint32_t a = state[0];
	uint32_t b = state[1];
	uint32_t c = state[2];
	uint32_t d = state[3];
	static const int FIRST_SHIFTS[4] = { 3, 7, 11, 19 };
	static const int SECOND_SHIFTS[4] = { 3, 5, 9, 13 };
	static const int THIRD_SHIFTS[4] = { 3, 9, 11, 15 };
	static const uint32_t THIRD_OFFSETS[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };

	for (size_t index = 0; index < 16; index++) {
		uint32_t value = a + ((b & c) | (~b & d)) + words[index];
		a = d;
		d = c;
		c = b;
		b = std::rotl(value, FIRST_SHIFTS[index % 4]);
	}

	for (size_t index = 0; index < 16; index++) {
		size_t offset = (index % 4) * 4 + index / 4;
		uint32_t value = a + ((b & c) | (b & d) | (c & d)) + words[offset] + 0x5A827999;
		a = d;
		d = c;
		c = b;
		b = std::rotl(value, SECOND_SHIFTS[index % 4]);
	}

	for (size_t index = 0; index < 16; index++) {
		uint32_t value = a + (b ^ c ^ d) + words[THIRD_OFFSETS[index]] + 0x6ED9EBA1;
		a = d;
		d = c;
		c = b;
		b = std::rotl(value, THIRD_SHIFTS[index % 4]);
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

std::array<uint8_t, 16> md4(const uint8_t *data) {
	uint32_t state[4] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 };
	compressBlock(state, data);

	uint8_t padding[64] = { 0x80 };
	writeUInt32LE(&padding[56], 512);
	compressBlock(state, padding);

	std::array<uint8_t, 16> result;
	for (size_t index = 0; index < result.size() / 4; index++)
		writeUInt32LE(&result[index * 4], state[index]);
	return result;
}
