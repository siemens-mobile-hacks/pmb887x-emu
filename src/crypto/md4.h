#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Hashes exactly 64 bytes.
std::array<uint8_t, 16> md4(const uint8_t *data);
