#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct DetectedBoard {
	std::string device;
	std::string vendor;
	std::string model;
};

std::vector<DetectedBoard> detectBoards(const std::filesystem::path &boardDir, const std::vector<uint8_t> &fullflash);
std::optional<std::string> findBoardByAlias(const std::filesystem::path &boardDir, const std::string &alias);
std::string getBoardPlatform(const std::filesystem::path &configPath);
