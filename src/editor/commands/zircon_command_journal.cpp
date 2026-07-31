#include "zircon_command_journal.h"

// NOTE: zstd is reached directly here because kotek provides no
// compression wrapper module yet and kotek/ sources must not be
// modified from the zircon layer; the include path comes from the
// zstd::libzstd_static target linked privately in CMake (same pattern
// as <gtest/gtest.h> in engine unit tests)
#include <zstd.h>

namespace
{
	/// @brief \~english converts a static_path to a zero terminated
	/// narrow string view usable by std fstream open; the path is
	/// expected to be ASCII/UTF-8 (paths are built from
	/// kFolderIndex_* roots and ASCII file names)
	kotek::static_cstring_t<KOTEK_DEF_MAXIMUM_OS_PATH_LENGTH>
	zircon_journal_convert_path(
		const ktk_filesystem_path& path
	) noexcept
	{
		kotek::static_cstring_t<KOTEK_DEF_MAXIMUM_OS_PATH_LENGTH>
			result;

		auto u8_string = path.u8string();

		result.append(
			reinterpret_cast<const char*>(u8_string.data()),
			u8_string.size()
		);

		return result;
	}
} // namespace

zircon_command_journal::zircon_command_journal(void) :
	m_is_open{}, m_entries_per_block{
		zircon_DEF_COMMAND_HISTORY_JOURNAL_ENTRIES_PER_BLOCK
	},
	m_current_block_entry_count{}, m_total_entry_count{},
	m_total_raw_entry_bytes{}, m_cache_block_ids{},
	m_cache_next_slot{}
{
	for (kotek::uint32_t i = 0; i < _k_block_cache_size; ++i)
	{
		this->m_cache_block_ids[i] =
			zircon_DEF_COMMAND_HISTORY_INVALID_NODE_ID;
	}
}

zircon_command_journal::~zircon_command_journal(void)
{
	this->close();
}

bool zircon_command_journal::open(
	const ktk_filesystem_path& journal_file_path,
	const ktk_filesystem_path& snapshot_file_path
) noexcept
{
	KOTEK_ASSERT(
		this->m_is_open == false,
		"journal is already opened, close it first"
	);

	auto journal_path_string =
		zircon_journal_convert_path(journal_file_path);
	auto snapshot_path_string =
		zircon_journal_convert_path(snapshot_file_path);

	// detect an existing journal to continue appending to it
	bool journal_exists = false;
	{
		kotek::cfstream_t probe;
		probe.open(
			journal_path_string.c_str(),
			std::ios::in | std::ios::binary
		);
		if (probe.is_open())
		{
			probe.seekg(0, std::ios::end);
			journal_exists =
				probe.tellg() >=
				static_cast<std::streamoff>(_k_header_size);
			probe.close();
		}
	}

	if (journal_exists)
	{
		this->m_journal_stream.open(
			journal_path_string.c_str(),
			std::ios::in | std::ios::out | std::ios::binary
		);
	}
	else
	{
		// create the file first, std fstream with in|out fails on a
		// missing file
		this->m_journal_stream.open(
			journal_path_string.c_str(),
			std::ios::out | std::ios::trunc | std::ios::binary
		);

		if (this->m_journal_stream.is_open())
		{
			this->m_journal_stream.close();
		}

		this->m_journal_stream.clear();
		this->m_journal_stream.open(
			journal_path_string.c_str(),
			std::ios::in | std::ios::out | std::ios::binary
		);
	}

	if (this->m_journal_stream.is_open() == false)
	{
		KOTEK_MESSAGE_ERROR(
			"failed to open journal file: {}",
			journal_path_string.c_str()
		);
		return false;
	}

	this->m_snapshot_stream.open(
		snapshot_path_string.c_str(),
		std::ios::in | std::ios::out | std::ios::binary
	);

	if (this->m_snapshot_stream.is_open() == false)
	{
		this->m_snapshot_stream.clear();
		this->m_snapshot_stream.open(
			snapshot_path_string.c_str(),
			std::ios::out | std::ios::trunc | std::ios::binary
		);

		if (this->m_snapshot_stream.is_open())
		{
			this->m_snapshot_stream.close();
		}

		this->m_snapshot_stream.clear();
		this->m_snapshot_stream.open(
			snapshot_path_string.c_str(),
			std::ios::in | std::ios::out | std::ios::binary
		);
	}

	if (this->m_snapshot_stream.is_open() == false)
	{
		KOTEK_MESSAGE_ERROR(
			"failed to open snapshot file: {}",
			snapshot_path_string.c_str()
		);
		this->m_journal_stream.close();
		return false;
	}

	this->m_is_open = true;

	if (journal_exists)
	{
		if (this->scan_existing_file() == false)
		{
			this->close();
			return false;
		}
	}
	else
	{
		if (this->write_file_headers() == false)
		{
			this->close();
			return false;
		}
	}

	this->m_journal_stream.seekp(0, std::ios::end);
	this->m_snapshot_stream.seekp(0, std::ios::end);

	return true;
}

void zircon_command_journal::close(void) noexcept
{
	if (this->m_is_open == false)
		return;

	this->flush();

	if (this->m_journal_stream.is_open())
	{
		this->m_journal_stream.flush();
		this->m_journal_stream.close();
	}

	if (this->m_snapshot_stream.is_open())
	{
		this->m_snapshot_stream.flush();
		this->m_snapshot_stream.close();
	}

	this->m_is_open = false;
}

void zircon_command_journal::flush(void) noexcept
{
	if (this->m_is_open == false)
		return;

	if (this->m_current_block_entry_count > 0)
	{
		this->flush_block();
	}

	if (this->m_journal_stream.is_open())
	{
		this->m_journal_stream.flush();
	}

	if (this->m_snapshot_stream.is_open())
	{
		this->m_snapshot_stream.flush();
	}
}

bool zircon_command_journal::is_open(void) const noexcept
{
	return this->m_is_open;
}

bool zircon_command_journal::append_entry(
	const zircon_command_journal_entry_header& header,
	const unsigned char* p_payload,
	kotek::uint32_t payload_size,
	zircon_command_journal_locator& locator_out
) noexcept
{
	KOTEK_ASSERT(this->m_is_open, "journal is not opened!");
	KOTEK_ASSERT(
		header.m_payload_size == payload_size,
		"header payload size mismatch"
	);
	KOTEK_ASSERT(
		payload_size == 0 || p_payload,
		"non zero payload size requires a valid payload pointer"
	);
	KOTEK_ASSERT(
		sizeof(header) + payload_size <=
			zircon_DEF_MAXIMUM_COMMAND_SIZE,
		"entry is too big for the journal, increase "
		"zircon_DEF_MAXIMUM_COMMAND_SIZE or shrink the delta"
	);

	if (this->m_is_open == false)
		return false;

	locator_out.m_block_id = static_cast<kotek::uint32_t>(
		this->m_block_file_offsets.size()
	);
	locator_out.m_offset_in_block = static_cast<kotek::uint32_t>(
		this->m_block_accumulator.size()
	);

	const unsigned char* p_header_bytes =
		reinterpret_cast<const unsigned char*>(&header);

	this->m_block_accumulator.insert(
		this->m_block_accumulator.end(),
		p_header_bytes,
		p_header_bytes + sizeof(header)
	);

	if (payload_size)
	{
		this->m_block_accumulator.insert(
			this->m_block_accumulator.end(),
			p_payload,
			p_payload + payload_size
		);
	}

	++this->m_current_block_entry_count;
	++this->m_total_entry_count;
	this->m_total_raw_entry_bytes +=
		sizeof(header) + payload_size;

	if (this->m_current_block_entry_count >=
	    this->m_entries_per_block)
	{
		return this->flush_block();
	}

	return true;
}

bool zircon_command_journal::read_entry(
	const zircon_command_journal_locator& locator,
	zircon_command_journal_entry_header& header_out,
	unsigned char* p_payload_buffer,
	kotek::uint32_t payload_buffer_capacity
) noexcept
{
	KOTEK_ASSERT(this->m_is_open, "journal is not opened!");

	if (this->m_is_open == false)
		return false;

	const unsigned char* p_block_data = nullptr;
	kotek::uint32_t block_raw_size = 0;

	if (this->load_block(
			locator.m_block_id, p_block_data, block_raw_size
		) == false)
	{
		return false;
	}

	if (locator.m_offset_in_block + sizeof(header_out) >
	    block_raw_size)
	{
		KOTEK_MESSAGE_ERROR(
			"journal locator points outside of block {} "
			"(offset {})",
			locator.m_block_id,
			locator.m_offset_in_block
		);
		return false;
	}

	kotek::ktk::memory::memcpy(
		&header_out,
		p_block_data + locator.m_offset_in_block,
		sizeof(header_out)
	);

	if (p_payload_buffer)
	{
		if (locator.m_offset_in_block + sizeof(header_out) +
		        header_out.m_payload_size >
		    block_raw_size)
		{
			KOTEK_MESSAGE_ERROR(
				"journal entry payload is out of block {} bounds",
				locator.m_block_id
			);
			return false;
		}

		KOTEK_ASSERT(
			header_out.m_payload_size <= payload_buffer_capacity,
			"payload buffer is too small: need {} got {}",
			header_out.m_payload_size,
			payload_buffer_capacity
		);

		if (header_out.m_payload_size > payload_buffer_capacity)
			return false;

		kotek::ktk::memory::memcpy(
			p_payload_buffer,
			p_block_data + locator.m_offset_in_block +
				sizeof(header_out),
			header_out.m_payload_size
		);
	}

	return true;
}

kotek::uint64_t
zircon_command_journal::get_total_entry_count(void) const noexcept
{
	return this->m_total_entry_count;
}

kotek::uint64_t zircon_command_journal::get_total_raw_entry_bytes(
	void
) const noexcept
{
	return this->m_total_raw_entry_bytes;
}

kotek::uint64_t
zircon_command_journal::get_journal_file_size(void) noexcept
{
	kotek::uint64_t result{};

	if (this->m_journal_stream.is_open())
	{
		auto current = this->m_journal_stream.tellp();
		this->m_journal_stream.seekp(0, std::ios::end);
		result = static_cast<kotek::uint64_t>(
			this->m_journal_stream.tellp()
		);
		this->m_journal_stream.seekp(current);
	}

	return result;
}

kotek::uint64_t
zircon_command_journal::get_snapshot_file_size(void) noexcept
{
	kotek::uint64_t result{};

	if (this->m_snapshot_stream.is_open())
	{
		auto current = this->m_snapshot_stream.tellp();
		this->m_snapshot_stream.seekp(0, std::ios::end);
		result = static_cast<kotek::uint64_t>(
			this->m_snapshot_stream.tellp()
		);
		this->m_snapshot_stream.seekp(current);
	}

	return result;
}

bool zircon_command_journal::append_snapshot(
	kotek::uint32_t node_id,
	const unsigned char* p_raw,
	kotek::uint32_t raw_size,
	kotek::uint64_t& offset_out,
	kotek::uint32_t& compressed_size_out
) noexcept
{
	KOTEK_ASSERT(this->m_is_open, "journal is not opened!");
	KOTEK_ASSERT(
		raw_size == 0 || p_raw,
		"non zero raw size requires a valid buffer"
	);

	if (this->m_is_open == false)
		return false;

	const kotek::size_t bound =
		ZSTD_compressBound(raw_size ? raw_size : 1);

	if (this->m_compression_scratch.size() < bound)
	{
		this->m_compression_scratch.resize(bound);
	}

	const kotek::size_t compressed_size = ZSTD_compress(
		this->m_compression_scratch.data(),
		bound,
		p_raw,
		raw_size,
		ZSTD_CLEVEL_DEFAULT
	);

	if (ZSTD_isError(compressed_size))
	{
		KOTEK_MESSAGE_ERROR(
			"zstd failed to compress a snapshot: {}",
			ZSTD_getErrorName(compressed_size)
		);
		return false;
	}

	kotek::uint32_t record_header[_k_snapshot_record_header_size /
		sizeof(kotek::uint32_t)];
	record_header[0] = node_id;
	record_header[1] =
		static_cast<kotek::uint32_t>(compressed_size);
	record_header[2] = raw_size;

	this->m_snapshot_stream.seekp(0, std::ios::end);

	offset_out = static_cast<kotek::uint64_t>(
		this->m_snapshot_stream.tellp()
	);
	compressed_size_out =
		static_cast<kotek::uint32_t>(compressed_size);

	this->m_snapshot_stream.write(
		reinterpret_cast<const char*>(record_header),
		sizeof(record_header)
	);
	this->m_snapshot_stream.write(
		reinterpret_cast<const char*>(
			this->m_compression_scratch.data()
		),
		compressed_size
	);

	if (this->m_snapshot_stream.good() == false)
	{
		KOTEK_MESSAGE_ERROR("failed to write a snapshot record");
		return false;
	}

	return true;
}

bool zircon_command_journal::read_snapshot(
	kotek::uint64_t offset,
	kotek::uint32_t compressed_size,
	kotek::uint32_t raw_size,
	unsigned char* p_raw_buffer,
	kotek::uint32_t raw_buffer_capacity
) noexcept
{
	KOTEK_ASSERT(this->m_is_open, "journal is not opened!");
	KOTEK_ASSERT(p_raw_buffer, "must be a valid buffer");
	KOTEK_ASSERT(
		raw_size <= raw_buffer_capacity,
		"snapshot buffer is too small"
	);

	if (this->m_is_open == false)
		return false;

	if (this->m_compression_scratch.size() < compressed_size)
	{
		this->m_compression_scratch.resize(compressed_size);
	}

	this->m_snapshot_stream.seekg(
		static_cast<std::streamoff>(offset) +
			_k_snapshot_record_header_size,
		std::ios::beg
	);
	this->m_snapshot_stream.read(
		reinterpret_cast<char*>(this->m_compression_scratch.data()),
		compressed_size
	);

	if (this->m_snapshot_stream.good() == false)
	{
		KOTEK_MESSAGE_ERROR(
			"failed to read a snapshot at offset {}", offset
		);
		return false;
	}

	const kotek::size_t decompressed_size = ZSTD_decompress(
		p_raw_buffer,
		raw_buffer_capacity,
		this->m_compression_scratch.data(),
		compressed_size
	);

	if (ZSTD_isError(decompressed_size))
	{
		KOTEK_MESSAGE_ERROR(
			"zstd failed to decompress a snapshot: {}",
			ZSTD_getErrorName(decompressed_size)
		);
		return false;
	}

	if (decompressed_size != raw_size)
	{
		KOTEK_MESSAGE_ERROR(
			"snapshot raw size mismatch: expected {} got {}",
			raw_size,
			decompressed_size
		);
		return false;
	}

	return true;
}

bool zircon_command_journal::write_file_headers(void) noexcept
{
	kotek::uint32_t journal_header[_k_header_size /
		sizeof(kotek::uint32_t)];
	journal_header[0] = zircon_DEF_COMMAND_JOURNAL_MAGIC;
	journal_header[1] = zircon_DEF_COMMAND_JOURNAL_VERSION;
	journal_header[2] = this->m_entries_per_block;
	journal_header[3] = 0;

	this->m_journal_stream.seekp(0, std::ios::beg);
	this->m_journal_stream.write(
		reinterpret_cast<const char*>(journal_header),
		sizeof(journal_header)
	);

	kotek::uint32_t snapshot_header[_k_snapshot_header_size /
		sizeof(kotek::uint32_t)];
	snapshot_header[0] = zircon_DEF_COMMAND_SNAPSHOT_MAGIC;
	snapshot_header[1] = zircon_DEF_COMMAND_SNAPSHOT_VERSION;
	snapshot_header[2] = 0;
	snapshot_header[3] = 0;

	this->m_snapshot_stream.seekp(0, std::ios::beg);
	this->m_snapshot_stream.write(
		reinterpret_cast<const char*>(snapshot_header),
		sizeof(snapshot_header)
	);

	if (this->m_journal_stream.good() == false ||
	    this->m_snapshot_stream.good() == false)
	{
		KOTEK_MESSAGE_ERROR("failed to write journal file headers");
		return false;
	}

	return true;
}

bool zircon_command_journal::scan_existing_file(void) noexcept
{
	kotek::uint32_t file_header[_k_header_size /
		sizeof(kotek::uint32_t)];

	this->m_journal_stream.seekg(0, std::ios::beg);
	this->m_journal_stream.read(
		reinterpret_cast<char*>(file_header), sizeof(file_header)
	);

	if (this->m_journal_stream.good() == false)
	{
		KOTEK_MESSAGE_ERROR("failed to read the journal header");
		return false;
	}

	if (file_header[0] != zircon_DEF_COMMAND_JOURNAL_MAGIC)
	{
		KOTEK_MESSAGE_ERROR(
			"journal file has an invalid magic, refusing to "
			"append (got {:#x})",
			file_header[0]
		);
		return false;
	}

	if (file_header[1] != zircon_DEF_COMMAND_JOURNAL_VERSION)
	{
		KOTEK_MESSAGE_ERROR(
			"journal file version {} is not supported (expected "
			"{})",
			file_header[1],
			zircon_DEF_COMMAND_JOURNAL_VERSION
		);
		return false;
	}

	this->m_entries_per_block = file_header[2];

	// scan block headers to rebuild the block index; note that the
	// last block may be absent from the file entirely when the
	// previous session ended without a flush (its entries are lost,
	// everything before it stays valid)
	this->m_block_file_offsets.clear();

	kotek::uint64_t offset = _k_header_size;

	this->m_journal_stream.seekg(0, std::ios::end);
	const kotek::uint64_t file_size = static_cast<kotek::uint64_t>(
		this->m_journal_stream.tellg()
	);

	while (offset + _k_block_header_size <= file_size)
	{
		kotek::uint32_t block_header[_k_block_header_size /
			sizeof(kotek::uint32_t)];

		this->m_journal_stream.seekg(
			static_cast<std::streamoff>(offset), std::ios::beg
		);
		this->m_journal_stream.read(
			reinterpret_cast<char*>(block_header),
			sizeof(block_header)
		);

		if (this->m_journal_stream.good() == false)
		{
			// reset the stream state so the caller can keep using the
			// journal (a sticky failbit turns one bad read into every
			// later read failing at any offset)
			this->m_journal_stream.clear();
			KOTEK_MESSAGE_ERROR(
				"failed to scan a journal block at offset {}", offset
			);
			return false;
		}

		const kotek::uint32_t compressed_size = block_header[0];
		const kotek::uint32_t entry_count = block_header[2];

		if (offset + _k_block_header_size + compressed_size >
		    file_size)
		{
			KOTEK_MESSAGE_WARNING(
				"journal file has a truncated tail block at "
				"offset {}, ignoring it",
				offset
			);
			break;
		}

		this->m_block_file_offsets.push_back(offset);
		this->m_total_entry_count += entry_count;

		offset += _k_block_header_size + compressed_size;
	}

	// raw bytes are unknown without decompressing everything; the
	// counter is only used for compression budgets of the running
	// session so it starts from zero on load
	this->m_total_raw_entry_bytes = 0;

	return true;
}

bool zircon_command_journal::flush_block(void) noexcept
{
	if (this->m_current_block_entry_count == 0)
		return true;

	const kotek::uint32_t raw_size = static_cast<kotek::uint32_t>(
		this->m_block_accumulator.size()
	);

	const kotek::size_t bound = ZSTD_compressBound(raw_size);

	if (this->m_compression_scratch.size() < bound)
	{
		this->m_compression_scratch.resize(bound);
	}

	const kotek::size_t compressed_size = ZSTD_compress(
		this->m_compression_scratch.data(),
		bound,
		this->m_block_accumulator.data(),
		raw_size,
		ZSTD_CLEVEL_DEFAULT
	);

	if (ZSTD_isError(compressed_size))
	{
		KOTEK_MESSAGE_ERROR(
			"zstd failed to compress a journal block: {}",
			ZSTD_getErrorName(compressed_size)
		);
		return false;
	}

	kotek::uint32_t block_header[_k_block_header_size /
		sizeof(kotek::uint32_t)];
	block_header[0] = static_cast<kotek::uint32_t>(compressed_size);
	block_header[1] = raw_size;
	block_header[2] = this->m_current_block_entry_count;

	this->m_journal_stream.seekp(0, std::ios::end);

	const kotek::uint64_t block_file_offset =
		static_cast<kotek::uint64_t>(
			this->m_journal_stream.tellp()
		);

	this->m_journal_stream.write(
		reinterpret_cast<const char*>(block_header),
		sizeof(block_header)
	);
	this->m_journal_stream.write(
		reinterpret_cast<const char*>(
			this->m_compression_scratch.data()
		),
		compressed_size
	);

	if (this->m_journal_stream.good() == false)
	{
		this->m_journal_stream.clear();
		KOTEK_MESSAGE_ERROR("failed to write a journal block");
		return false;
	}

	this->m_block_file_offsets.push_back(block_file_offset);
	this->m_block_accumulator.clear();
	this->m_current_block_entry_count = 0;

	return true;
}

bool zircon_command_journal::load_block(
	kotek::uint32_t block_id,
	const unsigned char*& pp_data,
	kotek::uint32_t& raw_size_out
) noexcept
{
	// the still-not-flushed tail block is served from the
	// accumulator
	if (block_id ==
	    static_cast<kotek::uint32_t>(
			this->m_block_file_offsets.size()
		))
	{
		if (this->m_current_block_entry_count > 0)
		{
			pp_data = this->m_block_accumulator.data();
			raw_size_out = static_cast<kotek::uint32_t>(
				this->m_block_accumulator.size()
			);
			return true;
		}

		return false;
	}

	if (block_id >
	    static_cast<kotek::uint32_t>(
			this->m_block_file_offsets.size()
		))
	{
		KOTEK_MESSAGE_ERROR(
			"journal block {} does not exist (blocks: {})",
			block_id,
			this->m_block_file_offsets.size()
		);
		return false;
	}

	for (kotek::uint32_t i = 0; i < _k_block_cache_size; ++i)
	{
		if (this->m_cache_block_ids[i] == block_id)
		{
			pp_data = this->m_cache_blocks[i].data();
			raw_size_out = static_cast<kotek::uint32_t>(
				this->m_cache_blocks[i].size()
			);
			return true;
		}
	}

	// cache miss, read + decompress the block
	const kotek::uint64_t file_offset =
		this->m_block_file_offsets[block_id];

	kotek::uint32_t block_header[_k_block_header_size /
		sizeof(kotek::uint32_t)];

	this->m_journal_stream.seekg(
		static_cast<std::streamoff>(file_offset), std::ios::beg
	);
	this->m_journal_stream.read(
		reinterpret_cast<char*>(block_header), sizeof(block_header)
	);

	if (this->m_journal_stream.good() == false)
	{
		// reset the stream state so one bad read does not poison every
		// later read (a sticky failbit fails all of them at any offset)
		this->m_journal_stream.clear();
		KOTEK_MESSAGE_ERROR(
			"failed to read a journal block header at offset {}",
			file_offset
		);
		return false;
	}

	const kotek::uint32_t compressed_size = block_header[0];
	const kotek::uint32_t raw_size = block_header[1];

	if (this->m_compression_scratch.size() < compressed_size)
	{
		this->m_compression_scratch.resize(compressed_size);
	}

	this->m_journal_stream.read(
		reinterpret_cast<char*>(this->m_compression_scratch.data()),
		compressed_size
	);

	if (this->m_journal_stream.good() == false)
	{
		this->m_journal_stream.clear();
		KOTEK_MESSAGE_ERROR(
			"failed to read a journal block payload at offset {}",
			file_offset
		);
		return false;
	}

	const kotek::uint32_t cache_slot = this->m_cache_next_slot;
	this->m_cache_next_slot =
		(this->m_cache_next_slot + 1) % _k_block_cache_size;

	auto& cache_block = this->m_cache_blocks[cache_slot];

	if (cache_block.size() < raw_size)
	{
		cache_block.resize(raw_size);
	}

	const kotek::size_t decompressed_size = ZSTD_decompress(
		cache_block.data(),
		raw_size,
		this->m_compression_scratch.data(),
		compressed_size
	);

	if (ZSTD_isError(decompressed_size))
	{
		KOTEK_MESSAGE_ERROR(
			"zstd failed to decompress journal block {}: {}",
			block_id,
			ZSTD_getErrorName(decompressed_size)
		);
		return false;
	}

	if (decompressed_size != raw_size)
	{
		KOTEK_MESSAGE_ERROR(
			"journal block {} raw size mismatch: expected {} got "
			"{}",
			block_id,
			raw_size,
			decompressed_size
		);
		return false;
	}

	cache_block.resize(raw_size);
	this->m_cache_block_ids[cache_slot] = block_id;

	pp_data = cache_block.data();
	raw_size_out = raw_size;

	return true;
}
