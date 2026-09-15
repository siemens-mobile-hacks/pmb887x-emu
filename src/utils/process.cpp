#include "process.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#endif

#if defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <linux/limits.h>
#include <unistd.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

#ifndef _WIN32
extern char **environ;
#endif

std::filesystem::path getExecutableDirectory() {
#ifdef _WIN32
	char buffer[MAX_PATH];
	GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
	char buffer[PATH_MAX];
	uint32_t size = sizeof(buffer);
	if (_NSGetExecutablePath(buffer, &size) == 0)
		return std::filesystem::path(buffer).parent_path();
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
	int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
	if (sysctl(mib, 4, buffer, &size, nullptr, 0) == 0)
		return std::filesystem::path(buffer).parent_path();
	return std::filesystem::current_path();
#else
#error "Unsupported platform"
#endif
}

void setEnvironmentVariable(const std::string &name, const std::string &value) {
#ifdef _WIN32
	_putenv((name + "=" + value).c_str());
#else
	setenv(name.c_str(), value.c_str(), true);
#endif
}

int executeProcess(const std::vector<std::string> &argv) {
	std::vector<char *> args;
	args.reserve(argv.size() + 1);
	for (const auto &arg : argv)
		args.push_back((char *) arg.c_str());
	args.push_back(nullptr);

#ifdef _WIN32
	intptr_t result = _spawnvp(_P_WAIT, args[0], args.data());
	if (result == -1)
		throw std::runtime_error("_spawnvp failed");
	return (int) result;
#else
	pid_t pid;
	int result = posix_spawnp(&pid, args[0], nullptr, nullptr, args.data(), environ);
	if (result != 0)
		throw std::runtime_error("posix_spawn failed");

	int status = 0;
	while (waitpid(pid, &status, 0) == -1) {
		if (errno != EINTR)
			throw std::runtime_error("waitpid failed");
	}

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
#endif
}
