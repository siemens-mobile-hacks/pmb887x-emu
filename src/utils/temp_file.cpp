#include "temp_file.h"

#include "file.h"

#include <cerrno>
#include <filesystem>
#include <random>
#include <stdexcept>

#ifdef __linux__
#include <sys/mman.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>

static bool writeDescriptor(int fd, const std::vector<uint8_t> &data) {
	size_t done = 0;
	while (done < data.size()) {
		ssize_t written = write(fd, data.data() + done, data.size() - done);
		if (written < 0 && errno == EINTR)
			continue;
		if (written <= 0)
			return false;
		done += (size_t) written;
	}
	return true;
}
#endif

TempFileCopy::~TempFileCopy() {
#if defined(__linux__) || defined(__APPLE__)
	if (m_fd >= 0)
		close(m_fd);
#endif

	if (m_isTempFile) {
		std::error_code error;
		std::filesystem::remove(m_filePath, error);
	}
}

const std::string &TempFileCopy::create(const std::vector<uint8_t> &data) {
	if (!m_filePath.empty())
		throw std::runtime_error("Temporary file already exists");

#ifdef __linux__
	// The descriptor is inherited by QEMU, which opens it again through /dev/fd.
	m_fd = memfd_create("pmb887x-emu", 0);
	if (m_fd >= 0) {
		if (writeDescriptor(m_fd, data)) {
			m_filePath = "/dev/fd/" + std::to_string(m_fd);
			return m_filePath;
		}
		close(m_fd);
		m_fd = -1;
	}
#elif defined(__APPLE__)
	std::error_code tempError;
	auto tempDirectory = std::filesystem::temp_directory_path(tempError);
	if (!tempError) {
		auto pattern = (tempDirectory / "pmb887x-emu-XXXXXX").string();
		m_fd = mkstemp(pattern.data());
		if (m_fd >= 0) {
			if (unlink(pattern.c_str()) == 0 && writeDescriptor(m_fd, data)) {
				m_filePath = "/dev/fd/" + std::to_string(m_fd);
				return m_filePath;
			}
			close(m_fd);
			m_fd = -1;
			std::filesystem::remove(pattern, tempError);
		}
	}
#endif

	std::random_device random;
	std::error_code error;
	auto directory = std::filesystem::temp_directory_path(error);
	if (error)
		throw std::runtime_error("Can't create temporary file");

	m_filePath = (directory / ("pmb887x-emu-" + std::to_string(random()) + ".bin")).string();
	if (!writeFile(m_filePath, data)) {
		std::filesystem::remove(m_filePath, error);
		m_filePath.clear();
		throw std::runtime_error("Can't create temporary file");
	}
	m_isTempFile = true;
	return m_filePath;
}
