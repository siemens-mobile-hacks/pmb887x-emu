#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Exposes a blob to other processes as a file path: an anonymous file on Linux and macOS,
// a temporary file elsewhere.
class TempFileCopy {
public:
	TempFileCopy() = default;
	TempFileCopy(const TempFileCopy &) = delete;
	TempFileCopy &operator=(const TempFileCopy &) = delete;
	~TempFileCopy();
	const std::string &create(const std::vector<uint8_t> &data);

private:
	int m_fd = -1;
	std::string m_filePath;
	bool m_isTempFile = false;
};
