#include <catch2/catch_test_macros.hpp>

#include "board_config.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using namespace std::string_view_literals;

struct FullflashMarker {
	size_t offset;
	std::string_view bytes;
};

static std::vector<std::string> detectDevices(const std::vector<FullflashMarker> &markers) {
	std::vector<uint8_t> fullflash(0x100000, 0xFF);
	for (const auto &marker : markers)
		std::copy(marker.bytes.begin(), marker.bytes.end(), fullflash.begin() + marker.offset);

	std::vector<std::string> devices;
	for (const auto &board : detectBoards(BOARD_DIR, fullflash))
		devices.push_back(board.device);
	return devices;
}

TEST_CASE("APOXI boards are detected by their fullflash markers") {
	const auto son80 = FullflashMarker { 0x40C00, "DWD_DRIVER_SON80_17.18.00 SMARTI_PM SGOLD2\0"sv };

	CHECK(detectDevices({ son80, { 0x84FE4, "+DDLI:\"KE970\",\"SW 1.1\""sv } }) == std::vector<std::string> { "lg-ke970" });
	CHECK(detectDevices({ son80, { 0x84FE4, "+DDLI:\"KE800\",\"SW 1.1\""sv } }) == std::vector<std::string> { "lg-ke800" });
	CHECK(detectDevices({ { 0x40000, "VS70VA57\0"sv } }) == std::vector<std::string> { "panasonic-705p" });
	CHECK(detectDevices({ { 0x20C20, "NBG_LEOPARD_05.23 SMARTI_SDB1 SGOLDliteV1.1\0"sv } }) == std::vector<std::string> { "panasonic-vs7" });
	CHECK(detectDevices({ { 0x20C20, "NBG_LEOPARD_05.19.02 SMARTI_SDB1 SGOLDliteV1.1\0"sv } }) == std::vector<std::string> { "panasonic-vs7" });
	CHECK(detectDevices({
		{ 0x40C00, "DEMOBL SMARTI_PM SGOLD\0"sv },
		{ 0x90088, "DEMOSTAT\0UNUSED\0\0SP85\0\0\0\0SIEMENS\0undefined\0"sv },
	}) == std::vector<std::string> { "siemens-sl98" });
}

TEST_CASE("A board is detected only when all of its patterns match") {
	CHECK(detectDevices({ { 0x40C00, "DWD_DRIVER_SON80_17.18.00 SMARTI_PM SGOLD2\0"sv } }).empty());
	CHECK(detectDevices({ { 0x84FE4, "+DDLI:\"KE970\""sv } }).empty());
	CHECK(detectDevices({}).empty());
}

static void writeBoardConfig(const std::filesystem::path &path, const std::string &aliases) {
	std::ofstream output(path);
	output << "[board]\naliases = " << aliases << "\n";
}

TEST_CASE("Board aliases resolve to board names") {
	CHECK(findBoardByAlias(BOARD_DIR, "panasonic-sa7") == "panasonic-vs7");
	CHECK(findBoardByAlias(BOARD_DIR, "panasonic-vs3") == "panasonic-vs7");

	auto boardDir = std::filesystem::temp_directory_path() / "pmb887x-emu-board-aliases";
	std::filesystem::remove_all(boardDir);
	std::filesystem::create_directories(boardDir);
	writeBoardConfig(boardDir / "vendor-first.toml", R"(["vendor-first-a", "vendor-first-b"])");
	writeBoardConfig(boardDir / "vendor-second.toml", R"(["vendor-second-a"])");

	CHECK(findBoardByAlias(boardDir, "vendor-first-b") == "vendor-first");
	CHECK(findBoardByAlias(boardDir, "vendor-second-a") == "vendor-second");
	CHECK(findBoardByAlias(boardDir, "vendor-first") == std::nullopt);

	writeBoardConfig(boardDir / "vendor-third.toml", R"(["vendor-first-a"])");
	CHECK_THROWS(findBoardByAlias(boardDir, "vendor-first-a"));

	std::filesystem::remove_all(boardDir);
}
