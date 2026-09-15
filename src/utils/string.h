#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

std::string joinStrings(const std::vector<std::string> &strings, const std::string &delimiter = "");
std::string bytesToHex(const uint8_t *data, size_t size);
