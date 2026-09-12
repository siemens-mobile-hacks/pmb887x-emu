#include "utils.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
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

#ifdef __linux__
#include <sys/mman.h>
#endif

#if defined(__APPLE__) || defined(__MACH__)
extern char **environ;
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

std::string esnToHex(uint32_t esn) {
	const char hex[] = "0123456789ABCDEF";
	std::string out;
	for (int shift = 28; shift >= 0; shift -= 4)
		out += hex[(esn >> shift) & 0xF];
	return out;
}

std::string esnCachePath(const std::string &fullflash) {
	return fullflash + ".esn";
}

bool readEsnCache(const std::string &fullflash, const std::string &imei, const std::string &key, uint32_t &esn) {
	std::ifstream in(esnCachePath(fullflash));
	if (!in)
		return false;

	std::string line, cachedImei, cachedKey, value;
	while (std::getline(in, line)) {
		if (line.starts_with("IMEI="))
			cachedImei = line.substr(5);
		else if (line.starts_with("KEY="))
			cachedKey = line.substr(4);
		else if (line.starts_with("ESN="))
			value = line.substr(4);
	}

	const bool valid = value.size() == 8 && std::all_of(value.begin(), value.end(), [](uint8_t c) { return std::isxdigit(c); });
	if (!valid || cachedImei != imei || cachedKey != key)
		return false;

	esn = static_cast<uint32_t>(std::stoul(value, nullptr, 16));
	return true;
}

// Caching is best effort: a read-only directory just means the search runs again next time.
void writeEsnCache(const std::string &fullflash, const std::string &imei, const std::string &key, uint32_t esn) {
	std::ofstream out(esnCachePath(fullflash), std::ios::trunc);
	if (!out)
		return;
	out << "# ESN recovered by pmb887x-emu, delete this file to search again\n"
		<< "IMEI=" << imei << "\n"
		<< "KEY=" << key << "\n"
		<< "ESN=" << esnToHex(esn) << "\n";
}

bool readFile(const std::string &path, std::vector<uint8_t> &data) {
	std::ifstream in(path, std::ios::binary | std::ios::ate);
	if (!in)
		return false;
	std::streamsize size = in.tellg();
	if (size < 0)
		return false;
	data.resize(static_cast<size_t>(size));
	in.seekg(0);
	return static_cast<bool>(in.read(reinterpret_cast<char *>(data.data()), size));
}

bool writeFile(const std::string &path, const std::vector<uint8_t> &data) {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out)
		return false;
	return static_cast<bool>(out.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size())));
}

TempFileCopy::~TempFileCopy() {
#ifdef __linux__
	if (fd >= 0)
		close(fd);
#endif
	if (isTempFile) {
		std::error_code ec;
		std::filesystem::remove(filePath, ec);
	}
}

bool TempFileCopy::create(const std::vector<uint8_t> &data) {
#ifdef __linux__
	// The descriptor is inherited by QEMU, which opens it again through /dev/fd.
	fd = memfd_create("pmb887x-emu", 0);
	if (fd >= 0) {
		size_t done = 0;
		while (done < data.size()) {
			ssize_t written = write(fd, data.data() + done, data.size() - done);
			if (written <= 0) {
				close(fd);
				fd = -1;
				break;
			}
			done += static_cast<size_t>(written);
		}
		if (fd >= 0) {
			filePath = "/dev/fd/" + std::to_string(fd);
			return true;
		}
	}
#endif

	std::random_device rd;
	std::error_code ec;
	std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
	if (ec)
		return false;
	filePath = (dir / ("pmb887x-emu-" + std::to_string(rd()) + ".bin")).string();
	if (!writeFile(filePath, data))
		return false;
	isTempFile = true;
	return true;
}

// Writes back only the 4 KB chunks that differ from the original content.
bool patchFile(const std::string &path, const std::vector<uint8_t> &original, const std::vector<uint8_t> &data) {
	constexpr size_t chunk = 4096;
	std::fstream out(path, std::ios::binary | std::ios::in | std::ios::out);
	if (!out || original.size() != data.size())
		return false;
	for (size_t offset = 0; offset < data.size(); offset += chunk) {
		size_t size = std::min(chunk, data.size() - offset);
		if (memcmp(&original[offset], &data[offset], size) == 0)
			continue;
		out.seekp(static_cast<std::streamoff>(offset));
		if (!out.write(reinterpret_cast<const char *>(&data[offset]), static_cast<std::streamsize>(size)))
			return false;
	}
	out.flush();
	return static_cast<bool>(out);
}
