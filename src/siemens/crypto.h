#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace siemens {

inline constexpr size_t CIPHER_BATCH_SIZE = 8;

using CipherKeyBatch = std::array<std::array<uint8_t, 64>, CIPHER_BATCH_SIZE>;

void cipherEncrypt(uint8_t *data, size_t size, const uint8_t *key);
void cipherDecrypt(uint8_t *data, size_t size, const uint8_t *key);
void cipherDecryptBatch(uint8_t *data, size_t size, const CipherKeyBatch &keys);
void imeiCipherEncrypt(uint8_t *data, bool fullBlock);
void imeiCipherDecrypt(uint8_t *data, bool fullBlock);
void calculateBkeyAndHash(uint32_t esn, uint32_t skey, std::array<uint8_t, 16> &bkey, std::array<uint8_t, 16> &hash);
std::array<uint8_t, 8> packImei(const std::string &imei);
uint8_t calculateImeiCheckDigit(const std::string &imei);
std::array<uint8_t, 64> buildEepromKey(uint32_t esn, const std::string &imei);
std::array<uint8_t, 64> buildCipherKey1(uint32_t code, uint32_t esn, const std::array<uint8_t, 8> &imei);
std::array<uint8_t, 64> buildCipherKey2(uint32_t code, uint32_t esn, const std::array<uint8_t, 8> &imei);

}
