#pragma once

#include "zircon_command_definitions.h"

/// @brief \~english header of one journaled command entry, payloads
/// (deltas) follow the header inside a compressed block. All fields
/// are stored little endian on disk (x86/x64 writer and reader).
struct zircon_command_journal_entry_header
{
	kotek::uint32_t m_node_id;
	kotek::uint32_t m_parent_node_id;
	kotek::uint32_t m_command_type;
	kotek::uint32_t m_entity_id;
	kotek::uint32_t m_payload_size;
};

/// @brief \~english identifies one entry inside the journal file for
/// later random access: block index + byte offset inside the block's
/// raw (decompressed) blob
struct zircon_command_journal_locator
{
	kotek::uint32_t m_block_id;
	kotek::uint32_t m_offset_in_block;
};

/// @brief \~english append-only binary journal for the command
/// history (task Z6). Entries are accumulated in RAM and flushed as
/// zstd-compressed blocks of
/// zircon_DEF_COMMAND_HISTORY_JOURNAL_ENTRIES_PER_BLOCK entries.
/// World snapshots live in a second append-only file, also
/// zstd-compressed. Nothing is ever deleted or rewritten; closing
/// only flushes buffers.
///
/// File layout of the journal (all integers little endian):
///   [u32 magic][u32 version][u32 entries_per_block][u32 reserved]
///   then per block:
///   [u32 compressed_size][u32 raw_size][u32 entry_count]
///   [zstd frame of raw_size bytes decompressed]
/// Raw block blob is a concatenation of entries:
///   [zircon_command_journal_entry_header][payload bytes]
///
/// File layout of the snapshot file:
///   [u32 magic][u32 version][u32 reserved][u32 reserved]
///   then per snapshot:
///   [u32 node_id][u32 compressed_size][u32 raw_size][zstd frame]
///
/// @note file IO uses kotek::cfstream_t (std fstream alias) in binary
/// mode because ktkIFileSystem has no read-by-handle API; this is the
/// same approach the previous history implementation used.
class zircon_command_journal
{
public:
	zircon_command_journal(void);
	~zircon_command_journal(void);

	zircon_command_journal(const zircon_command_journal&) = delete;
	zircon_command_journal& operator=(
		const zircon_command_journal&) = delete;

	/// @brief \~english opens (or creates) the journal and snapshot
	/// files. An existing non-empty journal is scanned (block index
	/// is rebuilt) and opened for appending; files are never
	/// truncated or deleted here.
	bool open(
		const ktk_filesystem_path& journal_file_path,
		const ktk_filesystem_path& snapshot_file_path
	) noexcept;

	/// @brief \~english flushes pending entries and closes both
	/// files; the content stays on disk (full retention)
	void close(void) noexcept;

	/// @brief \~english flushes the current block to disk even if it
	/// is not full yet (used by close and by tests)
	void flush(void) noexcept;

	bool is_open(void) const noexcept;

	/// @brief \~english appends one entry (header + payload) to the
	/// current block; the locator identifies the entry for
	/// read_entry. Payload is the command delta (see
	/// zircon_interface_command_delta).
	bool append_entry(
		const zircon_command_journal_entry_header& header,
		const unsigned char* p_payload,
		kotek::uint32_t payload_size,
		zircon_command_journal_locator& locator_out
	) noexcept;

	/// @brief \~english reads one entry back; p_payload_buffer may be
	/// nullptr when only the header is needed
	bool read_entry(
		const zircon_command_journal_locator& locator,
		zircon_command_journal_entry_header& header_out,
		unsigned char* p_payload_buffer,
		kotek::uint32_t payload_buffer_capacity
	) noexcept;

	/// @brief \~english amount of entries written so far (including
	/// the ones still buffered in the current block)
	kotek::uint64_t get_total_entry_count(void) const noexcept;

	/// @brief \~english uncompressed bytes of all entries (headers +
	/// payloads), used for compression budget checks
	kotek::uint64_t get_total_raw_entry_bytes(void) const noexcept;

	/// @brief \~english current sizes of both files on disk
	kotek::uint64_t get_journal_file_size(void) noexcept;
	kotek::uint64_t get_snapshot_file_size(void) noexcept;

	/// @brief \~english appends a zstd-compressed world snapshot;
	/// offset_out + compressed_size_out identify it for read_snapshot
	bool append_snapshot(
		kotek::uint32_t node_id,
		const unsigned char* p_raw,
		kotek::uint32_t raw_size,
		kotek::uint64_t& offset_out,
		kotek::uint32_t& compressed_size_out
	) noexcept;

	bool read_snapshot(
		kotek::uint64_t offset,
		kotek::uint32_t compressed_size,
		kotek::uint32_t raw_size,
		unsigned char* p_raw_buffer,
		kotek::uint32_t raw_buffer_capacity
	) noexcept;

	/// @brief \~english iterates all entries in append order and
	/// invokes callback(header, p_payload) for each; used by the
	/// history to rebuild its tree after loading an existing journal
	template <typename CallbackType>
	bool read_all_entries(CallbackType&& callback) noexcept
	{
		bool result = true;

		zircon_command_journal_entry_header header;
		kotek::hybrid_vector_t<unsigned char, 256> payload;

		const kotek::uint32_t block_count =
			static_cast<kotek::uint32_t>(
				this->m_block_file_offsets.size()
			);

		for (kotek::uint32_t block_id = 0;
		     block_id < block_count && result; ++block_id)
		{
			const unsigned char* p_block_data = nullptr;
			kotek::uint32_t block_raw_size = 0;

			if (this->load_block(block_id, p_block_data, block_raw_size) ==
			    false)
			{
				return false;
			}

			kotek::uint32_t offset = 0;
			while (offset < block_raw_size)
			{
				if (offset + sizeof(header) > block_raw_size)
				{
					KOTEK_MESSAGE_ERROR(
						"journal block {} is corrupted (truncated "
						"entry header)",
						block_id
					);
					return false;
				}

				kotek::ktk::memory::memcpy(
					&header, p_block_data + offset, sizeof(header)
				);

				if (offset + sizeof(header) + header.m_payload_size >
				    block_raw_size)
				{
					KOTEK_MESSAGE_ERROR(
						"journal block {} is corrupted (truncated "
						"entry payload)",
						block_id
					);
					return false;
				}

				const unsigned char* p_payload =
					p_block_data + offset + sizeof(header);

				callback(header, p_payload);

				offset += static_cast<kotek::uint32_t>(
					sizeof(header) + header.m_payload_size
				);
			}
		}

		(void)payload;
		return result;
	}

private:
	bool write_file_headers(void) noexcept;
	bool scan_existing_file(void) noexcept;
	bool flush_block(void) noexcept;

	/// @brief \~english loads (and caches) the decompressed content
	/// of a block; pp_data points into the cache and stays valid
	/// until the next load_block that evicts the slot
	bool load_block(
		kotek::uint32_t block_id,
		const unsigned char*& pp_data,
		kotek::uint32_t& raw_size_out
	) noexcept;

private:
	static constexpr kotek::uint32_t _k_header_size = 16;
	static constexpr kotek::uint32_t _k_block_header_size = 12;
	static constexpr kotek::uint32_t _k_snapshot_header_size = 16;
	static constexpr kotek::uint32_t _k_snapshot_record_header_size =
		12;
	static constexpr kotek::uint32_t _k_block_cache_size = 2;

	bool m_is_open;
	kotek::uint32_t m_entries_per_block;
	kotek::uint32_t m_current_block_entry_count;
	kotek::uint64_t m_total_entry_count;
	kotek::uint64_t m_total_raw_entry_bytes;

	kotek::cfstream_t m_journal_stream;
	kotek::cfstream_t m_snapshot_stream;

	kotek::hybrid_vector_t<
		unsigned char,
		zircon_DEF_COMMAND_HISTORY_JOURNAL_BLOCK_INLINE_SIZE>
		m_block_accumulator;
	kotek::hybrid_vector_t<unsigned char, 256> m_compression_scratch;

	/// @brief \~english file offsets of all blocks, filled at open
	/// (scan) and while appending
	kotek::hybrid_vector_t<kotek::uint64_t, 256> m_block_file_offsets;

	/// @brief \~english two-slot decompressed block cache (sequential
	/// undo/redo walks have almost perfect locality)
	kotek::uint32_t m_cache_block_ids[_k_block_cache_size];
	kotek::uint32_t m_cache_next_slot;
	kotek::hybrid_vector_t<
		unsigned char,
		zircon_DEF_COMMAND_HISTORY_JOURNAL_BLOCK_INLINE_SIZE>
		m_cache_blocks[_k_block_cache_size];
};
