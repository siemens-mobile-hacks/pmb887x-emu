#pragma once

#include <cstdint>
#include <string>
#include <vector>

bool readFile(const std::string &path, std::vector<uint8_t> &data);
bool writeFile(const std::string &path, const std::vector<uint8_t> &data);
bool replaceFile(const std::string &path, const std::vector<uint8_t> &data);
