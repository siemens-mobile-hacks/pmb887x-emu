#include "board_config.h"

#include <ptr89.h>
#include <toml++/toml.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>

static const uint32_t FULLFLASH_BASE = 0xA0000000;

struct BoardConfig {
	std::filesystem::path path;
	toml::table config;
};

struct BoardPattern {
	std::optional<size_t> offset;
	std::shared_ptr<Ptr89::PtrExp> pattern;
};

static BoardPattern parseBoardPattern(const toml::node &node) {
	const toml::table *table = node.as_table();
	if (!table)
		throw std::runtime_error("board.patterns entry must be a table");

	auto pattern = (*table)["pattern"].value<std::string>();
	if (!pattern)
		throw std::runtime_error("board.patterns entry needs a pattern");

	BoardPattern result = { std::nullopt, Ptr89::Pattern::parse(*pattern) };
	if (table->contains("offset")) {
		auto offset = (*table)["offset"].value<int64_t>();
		if (!offset)
			throw std::runtime_error("board.patterns offset must be an integer");
		if (*offset < 0)
			throw std::runtime_error("board.patterns offset must not be negative");
		result.offset = *offset;
	}
	return result;
}

static std::vector<BoardPattern> parseBoardPatterns(const std::filesystem::path &path, const toml::array &nodes) {
	try {
		if (nodes.empty())
			throw std::runtime_error("board.patterns is empty");

		std::vector<BoardPattern> patterns;
		for (const toml::node &node : nodes)
			patterns.push_back(parseBoardPattern(node));
		std::stable_partition(patterns.begin(), patterns.end(), [](const BoardPattern &pattern) { return pattern.offset.has_value(); });
		return patterns;
	} catch (const std::exception &error) {
		throw std::runtime_error(path.string() + ": " + error.what());
	}
}

static bool matchBoardPattern(const BoardPattern &pattern, const Ptr89::Pattern::Memory &memory) {
	if (pattern.offset)
		return Ptr89::Pattern::checkPattern(pattern.pattern, *pattern.offset, memory);
	return !Ptr89::Pattern::find(pattern.pattern, memory, 1).empty();
}

static std::vector<BoardConfig> loadBoardConfigs(const std::filesystem::path &boardDir) {
	std::vector<BoardConfig> boards;
	for (const auto &entry : std::filesystem::directory_iterator(boardDir)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".toml")
			continue;
		try {
			boards.push_back({ entry.path(), toml::parse_file(entry.path().string()) });
		} catch (const toml::parse_error &error) {
			throw std::runtime_error(entry.path().string() + ": " + std::string(error.description()));
		}
	}
	std::sort(boards.begin(), boards.end(), [](const BoardConfig &a, const BoardConfig &b) { return a.path < b.path; });
	return boards;
}

std::vector<DetectedBoard> detectBoards(const std::filesystem::path &boardDir, const std::vector<uint8_t> &fullflash) {
	Ptr89::Pattern::Memory memory = { FULLFLASH_BASE, fullflash.data(), fullflash.size() };

	std::vector<DetectedBoard> boards;
	for (const auto &board : loadBoardConfigs(boardDir)) {
		const toml::array *nodes = board.config["board"]["patterns"].as_array();
		if (!nodes)
			continue;

		auto patterns = parseBoardPatterns(board.path, *nodes);
		auto isMatched = [&memory](const BoardPattern &pattern) { return matchBoardPattern(pattern, memory); };
		if (!std::all_of(patterns.begin(), patterns.end(), isMatched))
			continue;

		boards.push_back({
			board.path.stem().string(),
			board.config["board"]["vendor"].value_or(std::string()),
			board.config["board"]["model"].value_or(std::string()),
		});
	}
	return boards;
}

std::optional<std::string> findBoardByAlias(const std::filesystem::path &boardDir, const std::string &alias) {
	std::vector<std::string> devices;
	for (const auto &board : loadBoardConfigs(boardDir)) {
		const toml::array *aliases = board.config["board"]["aliases"].as_array();
		if (!aliases)
			continue;

		auto isAlias = [&alias](const toml::node &node) { return node.value<std::string>() == alias; };
		if (std::any_of(aliases->begin(), aliases->end(), isAlias))
			devices.push_back(board.path.stem().string());
	}

	if (devices.empty())
		return std::nullopt;
	if (devices.size() > 1)
		throw std::runtime_error("Device alias " + alias + " is used by several boards");
	return devices[0];
}
