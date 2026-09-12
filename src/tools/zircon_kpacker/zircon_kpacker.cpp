// zircon_kpacker — the .kpack host tool (filesystem plan phase B2b).
//
// Packs a folder tree into a .kpack archive (the format spec is kotek's
// kotek_kpack_format.h — magic/version, hash-keyed entry table, 64 KB
// independent compression blocks, 4 KB-aligned data spans in load order)
// and maintains existing archives OFFLINE.
//
// The encoder is NEVER reimplemented here: the tool links kotek's
// kotek.core.filesystem.pack module sources and drives THE shared
// kpack_write_file (the same function the engine's tests and any future
// pipeline code use), so writer, reader and tool can never drift on the
// format. The `verify` command additionally mounts the archive through the
// REAL runtime reader (ktkFileSystem_Pack) and cross-checks every named
// entry byte-for-byte against the tool's own independent block walk.
//
// MUTATION MODEL (owner requirement: offline mutation of an existing pack):
// the runtime NEVER mutates a shipped pack — `add`/`remove` read the whole
// entry set (names from the embedded names manifest, bytes decompressed
// block-by-block) and REWRITE the archive wholesale through
// kpack_write_file into a temp sibling that is swapped over the original
// (MoveFileEx replace). No in-place surgery. Intended for small packs,
// tool-side, offline; residency during a rewrite is the pack's raw content.
//
// NAMES MANIFEST (the optional name sidecar): .kpack v1 keys entries by a
// 64-bit fnv1a name hash ONLY — names are not stored. The tool therefore
// embeds a reserved last entry, `_kpack_names.json` (zstd), holding
// {"names":[...]} in entry order. `pack` always writes it (opt out with
// --no-names-manifest); `list`/`verify` use it when present; `add`/`remove`
// REQUIRE it (a rewrite must re-emit the kept entries by name through the
// shared encoder — a hash is not invertible). Packs without the manifest
// are list/verify-only (hashes shown).
//
// Host tool: std is allowed (AGENTS.md §2.1 exemption, same as
// zircon_shaderpack/zircon_generator); the embedded discipline is kept
// anyway — bounded scratch, streaming IO (pack sources are memory-mapped
// read-only and flow through the encoder one 64 KB block at a time; decode
// walks hold one entry at a time), no exceptions, fixed exit codes:
//   0 success, 1 operational failure (io/corrupt/codec/conflict), 2 usage.
//
// Namespace note: the tool speaks to kotek through kotek's own kun_*
// qualifier macros (rename-safe), the same convention kotek's code uses —
// the lowercase consumer alias header would drag the whole core umbrella
// into a one-file tool for two type names.

#include <kotek.core.filesystem.pack/include/kotek_kpack_format.h>
#include <kotek.core.filesystem.pack/include/kotek_filesystem_pack.h>
#include <kotek.core.log/include/kotek_log.h>

#ifdef KOTEK_USE_LOG_LIBRARY_SPDLOG
	#include <spdlog/sinks/stdout_sinks.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <zstd.h>
#include <zlib.h>

#include <windows.h>

// ---------------------------------------------------------------------------
// the logger shim: the tool IS the log module for the kotek objects it
// links. The shared encoder and the reader report through KOTEK_MESSAGE_*,
// which resolve the logger through these module-slot functions (the exact
// surface kotek.core.log provides in the engine); the tool installs a
// console logger so encoder/reader diagnostics reach the user. The shim
// compiles the branch of the ACTIVE log backend (SPDLOG / CUSTOM — the
// driver's ZIRCON_KOTEK_LOG_LIBRARY mirrors the engine tree).
// ---------------------------------------------------------------------------
KOTEK_BEGIN_NAMESPACE_KOTEK

namespace
{
#ifdef KOTEK_USE_LOG_LIBRARY_SPDLOG
	spdlog::logger* g_p_kpacker_logger = nullptr;
#elif defined(KOTEK_USE_LOG_LIBRARY_CUSTOM)
	Core::ktkCustomLogger* g_p_kpacker_logger = nullptr;
#endif
}

#ifdef KOTEK_USE_LOG_LIBRARY_SPDLOG

spdlog::logger* Get_LoggerMain()
{
	return g_p_kpacker_logger;
}

spdlog::logger* Get_LoggerMsvcOutput()
{
	return nullptr;
}

void Set_LoggerMain(void* p_logger)
{
	g_p_kpacker_logger = static_cast<spdlog::logger*>(p_logger);
}

void Set_LoggerMsvcOutput(void*)
{
}

#elif defined(KOTEK_USE_LOG_LIBRARY_CUSTOM)

Core::ktkCustomLogger* Get_LoggerMain()
{
	return g_p_kpacker_logger;
}

Core::ktkCustomLogger* Get_LoggerMsvcOutput()
{
	return nullptr;
}

void Set_LoggerMain(void* p_logger)
{
	g_p_kpacker_logger = static_cast<Core::ktkCustomLogger*>(p_logger);
}

void Set_LoggerMsvcOutput(void*)
{
}

#endif

KOTEK_END_NAMESPACE_KOTEK

namespace
{
	using kun_kotek kun_core eKpackCompression;
	using kun_kotek kun_core eKpackReadResult;
	using kun_kotek kun_core kpack_writer_entry_t;

	/// the reserved entry carrying the entry names (see the file header) —
	/// always the LAST entry, always zstd
	constexpr char k_names_manifest_name[] = "_kpack_names.json";

	constexpr uint64_t k_header_size = 20;
	constexpr uint64_t k_entry_record_size = 45;
	constexpr uint64_t k_block_size = KOTEK_DEF_FILESYSTEM_PACK_BLOCK_SIZE;
	constexpr uint64_t k_max_packed_block =
		KOTEK_DEF_FILESYSTEM_PACK_MAX_PACKED_BLOCK_SIZE;

	// ---------------------------------------------------------------------
	// little-endian field decoders (the format is LE by spec)
	// ---------------------------------------------------------------------
	uint32_t tool_decode_u32(const uint8_t* p)
	{
		return static_cast<uint32_t>(p[0]) |
			(static_cast<uint32_t>(p[1]) << 8) |
			(static_cast<uint32_t>(p[2]) << 16) |
			(static_cast<uint32_t>(p[3]) << 24);
	}

	uint64_t tool_decode_u64(const uint8_t* p)
	{
		return static_cast<uint64_t>(tool_decode_u32(p)) |
			(static_cast<uint64_t>(tool_decode_u32(p + 4)) << 32);
	}

	const char* tool_codec_name(eKpackCompression codec)
	{
		switch (codec)
		{
		case eKpackCompression::kStored:
			return "stored";
		case eKpackCompression::kZstd:
			return "zstd";
		case eKpackCompression::kZlib:
			return "zlib";
		default:
			return "<unknown>";
		}
	}

	bool tool_parse_codec(const char* p_text, eKpackCompression& out)
	{
		if (p_text == nullptr)
			return false;

		std::string folded;
		for (const char* p = p_text; *p; ++p)
			folded.push_back(
				static_cast<char>(std::tolower(
					static_cast<unsigned char>(*p))));

		if (folded == "stored")
		{
			out = eKpackCompression::kStored;
			return true;
		}
		if (folded == "zstd")
		{
			out = eKpackCompression::kZstd;
			return true;
		}
		if (folded == "zlib")
		{
			out = eKpackCompression::kZlib;
			return true;
		}
		return false;
	}

	std::string tool_fold(const std::string& text)
	{
		std::string folded;
		folded.reserve(text.size());
		for (char symbol : text)
			folded.push_back(
				symbol == '\\'
					? '/'
					: static_cast<char>(std::tolower(
						  static_cast<unsigned char>(symbol))));
		return folded;
	}

	// ---------------------------------------------------------------------
	// the tool's own pack table walk (list/verify/mutation) — the format
	// header is the only contract; every offset is bounds-checked against
	// the file size. Structural violations are reported through out_error
	// with the offending entry index.
	// ---------------------------------------------------------------------
	struct tool_entry_t
	{
		uint64_t name_hash = 0;
		uint64_t data_offset = 0;
		uint64_t raw_size = 0;
		uint64_t packed_size = 0;
		uint64_t block_table_offset = 0;
		uint32_t block_count = 0;
		eKpackCompression compression = eKpackCompression::kStored;
		std::vector<uint32_t> block_sizes;
	};

	struct tool_table_t
	{
		uint64_t file_size = 0;
		uint32_t flags = 0;
		std::vector<tool_entry_t> entries;
	};

	bool tool_read_pack_table(
		const std::filesystem::path& pack_path,
		tool_table_t& out,
		std::string& out_error
	)
	{
		std::error_code ec;
		out.file_size =
			static_cast<uint64_t>(std::filesystem::file_size(pack_path, ec));

		if (ec)
		{
			out_error = "cannot stat the pack: " + ec.message();
			return false;
		}

		if (out.file_size < k_header_size)
		{
			out_error = "truncated: smaller than the 20-byte header";
			return false;
		}

		FILE* p_file = fopen(pack_path.string().c_str(), "rb");

		if (p_file == nullptr)
		{
			out_error = "cannot open the pack for reading";
			return false;
		}

		bool status = false;
		uint8_t header[k_header_size] = {};

		if (fread(header, 1, sizeof(header), p_file) != sizeof(header))
		{
			out_error = "truncated: cannot read the header";
			goto done;
		}

		if (memcmp(header, kun_kotek kun_core kKpackMagic,
			    sizeof(kun_kotek kun_core kKpackMagic)) != 0)
		{
			out_error = "bad magic (not a v1 .kpack)";
			goto done;
		}

		out.flags = tool_decode_u32(header + 12);

		if (out.flags != 0)
		{
			out_error = "reserved header flags are not zero";
			goto done;
		}

		{
			const uint32_t entry_count = tool_decode_u32(header + 8);
			const uint64_t entry_table_bytes =
				static_cast<uint64_t>(entry_count) * k_entry_record_size;

			if (k_header_size + entry_table_bytes > out.file_size)
			{
				out_error = "truncated: the entry table runs past the file";
				goto done;
			}

			out.entries.resize(entry_count);

			for (uint32_t i = 0; i < entry_count; ++i)
			{
				uint8_t record[k_entry_record_size] = {};

				if (fread(record, 1, sizeof(record), p_file) !=
				    sizeof(record))
				{
					out_error = "truncated: cannot read entry record";
					goto done;
				}

				tool_entry_t& entry = out.entries[i];

				entry.name_hash = tool_decode_u64(record + 0);
				entry.data_offset = tool_decode_u64(record + 8);
				entry.raw_size = tool_decode_u64(record + 16);
				entry.packed_size = tool_decode_u64(record + 24);
				entry.compression =
					static_cast<eKpackCompression>(record[32]);
				entry.block_count = tool_decode_u32(record + 33);
				entry.block_table_offset = tool_decode_u64(record + 37);

				char where[64];
				snprintf(where, sizeof(where), " (entry %u)", i);

				if (static_cast<uint8_t>(entry.compression) >=
				    static_cast<uint8_t>(
						eKpackCompression::kEndOfEnum))
				{
					out_error =
						std::string("unknown compression id") + where;
					goto done;
				}

				if (entry.block_count !=
				    kun_kotek kun_core kpack_block_count_for_size(
						entry.raw_size))
				{
					out_error = std::string(
						"block_count != ceil(raw_size / 64 KB)") + where;
					goto done;
				}

				const uint64_t block_table_bytes =
					static_cast<uint64_t>(entry.block_count) * 4;

				if (entry.block_table_offset +
					    block_table_bytes >
				    out.file_size)
				{
					out_error = std::string(
						"the block table runs past the file") + where;
					goto done;
				}

				entry.block_sizes.resize(entry.block_count);

				if (entry.block_count > 0)
				{
					if (_fseeki64(p_file,
						    static_cast<__int64>(
								entry.block_table_offset),
						    SEEK_SET) != 0)
					{
						out_error =
							std::string("cannot seek to the block "
							            "table") +
							where;
						goto done;
					}

					uint64_t packed_sum = 0;

					for (uint32_t b = 0; b < entry.block_count; ++b)
					{
						uint8_t size_bytes[4] = {};

						if (fread(size_bytes, 1, 4, p_file) != 4)
						{
							out_error = std::string(
								"truncated: cannot read the block "
								"table") +
								where;
							goto done;
						}

						entry.block_sizes[b] =
							tool_decode_u32(size_bytes);
						packed_sum += entry.block_sizes[b];
					}

					if (packed_sum != entry.packed_size)
					{
						out_error = std::string(
							"packed_size != the block table sum") +
							where;
						goto done;
					}

					// every block span must fit the file
					uint64_t cursor = entry.data_offset;

					for (uint32_t b = 0; b < entry.block_count; ++b)
					{
						cursor += entry.block_sizes[b];
					}

					if (entry.data_offset >
					    out.file_size)
					{
						out_error = std::string(
							"the data offset is past the file") +
							where;
						goto done;
					}

					if (cursor > out.file_size)
					{
						out_error = std::string(
							"the data span runs past the file") +
							where;
						goto done;
					}

					if (entry.data_offset %
						    KOTEK_DEF_FILESYSTEM_PACK_DATA_ALIGNMENT !=
					    0)
					{
						out_error = std::string(
							"the data offset is not 4 KB-aligned") +
							where;
						goto done;
					}
				}

				// position the stream at the next entry record (the
				// block-table detour moved it)
				if (_fseeki64(p_file,
					    static_cast<__int64>(
							k_header_size +
							(static_cast<uint64_t>(i) + 1) *
								k_entry_record_size),
					    SEEK_SET) != 0)
				{
					out_error = "cannot seek to the next entry record";
					goto done;
				}
			}

			// duplicate name hashes make a pack ambiguous (the writer
			// rejects them; a hand-crafted pack could carry them)
			for (uint32_t i = 0; i < entry_count; ++i)
			{
				for (uint32_t j = i + 1; j < entry_count; ++j)
				{
					if (out.entries[i].name_hash ==
					    out.entries[j].name_hash)
					{
						out_error =
							"two entries share one name hash";
						goto done;
					}
				}
			}
		}

		status = true;

	done:
		fclose(p_file);
		return status;
	}

	// ---------------------------------------------------------------------
	// decodes one entry's raw bytes block-by-block through a reused packed
	// scratch (one 65920-byte buffer per call, never the whole pack)
	// ---------------------------------------------------------------------
	bool tool_decode_entry(
		FILE* p_file,
		const tool_entry_t& entry,
		std::vector<uint8_t>& out_raw,
		std::string& out_error
	)
	{
		out_raw.resize(static_cast<size_t>(entry.raw_size));

		if (entry.block_count == 0)
			return true;

		std::vector<uint8_t> packed_scratch;

		if (entry.compression != eKpackCompression::kStored)
			packed_scratch.resize(static_cast<size_t>(k_max_packed_block));

		uint64_t block_offset = entry.data_offset;

		for (uint32_t b = 0; b < entry.block_count; ++b)
		{
			const uint64_t raw_left =
				entry.raw_size - static_cast<uint64_t>(b) * k_block_size;
			const size_t expected_raw = static_cast<size_t>(
				raw_left < k_block_size ? raw_left : k_block_size);
			const uint32_t packed_size = entry.block_sizes[b];

			if (_fseeki64(
				    p_file, static_cast<__int64>(block_offset), SEEK_SET
			    ) != 0)
			{
				out_error = "cannot seek to a block";
				return false;
			}

			uint8_t* p_raw_block =
				out_raw.data() + static_cast<size_t>(b) * k_block_size;

			if (entry.compression == eKpackCompression::kStored)
			{
				if (fread(p_raw_block, 1, expected_raw, p_file) !=
				    expected_raw)
				{
					out_error = "short read on a stored block";
					return false;
				}
			}
			else
			{
				if (packed_size > k_max_packed_block)
				{
					out_error = "a packed block exceeds the scratch";
					return false;
				}

				if (fread(packed_scratch.data(), 1, packed_size,
					    p_file) != packed_size)
				{
					out_error = "short read on a packed block";
					return false;
				}

				if (entry.compression == eKpackCompression::kZstd)
				{
					const size_t decoded = ZSTD_decompress(
						p_raw_block, expected_raw,
						packed_scratch.data(), packed_size);

					if (ZSTD_isError(decoded) ||
					    decoded != expected_raw)
					{
						out_error =
							std::string("zstd block decompression "
							            "failed (") +
							(ZSTD_isError(decoded)
									? ZSTD_getErrorName(decoded)
									: "size mismatch") +
							")";
						return false;
					}
				}
				else
				{
					uLongf dest_len =
						static_cast<uLongf>(expected_raw);

					const int zlib_status = uncompress(
						p_raw_block, &dest_len, packed_scratch.data(),
						static_cast<uLong>(packed_size));

					if (zlib_status != Z_OK ||
					    dest_len != expected_raw)
					{
						out_error =
							"zlib block decompression failed (status " +
							std::to_string(zlib_status) + ")";
						return false;
					}
				}
			}

			block_offset += packed_size;
		}

		return true;
	}

	// ---------------------------------------------------------------------
	// minimal json: the tool emits/reads exactly two shapes — the embedded
	// names manifest {"names":["a",...]} and the load-order manifest
	// ["a",...]. A general json library is overkill here (and the tool
	// links none); strings support the \" \\ \/ escapes on both sides.
	// ---------------------------------------------------------------------
	void tool_json_escape_and_emit(std::string& out, const std::string& text)
	{
		out.push_back('"');

		for (const unsigned char symbol : text)
		{
			switch (symbol)
			{
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			default:
				if (symbol < 0x20)
				{
					char escape[8];
					snprintf(
						escape, sizeof(escape), "\\u%04x", symbol);
					out += escape;
				}
				else
				{
					out.push_back(static_cast<char>(symbol));
				}
				break;
			}
		}

		out.push_back('"');
	}

	void tool_json_skip_ws(const char*& p)
	{
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			++p;
	}

	bool tool_json_parse_string(const char*& p, std::string& out)
	{
		tool_json_skip_ws(p);

		if (*p != '"')
			return false;

		++p;
		out.clear();

		while (*p != '\0' && *p != '"')
		{
			if (*p == '\\')
			{
				++p;

				if (*p == '"' || *p == '\\' || *p == '/')
				{
					out.push_back(*p);
					++p;
				}
				else if (*p == 'n')
				{
					out.push_back('\n');
					++p;
				}
				else if (*p == 't')
				{
					out.push_back('\t');
					++p;
				}
				else if (*p == 'r')
				{
					out.push_back('\r');
					++p;
				}
				else if (*p == 'u')
				{
					// \u00XX — the only form the emitter produces
					// (control bytes); names are paths, anything
					// beyond byte range is rejected
					unsigned value = 0;
					bool ok = true;

					for (int d = 0; d < 4; ++d)
					{
						++p;
						const char h = *p;
						value <<= 4;

						if (h >= '0' && h <= '9')
							value |= static_cast<unsigned>(h - '0');
						else if (h >= 'a' && h <= 'f')
							value |= static_cast<unsigned>(
								h - 'a' + 10);
						else if (h >= 'A' && h <= 'F')
							value |= static_cast<unsigned>(
								h - 'A' + 10);
						else
							ok = false;
					}
					++p;

					if (ok == false || value > 0xFF)
						return false;

					out.push_back(static_cast<char>(value));
				}
				else
				{
					return false;
				}
			}
			else
			{
				out.push_back(*p);
				++p;
			}
		}

		if (*p != '"')
			return false;

		++p;
		return true;
	}

	/// parses ["a","b",...] starting at *pp (ws-tolerant), advancing past
	/// the closing ']'
	bool tool_json_parse_string_array(
		const char*& p, std::vector<std::string>& out
	)
	{
		tool_json_skip_ws(p);

		if (*p != '[')
			return false;

		++p;
		tool_json_skip_ws(p);

		if (*p == ']')
		{
			++p;
			return true;
		}

		for (;;)
		{
			std::string element;

			if (tool_json_parse_string(p, element) == false)
				return false;

			out.push_back(std::move(element));
			tool_json_skip_ws(p);

			if (*p == ',')
			{
				++p;
				continue;
			}

			if (*p == ']')
			{
				++p;
				return true;
			}

			return false;
		}
	}

	/// extracts the "names" array from the embedded manifest document
	bool tool_parse_names_manifest(
		const std::vector<uint8_t>& raw, std::vector<std::string>& out_names
	)
	{
		std::string text(raw.begin(), raw.end());
		const char* p = text.c_str();

		tool_json_skip_ws(p);

		if (*p != '{')
			return false;

		++p;

		std::string key;

		if (tool_json_parse_string(p, key) == false || key != "names")
			return false;

		tool_json_skip_ws(p);

		if (*p != ':')
			return false;

		++p;

		if (tool_json_parse_string_array(p, out_names) == false)
			return false;

		tool_json_skip_ws(p);

		return *p == '}';
	}

	std::string tool_emit_names_manifest(
		const std::vector<std::string>& names
	)
	{
		std::string json = "{\"names\":[";

		for (size_t i = 0; i < names.size(); ++i)
		{
			if (i)
				json.push_back(',');

			tool_json_escape_and_emit(json, names[i]);
		}

		json += "]}";
		return json;
	}

	// ---------------------------------------------------------------------
	// file mapping (pack/add): source files flow through read-only mappings
	// — the tool never copies a whole file into its own buffers; the
	// encoder consumes the view one 64 KB block at a time and the OS page
	// cache is the only file-content cache (the house streaming posture)
	// ---------------------------------------------------------------------
	struct tool_mapped_file_t
	{
		HANDLE h_file = nullptr;
		HANDLE h_mapping = nullptr;
		const uint8_t* p_view = nullptr;
		uint64_t size = 0;

		~tool_mapped_file_t() { this->close(); }

		tool_mapped_file_t() = default;
		tool_mapped_file_t(const tool_mapped_file_t&) = delete;
		tool_mapped_file_t& operator=(const tool_mapped_file_t&) = delete;

		bool open(const std::filesystem::path& path, std::string& out_error)
		{
			this->h_file = CreateFileA(
				path.string().c_str(), GENERIC_READ, FILE_SHARE_READ,
				nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

			if (this->h_file == INVALID_HANDLE_VALUE)
			{
				this->h_file = nullptr;
				out_error = "cannot open '" + path.string() +
					"' (GetLastError " +
					std::to_string(GetLastError()) + ")";
				return false;
			}

			LARGE_INTEGER file_size{};

			if (GetFileSizeEx(this->h_file, &file_size) == FALSE)
			{
				out_error =
					"cannot size '" + path.string() + "'";
				this->close();
				return false;
			}

			this->size = static_cast<uint64_t>(file_size.QuadPart);

			// a 0-byte file maps to a null view with size 0 — legal (the
			// writer emits a 0-block entry)
			if (this->size == 0)
				return true;

			this->h_mapping = CreateFileMappingA(
				this->h_file, nullptr, PAGE_READONLY, 0, 0, nullptr);

			if (this->h_mapping == nullptr)
			{
				out_error = "cannot map '" + path.string() +
					"' (GetLastError " +
					std::to_string(GetLastError()) + ")";
				this->close();
				return false;
			}

			this->p_view = static_cast<const uint8_t*>(MapViewOfFile(
				this->h_mapping, FILE_MAP_READ, 0, 0, 0));

			if (this->p_view == nullptr)
			{
				out_error = "cannot view '" + path.string() +
					"' (GetLastError " +
					std::to_string(GetLastError()) + ")";
				this->close();
				return false;
			}

			return true;
		}

		void close()
		{
			if (this->p_view)
			{
				UnmapViewOfFile(this->p_view);
				this->p_view = nullptr;
			}

			if (this->h_mapping)
			{
				CloseHandle(this->h_mapping);
				this->h_mapping = nullptr;
			}

			if (this->h_file)
			{
				CloseHandle(this->h_file);
				this->h_file = nullptr;
			}

			this->size = 0;
		}
	};

	// ---------------------------------------------------------------------
	// one entry of a wholesale rewrite (mutation model): name + raw bytes +
	// the codec to write with
	// ---------------------------------------------------------------------
	struct tool_rewrite_entry_t
	{
		std::string name;
		std::vector<uint8_t> data;
		eKpackCompression compression = eKpackCompression::kZstd;
	};

	/// writes a complete pack (the rewrite entries in order + the embedded
	/// names manifest as the last entry) into a temp sibling and swaps it
	/// over pack_path. Returns false with the reason on any failure — the
	/// original pack is untouched unless the swap itself ran.
	bool tool_write_pack_with_swap(
		const std::filesystem::path& pack_path,
		std::vector<tool_rewrite_entry_t>& entries,
		bool with_names_manifest,
		std::string& out_error
	)
	{
		std::vector<kpack_writer_entry_t> writer_entries;
		std::vector<std::string> names;
		std::string manifest_json;

		writer_entries.reserve(entries.size() + 1);
		names.reserve(entries.size());

		for (tool_rewrite_entry_t& entry : entries)
		{
			writer_entries.push_back(
				{entry.name.c_str(),
				 entry.data.empty() ? nullptr : entry.data.data(),
				 entry.data.size(), entry.compression});
			names.push_back(entry.name);
		}

		if (with_names_manifest)
		{
			manifest_json = tool_emit_names_manifest(names);

			writer_entries.push_back(
				{k_names_manifest_name,
				 reinterpret_cast<const uint8_t*>(
					 manifest_json.data()),
				 manifest_json.size(), eKpackCompression::kZstd});
		}

		const std::filesystem::path temp_path =
			pack_path.string() + ".kpackertmp";

		std::error_code ec;
		std::filesystem::remove(temp_path, ec);
		ec.clear();

		const bool wrote = kun_kotek kun_core kpack_write_file(
			temp_path.string().c_str(), writer_entries.data(),
			writer_entries.size());

		if (wrote == false)
		{
			// the encoder removes its partial output itself
			out_error =
				"the encoder failed — the original pack is untouched";
			return false;
		}

		if (MoveFileExA(temp_path.string().c_str(),
			    pack_path.string().c_str(),
			    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ==
		    FALSE)
		{
			out_error = "the rewritten pack is at '" +
				temp_path.string() +
				"' — swapping it over the original failed "
				"(GetLastError " +
				std::to_string(GetLastError()) + ")";
			return false;
		}

		return true;
	}

	/// reads an existing pack for mutation: the table + the names manifest
	/// (required) + every entry's raw bytes, in table order minus the
	/// manifest entry itself
	bool tool_read_pack_for_rewrite(
		const std::filesystem::path& pack_path,
		std::vector<tool_rewrite_entry_t>& out_entries,
		std::string& out_error
	)
	{
		tool_table_t table;

		if (tool_read_pack_table(pack_path, table, out_error) == false)
			return false;

		FILE* p_file = fopen(pack_path.string().c_str(), "rb");

		if (p_file == nullptr)
		{
			out_error = "cannot open the pack for reading";
			return false;
		}

		bool status = false;
		const uint64_t manifest_hash = kun_kotek kun_core kpack_hash_name(
			k_names_manifest_name, sizeof(k_names_manifest_name) - 1);

		std::vector<std::string> names;
		bool manifest_found = false;
		size_t manifest_index = 0;

		for (size_t i = 0; i < table.entries.size(); ++i)
		{
			if (table.entries[i].name_hash == manifest_hash)
			{
				manifest_found = true;
				manifest_index = i;
				break;
			}
		}

		if (manifest_found == false)
		{
			out_error =
				"the pack carries no names manifest ('_kpack_names.json') "
				"— mutation must re-emit the kept entries BY NAME through "
				"the shared encoder and a hash is not invertible; repack "
				"from the source folder with `zircon_kpacker pack`";
			goto done;
		}

		{
			std::vector<uint8_t> manifest_raw;

			if (tool_decode_entry(p_file, table.entries[manifest_index],
				    manifest_raw, out_error) == false)
			{
				out_error =
					"the names manifest failed to decode: " + out_error;
				goto done;
			}

			if (tool_parse_names_manifest(manifest_raw, names) == false)
			{
				out_error = "the names manifest is malformed";
				goto done;
			}

			// the manifest lists every entry but itself, in order
			if (names.size() != table.entries.size() - 1)
			{
				out_error =
					"the names manifest disagrees with the entry table "
					"(count mismatch) — the pack was modified outside "
					"the tool";
				goto done;
			}
		}

		{
			size_t name_index = 0;

			for (size_t i = 0; i < table.entries.size(); ++i)
			{
				if (i == manifest_index)
					continue;

				const tool_entry_t& entry = table.entries[i];

				tool_rewrite_entry_t kept;
				kept.compression = entry.compression;

				if (tool_decode_entry(
					    p_file, entry, kept.data, out_error
				    ) == false)
				{
					out_error = "entry '" + names[name_index] +
						"' failed to decode: " + out_error;
					goto done;
				}

				// the name is trusted from the manifest, but prove it
				// addresses the entry it was paired with
				const uint64_t name_hash =
					kun_kotek kun_core kpack_hash_name(
						names[name_index].c_str(),
						names[name_index].size());

				if (name_hash != entry.name_hash)
				{
					out_error = "the names manifest entry '" +
						names[name_index] +
						"' hashes to a different entry — the pack was "
						"modified outside the tool";
					goto done;
				}

				kept.name = std::move(names[name_index]);
				++name_index;

				out_entries.push_back(std::move(kept));
			}
		}

		status = true;

	done:
		fclose(p_file);
		return status;
	}

	void tool_print_size_line(
		const char* p_label, uint64_t raw, uint64_t packed
	)
	{
		if (raw > 0)
		{
			printf(
				"%s%llu raw, %llu packed (%.1f%%)\n", p_label,
				static_cast<unsigned long long>(raw),
				static_cast<unsigned long long>(packed),
				100.0 * static_cast<double>(packed) /
					static_cast<double>(raw));
		}
		else
		{
			printf("%s0 raw, %llu packed\n", p_label,
				static_cast<unsigned long long>(packed));
		}
	}

	// ---------------------------------------------------------------------
	// argument parsing (shared shape: --flag value pairs per command)
	// ---------------------------------------------------------------------
	struct tool_args_t
	{
		std::string command;
		std::vector<std::pair<std::string, std::string>> options;

		bool get(const char* p_name, std::string& out_value) const
		{
			for (const auto& option : this->options)
			{
				if (option.first == p_name)
				{
					out_value = option.second;
					return true;
				}
			}
			return false;
		}

		bool has(const char* p_name) const
		{
			std::string ignored;
			return this->get(p_name, ignored);
		}
	};

	bool tool_parse_args(
		int argc,
		char** argv,
		tool_args_t& out,
		std::string& out_error
	)
	{
		if (argc < 2)
		{
			out_error = "no command";
			return false;
		}

		out.command = argv[1];

		for (int i = 2; i < argc; ++i)
		{
			const char* p_arg = argv[i];

			if (p_arg[0] != '-' || p_arg[1] != '-')
			{
				out_error = std::string("unexpected argument '") + p_arg +
					"' (options are --name value pairs)";
				return false;
			}

			const char* p_name = p_arg + 2;

			// valueless flags
			if (strcmp(p_name, "no-names-manifest") == 0)
			{
				out.options.emplace_back(p_name, "1");
				continue;
			}

			if (i + 1 >= argc)
			{
				out_error =
					std::string("option '--") + p_name + "' needs a value";
				return false;
			}

			out.options.emplace_back(p_name, argv[i + 1]);
			++i;
		}

		return true;
	}

	// ---------------------------------------------------------------------
	// pack
	// ---------------------------------------------------------------------
	struct tool_walk_file_t
	{
		std::filesystem::path disk_path;
		std::string entry_name; // root-relative, forward slashes
		uint64_t size = 0;
	};

	int tool_command_pack(const tool_args_t& args)
	{
		std::string root_text, out_text, order_text, extensions_text,
			compression_text;

		if (args.get("root", root_text) == false ||
		    args.get("out", out_text) == false)
		{
			fprintf(stderr,
				"kpacker: pack needs --root <folder> and --out "
				"<file.kpack>\n");
			return 2;
		}

		eKpackCompression compression = eKpackCompression::kZstd;

		if (args.get("compression", compression_text) &&
		    tool_parse_codec(compression_text.c_str(), compression) ==
			    false)
		{
			fprintf(stderr,
				"kpacker: unknown compression '%s' (stored|zstd|zlib)\n",
				compression_text.c_str());
			return 2;
		}

		const bool with_names_manifest =
			args.has("no-names-manifest") == false;

		std::vector<std::string> extensions;

		if (args.get("extensions", extensions_text))
		{
			size_t begin = 0;

			while (begin <= extensions_text.size())
			{
				const size_t comma =
					extensions_text.find(',', begin);
				const std::string one = extensions_text.substr(
					begin,
					comma == std::string::npos
						? std::string::npos
						: comma - begin);

				if (one.empty() || one[0] != '.')
				{
					fprintf(stderr,
						"kpacker: --extensions entries must start "
						"with a dot ('%s')\n",
						one.c_str());
					return 2;
				}

				extensions.push_back(tool_fold(one));

				if (comma == std::string::npos)
					break;

				begin = comma + 1;
			}
		}

		std::error_code ec;
		const std::filesystem::path root(root_text);

		if (std::filesystem::is_directory(root, ec) == false)
		{
			fprintf(stderr,
				"kpacker: --root '%s' is not an existing directory\n",
				root_text.c_str());
			return 1;
		}

		// bounded recursive walk (the iterator streams; the collected set
		// is capped by the reader's entry cap minus the manifest slot)
		std::vector<tool_walk_file_t> files;
		constexpr size_t k_max_content_entries =
			KOTEK_DEF_FILESYSTEM_PACK_MAX_ENTRIES - 1;

		{
			std::filesystem::recursive_directory_iterator it(
				root,
				std::filesystem::directory_options::
					skip_permission_denied,
				ec);

			if (ec)
			{
				fprintf(stderr,
					"kpacker: cannot walk '%s' (%s)\n",
					root_text.c_str(), ec.message().c_str());
				return 1;
			}

			const std::filesystem::recursive_directory_iterator end;

			for (; it != end; it.increment(ec))
			{
				if (ec)
				{
					fprintf(stderr,
						"kpacker: walk error under '%s' (%s)\n",
						root_text.c_str(), ec.message().c_str());
					return 1;
				}

				if (it->is_regular_file(ec) == false || ec)
				{
					ec.clear();
					continue;
				}

				const std::filesystem::path relative =
					std::filesystem::relative(it->path(), root, ec);

				if (ec)
				{
					fprintf(stderr,
						"kpacker: cannot relativize '%s'\n",
						it->path().string().c_str());
					return 1;
				}

				if (extensions.empty() == false)
				{
					const std::string extension =
						tool_fold(relative.extension().string());

					bool accepted = false;

					for (const std::string& wanted : extensions)
					{
						if (extension == wanted)
						{
							accepted = true;
							break;
						}
					}

					if (accepted == false)
						continue;
				}

				if (files.size() >= k_max_content_entries)
				{
					fprintf(stderr,
						"kpacker: more than %llu content files under "
						"the root — the reader's entry cap is %u (one "
						"slot is reserved for the names manifest); "
						"split the tree into several packs\n",
						static_cast<unsigned long long>(
							k_max_content_entries),
						static_cast<unsigned>(
							KOTEK_DEF_FILESYSTEM_PACK_MAX_ENTRIES));
					return 1;
				}

				tool_walk_file_t file;
				file.disk_path = it->path();
				file.entry_name = relative.generic_string();
				file.size = static_cast<uint64_t>(
					std::filesystem::file_size(it->path(), ec));

				if (ec)
				{
					fprintf(stderr,
						"kpacker: cannot stat '%s'\n",
						it->path().string().c_str());
					return 1;
				}

				files.push_back(std::move(file));
			}
		}

		// a case-only-collision dies here with a clear message (the two
		// names fold to one hash, which the encoder would reject later)
		{
			std::vector<std::string> folded_names;
			folded_names.reserve(files.size());

			for (const tool_walk_file_t& file : files)
				folded_names.push_back(tool_fold(file.entry_name));

			std::sort(folded_names.begin(), folded_names.end());

			for (size_t i = 1; i < folded_names.size(); ++i)
			{
				if (folded_names[i] == folded_names[i - 1])
				{
					fprintf(stderr,
						"kpacker: two files fold to the same entry name "
						"('%s') — case/separator variants are one entry\n",
						folded_names[i].c_str());
					return 1;
				}
			}
		}

		// entry order: the load-order manifest when given, else
		// lexicographic after the fold rule
		if (args.get("order", order_text))
		{
			FILE* p_order = fopen(order_text.c_str(), "rb");

			if (p_order == nullptr)
			{
				fprintf(stderr,
					"kpacker: cannot open the load-order manifest "
					"'%s'\n",
					order_text.c_str());
				return 1;
			}

			_fseeki64(p_order, 0, SEEK_END);
			const __int64 order_size = _ftelli64(p_order);
			_fseeki64(p_order, 0, SEEK_SET);

			std::string order_json;

			if (order_size > 0)
			{
				order_json.resize(static_cast<size_t>(order_size));

				if (fread(order_json.data(), 1, order_json.size(),
					    p_order) != order_json.size())
				{
					fclose(p_order);
					fprintf(stderr,
						"kpacker: cannot read the load-order "
						"manifest '%s'\n",
						order_text.c_str());
					return 1;
				}
			}

			fclose(p_order);

			std::vector<std::string> order_names;
			const char* p_cursor = order_json.c_str();

			if (tool_json_parse_string_array(p_cursor, order_names) ==
			    false)
			{
				fprintf(stderr,
					"kpacker: the load-order manifest '%s' is not a "
					"json array of root-relative paths\n",
					order_text.c_str());
				return 1;
			}

			std::vector<tool_walk_file_t> ordered;
			std::vector<bool> consumed(files.size(), false);

			for (const std::string& wanted : order_names)
			{
				if (wanted.find('\\') != std::string::npos)
				{
					fprintf(stderr,
						"kpacker: the load-order manifest entry '%s' "
						"uses a backslash — paths are forward slashes\n",
						wanted.c_str());
					return 1;
				}

				const std::string folded_wanted = tool_fold(wanted);
				bool matched = false;

				for (size_t i = 0; i < files.size(); ++i)
				{
					if (consumed[i] == false &&
					    tool_fold(files[i].entry_name) ==
						    folded_wanted)
					{
						ordered.push_back(files[i]);
						consumed[i] = true;
						matched = true;
						break;
					}
				}

				if (matched == false)
				{
					fprintf(stderr,
						"kpacker: the load-order manifest entry '%s' "
						"matches no walked file (or is duplicated)\n",
						wanted.c_str());
					return 1;
				}
			}

			// unlisted files keep their place at the tail, in the
			// default lexicographic order (a partial manifest orders
			// what it names, never drops content)
			std::vector<tool_walk_file_t> rest;

			for (size_t i = 0; i < files.size(); ++i)
			{
				if (consumed[i] == false)
					rest.push_back(files[i]);
			}

			std::sort(rest.begin(), rest.end(),
				[](const tool_walk_file_t& left,
				    const tool_walk_file_t& right) {
					return tool_fold(left.entry_name) <
						tool_fold(right.entry_name);
				});

			if (rest.empty() == false)
			{
				fprintf(stderr,
					"kpacker: warning: %llu walked file(s) are not in "
					"the load-order manifest — packed after the listed "
					"ones, lexicographically\n",
					static_cast<unsigned long long>(rest.size()));
			}

			ordered.insert(
				ordered.end(), rest.begin(), rest.end());
			files = std::move(ordered);
		}
		else
		{
			std::sort(files.begin(), files.end(),
				[](const tool_walk_file_t& left,
				    const tool_walk_file_t& right) {
					return tool_fold(left.entry_name) <
						tool_fold(right.entry_name);
				});
		}

		// map every source read-only and hand the views to the encoder —
		// the write is block-streamed, the reads page in per block
		std::vector<tool_mapped_file_t> mappings(files.size());
		std::vector<kpack_writer_entry_t> writer_entries;
		writer_entries.reserve(files.size() + 1);

		for (size_t i = 0; i < files.size(); ++i)
		{
			std::string map_error;

			if (mappings[i].open(files[i].disk_path, map_error) == false)
			{
				fprintf(stderr, "kpacker: %s\n", map_error.c_str());
				return 1;
			}

			writer_entries.push_back(
				{files[i].entry_name.c_str(),
				 mappings[i].size ? mappings[i].p_view : nullptr,
				 static_cast<size_t>(mappings[i].size), compression});
		}

		std::string manifest_json;

		if (with_names_manifest)
		{
			std::vector<std::string> names;
			names.reserve(files.size());

			for (const tool_walk_file_t& file : files)
				names.push_back(file.entry_name);

			manifest_json = tool_emit_names_manifest(names);

			writer_entries.push_back(
				{k_names_manifest_name,
				 reinterpret_cast<const uint8_t*>(manifest_json.data()),
				 manifest_json.size(), eKpackCompression::kZstd});
		}

		const std::filesystem::path out_path(out_text);

		if (out_path.has_parent_path())
		{
			std::filesystem::create_directories(
				out_path.parent_path(), ec);
			ec.clear();
		}

		const bool wrote = kun_kotek kun_core kpack_write_file(
			out_path.string().c_str(), writer_entries.data(),
			writer_entries.size());

		if (wrote == false)
		{
			fprintf(stderr,
				"kpacker: the encoder failed (see the log above)\n");
			return 1;
		}

		uint64_t raw_total = 0;

		for (const tool_walk_file_t& file : files)
			raw_total += file.size;

		const uint64_t packed_total = static_cast<uint64_t>(
			std::filesystem::file_size(out_path, ec));

		printf("packed %llu file(s) -> '%s'\n",
			static_cast<unsigned long long>(files.size()),
			out_path.string().c_str());
		tool_print_size_line("  total: ", raw_total, packed_total);

		if (with_names_manifest)
			printf("  names manifest: embedded as '%s' (zstd, last "
			       "entry)\n",
				k_names_manifest_name);

		return 0;
	}

	// ---------------------------------------------------------------------
	// add / remove (the wholesale-rewrite mutation model)
	// ---------------------------------------------------------------------
	int tool_command_add(const tool_args_t& args)
	{
		std::string pack_text, file_text, name_text, compression_text;

		if (args.get("pack", pack_text) == false ||
		    args.get("file", file_text) == false ||
		    args.get("name", name_text) == false)
		{
			fprintf(stderr,
				"kpacker: add needs --pack <file.kpack> --file <disk "
				"path> --name <entry name>\n");
			return 2;
		}

		if (tool_fold(name_text) == k_names_manifest_name)
		{
			fprintf(stderr,
				"kpacker: '%s' is the reserved names-manifest entry\n",
				name_text.c_str());
			return 1;
		}

		eKpackCompression compression = eKpackCompression::kZstd;

		if (args.get("compression", compression_text) &&
		    tool_parse_codec(compression_text.c_str(), compression) ==
			    false)
		{
			fprintf(stderr,
				"kpacker: unknown compression '%s' (stored|zstd|zlib)\n",
				compression_text.c_str());
			return 2;
		}

		const std::filesystem::path pack_path(pack_text);
		std::vector<tool_rewrite_entry_t> entries;
		std::string error;

		if (tool_read_pack_for_rewrite(pack_path, entries, error) ==
		    false)
		{
			fprintf(stderr, "kpacker: %s\n", error.c_str());
			return 1;
		}

		const uint64_t new_hash = kun_kotek kun_core kpack_hash_name(
			name_text.c_str(), name_text.size());

		for (const tool_rewrite_entry_t& entry : entries)
		{
			if (kun_kotek kun_core kpack_hash_name(
				    entry.name.c_str(), entry.name.size()) ==
			    new_hash)
			{
				fprintf(stderr,
					"kpacker: the entry '%s' already exists in the "
					"pack — remove it first\n",
					name_text.c_str());
				return 1;
			}
		}

		tool_mapped_file_t added;

		if (added.open(std::filesystem::path(file_text), error) == false)
		{
			fprintf(stderr, "kpacker: %s\n", error.c_str());
			return 1;
		}

		tool_rewrite_entry_t new_entry;
		new_entry.name = name_text;
		new_entry.compression = compression;

		if (added.size > 0)
		{
			new_entry.data.resize(static_cast<size_t>(added.size));
			memcpy(new_entry.data.data(), added.p_view,
				static_cast<size_t>(added.size));
		}

		entries.push_back(std::move(new_entry));
		added.close();

		if (tool_write_pack_with_swap(pack_path, entries, true, error) ==
		    false)
		{
			fprintf(stderr, "kpacker: %s\n", error.c_str());
			return 1;
		}

		printf("added '%s' (%llu raw bytes, %s) -> '%s' now holds %llu "
		       "entries\n",
			name_text.c_str(),
			static_cast<unsigned long long>(entries.back().data.size()),
			tool_codec_name(compression), pack_text.c_str(),
			static_cast<unsigned long long>(entries.size()));
		return 0;
	}

	int tool_command_remove(const tool_args_t& args)
	{
		std::string pack_text, name_text;

		if (args.get("pack", pack_text) == false ||
		    args.get("name", name_text) == false)
		{
			fprintf(stderr,
				"kpacker: remove needs --pack <file.kpack> --name "
				"<entry name>\n");
			return 2;
		}

		if (tool_fold(name_text) == k_names_manifest_name)
		{
			fprintf(stderr,
				"kpacker: '%s' is the reserved names-manifest entry\n",
				name_text.c_str());
			return 1;
		}

		const std::filesystem::path pack_path(pack_text);
		std::vector<tool_rewrite_entry_t> entries;
		std::string error;

		if (tool_read_pack_for_rewrite(pack_path, entries, error) ==
		    false)
		{
			fprintf(stderr, "kpacker: %s\n", error.c_str());
			return 1;
		}

		const uint64_t target_hash = kun_kotek kun_core kpack_hash_name(
			name_text.c_str(), name_text.size());

		size_t removed_index = entries.size();

		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (kun_kotek kun_core kpack_hash_name(
				    entries[i].name.c_str(), entries[i].name.size()) ==
			    target_hash)
			{
				removed_index = i;
				break;
			}
		}

		if (removed_index == entries.size())
		{
			fprintf(stderr,
				"kpacker: the entry '%s' is not in the pack\n",
				name_text.c_str());
			return 1;
		}

		entries.erase(entries.begin() + removed_index);

		if (tool_write_pack_with_swap(pack_path, entries, true, error) ==
		    false)
		{
			fprintf(stderr, "kpacker: %s\n", error.c_str());
			return 1;
		}

		printf("removed '%s' — '%s' now holds %llu entries\n",
			name_text.c_str(), pack_text.c_str(),
			static_cast<unsigned long long>(entries.size()));
		return 0;
	}

	// ---------------------------------------------------------------------
	// list
	// ---------------------------------------------------------------------
	int tool_command_list(const tool_args_t& args)
	{
		std::string pack_text;

		if (args.get("pack", pack_text) == false)
		{
			fprintf(stderr,
				"kpacker: list needs --pack <file.kpack>\n");
			return 2;
		}

		tool_table_t table;
		std::string error;

		if (tool_read_pack_table(
			    std::filesystem::path(pack_text), table, error
		    ) == false)
		{
			fprintf(stderr, "kpacker: %s\n", error.c_str());
			return 1;
		}

		// names when the manifest exists (hash-only display otherwise)
		std::vector<std::string> names;
		size_t manifest_index = table.entries.size();
		const uint64_t manifest_hash = kun_kotek kun_core kpack_hash_name(
			k_names_manifest_name, sizeof(k_names_manifest_name) - 1);

		for (size_t i = 0; i < table.entries.size(); ++i)
		{
			if (table.entries[i].name_hash == manifest_hash)
			{
				manifest_index = i;
				break;
			}
		}

		if (manifest_index != table.entries.size())
		{
			FILE* p_file =
				fopen(std::filesystem::path(pack_text).string().c_str(),
					"rb");

			if (p_file)
			{
				std::vector<uint8_t> manifest_raw;
				std::string decode_error;

				if (tool_decode_entry(p_file,
					    table.entries[manifest_index],
					    manifest_raw, decode_error))
				{
					tool_parse_names_manifest(manifest_raw, names);
				}

				fclose(p_file);
			}
		}

		printf("pack: %s (%llu bytes, %llu entries)\n", pack_text.c_str(),
			static_cast<unsigned long long>(table.file_size),
			static_cast<unsigned long long>(table.entries.size()));

		if (names.empty() && manifest_index == table.entries.size())
		{
			printf("  (no names manifest — entries are hash-keyed; the "
			       "columns below show hashes)\n");
		}

		uint64_t raw_total = 0;
		uint64_t packed_total = 0;
		size_t name_index = 0;

		for (size_t i = 0; i < table.entries.size(); ++i)
		{
			const tool_entry_t& entry = table.entries[i];

			std::string display;

			if (i == manifest_index)
			{
				display = k_names_manifest_name;
			}
			else if (name_index < names.size())
			{
				display = names[name_index];
				++name_index;
			}
			else
			{
				char hash_text[24];
				snprintf(hash_text, sizeof(hash_text), "hash=%016llx",
					static_cast<unsigned long long>(
						entry.name_hash));
				display = hash_text;
			}

			printf("  [%3llu] %-40s %-6s raw=%-10llu packed=%-10llu "
			       "blocks=%-5llu offset=%llu\n",
				static_cast<unsigned long long>(i), display.c_str(),
				tool_codec_name(entry.compression),
				static_cast<unsigned long long>(entry.raw_size),
				static_cast<unsigned long long>(entry.packed_size),
				static_cast<unsigned long long>(entry.block_count),
				static_cast<unsigned long long>(entry.data_offset));

			raw_total += entry.raw_size;
			packed_total += entry.packed_size;
		}

		tool_print_size_line("  total: ", raw_total, packed_total);
		return 0;
	}

	// ---------------------------------------------------------------------
	// verify — the CI integrity gate: full structural validation + every
	// block decompresses cleanly + the REAL runtime reader mounts the pack
	// and re-reads every named entry byte-for-byte
	// ---------------------------------------------------------------------
	int tool_command_verify(const tool_args_t& args)
	{
		std::string pack_text;

		if (args.get("pack", pack_text) == false)
		{
			fprintf(stderr,
				"kpacker: verify needs --pack <file.kpack>\n");
			return 2;
		}

		const std::filesystem::path pack_path(pack_text);

		tool_table_t table;
		std::string error;

		if (tool_read_pack_table(pack_path, table, error) == false)
		{
			fprintf(stderr,
				"kpacker: CORRUPT: %s\n", error.c_str());
			return 1;
		}

		FILE* p_file = fopen(pack_path.string().c_str(), "rb");

		if (p_file == nullptr)
		{
			fprintf(stderr,
				"kpacker: cannot open the pack for reading\n");
			return 1;
		}

		uint64_t total_blocks = 0;
		uint64_t raw_total = 0;

		for (size_t i = 0; i < table.entries.size(); ++i)
		{
			std::vector<uint8_t> raw;
			std::string decode_error;

			if (tool_decode_entry(
				    p_file, table.entries[i], raw, decode_error
			    ) == false)
			{
				fclose(p_file);
				fprintf(stderr,
					"kpacker: CORRUPT: entry %llu: %s\n",
					static_cast<unsigned long long>(i),
					decode_error.c_str());
				return 1;
			}

			if (raw.size() != table.entries[i].raw_size)
			{
				fclose(p_file);
				fprintf(stderr,
					"kpacker: CORRUPT: entry %llu decoded to %llu "
					"bytes, the table says %llu\n",
					static_cast<unsigned long long>(i),
					static_cast<unsigned long long>(raw.size()),
					static_cast<unsigned long long>(
						table.entries[i].raw_size));
				return 1;
			}

			total_blocks += table.entries[i].block_count;
			raw_total += table.entries[i].raw_size;
		}

		// the REAL runtime reader: mount-time validation must accept the
		// pack (this is the exact code path the engine boots through)
		kun_kotek kun_core ktkFileSystem_Pack reader;
		reader.Initialize(ktk_filesystem_path{});

		int exit_code = 1;

		if (reader.Mount(ktk_filesystem_path{
			    pack_path.string().c_str()}) == false)
		{
			fprintf(stderr,
				"kpacker: CORRUPT: the runtime reader rejected the "
				"mount\n");
		}
		else if (reader.Get_MountedPackCount() != 1)
		{
			fprintf(stderr,
				"kpacker: CORRUPT: the runtime reader mounted %u "
				"packs, expected 1\n",
				reader.Get_MountedPackCount());
		}
		else
		{
			// per-entry re-read through the runtime reader, cross-checked
			// against the tool's own decode (addressed BY NAME, so it runs
			// when the names manifest is there; without one the mount +
			// block-walk checks above are already the full gate)
			const uint64_t manifest_hash =
				kun_kotek kun_core kpack_hash_name(
					k_names_manifest_name,
					sizeof(k_names_manifest_name) - 1);

			bool has_manifest = false;

			for (size_t i = 0; i < table.entries.size(); ++i)
			{
				if (table.entries[i].name_hash == manifest_hash)
				{
					has_manifest = true;
					break;
				}
			}

			if (has_manifest == false)
			{
				printf("  note: no names manifest — the runtime re-read "
				       "is limited to the mount check\n");
				printf("verify: OK — %llu entries, %llu blocks, %llu raw "
				       "bytes; the runtime reader mounted the pack\n",
					static_cast<unsigned long long>(
						table.entries.size()),
					static_cast<unsigned long long>(total_blocks),
					static_cast<unsigned long long>(raw_total));
				exit_code = 0;
			}
			else
			{
				std::vector<tool_rewrite_entry_t> named_entries;
				std::string rewrite_error;
				bool content_checked = true;

				if (tool_read_pack_for_rewrite(
					    pack_path, named_entries, rewrite_error
				    ) == false)
				{
					// the manifest entry is there but broken (malformed
					// json, count/hash mismatch) — that IS a corrupt
					// pack, not a manifest-less one
					fprintf(stderr,
						"kpacker: CORRUPT: the names manifest: %s\n",
						rewrite_error.c_str());
					content_checked = false;
				}
				else
				{
				for (const tool_rewrite_entry_t& named : named_entries)
				{
					std::vector<uint8_t> re_read(named.data.size() + 1);
					size_t re_read_size = re_read.size();

					const eKpackReadResult read_result = reader.Read_File(
						ktk_filesystem_path{named.name.c_str()},
						re_read.data(), re_read_size);

					if (read_result != eKpackReadResult::kSuccess)
					{
						fprintf(stderr,
							"kpacker: CORRUPT: the runtime reader "
							"failed on '%s'\n",
							named.name.c_str());
						content_checked = false;
						break;
					}

					if (re_read_size != named.data.size() ||
					    (named.data.empty() == false &&
						    memcmp(re_read.data(), named.data.data(),
							    named.data.size()) != 0))
					{
						fprintf(stderr,
							"kpacker: CORRUPT: the runtime reader's "
							"bytes for '%s' differ from the block "
							"walk\n",
							named.name.c_str());
						content_checked = false;
						break;
					}
				}
			}

			if (content_checked)
			{
				printf("verify: OK — %llu entries, %llu blocks, "
				       "%llu raw bytes; the runtime reader mounted the "
				       "pack and re-read every named entry\n",
					static_cast<unsigned long long>(
						table.entries.size()),
					static_cast<unsigned long long>(total_blocks),
					static_cast<unsigned long long>(raw_total));
				exit_code = 0;
			}
		}
	}

		reader.Shutdown();
		fclose(p_file);

		if (exit_code != 0)
			fprintf(stderr, "verify: FAILED\n");

		return exit_code;
	}

	void tool_print_help()
	{
		printf(
			"zircon_kpacker — the .kpack host tool (filesystem plan B2b)\n"
			"\n"
			".kpack v1: entries keyed by a 64-bit fnv1a name hash (names are\n"
			"NOT stored), 64 KB independent compression blocks, 4 KB-aligned\n"
			"data spans in load order. Spec: kotek's kotek_kpack_format.h.\n"
			"\n"
			"usage:\n"
			"  zircon_kpacker pack --root <folder> --out <file.kpack>\n"
			"      [--compression stored|zstd|zlib]  (default: zstd)\n"
			"      [--order <load-order.json>] [--extensions .json,.txt,...]\n"
			"      [--no-names-manifest]\n"
			"  zircon_kpacker add --pack <file.kpack> --file <disk path>\n"
			"      --name <entry name> [--compression stored|zstd|zlib]\n"
			"  zircon_kpacker remove --pack <file.kpack> --name <entry name>\n"
			"  zircon_kpacker list --pack <file.kpack>\n"
			"  zircon_kpacker verify --pack <file.kpack>\n"
			"  zircon_kpacker --help\n"
			"\n"
			"pack: walks <folder> recursively (bounded by the reader's entry\n"
			"  cap) and writes one .kpack through kotek's shared encoder\n"
			"  (kpack_write_file). Entry order (= load order on disk): the\n"
			"  load-order manifest when given — a json ARRAY of root-relative\n"
			"  paths with forward slashes, e.g.\n"
			"    [\"configs/sys_info.json\", \"textures/a.png\"]\n"
			"  listed files first in manifest order, unlisted files appended\n"
			"  lexicographically — else plain lexicographic after the fold\n"
			"  rule ('\\'->'/', case-folded). --extensions keeps only the\n"
			"  listed suffixes (case-insensitive). Source files are\n"
			"  memory-mapped read-only (never copied whole into RAM).\n"
			"\n"
			"names: .kpack stores only name HASHES. The tool embeds a\n"
			"  reserved last entry '_kpack_names.json' (zstd) with the entry\n"
			"  names in order; add/remove REQUIRE it (a rewrite re-emits the\n"
			"  kept entries by name — a hash is not invertible), list/verify\n"
			"  use it when present, packs built with --no-names-manifest are\n"
			"  list/verify-only (list shows hash+offset+sizes+codec).\n"
			"\n"
			"mutation model (OFFLINE): add/remove read the whole entry set\n"
			"  and REWRITE the pack wholesale through the shared encoder\n"
			"  (temp sibling + replace; no in-place surgery). Small packs,\n"
			"  tool-side. The runtime never mutates a shipped pack — editor\n"
			"  saves land in the data_user override dir instead.\n"
			"\n"
			"verify (the CI integrity gate): full structural validation\n"
			"  (magic/flags/tables/offsets/alignment/duplicate hashes),\n"
			"  every block decompressed cleanly (v1 stores no checksums —\n"
			"  clean decompression + size consistency is the gate), the real\n"
			"  runtime reader (ktkFileSystem_Pack) mounts the pack, and every\n"
			"  named entry is re-read through it byte-for-byte.\n"
			"\n"
			"exit codes: 0 ok, 1 operational failure, 2 usage error\n");
	}
} // namespace

int main(int argc, char** argv)
{
	// the console logger for the encoder/reader diagnostics (the tool IS
	// their log module — see the shim at the top of the file; the branch
	// matches the engine tree's log backend)
#ifdef KOTEK_USE_LOG_LIBRARY_SPDLOG
	auto p_kpacker_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
	spdlog::logger kpacker_logger("kpacker", p_kpacker_sink);
	kun_kotek Set_LoggerMain(&kpacker_logger);
#elif defined(KOTEK_USE_LOG_LIBRARY_CUSTOM)
	kun_kotek kun_core ktkCustomLogger tool_logger(nullptr, false);
	kun_kotek Set_LoggerMain(&tool_logger);
#endif

	int exit_code = 2;

	if (argc >= 2 &&
	    (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))
	{
		tool_print_help();
		exit_code = 0;
	}
	else
	{
		tool_args_t args;
		std::string error;

		if (tool_parse_args(argc, argv, args, error) == false)
		{
			fprintf(stderr, "kpacker: %s (try --help)\n", error.c_str());
		}
		else if (args.command == "pack")
		{
			exit_code = tool_command_pack(args);
		}
		else if (args.command == "add")
		{
			exit_code = tool_command_add(args);
		}
		else if (args.command == "remove")
		{
			exit_code = tool_command_remove(args);
		}
		else if (args.command == "list")
		{
			exit_code = tool_command_list(args);
		}
		else if (args.command == "verify")
		{
			exit_code = tool_command_verify(args);
		}
		else
		{
			fprintf(stderr,
				"kpacker: unknown command '%s' (try --help)\n",
				args.command.c_str());
		}
	}

	kun_kotek Set_LoggerMain(nullptr);
	return exit_code;
}
