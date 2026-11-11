#pragma once

#include <cstdint>
#include <string>
#include <vector>

class TempFileCopy {
public:
	TempFileCopy() = default;
	TempFileCopy(const TempFileCopy &) = delete;
	TempFileCopy &operator=(const TempFileCopy &) = delete;
	~TempFileCopy();
	const std::string &create(const std::vector<uint8_t> &data);

private:
#if defined(__linux__) || defined(__APPLE__)
	int m_fd = -1;
#endif
	std::string m_filePath;
	bool m_isTempFile = false;
};
