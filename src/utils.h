#pragma once

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
