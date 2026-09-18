#include <catch2/catch_test_macros.hpp>

#include "utils/binary.h"
#include "utils/file.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <random>
#include <vector>

TEST_CASE("Binary integers use the requested byte order") {
	std::array<uint8_t, 8> data{};

	writeUInt8(data.data(), 0x81);
	CHECK(readUInt8(data.data()) == 0x81);
	CHECK(readInt8(data.data()) == -127);
	writeInt8(data.data(), -2);
	CHECK(data[0] == 0xFE);

	writeUInt16LE(data.data(), 0x1234);
	CHECK(data[0] == 0x34);
	CHECK(data[1] == 0x12);
	CHECK(readUInt16LE(data.data()) == 0x1234);
	CHECK(readInt16LE(data.data()) == 0x1234);
	writeInt16LE(data.data(), -2);
	CHECK(readInt16LE(data.data()) == -2);

	writeUInt16BE(data.data(), 0x1234);
	CHECK(data[0] == 0x12);
	CHECK(data[1] == 0x34);
	CHECK(readUInt16BE(data.data()) == 0x1234);
	writeInt16BE(data.data(), -2);
	CHECK(readInt16BE(data.data()) == -2);

	writeUInt32LE(data.data(), 0x12345678);
	CHECK(readUInt32LE(data.data()) == 0x12345678);
	CHECK(readInt32LE(data.data()) == 0x12345678);
	writeInt32LE(data.data(), -2);
	CHECK(readInt32LE(data.data()) == -2);

	writeUInt32BE(data.data(), 0x12345678);
	CHECK(readUInt32BE(data.data()) == 0x12345678);
	writeInt32BE(data.data(), -2);
	CHECK(readInt32BE(data.data()) == -2);

	writeUInt64LE(data.data(), UINT64_C(0x123456789ABCDEF0));
	CHECK(readUInt64LE(data.data()) == UINT64_C(0x123456789ABCDEF0));
	CHECK(readInt64LE(data.data()) == INT64_C(0x123456789ABCDEF0));
	writeInt64LE(data.data(), -2);
	CHECK(readInt64LE(data.data()) == -2);

	writeUInt64BE(data.data(), UINT64_C(0x123456789ABCDEF0));
	CHECK(readUInt64BE(data.data()) == UINT64_C(0x123456789ABCDEF0));
	writeInt64BE(data.data(), -2);
	CHECK(readInt64BE(data.data()) == -2);
}

TEST_CASE("Binary floating-point values use the requested byte order") {
	std::array<uint8_t, 8> data{};

	writeFloatLE(data.data(), 1.0f);
	CHECK(readUInt32LE(data.data()) == 0x3F800000);
	CHECK(readFloatLE(data.data()) == 1.0f);

	writeFloatBE(data.data(), 1.0f);
	CHECK(readUInt32BE(data.data()) == 0x3F800000);
	CHECK(readFloatBE(data.data()) == 1.0f);

	writeDoubleLE(data.data(), 1.0);
	CHECK(readUInt64LE(data.data()) == UINT64_C(0x3FF0000000000000));
	CHECK(readDoubleLE(data.data()) == 1.0);

	writeDoubleBE(data.data(), 1.0);
	CHECK(readUInt64BE(data.data()) == UINT64_C(0x3FF0000000000000));
	CHECK(readDoubleBE(data.data()) == 1.0);
}

TEST_CASE("File replacement preserves content and permissions") {
	auto name = "pmb887x-replace-file-test-" + std::to_string(std::random_device{}()) + ".bin";
	auto path = std::filesystem::temp_directory_path() / name;
	std::vector<uint8_t> original = { 1, 2, 3 };
	std::vector<uint8_t> replacement = { 4, 5, 6, 7 };
	std::vector<uint8_t> result;
	std::filesystem::remove(path);

	REQUIRE(writeFile(path.string(), original));
	auto permissions = std::filesystem::status(path).permissions();
	REQUIRE(replaceFile(path.string(), replacement));
	REQUIRE(readFile(path.string(), result));
	CHECK(result == replacement);
	CHECK(std::filesystem::status(path).permissions() == permissions);

	std::filesystem::remove(path);
}
