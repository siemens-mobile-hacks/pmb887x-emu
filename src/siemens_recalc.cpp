#include "siemens_recalc.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <map>
#include <thread>

namespace {

constexpr uint32_t rol32(uint32_t x, unsigned n) {
	return (x << n) | (x >> (32 - n));
}

uint32_t rd32(const uint8_t *p) {
	return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

void wr32(uint8_t *p, uint32_t v) {
	p[0] = v & 0xFF;
	p[1] = (v >> 8) & 0xFF;
	p[2] = (v >> 16) & 0xFF;
	p[3] = (v >> 24) & 0xFF;
}

uint16_t rd16(const uint8_t *p) {
	return (uint16_t) (p[0] | (p[1] << 8));
}

/* ---------------------------------------------------------------- MD5 --- */

constexpr uint32_t MD5_INIT[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };

#define MD5_F1(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define MD5_F2(x, y, z) MD5_F1((z), (x), (y))
#define MD5_F3(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_F4(x, y, z) ((y) ^ ((x) | ~(z)))
#define MD5_STEP(f, a, b, c, d, x, t, s) { (a) += f((b), (c), (d)) + (x) + (t); (a) = rol32((a), (s)) + (b); }

// One MD5 compression round. Unrolled because the ESN brute force runs it billions of times.
void md5Transform(uint32_t h[4], const uint32_t M[16]) {
	uint32_t a = h[0], b = h[1], c = h[2], d = h[3];

	MD5_STEP(MD5_F1, a, b, c, d, M[0], 0xd76aa478, 7)
	MD5_STEP(MD5_F1, d, a, b, c, M[1], 0xe8c7b756, 12)
	MD5_STEP(MD5_F1, c, d, a, b, M[2], 0x242070db, 17)
	MD5_STEP(MD5_F1, b, c, d, a, M[3], 0xc1bdceee, 22)
	MD5_STEP(MD5_F1, a, b, c, d, M[4], 0xf57c0faf, 7)
	MD5_STEP(MD5_F1, d, a, b, c, M[5], 0x4787c62a, 12)
	MD5_STEP(MD5_F1, c, d, a, b, M[6], 0xa8304613, 17)
	MD5_STEP(MD5_F1, b, c, d, a, M[7], 0xfd469501, 22)
	MD5_STEP(MD5_F1, a, b, c, d, M[8], 0x698098d8, 7)
	MD5_STEP(MD5_F1, d, a, b, c, M[9], 0x8b44f7af, 12)
	MD5_STEP(MD5_F1, c, d, a, b, M[10], 0xffff5bb1, 17)
	MD5_STEP(MD5_F1, b, c, d, a, M[11], 0x895cd7be, 22)
	MD5_STEP(MD5_F1, a, b, c, d, M[12], 0x6b901122, 7)
	MD5_STEP(MD5_F1, d, a, b, c, M[13], 0xfd987193, 12)
	MD5_STEP(MD5_F1, c, d, a, b, M[14], 0xa679438e, 17)
	MD5_STEP(MD5_F1, b, c, d, a, M[15], 0x49b40821, 22)

	MD5_STEP(MD5_F2, a, b, c, d, M[1], 0xf61e2562, 5)
	MD5_STEP(MD5_F2, d, a, b, c, M[6], 0xc040b340, 9)
	MD5_STEP(MD5_F2, c, d, a, b, M[11], 0x265e5a51, 14)
	MD5_STEP(MD5_F2, b, c, d, a, M[0], 0xe9b6c7aa, 20)
	MD5_STEP(MD5_F2, a, b, c, d, M[5], 0xd62f105d, 5)
	MD5_STEP(MD5_F2, d, a, b, c, M[10], 0x02441453, 9)
	MD5_STEP(MD5_F2, c, d, a, b, M[15], 0xd8a1e681, 14)
	MD5_STEP(MD5_F2, b, c, d, a, M[4], 0xe7d3fbc8, 20)
	MD5_STEP(MD5_F2, a, b, c, d, M[9], 0x21e1cde6, 5)
	MD5_STEP(MD5_F2, d, a, b, c, M[14], 0xc33707d6, 9)
	MD5_STEP(MD5_F2, c, d, a, b, M[3], 0xf4d50d87, 14)
	MD5_STEP(MD5_F2, b, c, d, a, M[8], 0x455a14ed, 20)
	MD5_STEP(MD5_F2, a, b, c, d, M[13], 0xa9e3e905, 5)
	MD5_STEP(MD5_F2, d, a, b, c, M[2], 0xfcefa3f8, 9)
	MD5_STEP(MD5_F2, c, d, a, b, M[7], 0x676f02d9, 14)
	MD5_STEP(MD5_F2, b, c, d, a, M[12], 0x8d2a4c8a, 20)

	MD5_STEP(MD5_F3, a, b, c, d, M[5], 0xfffa3942, 4)
	MD5_STEP(MD5_F3, d, a, b, c, M[8], 0x8771f681, 11)
	MD5_STEP(MD5_F3, c, d, a, b, M[11], 0x6d9d6122, 16)
	MD5_STEP(MD5_F3, b, c, d, a, M[14], 0xfde5380c, 23)
	MD5_STEP(MD5_F3, a, b, c, d, M[1], 0xa4beea44, 4)
	MD5_STEP(MD5_F3, d, a, b, c, M[4], 0x4bdecfa9, 11)
	MD5_STEP(MD5_F3, c, d, a, b, M[7], 0xf6bb4b60, 16)
	MD5_STEP(MD5_F3, b, c, d, a, M[10], 0xbebfbc70, 23)
	MD5_STEP(MD5_F3, a, b, c, d, M[13], 0x289b7ec6, 4)
	MD5_STEP(MD5_F3, d, a, b, c, M[0], 0xeaa127fa, 11)
	MD5_STEP(MD5_F3, c, d, a, b, M[3], 0xd4ef3085, 16)
	MD5_STEP(MD5_F3, b, c, d, a, M[6], 0x04881d05, 23)
	MD5_STEP(MD5_F3, a, b, c, d, M[9], 0xd9d4d039, 4)
	MD5_STEP(MD5_F3, d, a, b, c, M[12], 0xe6db99e5, 11)
	MD5_STEP(MD5_F3, c, d, a, b, M[15], 0x1fa27cf8, 16)
	MD5_STEP(MD5_F3, b, c, d, a, M[2], 0xc4ac5665, 23)

	MD5_STEP(MD5_F4, a, b, c, d, M[0], 0xf4292244, 6)
	MD5_STEP(MD5_F4, d, a, b, c, M[7], 0x432aff97, 10)
	MD5_STEP(MD5_F4, c, d, a, b, M[14], 0xab9423a7, 15)
	MD5_STEP(MD5_F4, b, c, d, a, M[5], 0xfc93a039, 21)
	MD5_STEP(MD5_F4, a, b, c, d, M[12], 0x655b59c3, 6)
	MD5_STEP(MD5_F4, d, a, b, c, M[3], 0x8f0ccc92, 10)
	MD5_STEP(MD5_F4, c, d, a, b, M[10], 0xffeff47d, 15)
	MD5_STEP(MD5_F4, b, c, d, a, M[1], 0x85845dd1, 21)
	MD5_STEP(MD5_F4, a, b, c, d, M[8], 0x6fa87e4f, 6)
	MD5_STEP(MD5_F4, d, a, b, c, M[15], 0xfe2ce6e0, 10)
	MD5_STEP(MD5_F4, c, d, a, b, M[6], 0xa3014314, 15)
	MD5_STEP(MD5_F4, b, c, d, a, M[13], 0x4e0811a1, 21)
	MD5_STEP(MD5_F4, a, b, c, d, M[4], 0xf7537e82, 6)
	MD5_STEP(MD5_F4, d, a, b, c, M[11], 0xbd3af235, 10)
	MD5_STEP(MD5_F4, c, d, a, b, M[2], 0x2ad7d2bb, 15)
	MD5_STEP(MD5_F4, b, c, d, a, M[9], 0xeb86d391, 21)

	h[0] += a;
	h[1] += b;
	h[2] += c;
	h[3] += d;
}

std::array<uint8_t, 16> md5(const uint8_t *data, size_t len) {
	uint32_t h[4] = { MD5_INIT[0], MD5_INIT[1], MD5_INIT[2], MD5_INIT[3] };

	std::vector<uint8_t> msg(data, data + len);
	msg.push_back(0x80);
	while (msg.size() % 64 != 56)
		msg.push_back(0);
	uint64_t bits = (uint64_t) len * 8;
	for (int i = 0; i < 8; i++)
		msg.push_back((bits >> (i * 8)) & 0xFF);

	for (size_t off = 0; off < msg.size(); off += 64) {
		uint32_t M[16];
		for (int i = 0; i < 16; i++)
			M[i] = rd32(&msg[off + i * 4]);
		md5Transform(h, M);
	}

	std::array<uint8_t, 16> out;
	for (int i = 0; i < 4; i++)
		wr32(&out[i * 4], h[i]);
	return out;
}

/* ---------------------------------------------------------------- MD4 --- */

void md4Compress(uint32_t st[4], const uint8_t *block) {
	uint32_t X[16];
	for (int i = 0; i < 16; i++)
		X[i] = rd32(block + i * 4);

	uint32_t a = st[0], b = st[1], c = st[2], d = st[3];
	static const unsigned S1[4] = { 3, 7, 11, 19 };
	static const unsigned S2[4] = { 3, 5, 9, 13 };
	static const unsigned S3[4] = { 3, 9, 11, 15 };
	static const unsigned O3[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };

	for (unsigned i = 0; i < 16; i++) {
		uint32_t v = a + ((b & c) | (~b & d)) + X[i];
		a = d; d = c; c = b; b = rol32(v, S1[i % 4]);
	}
	for (unsigned i = 0; i < 16; i++) {
		unsigned k = (i % 4) * 4 + i / 4;
		uint32_t v = a + ((b & c) | (b & d) | (c & d)) + X[k] + 0x5A827999;
		a = d; d = c; c = b; b = rol32(v, S2[i % 4]);
	}
	for (unsigned i = 0; i < 16; i++) {
		uint32_t v = a + (b ^ c ^ d) + X[O3[i]] + 0x6ED9EBA1;
		a = d; d = c; c = b; b = rol32(v, S3[i % 4]);
	}

	st[0] += a;
	st[1] += b;
	st[2] += c;
	st[3] += d;
}

// MD4 of exactly 64 bytes.
std::array<uint8_t, 16> md4Block(const uint8_t *key64) {
	uint32_t st[4] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 };
	md4Compress(st, key64);

	uint8_t pad[64] = { 0x80 };
	wr32(&pad[56], 512);
	md4Compress(st, pad);

	std::array<uint8_t, 16> out;
	for (int i = 0; i < 4; i++)
		wr32(&out[i * 4], st[i]);
	return out;
}

/* ------------------------------------------------------- Stream cipher --- */

// RC4 key schedule with MD4(key) as the key, followed by a modified PRGA with ciphertext feedback.
void streamCipher(uint8_t *data, size_t size, const uint8_t *key64, bool encrypt) {
	std::array<uint8_t, 16> key = md4Block(key64);

	uint8_t S[256];
	for (int i = 0; i < 256; i++)
		S[i] = i;
	for (int i = 0, j = 0; i < 256; i++) {
		j = (j + key[i & 0xF] + S[i]) & 0xFF;
		std::swap(S[i], S[j]);
	}

	uint8_t i = 0, j = 0, prev = 0;
	for (size_t k = 0; k < size; k++) {
		i++;
		prev += S[i];
		j += prev;
		uint8_t t = S[j] + prev;
		S[i] = S[j];
		S[j] = prev;
		if (encrypt) {
			prev = S[t] ^ data[k];
			data[k] = prev;
		} else {
			prev = data[k];
			data[k] = S[t] ^ prev;
		}
	}
}

/* ------------------------------------------------------------ Constants --- */

// 64-byte cipher key blocks embedded in PapuaUtils. Some words are replaced with ESN / SKEY / IMEI at runtime.
const uint8_t KEY_EEP[64] = {
	0x14, 0x0a, 0xee, 0xe5, 0xd7, 0xfb, 0xff, 0xf8, 0x76, 0x4d, 0xc6, 0x03, 0xfd, 0xb2, 0xf9, 0xb2,
	0xb4, 0xcd, 0x66, 0xa2, 0x26, 0xf7, 0x10, 0xa4, 0x3e, 0x11, 0xc2, 0x43, 0xf4, 0x51, 0xf6, 0xaf,
	0xdc, 0x6d, 0x98, 0x8c, 0x08, 0x8f, 0x58, 0x8e, 0xdc, 0x4f, 0xb8, 0x8e, 0x08, 0xe2, 0x28, 0x41,
	0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const uint8_t KEY_MK1[64] = {
	0x00, 0x00, 0x00, 0x00, 0xee, 0xee, 0xee, 0xee, 0x0a, 0xd0, 0xdc, 0x9c, 0xaf, 0xec, 0xaa, 0xb9,
	0x51, 0xa5, 0x7b, 0x85, 0xe0, 0x80, 0xe9, 0xb3, 0xf6, 0x7f, 0x01, 0xf8, 0x9d, 0x71, 0x8f, 0x42,
	0x4d, 0xf5, 0xa0, 0xe1, 0xd2, 0x1e, 0xd2, 0x4f, 0xdc, 0x6d, 0x98, 0x8c, 0x08, 0x8f, 0x58, 0x8e,
	0xdc, 0x4f, 0xb8, 0x8e, 0x08, 0xe2, 0x28, 0x08, 0x1a, 0x32, 0x54, 0x76, 0x98, 0x10, 0x32, 0x54,
};
const uint8_t KEY_MK2[64] = {
	0x08, 0x1a, 0x32, 0x54, 0x76, 0x98, 0x10, 0x32, 0xd8, 0xc5, 0x72, 0x7c, 0x25, 0xf8, 0xc8, 0x59,
	0xdb, 0xee, 0x22, 0xca, 0xb5, 0x7d, 0xb4, 0xf3, 0xa5, 0x5e, 0x6e, 0xb1, 0x0d, 0xb1, 0xa5, 0x67,
	0xdc, 0x6d, 0x98, 0x8c, 0x08, 0x8f, 0x58, 0x8e, 0xdc, 0x4f, 0xb8, 0x8e, 0x08, 0xe2, 0x28, 0x41,
	0x16, 0xea, 0xfa, 0xf7, 0xd7, 0xc4, 0xd9, 0x2a, 0x3b, 0xf2, 0x0b, 0x7f, 0xee, 0xee, 0xee, 0xee,
};

// Nibble substitution boxes and round keys of the IMEI block cipher (EEPROM blocks 76 and 9).
const uint8_t SBOX_HI[16] = { 0x03, 0x0f, 0x01, 0x0d, 0x05, 0x0b, 0x0d, 0x09, 0x0d, 0x07, 0x06, 0x00, 0x0e, 0x06, 0x0b, 0x08 };
const uint8_t SBOX_LO[16] = { 0x0a, 0x0e, 0x01, 0x05, 0x03, 0x06, 0x02, 0x0f, 0x0b, 0x0a, 0x03, 0x05, 0x06, 0x05, 0x04, 0x02 };
const uint8_t FEISTEL_KEY[8] = { 0xe3, 0xb7, 0x5c, 0x13, 0xb0, 0xd2, 0xc4, 0x19 };

// Constant tail of EEPROM block 52 (after BKEY and the VerDown byte); the rest is 0xFF.
const uint8_t BLOCK52_TAIL[145] = {
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

/* ------------------------------------------------------ Block builders --- */

// IMEI packed into 8 bytes: 0x?A, then digit pairs (high nibble = even digit).
std::array<uint8_t, 8> packImei8(const std::string &imei) {
	std::array<uint8_t, 8> out{};
	auto digit = [&](size_t i) { return (uint8_t) (imei[i] - '0'); };
	out[0] = 0x0A | (digit(0) << 4);
	for (size_t k = 1; k < 8; k++)
		out[k] = digit(2 * k - 1) | (k < 7 ? (digit(2 * k) << 4) : 0);
	return out;
}

uint8_t luhnCheckDigit(const std::string &imei) {
	unsigned sum = 0;
	for (size_t i = 0; i < 14; i++) {
		unsigned d = (unsigned) (imei[i] - '0') << (i & 1);
		sum += d / 10 + d % 10;
	}
	return (10 - sum % 10) % 10;
}

void feistelRound(uint8_t *buf, uint8_t roundKey) {
	uint8_t mask = 0;
	for (int k = 0; k < 5; k++) {
		uint8_t old = buf[k];
		uint8_t a = (SBOX_HI[old >> 4] << 4) + SBOX_LO[old & 0xF];
		buf[k] = a ^ roundKey ^ buf[k + 5] ^ mask;
		buf[k + 5] = old;
		mask ^= 0xFF;
	}
}

// EEPROM LITE block 76 (variant 0) and FULL block 9 (variant 1).
std::vector<uint8_t> buildImeiBlock(const std::string &imei, int variant) {
	std::vector<uint8_t> buf(10, 0);
	for (size_t i = 0; i < 14; i++) {
		uint8_t d = imei[i] - '0';
		if (i & 1)
			buf[i / 2] |= d << 4;
		else
			buf[i / 2] = d;
	}
	buf[7] = luhnCheckDigit(imei) << 4;

	uint8_t xorEven = 0, xorOdd = 0;
	for (int i = 0; i < 8; i++)
		(i & 1 ? xorOdd : xorEven) ^= buf[i];
	buf[8] = variant ? xorOdd : xorEven;
	buf[9] = xorOdd ^ xorEven ^ 0xFF;

	for (int r = variant ? 0 : 1; r < 8; r += 2)
		feistelRound(buf.data(), FEISTEL_KEY[r]);
	return buf;
}

void appendChecksum(uint8_t *buf, size_t off, size_t n) {
	uint8_t sum = 0, x = 0;
	for (size_t i = 0; i < n; i++) {
		sum += buf[off + i];
		x ^= buf[off + i];
	}
	buf[off + n] = sum;
	buf[off + n + 1] = x;
}

std::array<uint8_t, 64> keyEeprom(uint32_t esn, const std::string &imei) {
	std::array<uint8_t, 64> key;
	memcpy(key.data(), KEY_EEP, 64);
	wr32(&key[0x30], esn);
	std::array<uint8_t, 8> imei8 = packImei8(imei);
	memcpy(&key[0x38], imei8.data(), 8);
	return key;
}

// EEPROM FULL block 8
std::vector<uint8_t> buildBlock5008(const std::string &imei, uint32_t esn) {
	std::vector<uint8_t> buf(0xE0, 0);
	static const uint32_t head[10] = { 0x56605650, 0, 0x300, 0x670000, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0x6462FF00, 0x56805670, 0 };
	for (int i = 0; i < 10; i++)
		wr32(&buf[i * 4], head[i]);
	memset(&buf[0x28], 0xFF, 0xB8);
	wr32(&buf[0xD8], 0x50);
	wr32(&buf[0xDC], 0);
	appendChecksum(buf.data(), 8, 0x16);
	appendChecksum(buf.data(), 0x28, 0xB0);
	wr32(&buf[0x00], rd32(&buf[0x00]) ^ 0xBA1FE5D7);
	wr32(&buf[0x04], rd32(&buf[0x04]) ^ 0xD95D2DFD);
	wr32(&buf[0x20], rd32(&buf[0x20]) ^ 0xBA1FE5D7);
	wr32(&buf[0x24], rd32(&buf[0x24]) ^ 0xD95D2DFD);
	std::array<uint8_t, 64> key = keyEeprom(esn, imei);
	streamCipher(&buf[0], 0x20, key.data(), true);
	streamCipher(&buf[0x20], 0xC0, key.data(), true);
	return buf;
}

// EEPROM FULL block 77
std::vector<uint8_t> buildBlock5077(const std::string &imei, uint32_t esn) {
	std::vector<uint8_t> buf(0xE8, 0xFF);
	wr32(&buf[0], 0x56A05690);
	wr32(&buf[4], 0);
	buf[0xB] = 0;
	wr32(&buf[0xE0], 0x063DFF29);
	wr32(&buf[0xE4], 0xF3800B06);
	appendChecksum(buf.data(), 8, 0xD8);
	wr32(&buf[0], rd32(&buf[0]) ^ 0xBA1FE5D7);
	wr32(&buf[4], rd32(&buf[4]) ^ 0xD95D2DFD);
	std::array<uint8_t, 64> key = keyEeprom(esn, imei);
	streamCipher(buf.data(), buf.size(), key.data(), true);
	return buf;
}

// EEPROM FULL block 121: encrypted SKEY marker + 6 encrypted master codes
std::vector<uint8_t> buildBlock5121(const SiemensKeys &keys) {
	std::array<uint8_t, 8> imei8 = packImei8(keys.imei);

	auto key1 = [&](uint32_t code) {
		std::array<uint8_t, 64> key;
		memcpy(key.data(), KEY_MK1, 64);
		wr32(&key[0], code ^ 0x7F0BF23B);
		wr32(&key[4], keys.esn);
		memcpy(&key[0x38], imei8.data(), 8);
		return key;
	};
	auto key2 = [&](uint32_t code) {
		std::array<uint8_t, 64> key;
		memcpy(key.data(), KEY_MK2, 64);
		wr32(&key[0], ((uint32_t) imei8[0] << 8) + 8 + ((uint32_t) imei8[1] << 16) + ((uint32_t) imei8[2] << 24));
		memcpy(&key[4], &imei8[3], 4);
		wr32(&key[0x38], code ^ 0x7F0BF23B);
		wr32(&key[0x3C], keys.esn);
		return key;
	};

	std::vector<uint8_t> out(8 + 6 * 8);
	wr32(&out[0], 0x77C5742D);
	wr32(&out[4], 0xF49A4ADA);
	streamCipher(&out[0], 8, key1(keys.skey).data(), true);

	for (uint32_t k = 0; k < 6; k++) {
		uint8_t *entry = &out[8 + k * 8];
		wr32(entry, k ^ 0x77C5742D);
		wr32(entry + 4, 0xF49A4ADA);
		streamCipher(entry, 8, key2(keys.masterKeys[k]).data(), true);
		streamCipher(entry, 8, key1(keys.masterKeys[k]).data(), true);
	}
	return out;
}

// EEPROM FULL block 122
std::vector<uint8_t> buildBlock5122(uint32_t skey) {
	std::vector<uint8_t> out(6);
	wr32(&out[0], skey);
	out[4] = 0x58;
	out[5] = 0;
	return out;
}

// EEPROM FULL block 123 (short 12-byte variant used by x75/x85 firmware)
std::vector<uint8_t> buildBlock5123(const std::vector<uint8_t> &block5121, bool shortVariant) {
	std::vector<uint8_t> out(shortVariant ? 0xC : 0x20, 0);
	wr32(&out[0], shortVariant ? 0xFFFFFF3F : 0xFFFF1F07);
	memcpy(&out[4], block5121.data(), 8);
	return out;
}

// EEPROM LITE block 52
std::vector<uint8_t> buildBlock52(const std::array<uint8_t, 16> &bootKey, uint8_t verDown) {
	std::vector<uint8_t> out(0x122, 0xFF);
	memcpy(&out[0], bootKey.data(), 16);
	out[16] = verDown;
	memcpy(&out[17], BLOCK52_TAIL, sizeof(BLOCK52_TAIL));
	return out;
}

// EEPROM FULL block 468 (x85 only)
std::vector<uint8_t> buildBlock5468(const SiemensKeys &keys, const std::array<uint8_t, 16> &bootKey) {
	std::vector<uint8_t> out(0x31);
	memcpy(&out[0], bootKey.data(), 16);
	out[16] = 0x58;
	uint8_t esn[4];
	wr32(esn, keys.esn);
	std::array<uint8_t, 16> esnHash = md5(esn, 4);
	memcpy(&out[0x11], esnHash.data(), 16);
	std::array<uint8_t, 16> imeiHash = md5((const uint8_t *) keys.imei.data(), keys.imei.size());
	memcpy(&out[0x21], imeiHash.data(), 8);
	std::array<uint8_t, 16> totalHash = md5(out.data(), 0x29);
	memcpy(&out[0x29], totalHash.data(), 8);
	return out;
}

/* --------------------------------------------------------- Patching --- */

struct EepromEntry {
	uint32_t flags;
	uint32_t id;
	uint32_t size;
	uint32_t dataOffset;
};

bool readEntry(const uint8_t *hdr, EepromEntry &e) {
	e.flags = rd32(hdr);
	e.id = rd16(hdr + 4);
	e.size = rd32(hdr + 8);
	e.dataOffset = rd32(hdr + 12);
	return (e.flags == 0xFFFFFFC0 || e.flags == 0xFFFFFF00) && hdr[6] == 0 && e.id < 500 && e.size < 25000;
}

bool replaceBytes(std::vector<uint8_t> &data, const std::vector<size_t> &positions, const std::vector<uint8_t> &value) {
	bool changed = false;
	for (size_t i = 0; i < value.size(); i++) {
		if (data[positions[i]] != value[i]) {
			data[positions[i]] = value[i];
			changed = true;
		}
	}
	return changed;
}

struct Generated {
	std::vector<uint8_t> data;
	uint32_t bit;
};

struct FlashLayout {
	bool x85 = false;
	size_t blockSize = 0;
	size_t signatureOffset = 0;
	size_t hashOffset = 0;    // bootcore HASH
	size_t imeiOffset = 0;    // bootcore IMEI string
};

bool detectLayout(const std::vector<uint8_t> &ff, FlashLayout &layout) {
	if (ff.size() < 0x100000)
		return false;

	layout.x85 = memcmp(&ff[0x1200], "\x00\x03LS", 4) == 0;
	layout.blockSize = layout.x85 ? 0x40000 : 0x20000;
	layout.signatureOffset = layout.x85 ? 0x3FFE0 : 0;
	if (ff.size() % layout.blockSize != 0)
		return false;

	if (layout.x85) {
		layout.hashOffset = 0x3E400;
		layout.imeiOffset = 0x3E410;
	} else if (ff[0x200] == 2) {
		layout.hashOffset = 0x23C;
		layout.imeiOffset = 0x660;
	} else {
		layout.hashOffset = 0x238;
		layout.imeiOffset = 0x65C;
	}
	return true;
}

bool isBlank(const std::array<uint8_t, 16> &data) {
	for (uint8_t byte : data) {
		if (byte != 0xFF)
			return false;
	}
	return true;
}

// Calls fn(id, positions) for every valid EEPROM entry, where positions are the absolute
// offsets of the entry payload. FULL block ids are reported as id + 5000.
void walkEeprom(const std::vector<uint8_t> &ff, const FlashLayout &layout,
	const std::function<void(uint32_t, const std::vector<size_t> &)> &fn)
{
	for (size_t base = 0; base < ff.size(); base += layout.blockSize) {
		const uint8_t *signature = &ff[base + layout.signatureOffset];
		bool full;
		if (memcmp(signature, "EEFU", 4) == 0) {
			full = true;
		} else if (memcmp(signature, "EELI", 4) == 0) {
			full = false;
		} else {
			continue;
		}

		const size_t stride = layout.x85 ? 0x20 : 0x10;
		const uint32_t idBase = full ? 5000 : 0;
		const size_t leadingBytes = full ? 1 : 0;                     // FULL payloads carry one extra byte in front
		const size_t sizeExtra = (full ? 1 : 0) | (layout.x85 ? 0 : 1); // stored size = payload + sizeExtra

		size_t hdr = layout.x85 ? 0x3FFC0 : 0x1FFF0;
		while (hdr > 0x2000) {
			EepromEntry entry;
			bool valid = readEntry(&ff[base + hdr], entry);
			size_t next = hdr - stride;
			if (entry.flags == 0xFFFFFFFF)
				break;

			if (valid && layout.x85) {
				// Headers are not always contiguous on x85: find the next valid one.
				while (next > 0x2000) {
					EepromEntry probe;
					if (readEntry(&ff[base + next], probe))
						break;
					next -= stride;
					if (hdr - next > 0x420) {
						next = 0x200;
						break;
					}
				}
			}

			if (valid && entry.flags == 0xFFFFFFC0 && entry.size > sizeExtra) {
				const size_t payload = entry.size - sizeExtra;
				std::vector<size_t> positions;
				const bool inlineData = layout.x85 &&
					((full && entry.size <= 0x200) || (!full && entry.size <= 0x10));
				if (inlineData) {
					// Small x85 entries are stored right below the header, in 16-byte pieces every 32 bytes.
					size_t start = hdr - 0x10 - entry.size - (entry.size & ~0xFu);
					for (size_t i = start; i < hdr; i++) {
						if (!(i & 0x10))
							positions.push_back(base + i);
					}
					if (positions.size() >= leadingBytes + payload) {
						positions.erase(positions.begin(), positions.begin() + leadingBytes);
						positions.resize(payload);
						fn(entry.id + idBase, positions);
					}
				} else if ((size_t) entry.dataOffset + entry.size < layout.blockSize) {
					positions.reserve(payload);
					for (size_t i = 0; i < payload; i++)
						positions.push_back(base + entry.dataOffset + leadingBytes + i);
					fn(entry.id + idBase, positions);
				}
			}
			hdr = next;
		}
	}
}

} // namespace

std::string toHex(const uint8_t *data, size_t size) {
	static const char digits[] = "0123456789ABCDEF";
	std::string s;
	for (size_t i = 0; i < size; i++) {
		s += digits[data[i] >> 4];
		s += digits[data[i] & 0xF];
	}
	return s;
}

void siemensCalcKeys(uint32_t esn, uint32_t skey, std::array<uint8_t, 16> &bootKey, std::array<uint8_t, 16> &hash) {
	uint8_t block[16];
	wr32(&block[0], esn);
	wr32(&block[4], skey);
	for (int i = 0; i < 8; i++)
		block[8 + i] = block[i] ^ block[i + 3];
	bootKey = md5(block, 16);
	hash = md5(bootKey.data(), 16);
}

SiemensRecalcResult siemensRecalcFullflash(std::vector<uint8_t> &ff, const SiemensKeys &keys) {
	SiemensRecalcResult result;

	FlashLayout layout;
	if (!detectLayout(ff, layout)) {
		result.log.push_back("fullflash size or layout was not recognized");
		return result;
	}

	std::array<uint8_t, 16> bootKey, hash;
	siemensCalcKeys(keys.esn, keys.skey, bootKey, hash);

	// Bootcore: HASH + IMEI string
	const size_t hashOffset = layout.hashOffset;
	const size_t imeiOffset = layout.imeiOffset;
	if (rd32(&ff[hashOffset]) == 0xFFFFFFFF) {
		result.log.push_back("BCORE is erased, recalculation is not possible");
		return result;
	}
	result.structureOk = true;

	if (memcmp(&ff[hashOffset], hash.data(), 16) != 0) {
		memcpy(&ff[hashOffset], hash.data(), 16);
		result.log.push_back("BCORE: HASH replaced with " + toHex(hash.data(), 16));
		result.replaced++;
	}
	if (memcmp(&ff[imeiOffset], keys.imei.data(), 15) != 0) {
		memcpy(&ff[imeiOffset], keys.imei.data(), 15);
		result.log.push_back("BCORE: IMEI replaced with " + keys.imei);
		result.replaced++;
	}

	// EEPROM blocks. IDs >= 5000 live in EEFULL (ID - 5000), the others in EELITE.
	std::vector<uint8_t> block5121 = buildBlock5121(keys);
	std::map<uint32_t, Generated> generated = {
		{ 76,   { buildImeiBlock(keys.imei, 0), 0x001 } },
		{ 5008, { buildBlock5008(keys.imei, keys.esn), 0x002 } },
		{ 5009, { buildImeiBlock(keys.imei, 1), 0x004 } },
		{ 5077, { buildBlock5077(keys.imei, keys.esn), 0x008 } },
		{ 5121, { block5121, 0x010 } },
		{ 5122, { buildBlock5122(keys.skey), 0x020 } },
		{ 5123, { {}, 0x040 } },
		{ 52,   { buildBlock52(bootKey, keys.verDown), 0x080 } },
		{ 5468, { buildBlock5468(keys, bootKey), 0x100 } },
		{ 320,  { std::vector<uint8_t>(bootKey.begin(), bootKey.end()), 0x200 } },
	};
	const uint32_t mandatoryMask = 0xFF;
	uint32_t foundMask = 0;

	walkEeprom(ff, layout, [&](uint32_t id, const std::vector<size_t> &positions) {
		auto it = generated.find(id);
		if (it == generated.end())
			return;

		std::vector<uint8_t> value = it->second.data;
		if (id == 5123)
			value = buildBlock5123(block5121, positions.size() == 0xC);
		foundMask |= it->second.bit;

		if (positions.size() != value.size()) {
			result.log.push_back("EEPROM: block " + std::to_string(id) + " has unexpected size " +
				std::to_string(positions.size()) + ", not replaced");
			return;
		}

		if (replaceBytes(ff, positions, value)) {
			result.log.push_back("EEPROM: block " + std::to_string(id) + " replaced");
			result.replaced++;
		}
	});

	result.complete = (foundMask & mandatoryMask) == mandatoryMask;
	if (!result.complete)
		result.log.push_back("EEPROM: not all confidential blocks were found (mask " + std::to_string(foundMask) + ")");
	return result;
}

SiemensIdentity siemensReadIdentity(const std::vector<uint8_t> &ff) {
	SiemensIdentity identity;

	FlashLayout layout;
	if (!detectLayout(ff, layout))
		return identity;

	const char *imei = (const char *) &ff[layout.imeiOffset];
	for (int i = 0; i < 15; i++) {
		if (imei[i] < '0' || imei[i] > '9')
			return identity;
	}
	identity.imei.assign(imei, 15);
	memcpy(identity.hash.data(), &ff[layout.hashOffset], 16);

	bool haveSkey = false;
	walkEeprom(ff, layout, [&](uint32_t id, const std::vector<size_t> &positions) {
		if (id == 5122 && positions.size() >= 4) {
			uint8_t raw[4];
			for (int i = 0; i < 4; i++)
				raw[i] = ff[positions[i]];
			identity.skey = rd32(raw);
			haveSkey = true;
		} else if (id == 52 && positions.size() >= 16) {
			for (int i = 0; i < 16; i++)
				identity.bootKey[i] = ff[positions[i]];
			identity.hasBootKey = true;
		}
	});

	identity.ok = haveSkey && (!isBlank(identity.hash) || (identity.hasBootKey && !isBlank(identity.bootKey)));
	return identity;
}

bool siemensRecoverEsn(const SiemensIdentity &identity, uint32_t &esn, unsigned threadCount) {
	if (!identity.ok)
		return false;

	// One MD5 per candidate when the BootKey is known, two when only the bootcore HASH is.
	const bool useBootKey = identity.hasBootKey && !isBlank(identity.bootKey);
	const std::array<uint8_t, 16> &wanted = useBootKey ? identity.bootKey : identity.hash;
	uint32_t target[4];
	for (int i = 0; i < 4; i++)
		target[i] = rd32(wanted.data() + i * 4);

	if (!threadCount)
		threadCount = std::max(1u, std::thread::hardware_concurrency());

	const uint32_t skey = identity.skey;
	std::atomic<bool> found(false);
	std::atomic<uint32_t> result(0);

	std::vector<std::thread> workers;
	workers.reserve(threadCount);
	for (unsigned t = 0; t < threadCount; t++) {
		workers.emplace_back([&, t]() {
			// Both hashed messages are 16 bytes, so the padding words never change.
			uint32_t X[16] = {};
			uint32_t Y[16] = {};
			X[4] = Y[4] = 0x80;
			X[14] = Y[14] = 128;

			uint32_t ticks = 0;
			for (uint64_t candidate = t; candidate < 0x100000000ULL; candidate += threadCount) {
				if (++ticks >= 0x40000) {
					ticks = 0;
					if (found.load(std::memory_order_relaxed))
						return;
				}

				// The key block is ESN, SKEY and eight bytes derived from both (see siemensCalcKeys).
				const uint32_t value = (uint32_t) candidate;
				X[0] = value;
				X[1] = skey;
				X[2] = value ^ (value >> 24) ^ (skey << 8);
				X[3] = skey ^ (skey >> 24) ^ (X[2] << 8);

				uint32_t h[4] = { MD5_INIT[0], MD5_INIT[1], MD5_INIT[2], MD5_INIT[3] };
				md5Transform(h, X);
				if (!useBootKey) {
					Y[0] = h[0];
					Y[1] = h[1];
					Y[2] = h[2];
					Y[3] = h[3];
					h[0] = MD5_INIT[0];
					h[1] = MD5_INIT[1];
					h[2] = MD5_INIT[2];
					h[3] = MD5_INIT[3];
					md5Transform(h, Y);
				}

				if (h[0] == target[0] && h[1] == target[1] && h[2] == target[2] && h[3] == target[3]) {
					result.store(value, std::memory_order_relaxed);
					found.store(true, std::memory_order_release);
					return;
				}
			}
		});
	}
	for (std::thread &worker : workers)
		worker.join();

	if (!found.load(std::memory_order_acquire))
		return false;

	// Confirm the hit with the plain implementation before trusting it.
	const uint32_t candidate = result.load(std::memory_order_relaxed);
	std::array<uint8_t, 16> bootKey, hash;
	siemensCalcKeys(candidate, skey, bootKey, hash);
	if ((useBootKey ? bootKey : hash) != wanted)
		return false;

	esn = candidate;
	return true;
}
