#pragma once

#include <filesystem>
#include <string>
#include <vector>

std::filesystem::path getExecutableDirectory();

static inline bool isWindows() {
#ifdef _WIN32
	return true;
#else
	return false;
#endif
}

void setEnvironmentVariable(const std::string &name, const std::string &value);
int executeProcess(const std::vector<std::string> &argv);
