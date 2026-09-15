#include "string.h"

std::string joinStrings(const std::vector<std::string> &strings, const std::string &delimiter) {
	if (strings.empty())
		return "";

	auto result = strings[0];
	for (size_t i = 1; i < strings.size(); ++i) {
		result += delimiter;
		result += strings[i];
	}

	return result;
}

std::string bytesToHex(const uint8_t *data, size_t size) {
	static const char DIGITS[] = "0123456789ABCDEF";
	std::string result;
	result.reserve(size * 2);
	for (size_t index = 0; index < size; index++) {
		result += DIGITS[(data[index] >> 4)];
		result += DIGITS[(data[index] & 0xF)];
	}
	return result;
}
