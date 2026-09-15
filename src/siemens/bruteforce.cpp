#include "bruteforce.h"

#include "crypto/md5.h"
#include "siemens/crypto.h"
#include "siemens/eeprom.h"
#include "siemens/fullflash.h"
#include "utils/binary.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace siemens {

static const uint8_t BLOCK5077_TAIL[6] = { 0x3D, 0x06, 0x06, 0x0B, 0x80, 0xF3 };
static const std::array<uint8_t, 16> RECALCULATED_HASH = {
	0x54, 0xF8, 0x0A, 0xC1, 0x2A, 0xCD, 0x94, 0xB2, 0xF5, 0xCF, 0xFB, 0x9B, 0xF7, 0xE4, 0xD4, 0x93,
};
static const uint32_t RECALCULATED_ESN = 0x12345678;
static const uint64_t ESN_COUNT = 0x100000000ULL;
static const uint32_t PROGRESS_INTERVAL = 0x40000;

static CipherKeyBatch buildCipherKeyBatch(const std::array<uint8_t, 64> &key, size_t esnOffset, uint32_t firstCandidate) {
	CipherKeyBatch keys;
	for (size_t lane = 0; lane < CIPHER_BATCH_SIZE; lane++) {
		keys[lane] = key;
		writeUInt32LE(&keys[lane][esnOffset], firstCandidate + lane);
	}
	return keys;
}

template<typename Match>
static std::tuple<bool, uint32_t> bruteForceEsn(
	size_t threadCount,
	size_t batchSize,
	EsnRecoveryStage stage,
	const EsnProgressCallback &progress,
	Match matches
) {
	if (threadCount == 0)
		threadCount = std::max<size_t>(1, std::thread::hardware_concurrency());
	if (progress)
		progress(stage, 0);

	std::atomic<bool> found(false);
	std::atomic<uint32_t> result(0);
	std::exception_ptr progressError;
	uint32_t reportedPercent = 0;
	auto runWorker = [&](size_t thread) {
		uint32_t ticks = 0;
		uint32_t checkInterval = PROGRESS_INTERVAL / batchSize;
		uint64_t step = threadCount * batchSize;
		uint64_t firstCandidate = thread * batchSize;
		for (; firstCandidate < ESN_COUNT; firstCandidate += step) {
			if (++ticks >= checkInterval) {
				ticks = 0;
				if (found.load(std::memory_order_relaxed))
					return;
				if (progress && thread == 0) {
					uint32_t percent = (uint32_t) ((firstCandidate * 100) / ESN_COUNT);
					if (percent > reportedPercent) {
						reportedPercent = percent;
						progress(stage, percent);
					}
				}
			}

			auto match = matches((uint32_t) firstCandidate);
			if (match) {
				result.store(*match, std::memory_order_relaxed);
				found.store(true, std::memory_order_release);
				return;
			}
		}
	};

	std::vector<std::thread> workers;
	workers.reserve(threadCount - 1);
	for (size_t thread = 1; thread < threadCount; thread++)
		workers.emplace_back(runWorker, thread);

	try {
		runWorker(0);
	} catch (...) {
		progressError = std::current_exception();
		found.store(true, std::memory_order_release);
	}
	for (auto &worker : workers)
		worker.join();

	if (progressError)
		std::rethrow_exception(progressError);
	if (progress && reportedPercent < 100)
		progress(stage, 100);

	if (!found.load(std::memory_order_acquire))
		return { false, 0 };
	return { true, result.load(std::memory_order_relaxed) };
}

static bool hasValidChecksum(const uint8_t *data, size_t size) {
	uint8_t sum = 0;
	uint8_t xorValue = 0;
	for (size_t index = 0; index < size; index++) {
		sum += data[index];
		xorValue ^= data[index];
	}
	return data[size] == sum && data[size + 1] == xorValue;
}

static std::optional<uint32_t> findMd5Match(
	const Md5DigestBatch &hashes,
	const uint32_t target[4],
	uint32_t firstCandidate
) {
	for (size_t lane = 0; lane < MD5_BATCH_SIZE; lane++) {
		bool matches = hashes[0][lane] == target[0] && hashes[1][lane] == target[1] &&
			hashes[2][lane] == target[2] && hashes[3][lane] == target[3];
		if (matches)
			return firstCandidate + lane;
	}
	return std::nullopt;
}

static std::tuple<bool, uint32_t> recoverEsnFromHash(
	const uint8_t *wanted,
	size_t threadCount,
	EsnRecoveryStage stage,
	const EsnProgressCallback &progress
) {
	uint32_t target[4];
	for (size_t index = 0; index < 4; index++)
		target[index] = readUInt32LE(wanted + index * 4);

	return bruteForceEsn(threadCount, MD5_BATCH_SIZE, stage, progress, [&](uint32_t firstCandidate) -> std::optional<uint32_t> {
		Md5BlockBatch blocks{};
		for (size_t lane = 0; lane < MD5_BATCH_SIZE; lane++)
			blocks[0][lane] = firstCandidate + lane;
		blocks[1].fill(0x80);
		blocks[14].fill(32);
		Md5DigestBatch hashes;
		md5Batch(hashes, blocks);
		return findMd5Match(hashes, target, firstCandidate);
	});
}

static std::tuple<bool, uint32_t> recoverEsnFromBkeyOrHash(
	const FullflashInfo &info,
	size_t threadCount,
	const EsnProgressCallback &progress
) {
	if (info.skey.size() != 4)
		return { false, 0 };

	bool useBkey = info.bkey.size() == 16;
	const std::vector<uint8_t> &wanted = useBkey ? info.bkey : info.hash;
	if (wanted.size() != 16)
		return { false, 0 };
	uint32_t skey = readUInt32LE(info.skey.data());
	EsnRecoveryStage stage = useBkey ? EsnRecoveryStage::BKEY : EsnRecoveryStage::HASH;

	uint32_t target[4];
	for (size_t index = 0; index < 4; index++)
		target[index] = readUInt32LE(wanted.data() + index * 4);

	return bruteForceEsn(threadCount, MD5_BATCH_SIZE, stage, progress, [&](uint32_t firstCandidate) -> std::optional<uint32_t> {
		Md5BlockBatch bkeyBlocks{};
		bkeyBlocks[1].fill(skey);
		bkeyBlocks[4].fill(0x80);
		bkeyBlocks[14].fill(128);
		for (size_t lane = 0; lane < MD5_BATCH_SIZE; lane++) {
			uint32_t candidate = firstCandidate + lane;
			bkeyBlocks[0][lane] = candidate;
			bkeyBlocks[2][lane] = (candidate ^ (candidate >> 24) ^ (skey << 8));
			bkeyBlocks[3][lane] = (skey ^ (skey >> 24) ^ (bkeyBlocks[2][lane] << 8));
		}

		Md5DigestBatch calculated;
		md5Batch(calculated, bkeyBlocks);
		if (!useBkey) {
			Md5BlockBatch hashBlocks{};
			for (size_t index = 0; index < 4; index++)
				hashBlocks[index] = calculated[index];
			hashBlocks[4].fill(0x80);
			hashBlocks[14].fill(128);
			md5Batch(calculated, hashBlocks);
		}
		return findMd5Match(calculated, target, firstCandidate);
	});
}

static std::tuple<bool, uint32_t> recoverEsnFromSecurityMarker(
	const std::vector<uint8_t> &marker,
	const std::string &imei,
	uint32_t skey,
	size_t threadCount,
	EsnRecoveryStage stage,
	const EsnProgressCallback &progress
) {
	if (isErasedData(marker.data(), marker.size()))
		return { false, 0 };

	auto imei8 = packImei(imei);
	auto keyTemplate = buildCipherKey1(skey, 0, imei8);
	return bruteForceEsn(threadCount, CIPHER_BATCH_SIZE, stage, progress, [&](uint32_t firstCandidate) -> std::optional<uint32_t> {
		std::array<std::array<uint8_t, 8>, CIPHER_BATCH_SIZE> data;
		auto keys = buildCipherKeyBatch(keyTemplate, 4, firstCandidate);
		for (size_t lane = 0; lane < CIPHER_BATCH_SIZE; lane++)
			std::copy_n(marker.begin(), data[lane].size(), data[lane].begin());
		cipherDecryptBatch(data[0].data(), data[0].size(), keys);

		for (size_t lane = 0; lane < CIPHER_BATCH_SIZE; lane++) {
			bool firstWordMatches = readUInt32LE(&data[lane][0]) == 0x77C5742D;
			bool secondWordMatches = readUInt32LE(&data[lane][4]) == 0xF49A4ADA;
			bool markerMatches = firstWordMatches && secondWordMatches;
			if (markerMatches)
				return firstCandidate + lane;
		}
		return std::nullopt;
	});
}

static std::tuple<bool, uint32_t> recoverEsnFromBlock5008(
	const std::vector<uint8_t> &encrypted,
	const std::vector<uint8_t> &block5077,
	const std::string &imei,
	size_t threadCount,
	const EsnProgressCallback &progress
) {
	if (encrypted.size() != 0xE0)
		return { false, 0 };
	if (isErasedData(encrypted.data(), encrypted.size()))
		return { false, 0 };

	bool hasBlock5077 = block5077.size() == 0xE8 && !isErasedData(block5077.data(), block5077.size());
	auto keyTemplate = buildEepromKey(0, imei);
	auto matches = [&](uint32_t firstCandidate) -> std::optional<uint32_t> {
		std::array<std::array<uint8_t, 0x20>, CIPHER_BATCH_SIZE> data;
		auto keys = buildCipherKeyBatch(keyTemplate, 0x30, firstCandidate);
		for (size_t lane = 0; lane < CIPHER_BATCH_SIZE; lane++)
			std::copy_n(encrypted.begin(), data[lane].size(), data[lane].begin());
		cipherDecryptBatch(data[0].data(), data[0].size(), keys);

		for (size_t lane = 0; lane < CIPHER_BATCH_SIZE; lane++) {
			if (!hasValidChecksum(&data[lane][8], 0x16))
				continue;

			std::array<uint8_t, 0xE0> fullData;
			std::copy(encrypted.begin(), encrypted.end(), fullData.begin());
			cipherDecrypt(&fullData[0x20], 0xC0, keys[lane].data());
			if (!hasValidChecksum(&fullData[0x28], 0xB0))
				continue;
			if (!hasBlock5077)
				return firstCandidate + lane;

			std::array<uint8_t, 0xE8> verificationData;
			std::copy(block5077.begin(), block5077.end(), verificationData.begin());
			cipherDecrypt(verificationData.data(), verificationData.size(), keys[lane].data());
			if (hasValidChecksum(&verificationData[8], 0xD8))
				return firstCandidate + lane;
		}
		return std::nullopt;
	};
	return bruteForceEsn(threadCount, CIPHER_BATCH_SIZE, EsnRecoveryStage::BLOCK_5008, progress, matches);
}

static std::tuple<bool, uint32_t> recoverEsnFromBlock5077(
	const std::vector<uint8_t> &encrypted,
	const std::string &imei,
	size_t threadCount,
	const EsnProgressCallback &progress
) {
	if (encrypted.size() != 0xE8)
		return { false, 0 };
	if (isErasedData(encrypted.data(), encrypted.size()))
		return { false, 0 };

	auto keyTemplate = buildEepromKey(0, imei);
	auto matches = [&](uint32_t firstCandidate) -> std::optional<uint32_t> {
		std::array<std::array<uint8_t, 0xE8>, CIPHER_BATCH_SIZE> data;
		auto keys = buildCipherKeyBatch(keyTemplate, 0x30, firstCandidate);
		for (size_t lane = 0; lane < CIPHER_BATCH_SIZE; lane++)
			std::copy(encrypted.begin(), encrypted.end(), data[lane].begin());
		cipherDecryptBatch(data[0].data(), data[0].size(), keys);

		for (size_t lane = 0; lane < CIPHER_BATCH_SIZE; lane++) {
			if (!hasValidChecksum(&data[lane][8], 0xD8))
				continue;
			if (std::equal(data[lane].begin() + 0xE2, data[lane].end(), BLOCK5077_TAIL))
				return firstCandidate + lane;
		}
		return std::nullopt;
	};
	return bruteForceEsn(threadCount, CIPHER_BATCH_SIZE, EsnRecoveryStage::BLOCK_5077, progress, matches);
}

std::tuple<bool, uint32_t> recoverEsn(
	const Eeprom &eeprom,
	const FullflashInfo &info,
	size_t threadCount,
	const EsnProgressCallback &progress
) {
	bool hasKnownHash = info.hash.size() == RECALCULATED_HASH.size() &&
		std::equal(info.hash.begin(), info.hash.end(), RECALCULATED_HASH.begin());
	if (hasKnownHash) {
		if (progress) {
			progress(EsnRecoveryStage::KNOWN_HASH, 0);
			progress(EsnRecoveryStage::KNOWN_HASH, 100);
		}
		return { true, RECALCULATED_ESN };
	}

	if (eeprom.hasBlock(5468)) {
		auto block = eeprom.readBlock(5468);
		if (block.size() == 0x31 && block[16] == 0x58) {
			auto result = recoverEsnFromHash(&block[17], threadCount, EsnRecoveryStage::BLOCK_5468, progress);
			if (std::get<0>(result))
				return result;
		}
	}

	auto result = recoverEsnFromBkeyOrHash(info, threadCount, progress);
	if (std::get<0>(result))
		return result;

	if (info.imei.size() != 15)
		return { false, 0 };

	bool hasSlowBlocks = eeprom.hasBlock(5008) || eeprom.hasBlock(5077) || eeprom.hasBlock(5121) || eeprom.hasBlock(5123);
	if (hasSlowBlocks)
		spdlog::info("[otp] Slow ESN search started; use --siemens-recalc to replace the fullflash keys instead");

	std::vector<uint8_t> block5077;
	if (eeprom.hasBlock(5077))
		block5077 = eeprom.readBlock(5077);
	if (eeprom.hasBlock(5008)) {
		auto block = eeprom.readBlock(5008);
		result = recoverEsnFromBlock5008(block, block5077, info.imei, threadCount, progress);
		if (std::get<0>(result))
			return result;
	}

	std::vector<uint8_t> block5121;
	if (eeprom.hasBlock(5121))
		block5121 = eeprom.readBlock(5121);
	if (info.skey.size() == 4) {
		uint32_t skey = readUInt32LE(info.skey.data());
		if (block5121.size() >= 8) {
			std::vector<uint8_t> marker(block5121.begin(), block5121.begin() + 8);
			result = recoverEsnFromSecurityMarker(
				marker, info.imei, skey, threadCount,
				EsnRecoveryStage::BLOCK_5121, progress
			);
			if (std::get<0>(result))
				return result;
		}

		if (eeprom.hasBlock(5123)) {
			auto block = eeprom.readBlock(5123);
			if (block.size() >= 12) {
				std::vector<uint8_t> marker(block.begin() + 4, block.begin() + 12);
				result = recoverEsnFromSecurityMarker(
					marker, info.imei, skey, threadCount,
					EsnRecoveryStage::BLOCK_5123, progress
				);
				if (std::get<0>(result))
					return result;
			}
		}
	}

	if (!block5077.empty()) {
		result = recoverEsnFromBlock5077(block5077, info.imei, threadCount, progress);
		if (std::get<0>(result))
			return result;
	}

	return { false, 0 };
}

}
