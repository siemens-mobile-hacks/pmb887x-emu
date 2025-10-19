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

static bool isInsideCwd(const std::filesystem::path &path) {
	auto cwd = std::filesystem::current_path();

    auto normalizedPath = std::filesystem::canonical(path);
    auto normalizedCwd = std::filesystem::canonical(cwd);

    auto pathStr = normalizedPath.string();
    auto cwdStr = normalizedCwd.string();

    if (pathStr.length() < cwdStr.length())
        return false;

    return pathStr.compare(0, cwdStr.length(), cwdStr) == 0 &&
           (pathStr.length() == cwdStr.length() ||
            pathStr[cwdStr.length()] == std::filesystem::path::preferred_separator);
}

 std::filesystem::path normalizePath(const std::filesystem::path &path) {
	auto normalized = std::filesystem::canonical(path);
	auto cwd = std::filesystem::current_path();
	if (isInsideCwd(normalized))
		return std::filesystem::relative(normalized, cwd);
	return normalized;
}

static bool isSafeForUnixAndWin(const std::string &str) {
	if (str.empty())
		return false;

	return std::all_of(str.begin(), str.end(), [](char c) {
		return (
			std::isalnum(c) ||
			c == ',' || c == '.' || c == '/' ||
			c == '=' || c == '_' || c == '-'
		);
	});
}

std::string escapeShellArg(const std::string &arg) {
	if (isSafeForUnixAndWin(arg))
		return arg;

	if (isWindows()) {
		std::string result;
		result.reserve(arg.length() + 10);
		result += '"';

		size_t backslashes = 0;
		for (char c: arg) {
			if (c == '\\') {
				backslashes++;
			} else if (c == '"') {
				result.append(backslashes * 2 + 1, '\\');
				result += '"';
				backslashes = 0;
			} else {
				result.append(backslashes, '\\');
				result += c;
				backslashes = 0;
			}
		}

		result.append(backslashes * 2, '\\');
		result += '"';

		return result;
	} else {
		std::string result;
		result.reserve(arg.length() + 10);
		result += '\'';

		for (char c :arg) {
			if (c == '\'') {
				result += "'\\''";
			} else {
				result += c;
			}
		}

		result += '\'';
		return result;
	}
}

std::string joinCommandArguments(const std::vector<std::string> &args) {
    std::vector<std::string> escaped;
    escaped.reserve(args.size());
    std::transform(args.begin(), args.end(), std::back_inserter(escaped), [](const std::string &arg) {
		return escapeShellArg(arg);
	});

    return std::accumulate(
        std::next(escaped.begin()), escaped.end(), escaped[0],
        [](const std::string &a, const std::string &b) {
            return a + ' ' + b;
        }
    );
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
