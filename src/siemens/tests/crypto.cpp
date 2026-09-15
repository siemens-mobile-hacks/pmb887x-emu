#include <catch2/catch_test_macros.hpp>

#include "crypto/md4.h"
#include "crypto/md5.h"
#include "siemens/crypto.h"
#include "siemens/otp.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

TEST_CASE("MD5 matches standard vectors") {
	static const std::array<uint8_t, 16> EMPTY_HASH = { 0xD4, 0x1D, 0x8C, 0xD9, 0x8F, 0x00, 0xB2, 0x04, 0xE9, 0x80, 0x09, 0x98, 0xEC, 0xF8, 0x42, 0x7E };
	static const std::array<uint8_t, 16> ABC_HASH = { 0x90, 0x01, 0x50, 0x98, 0x3C, 0xD2, 0x4F, 0xB0, 0xD6, 0x96, 0x3F, 0x7D, 0x28, 0xE1, 0x7F, 0x72 };
	static const std::array<uint8_t, 16> LONG_HASH = { 0x57, 0xED, 0xF4, 0xA2, 0x2B, 0xE3, 0xC9, 0x55, 0xAC, 0x49, 0xDA, 0x2E, 0x21, 0x07, 0xB6, 0x7A };
	static const char LONG_MESSAGE[] = "12345678901234567890123456789012345678901234567890123456789012345678901234567890";

	CHECK(md5((const uint8_t *) "", 0) == EMPTY_HASH);
	CHECK(md5((const uint8_t *) "abc", 3) == ABC_HASH);
	CHECK(md5((const uint8_t *) LONG_MESSAGE, sizeof(LONG_MESSAGE) - 1) == LONG_HASH);
}

TEST_CASE("MD4 matches a known 64-byte block digest") {
	static const std::array<uint8_t, 16> ZERO_BLOCK_HASH = { 0x2F, 0x6F, 0x7B, 0x10, 0xC5, 0xCA, 0xDC, 0xA6, 0xD5, 0x77, 0x0F, 0x42, 0x8C, 0x89, 0x9B, 0xA7 };
	std::array<uint8_t, 64> block{};

	CHECK(md4(block.data()) == ZERO_BLOCK_HASH);
}

TEST_CASE("Siemens keys match known values") {
	static const uint32_t SKEY = 12345678;
	static const std::array<uint8_t, 16> EXPECTED_BKEY = { 0x20, 0x62, 0x67, 0x0E, 0x09, 0x53, 0x7A, 0x7F, 0xE0, 0x79, 0x46, 0x2A, 0x0E, 0xC9, 0x81, 0x68 };
	static const std::array<uint8_t, 16> EXPECTED_HASH = { 0x54, 0xF8, 0x0A, 0xC1, 0x2A, 0xCD, 0x94, 0xB2, 0xF5, 0xCF, 0xFB, 0x9B, 0xF7, 0xE4, 0xD4, 0x93 };
	std::array<uint8_t, 16> bkey;
	std::array<uint8_t, 16> hash;

	SiemensFW::calculateBkeyAndHash(0x12345678, SKEY, bkey, hash);

	CHECK(bkey == EXPECTED_BKEY);
	CHECK(hash == EXPECTED_HASH);
}

TEST_CASE("Siemens ciphers match known vectors") {
	// Independently calculated from CryptEEP.pas in PapuaUtils 0.7.9.
	static const std::array<uint8_t, 16> EXPECTED_CIPHER = { 0x66, 0x38, 0x88, 0x14, 0x48, 0xAE, 0xFA, 0xC7,
		0x6B, 0x98, 0xF9, 0x59, 0xB7, 0x43, 0x17, 0x48 };
	static const std::array<uint8_t, 10> EXPECTED_LITE_IMEI = { 0xDF, 0x70, 0xD8, 0x7E, 0xDF, 0xB1, 0xC2, 0xB1, 0xC2, 0xBE };
	static const std::array<uint8_t, 10> EXPECTED_FULL_IMEI = { 0xFC, 0x5C, 0xF8, 0x56, 0xFD, 0xBF, 0x65, 0xBE, 0x6A, 0xB8 };
	std::array<uint8_t, 64> key;
	for (size_t index = 0; index < key.size(); index++)
		key[index] = (uint8_t) index;

	std::array<uint8_t, 16> data;
	for (size_t index = 0; index < data.size(); index++)
		data[index] = (uint8_t) index;
	auto encrypted = data;
	SiemensFW::cipherEncrypt(encrypted.data(), encrypted.size(), key.data());
	CHECK(encrypted == EXPECTED_CIPHER);
	auto decrypted = EXPECTED_CIPHER;
	SiemensFW::cipherDecrypt(decrypted.data(), decrypted.size(), key.data());
	CHECK(decrypted == data);

	std::array<uint8_t, 10> imei = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	auto liteImei = imei;
	SiemensFW::imeiCipherEncrypt(liteImei.data(), false);
	CHECK(liteImei == EXPECTED_LITE_IMEI);
	auto decryptedLiteImei = EXPECTED_LITE_IMEI;
	SiemensFW::imeiCipherDecrypt(decryptedLiteImei.data(), false);
	CHECK(decryptedLiteImei == imei);

	auto fullImei = imei;
	SiemensFW::imeiCipherEncrypt(fullImei.data(), true);
	CHECK(fullImei == EXPECTED_FULL_IMEI);
	auto decryptedFullImei = EXPECTED_FULL_IMEI;
	SiemensFW::imeiCipherDecrypt(decryptedFullImei.data(), true);
	CHECK(decryptedFullImei == imei);
}

TEST_CASE("Siemens identities convert to exact OTP and reject malformed input") {
	CHECK(SiemensFW::esnToOtp("12345678") == "02004AB3C31100000000");
	CHECK(SiemensFW::imeiToOtp("490154203237518") == "000094104502237315FF");
	CHECK_THROWS_AS(SiemensFW::imeiToOtp("12345678901234X"), std::invalid_argument);
	CHECK_THROWS_AS(SiemensFW::esnToOtp("1234567Z"), std::invalid_argument);
}
