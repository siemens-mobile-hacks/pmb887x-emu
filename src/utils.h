#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

std::filesystem::path getExecutableDir();

static inline bool isWindows() {
#ifdef _WIN32
	return true;
#else
	return false;
#endif
}

static inline bool isOSX() {
#ifdef __APPLE__
	return true;
#else
	return false;
#endif
}

static inline bool isUNIX() {
	return !isOSX() && !isWindows();
}

void setEnv(const std::string &name, const std::string &value);
std::string convertESNtoOTP(const std::string &esn);
std::string convertIMEItoOTP(const std::string &imei);
int exec(const std::vector<std::string> &argv);
std::string strJoin(const std::vector<std::string> &vec, const std::string &delimiter = "");

std::string esnToHex(uint32_t esn);
std::string esnCachePath(const std::string &fullflash);
bool readEsnCache(const std::string &fullflash, const std::string &imei, const std::string &key, uint32_t &esn);
void writeEsnCache(const std::string &fullflash, const std::string &imei, const std::string &key, uint32_t esn);

bool readFile(const std::string &path, std::vector<uint8_t> &data);
bool writeFile(const std::string &path, const std::vector<uint8_t> &data);
bool patchFile(const std::string &path, const std::vector<uint8_t> &original, const std::vector<uint8_t> &data);

// Exposes a blob to other processes as a file path: an anonymous memory file on Linux,
// a temporary file elsewhere.
class TempFileCopy {
public:
	~TempFileCopy();
	bool create(const std::vector<uint8_t> &data);
	const std::string &path() const { return filePath; }

private:
	int fd = -1;
	std::string filePath;
	bool isTempFile = false;
};
