#include "eeprom.h"
#include "utils/binary.h"

#include <cstring>
#include <format>

namespace siemens {

static const size_t PARTITION_STEP = 0x10000;

static const size_t X65_PARTITION_SIZE = 0x20000;
static const size_t X85_PARTITION_SIZE = 0x40000;

static const size_t X65_HEADER_SIZE = 16;
static const size_t X85_HEADER_SIZE = 32;

static const size_t X65_ENTRY_SIZE = 16;
static const size_t X85_ENTRY_SIZE = 32;

static const size_t X85_DATA_CHUNK_SIZE = 0x10;

static const size_t X85_EELITE_MAX_LINEAR_SIZE = 0x10;
static const size_t X85_EEFULL_MAX_LINEAR_SIZE = 0x200;

static const uint32_t EEFULL_ID_OFFSET = 5000;
static const uint32_t ENTRY_STATE_MASK = 0xAAAAAAAA;

size_t Eeprom::alignDataOffset(size_t offset, size_t chunkSize) {
	if ((offset & chunkSize) == 0)
		return offset;

	size_t stride = chunkSize * 2;
	return offset + stride - (offset & (stride - 1));
}

bool Eeprom::isEntryStateValid(EntryState state) {
	switch (state) {
		case EntryState::FREE:
		case EntryState::WRITING:
		case EntryState::VALID:
		case EntryState::OBSOLETE:
			return true;
	}
	return false;
}

Eeprom::PartitionType Eeprom::readPartitionType(Layout layout, size_t blockOffset) const {
	size_t headerSize = layout == Layout::X65 ? X65_HEADER_SIZE : X85_HEADER_SIZE;
	size_t offset = layout == Layout::X65 ? blockOffset : blockOffset + PARTITION_STEP - headerSize;

	if (offset + headerSize > m_data.size())
		return PartitionType::UNKNOWN;

	PartitionType type;
	if (memcmp(&m_data[offset], "EELITE\0", 8) == 0) {
		type = PartitionType::EELITE;
	} else if (memcmp(&m_data[offset], "EEFULL\0", 8) == 0) {
		type = PartitionType::EEFULL;
	} else {
		return PartitionType::UNKNOWN;
	}

	if (m_data[offset + 10] != 0 || m_data[offset + 11] != 0 || m_data[offset + 12] != 0xF0)
		throw EepromError(std::format("Invalid partition header at {:08X}", offset));

	for (size_t index = 13; index < headerSize; index++) {
		if (m_data[offset + index] != 0xFF)
			throw EepromError(std::format("Invalid partition header at {:08X}", offset));
	}

	return type;
}

size_t Eeprom::getEefullMetadataSize(const Block &block, size_t entryOffset) const {
	uint8_t version = m_data[getBlockByteOffset(block, 0)];
	size_t metadataSize = (version & 0x80) != 0 && version != 0xFF ? 6 : 1;
	if (block.m_size < metadataSize)
		throw EepromError(std::format("EEFULL metadata exceeds block at {:08X}", entryOffset));
	return metadataSize;
}

Eeprom::Eeprom(std::vector<uint8_t> &fullflash) : m_data(fullflash) {
	parsePartitions();
}

void Eeprom::parsePartitions() {
	for (size_t offset = 0; offset < m_data.size(); offset += PARTITION_STEP) {
		if (m_layout == Layout::UNKNOWN || m_layout == Layout::X65) {
			PartitionType type = readPartitionType(Layout::X65, offset);
			if (type != PartitionType::UNKNOWN) {
				parsePartitionX65(type, offset);
				m_layout = Layout::X65;
			}
		}

		if (m_layout == Layout::UNKNOWN || m_layout == Layout::X85) {
			PartitionType type = readPartitionType(Layout::X85, offset);
			if (type != PartitionType::UNKNOWN) {
				parsePartitionX85(type, offset);
				m_layout = Layout::X85;
			}
		}
	}
}

void Eeprom::parsePartitionX65(PartitionType type, size_t partitionOffset) {
	if (partitionOffset + X65_PARTITION_SIZE > m_data.size())
		throw EepromError(std::format("Partition at {:08X} exceeds fullflash", partitionOffset));

	size_t entryCount = (X65_PARTITION_SIZE - X65_HEADER_SIZE) / X65_ENTRY_SIZE;
	for (size_t index = 1; index <= entryCount; index++) {
		size_t entryOffset = partitionOffset + X65_PARTITION_SIZE - X65_ENTRY_SIZE * index;
		auto state = (EntryState) (readUInt32LE(&m_data[entryOffset]) & ENTRY_STATE_MASK);
		if (!isEntryStateValid(state))
			throw EepromError(std::format("Unknown entry state at {:08X}", entryOffset));
		if (state == EntryState::FREE)
			return;
		if (state != EntryState::VALID)
			continue;

		uint32_t id = readUInt32LE(&m_data[entryOffset + 4]);
		uint32_t maxId = type == PartitionType::EELITE ?
			EEFULL_ID_OFFSET - 1 :
			0xFFFFFFFF - EEFULL_ID_OFFSET;
		if (id > maxId)
			throw EepromError(std::format("Block ID out of range at {:08X}", entryOffset));

		uint32_t storedSize = readUInt32LE(&m_data[entryOffset + 8]);
		if (storedSize == 0)
			throw EepromError(std::format("Zero block size at {:08X}", entryOffset));
		if (type == PartitionType::EEFULL)
			id += EEFULL_ID_OFFSET;

		uint32_t relativeOffset = readUInt32LE(&m_data[entryOffset + 12]);
		if (relativeOffset >= X65_PARTITION_SIZE)
			throw EepromError(std::format("Data offset out of range at {:08X}", entryOffset));
		if (storedSize > X65_PARTITION_SIZE - relativeOffset)
			throw EepromError(std::format("Block data out of range at {:08X}", entryOffset));

		Block block = { partitionOffset + relativeOffset, storedSize, BlockLayout::LINEAR };
		if (type == PartitionType::EEFULL) {
			size_t metadataSize = getEefullMetadataSize(block, entryOffset);
			block.m_offset++;
			block.m_size -= metadataSize;
		} else {
			block.m_size--;
		}
		m_blocks.try_emplace(id, block);
	}

	throw EepromError(std::format("No free EIT entry at {:08X}", partitionOffset));
}

void Eeprom::parsePartitionX85(PartitionType type, size_t blockOffset) {
	if (blockOffset < X85_PARTITION_SIZE - PARTITION_STEP)
		throw EepromError(std::format("Partition start underflow at {:08X}", blockOffset));

	size_t partitionOffset = blockOffset - (X85_PARTITION_SIZE - PARTITION_STEP);
	size_t entryCount = (X85_PARTITION_SIZE - X85_HEADER_SIZE) / X85_ENTRY_SIZE;
	for (size_t index = 1; index <= entryCount; index++) {
		size_t entryOffset = partitionOffset + X85_PARTITION_SIZE - X85_HEADER_SIZE - X85_ENTRY_SIZE * index;
		auto state = (EntryState) (readUInt32LE(&m_data[entryOffset]) & ENTRY_STATE_MASK);
		if (!isEntryStateValid(state))
			continue;
		if (state != EntryState::VALID)
			continue;

		uint32_t storedSize;
		if (type == PartitionType::EELITE) {
			storedSize = readUInt16LE(&m_data[entryOffset + 8]);
		} else {
			storedSize = readUInt32LE(&m_data[entryOffset + 8]);
		}
		if (storedSize == 0)
			throw EepromError(std::format("Zero block size at {:08X}", entryOffset));

		size_t inlineMaxSize = type == PartitionType::EEFULL ? X85_EEFULL_MAX_LINEAR_SIZE : X85_EELITE_MAX_LINEAR_SIZE;
		Block block;

		if (storedSize <= inlineMaxSize) {
			size_t reservedSize = X85_DATA_CHUNK_SIZE + storedSize + (storedSize & ~(X85_DATA_CHUNK_SIZE - 1));
			if (entryOffset < partitionOffset + reservedSize)
				throw EepromError(std::format("Inline data out of range at {:08X}", entryOffset));

			size_t storedOffset = entryOffset - reservedSize;
			block = { alignDataOffset(storedOffset, X85_DATA_CHUNK_SIZE), storedSize, BlockLayout::STRIDED };
		} else {
			uint32_t relativeOffset = readUInt32LE(&m_data[entryOffset + 12]);
			if (relativeOffset >= X85_PARTITION_SIZE)
				throw EepromError(std::format("Data offset out of range at {:08X}", entryOffset));
			if (storedSize > X85_PARTITION_SIZE - relativeOffset)
				throw EepromError(std::format("Block data out of range at {:08X}", entryOffset));

			block = { partitionOffset + relativeOffset, storedSize, BlockLayout::LINEAR };
		}

		uint32_t id = type == PartitionType::EELITE ?
			readUInt16LE(&m_data[entryOffset + 4]) :
			readUInt32LE(&m_data[entryOffset + 4]);
		uint32_t maxId = type == PartitionType::EELITE ?
			EEFULL_ID_OFFSET - 1 :
			0xFFFFFFFF - EEFULL_ID_OFFSET;
		if (id > maxId)
			throw EepromError(std::format("Block ID out of range at {:08X}", entryOffset));
		if (type == PartitionType::EEFULL)
			id += EEFULL_ID_OFFSET;

		if (type == PartitionType::EEFULL) {
			size_t metadataSize = getEefullMetadataSize(block, entryOffset);
			block.m_offset = getBlockByteOffset(block, 1);
			block.m_size -= metadataSize;
		}
		m_blocks.insert_or_assign(id, block);
	}
}

const Eeprom::Block &Eeprom::getBlockMetadata(uint32_t id) const {
	auto iterator = m_blocks.find(id);
	if (iterator == m_blocks.end())
		throw EepromError(std::format("Block {} not found", id));
	return iterator->second;
}

size_t Eeprom::getBlockByteOffset(const Block &block, size_t index) {
	if (block.m_layout == BlockLayout::LINEAR)
		return block.m_offset + index;

	size_t chunkOffset = (block.m_offset & (X85_DATA_CHUNK_SIZE - 1));
	return block.m_offset + index + (((chunkOffset + index) / X85_DATA_CHUNK_SIZE) * X85_DATA_CHUNK_SIZE);
}

bool Eeprom::hasBlock(uint32_t id) const {
	return m_blocks.contains(id);
}

size_t Eeprom::getBlockSize(uint32_t id) const {
	return getBlockMetadata(id).m_size;
}

std::vector<uint8_t> Eeprom::readBlock(uint32_t id) const {
	const Block &block = getBlockMetadata(id);
	std::vector<uint8_t> value;
	value.reserve(block.m_size);
	for (size_t index = 0; index < block.m_size; index++)
		value.push_back(m_data[getBlockByteOffset(block, index)]);
	return value;
}

bool Eeprom::writeBlock(uint32_t id, const std::vector<uint8_t> &value) {
	const Block &block = getBlockMetadata(id);
	if (block.m_size != value.size())
		throw EepromError(std::format("Block {} size {}, expected {}", id, value.size(), block.m_size));

	bool changed = false;
	for (size_t index = 0; index < value.size(); index++) {
		uint8_t &destination = m_data[getBlockByteOffset(block, index)];
		if (destination != value[index]) {
			destination = value[index];
			changed = true;
		}
	}
	return changed;
}

}
