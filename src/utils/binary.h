#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>

static inline bool isErasedData(const uint8_t *data, size_t size) {
	return size != 0 && std::all_of(data, data + size, [](uint8_t value) { return value == 0xFF; });
}

template<typename T>
static inline T readUnsignedLE(const uint8_t *data) {
	T value = 0;
	for (size_t index = 0; index < sizeof(T); index++)
		value |= ((T) data[index] << (index * 8));
	return value;
}

template<typename T>
static inline T readUnsignedBE(const uint8_t *data) {
	T value = 0;
	for (size_t index = 0; index < sizeof(T); index++)
		value |= ((T) data[index] << ((sizeof(T) - index - 1) * 8));
	return value;
}

template<typename T>
static inline void writeUnsignedLE(uint8_t *data, T value) {
	for (size_t index = 0; index < sizeof(T); index++)
		data[index] = (uint8_t) (value >> (index * 8));
}

template<typename T>
static inline void writeUnsignedBE(uint8_t *data, T value) {
	for (size_t index = 0; index < sizeof(T); index++)
		data[index] = (uint8_t) (value >> ((sizeof(T) - index - 1) * 8));
}

static inline uint8_t readUInt8(const uint8_t *data) {
	return data[0];
}

static inline int8_t readInt8(const uint8_t *data) {
	return std::bit_cast<int8_t>(readUInt8(data));
}

static inline uint16_t readUInt16LE(const uint8_t *data) {
	return readUnsignedLE<uint16_t>(data);
}

static inline uint16_t readUInt16BE(const uint8_t *data) {
	return readUnsignedBE<uint16_t>(data);
}

static inline int16_t readInt16LE(const uint8_t *data) {
	return std::bit_cast<int16_t>(readUInt16LE(data));
}

static inline int16_t readInt16BE(const uint8_t *data) {
	return std::bit_cast<int16_t>(readUInt16BE(data));
}

static inline uint32_t readUInt32LE(const uint8_t *data) {
	return readUnsignedLE<uint32_t>(data);
}

static inline uint32_t readUInt32BE(const uint8_t *data) {
	return readUnsignedBE<uint32_t>(data);
}

static inline int32_t readInt32LE(const uint8_t *data) {
	return std::bit_cast<int32_t>(readUInt32LE(data));
}

static inline int32_t readInt32BE(const uint8_t *data) {
	return std::bit_cast<int32_t>(readUInt32BE(data));
}

static inline uint64_t readUInt64LE(const uint8_t *data) {
	return readUnsignedLE<uint64_t>(data);
}

static inline uint64_t readUInt64BE(const uint8_t *data) {
	return readUnsignedBE<uint64_t>(data);
}

static inline int64_t readInt64LE(const uint8_t *data) {
	return std::bit_cast<int64_t>(readUInt64LE(data));
}

static inline int64_t readInt64BE(const uint8_t *data) {
	return std::bit_cast<int64_t>(readUInt64BE(data));
}

static inline float readFloatLE(const uint8_t *data) {
	return std::bit_cast<float>(readUInt32LE(data));
}

static inline float readFloatBE(const uint8_t *data) {
	return std::bit_cast<float>(readUInt32BE(data));
}

static inline double readDoubleLE(const uint8_t *data) {
	return std::bit_cast<double>(readUInt64LE(data));
}

static inline double readDoubleBE(const uint8_t *data) {
	return std::bit_cast<double>(readUInt64BE(data));
}

static inline void writeUInt8(uint8_t *data, uint8_t value) {
	data[0] = value;
}

static inline void writeInt8(uint8_t *data, int8_t value) {
	writeUInt8(data, std::bit_cast<uint8_t>(value));
}

static inline void writeUInt16LE(uint8_t *data, uint16_t value) {
	writeUnsignedLE(data, value);
}

static inline void writeUInt16BE(uint8_t *data, uint16_t value) {
	writeUnsignedBE(data, value);
}

static inline void writeInt16LE(uint8_t *data, int16_t value) {
	writeUInt16LE(data, std::bit_cast<uint16_t>(value));
}

static inline void writeInt16BE(uint8_t *data, int16_t value) {
	writeUInt16BE(data, std::bit_cast<uint16_t>(value));
}

static inline void writeUInt32LE(uint8_t *data, uint32_t value) {
	writeUnsignedLE(data, value);
}

static inline void writeUInt32BE(uint8_t *data, uint32_t value) {
	writeUnsignedBE(data, value);
}

static inline void writeInt32LE(uint8_t *data, int32_t value) {
	writeUInt32LE(data, std::bit_cast<uint32_t>(value));
}

static inline void writeInt32BE(uint8_t *data, int32_t value) {
	writeUInt32BE(data, std::bit_cast<uint32_t>(value));
}

static inline void writeUInt64LE(uint8_t *data, uint64_t value) {
	writeUnsignedLE(data, value);
}

static inline void writeUInt64BE(uint8_t *data, uint64_t value) {
	writeUnsignedBE(data, value);
}

static inline void writeInt64LE(uint8_t *data, int64_t value) {
	writeUInt64LE(data, std::bit_cast<uint64_t>(value));
}

static inline void writeInt64BE(uint8_t *data, int64_t value) {
	writeUInt64BE(data, std::bit_cast<uint64_t>(value));
}

static inline void writeFloatLE(uint8_t *data, float value) {
	writeUInt32LE(data, std::bit_cast<uint32_t>(value));
}

static inline void writeFloatBE(uint8_t *data, float value) {
	writeUInt32BE(data, std::bit_cast<uint32_t>(value));
}

static inline void writeDoubleLE(uint8_t *data, double value) {
	writeUInt64LE(data, std::bit_cast<uint64_t>(value));
}

static inline void writeDoubleBE(uint8_t *data, double value) {
	writeUInt64BE(data, std::bit_cast<uint64_t>(value));
}
