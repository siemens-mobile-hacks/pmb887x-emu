#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace SiemensFW {

class EepromError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

class Eeprom {
public:
	explicit Eeprom(std::vector<uint8_t> &fullflash);

	bool hasBlock(uint32_t id) const;
	size_t getBlockSize(uint32_t id) const;
	std::vector<uint8_t> readBlock(uint32_t id) const;
	bool writeBlock(uint32_t id, const std::vector<uint8_t> &value);

private:
	enum class Layout {
		UNKNOWN,
		X65,
		X85,
	};

	enum class BlockLayout {
		LINEAR,
		STRIDED,
	};

	enum class PartitionType {
		UNKNOWN,
		EELITE,
		EEFULL,
	};

	enum class EntryState : uint32_t {
		FREE = 0xAAAAAAAA,
		WRITING = 0xAAAAAAA8,
		VALID = 0xAAAAAA80,
		OBSOLETE = 0xAAAAAA00,
	};

	struct Block {
		size_t m_offset;
		size_t m_size;
		BlockLayout m_layout;
	};

	void parsePartitions();
	void parsePartitionX65(PartitionType type, size_t partitionOffset);
	void parsePartitionX85(PartitionType type, size_t blockOffset);
	const Block &getBlockMetadata(uint32_t id) const;
	PartitionType readPartitionType(Layout layout, size_t blockOffset) const;
	size_t getEefullMetadataSize(const Block &block, size_t entryOffset) const;
	static size_t alignDataOffset(size_t offset, size_t chunkSize);
	static size_t getBlockByteOffset(const Block &block, size_t index);
	static bool isEntryStateValid(EntryState state);

	std::vector<uint8_t> &m_data;
	Layout m_layout = Layout::UNKNOWN;
	std::unordered_map<uint32_t, Block> m_blocks;
};

}
