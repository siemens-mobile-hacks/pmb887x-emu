#include "file.h"

#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static std::filesystem::path createReplacementFile(const std::filesystem::path &path) {
#ifdef _WIN32
	wchar_t replacement[MAX_PATH];
	if (GetTempFileNameW(path.parent_path().c_str(), L"pmb", 0, replacement) == 0)
		return {};
	return replacement;
#else
	auto pattern = path.string() + ".tmp.XXXXXX";
	std::vector<char> replacement(pattern.begin(), pattern.end());
	replacement.push_back(0);
	int fd = mkstemp(replacement.data());
	if (fd < 0)
		return {};
	close(fd);
	return std::filesystem::path(replacement.data());
#endif
}

bool readFile(const std::string &path, std::vector<uint8_t> &data) {
	std::ifstream input(path, (std::ios::binary | std::ios::ate));
	if (!input)
		return false;

	std::streamsize size = input.tellg();
	if (size < 0)
		return false;

	data.resize((size_t) size);
	input.seekg(0);
	return (bool) input.read((char *) data.data(), size);
}

bool writeFile(const std::string &path, const std::vector<uint8_t> &data) {
	std::ofstream output(path, (std::ios::binary | std::ios::trunc));
	if (!output)
		return false;
	output.write((const char *) data.data(), (std::streamsize) data.size());
	output.close();
	return !output.fail();
}

bool replaceFile(const std::string &path, const std::vector<uint8_t> &data) {
	std::error_code error;
	auto target = std::filesystem::canonical(path, error);
	if (error)
		return false;

	auto replacement = createReplacementFile(target);
	if (replacement.empty())
		return false;
	if (!writeFile(replacement.string(), data)) {
		std::filesystem::remove(replacement, error);
		return false;
	}

	auto permissions = std::filesystem::status(target, error).permissions();
	if (error) {
		std::filesystem::remove(replacement, error);
		return false;
	}
	std::filesystem::permissions(replacement, permissions, error);
	if (error) {
		std::filesystem::remove(replacement, error);
		return false;
	}

#ifdef _WIN32
	bool replaced = ReplaceFileW(target.c_str(), replacement.c_str(), nullptr, 0, nullptr, nullptr);
	if (!replaced)
		std::filesystem::remove(replacement, error);
	return replaced;
#else
	std::filesystem::rename(replacement, target, error);
	if (!error)
		return true;

	std::filesystem::remove(replacement, error);
	return false;
#endif
}
