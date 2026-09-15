#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

inline constexpr size_t MD5_BATCH_SIZE = 8;

using Md5BlockBatch = std::array<std::array<uint32_t, MD5_BATCH_SIZE>, 16>;
using Md5DigestBatch = std::array<std::array<uint32_t, MD5_BATCH_SIZE>, 4>;

// Hashes complete one-block messages whose MD5 padding is already encoded.
void md5Batch(Md5DigestBatch &hashes, const Md5BlockBatch &blocks);
std::array<uint8_t, 16> md5(const uint8_t *data, size_t size);
