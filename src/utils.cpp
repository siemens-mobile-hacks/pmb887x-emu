#include "utils.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <stdexcept>

#ifdef _WIN32
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <limits.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <linux/limits.h>
#elif defined(__FreeBSD__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
#endif

#ifdef _WIN32
#include <process.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#endif

std::filesystem::path getExecutableDir() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        return std::filesystem::path(buffer).parent_path();
    }
    return std::filesystem::current_path();
#elif defined(__linux__)
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        return std::filesystem::path(buffer).parent_path();
    }
    return std::filesystem::current_path();
#elif defined(__FreeBSD__)
    char buffer[PATH_MAX];
    size_t size = sizeof(buffer);
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    if (sysctl(mib, 4, buffer, &size, nullptr, 0) == 0) {
        return std::filesystem::path(buffer).parent_path();
    }
    return std::filesystem::current_path();
#else
    #error "Unsupported platform"
#endif
}

void setEnv(const std::string &name, const std::string &value) {
#ifdef _WIN32
	_putenv((name + "=" + value).c_str());
#else
	setenv(name.c_str(), value.c_str(), true);
#endif
}

int exec(const std::vector<std::string> &argv) {
#ifdef _WIN32
	std::vector<char *> args;
	for (const auto &arg: argv)
		args.push_back(const_cast<char *>(arg.c_str()));
	args.push_back(nullptr);

    intptr_t result = _spawnvp(_P_WAIT, args[0], args.data());
    if (result == -1)
        throw std::runtime_error("_spawnvp failed");
    return static_cast<int>(result);
#else
	std::vector<char *> args;
	for (const auto &arg: argv)
		args.push_back(const_cast<char *>(arg.c_str()));
	args.push_back(nullptr);

	pid_t pid;
	int result = posix_spawnp(&pid, args[0], nullptr, nullptr, args.data(), environ);
	if (result != 0)
		throw std::runtime_error("posix_spawn failed");

	int status;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
#endif
}

std::string strJoin(const std::vector<std::string> &vec, const std::string &delimiter) {
	if (vec.empty())
		return "";

	std::string result = vec[0];
	for (size_t i = 1; i < vec.size(); ++i) {
		result += delimiter;
		result += vec[i];
	}

	return result;
}

std::string convertIMEItoOTP(const std::string &imei) {
	if (imei.length() != 15)
		throw std::invalid_argument("Invalid IMEI: " + imei);

	std::string otpImei;
	for (size_t i = 0; i < imei.length() - 1; i += 2) {
		otpImei += imei[i + 1];
		otpImei += imei[i];
	}

	return "0000" + otpImei + "FF";
}

std::string convertESNtoOTP(const std::string &esn) {
	if (esn.length() != 8)
		throw std::invalid_argument("Invalid ESN: " + esn);

	const uint8_t ESN_KEY[] = {0x32, 0xE5, 0xF7, 0x03};
	std::string otpEsn;

	const char hex[] = "0123456789ABCDEF";

	for (uint8_t i = 0; i < 4; i++) {
		std::string byteStr = esn.substr((3 - i) * 2, 2);
		uint8_t byteVal = std::stoul(byteStr, nullptr, 16);
		uint8_t result = byteVal ^ ESN_KEY[i];

		otpEsn += hex[result >> 4];
		otpEsn += hex[result & 0x0F];
	}

	return "0200" + otpEsn + "00000000";
}
