#include "md5.h"

#include "utils/binary.h"

#include <bit>
#include <vector>

static const uint32_t MD5_INITIAL_STATE[4] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 };

#define MD5_F1(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define MD5_F2(x, y, z) MD5_F1((z), (x), (y))
#define MD5_F3(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_F4(x, y, z) ((y) ^ ((x) | ~(z)))
#define MD5_STEP(function, a, b, c, d, word, constant, shift) \
	do { \
		(a) += function((b), (c), (d)) + (word) + (constant); \
		(a) = std::rotl((a), (shift)) + (b); \
	} while (0)

#define MD5_ROUNDS(step) \
	step(MD5_F1, a, b, c, d, 0, 0xD76AA478, 7); \
	step(MD5_F1, d, a, b, c, 1, 0xE8C7B756, 12); \
	step(MD5_F1, c, d, a, b, 2, 0x242070DB, 17); \
	step(MD5_F1, b, c, d, a, 3, 0xC1BDCEEE, 22); \
	step(MD5_F1, a, b, c, d, 4, 0xF57C0FAF, 7); \
	step(MD5_F1, d, a, b, c, 5, 0x4787C62A, 12); \
	step(MD5_F1, c, d, a, b, 6, 0xA8304613, 17); \
	step(MD5_F1, b, c, d, a, 7, 0xFD469501, 22); \
	step(MD5_F1, a, b, c, d, 8, 0x698098D8, 7); \
	step(MD5_F1, d, a, b, c, 9, 0x8B44F7AF, 12); \
	step(MD5_F1, c, d, a, b, 10, 0xFFFF5BB1, 17); \
	step(MD5_F1, b, c, d, a, 11, 0x895CD7BE, 22); \
	step(MD5_F1, a, b, c, d, 12, 0x6B901122, 7); \
	step(MD5_F1, d, a, b, c, 13, 0xFD987193, 12); \
	step(MD5_F1, c, d, a, b, 14, 0xA679438E, 17); \
	step(MD5_F1, b, c, d, a, 15, 0x49B40821, 22); \
	step(MD5_F2, a, b, c, d, 1, 0xF61E2562, 5); \
	step(MD5_F2, d, a, b, c, 6, 0xC040B340, 9); \
	step(MD5_F2, c, d, a, b, 11, 0x265E5A51, 14); \
	step(MD5_F2, b, c, d, a, 0, 0xE9B6C7AA, 20); \
	step(MD5_F2, a, b, c, d, 5, 0xD62F105D, 5); \
	step(MD5_F2, d, a, b, c, 10, 0x02441453, 9); \
	step(MD5_F2, c, d, a, b, 15, 0xD8A1E681, 14); \
	step(MD5_F2, b, c, d, a, 4, 0xE7D3FBC8, 20); \
	step(MD5_F2, a, b, c, d, 9, 0x21E1CDE6, 5); \
	step(MD5_F2, d, a, b, c, 14, 0xC33707D6, 9); \
	step(MD5_F2, c, d, a, b, 3, 0xF4D50D87, 14); \
	step(MD5_F2, b, c, d, a, 8, 0x455A14ED, 20); \
	step(MD5_F2, a, b, c, d, 13, 0xA9E3E905, 5); \
	step(MD5_F2, d, a, b, c, 2, 0xFCEFA3F8, 9); \
	step(MD5_F2, c, d, a, b, 7, 0x676F02D9, 14); \
	step(MD5_F2, b, c, d, a, 12, 0x8D2A4C8A, 20); \
	step(MD5_F3, a, b, c, d, 5, 0xFFFA3942, 4); \
	step(MD5_F3, d, a, b, c, 8, 0x8771F681, 11); \
	step(MD5_F3, c, d, a, b, 11, 0x6D9D6122, 16); \
	step(MD5_F3, b, c, d, a, 14, 0xFDE5380C, 23); \
	step(MD5_F3, a, b, c, d, 1, 0xA4BEEA44, 4); \
	step(MD5_F3, d, a, b, c, 4, 0x4BDECFA9, 11); \
	step(MD5_F3, c, d, a, b, 7, 0xF6BB4B60, 16); \
	step(MD5_F3, b, c, d, a, 10, 0xBEBFBC70, 23); \
	step(MD5_F3, a, b, c, d, 13, 0x289B7EC6, 4); \
	step(MD5_F3, d, a, b, c, 0, 0xEAA127FA, 11); \
	step(MD5_F3, c, d, a, b, 3, 0xD4EF3085, 16); \
	step(MD5_F3, b, c, d, a, 6, 0x04881D05, 23); \
	step(MD5_F3, a, b, c, d, 9, 0xD9D4D039, 4); \
	step(MD5_F3, d, a, b, c, 12, 0xE6DB99E5, 11); \
	step(MD5_F3, c, d, a, b, 15, 0x1FA27CF8, 16); \
	step(MD5_F3, b, c, d, a, 2, 0xC4AC5665, 23); \
	step(MD5_F4, a, b, c, d, 0, 0xF4292244, 6); \
	step(MD5_F4, d, a, b, c, 7, 0x432AFF97, 10); \
	step(MD5_F4, c, d, a, b, 14, 0xAB9423A7, 15); \
	step(MD5_F4, b, c, d, a, 5, 0xFC93A039, 21); \
	step(MD5_F4, a, b, c, d, 12, 0x655B59C3, 6); \
	step(MD5_F4, d, a, b, c, 3, 0x8F0CCC92, 10); \
	step(MD5_F4, c, d, a, b, 10, 0xFFEFF47D, 15); \
	step(MD5_F4, b, c, d, a, 1, 0x85845DD1, 21); \
	step(MD5_F4, a, b, c, d, 8, 0x6FA87E4F, 6); \
	step(MD5_F4, d, a, b, c, 15, 0xFE2CE6E0, 10); \
	step(MD5_F4, c, d, a, b, 6, 0xA3014314, 15); \
	step(MD5_F4, b, c, d, a, 13, 0x4E0811A1, 21); \
	step(MD5_F4, a, b, c, d, 4, 0xF7537E82, 6); \
	step(MD5_F4, d, a, b, c, 11, 0xBD3AF235, 10); \
	step(MD5_F4, c, d, a, b, 2, 0x2AD7D2BB, 15); \
	step(MD5_F4, b, c, d, a, 9, 0xEB86D391, 21)

// One MD5 compression round. Unrolled because the ESN brute force runs it billions of times.
static void compressBlock(uint32_t state[4], const uint32_t message[16]) {
	uint32_t a = state[0];
	uint32_t b = state[1];
	uint32_t c = state[2];
	uint32_t d = state[3];

#define MD5_SCALAR_STEP(function, a, b, c, d, word, constant, shift) \
	MD5_STEP(function, a, b, c, d, message[word], constant, shift)
	MD5_ROUNDS(MD5_SCALAR_STEP);
#undef MD5_SCALAR_STEP

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

static void compressBlocks(Md5DigestBatch &state, const Md5BlockBatch &message) {
	auto a = state[0];
	auto b = state[1];
	auto c = state[2];
	auto d = state[3];

#define MD5_BATCH_STEP(function, a, b, c, d, word, constant, shift) \
	do { \
		for (size_t lane = 0; lane < MD5_BATCH_SIZE; lane++) \
			MD5_STEP(function, a[lane], b[lane], c[lane], d[lane], message[word][lane], constant, shift); \
	} while (0)
	MD5_ROUNDS(MD5_BATCH_STEP);
#undef MD5_BATCH_STEP

	for (size_t lane = 0; lane < MD5_BATCH_SIZE; lane++) {
		state[0][lane] += a[lane];
		state[1][lane] += b[lane];
		state[2][lane] += c[lane];
		state[3][lane] += d[lane];
	}
}

void md5Batch(Md5DigestBatch &hashes, const Md5BlockBatch &blocks) {
	for (size_t index = 0; index < hashes.size(); index++)
		hashes[index].fill(MD5_INITIAL_STATE[index]);
	compressBlocks(hashes, blocks);
}

std::array<uint8_t, 16> md5(const uint8_t *data, size_t size) {
	uint32_t state[4] = { MD5_INITIAL_STATE[0], MD5_INITIAL_STATE[1], MD5_INITIAL_STATE[2], MD5_INITIAL_STATE[3] };

	std::vector<uint8_t> message(data, data + size);
	message.push_back(0x80);
	while (message.size() % 64 != 56)
		message.push_back(0);

	uint64_t bits = (uint64_t) size * 8;
	for (size_t i = 0; i < 8; i++)
		message.push_back(((bits >> (i * 8)) & 0xFF));

	for (size_t offset = 0; offset < message.size(); offset += 64) {
		uint32_t words[16];
		for (size_t i = 0; i < 16; i++)
			words[i] = readUInt32LE(&message[offset + i * 4]);
		compressBlock(state, words);
	}

	std::array<uint8_t, 16> result;
	for (size_t i = 0; i < result.size() / 4; i++)
		writeUInt32LE(&result[i * 4], state[i]);
	return result;
}
