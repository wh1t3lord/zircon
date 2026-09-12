#include "zircon_embedded_defaults.h"

// ---------------------------------------------------------------------
// the blobs. Namespace-scope constexpr POD — explicitly exempt from the
// no-static-storage rule (house rule 1a's owner decision: no linker
// singleton, no init hazard); read-only for the process lifetime, never
// freed, so handing out their addresses crosses no module/CRT line
// ---------------------------------------------------------------------

namespace
{
	/// 2x2 magenta/black checker, RGBA8, row 0 = magenta,black / row 1 =
	/// black,magenta — the Source-style "your texture is missing" signal
	constexpr kotek::uint8_t k_blob_texture_checker[] = {
		0xFF, 0x00, 0xFF, 0xFF, // magenta (opaque)
		0x00, 0x00, 0x00, 0xFF, // black (opaque)
		0x00, 0x00, 0x00, 0xFF, // black (opaque)
		0xFF, 0x00, 0xFF, 0xFF, // magenta (opaque)
	};

	/// the shader default is a SEMANTIC TOKEN, not a compiled blob — see
	/// the header's shader decision (a real binary is backend-specific and
	/// the backend is a runtime choice; the render side maps this token to
	/// its own built-in unlit-magenta material per backend). Spelled as
	/// explicit bytes: a uint8_t array cannot initialize from a string
	/// literal, and a reinterpret_cast of a char array would not be a
	/// constant expression for the table below. The text is
	/// "ZIRCON_SHADER_DEFAULT_UNLIT_MAGENTA" (35 bytes, no NUL)
	constexpr kotek::uint8_t k_blob_shader_token[] = {
		'Z', 'I', 'R', 'C', 'O', 'N', '_', 'S', 'H', 'A', 'D', 'E', 'R',
		'_', 'D', 'E', 'F', 'A', 'U', 'L', 'T', '_', 'U', 'N', 'L', 'I',
		'T', '_', 'M', 'A', 'G', 'E', 'N', 'T', 'A'};

	/// a canonical RIFF/WAVE header (PCM, mono, 8000 Hz, 16-bit) + 32
	/// zeroed samples (64 bytes of silence): 44 + 64 = 108 bytes, little
	/// endian fields
	constexpr kotek::uint8_t k_blob_audio_silence[] = {
		'R', 'I', 'F', 'F',
		0x64, 0x00, 0x00, 0x00, // chunk size = 36 + data size (100)
		'W', 'A', 'V', 'E',
		'f', 'm', 't', ' ',
		0x10, 0x00, 0x00, 0x00, // fmt chunk size = 16
		0x01, 0x00, // audio format = PCM
		0x01, 0x00, // channels = 1
		0x40, 0x1F, 0x00, 0x00, // sample rate = 8000
		0x80, 0x3E, 0x00, 0x00, // byte rate = 16000
		0x02, 0x00, // block align = 2
		0x10, 0x00, // bits per sample = 16
		'd', 'a', 't', 'a',
		0x40, 0x00, 0x00, 0x00, // data size = 64 (32 samples)
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	/// the json floor: an empty object — parses through every json
	/// consumer and reads as "no keys"
	constexpr kotek::uint8_t k_blob_json_empty[] = {'{', '}'};

	/// the table the accessor hands out, indexed by
	/// eZirconEmbeddedDefaultType (texture metadata zero/kNone for
	/// non-texture kinds)
	constexpr zircon_embedded_default_t k_defaults[static_cast<kotek::size_t>(
		eZirconEmbeddedDefaultType::kCount)] = {
		{eZirconEmbeddedDefaultType::kTexture_Checker_Magenta,
			k_blob_texture_checker,
			static_cast<kotek::uint32_t>(sizeof(k_blob_texture_checker)), 2, 2,
			eZirconEmbeddedTextureFormat::kRGBA8_Unorm},
		{eZirconEmbeddedDefaultType::kShader_Unlit_Magenta,
			k_blob_shader_token,
			static_cast<kotek::uint32_t>(sizeof(k_blob_shader_token)), 0, 0,
			eZirconEmbeddedTextureFormat::kNone},
		{eZirconEmbeddedDefaultType::kAudio_Silence, k_blob_audio_silence,
			static_cast<kotek::uint32_t>(sizeof(k_blob_audio_silence)), 0, 0,
			eZirconEmbeddedTextureFormat::kNone},
		{eZirconEmbeddedDefaultType::kJson_Empty, k_blob_json_empty,
			static_cast<kotek::uint32_t>(sizeof(k_blob_json_empty)), 0, 0,
			eZirconEmbeddedTextureFormat::kNone},
	};

	constexpr const char* k_type_names[static_cast<kotek::size_t>(
		eZirconEmbeddedDefaultType::kCount)] = {
		"texture-checker-magenta",
		"shader-unlit-magenta(token)",
		"audio-silence",
		"json-empty",
	};

	/// fnv1a-64 over the path text with the type byte folded in (the
	/// dedupe key of the loud-once set — one warning per (type, path))
	kotek::uint64_t default_pair_hash(eZirconEmbeddedDefaultType type,
		const kotek::static_path_t& path) noexcept
	{
		kotek::uint64_t hash = 14695981039346656037ULL;

		const char* p_text = path.c_str();

		while (*p_text)
		{
			hash ^= static_cast<kotek::uint64_t>(
				static_cast<unsigned char>(*p_text));
			hash *= 1099511628211ULL;
			++p_text;
		}

		hash ^= static_cast<kotek::uint64_t>(type);
		hash *= 1099511628211ULL;

		return hash;
	}

	const char* default_type_name(eZirconEmbeddedDefaultType type) noexcept
	{
		const kotek::size_t index = static_cast<kotek::size_t>(type);

		if (index < static_cast<kotek::size_t>(
				eZirconEmbeddedDefaultType::kCount))
		{
			return k_type_names[index];
		}

		return "unknown";
	}
} // namespace

const zircon_embedded_default_t& zircon_embedded_defaults::
	get_embedded_default(eZirconEmbeddedDefaultType type) noexcept
{
	const kotek::size_t index = static_cast<kotek::size_t>(type);

	KOTEK_ASSERT(
		index < static_cast<kotek::size_t>(eZirconEmbeddedDefaultType::kCount),
		"unknown embedded default type: {}",
		static_cast<kotek::uint32_t>(type));

	if (index >= static_cast<kotek::size_t>(eZirconEmbeddedDefaultType::kCount))
	{
		// a bad enum is a programmer error, but library code never faults —
		// the json floor is the safe answer (an empty object parses through
		// every consumer)
		return k_defaults[static_cast<kotek::size_t>(
			eZirconEmbeddedDefaultType::kJson_Empty)];
	}

	return k_defaults[index];
}

void zircon_embedded_defaults::log_default_used_once(
	eZirconEmbeddedDefaultType type,
	const kotek::static_path_t& path) noexcept
{
	const kotek::uint64_t pair_hash = default_pair_hash(type, path);

	for (kotek::size_t i = 0; i < this->m_warned_pairs.size(); ++i)
	{
		if (this->m_warned_pairs[i] == pair_hash)
			return;
	}

	if (this->m_warned_pairs.size() < ZIRCON_DEF_EMBEDDED_DEFAULTS_MAX_WARNED)
	{
		this->m_warned_pairs.push_back(pair_hash);

		KOTEK_MESSAGE_WARNING(
			"[embedded-defaults]: resource '{}' resolved to the EMBEDDED "
			"DEFAULT ({}) — missing or unreadable in data_user / data_game / "
			"mounted packs; the default content is in use",
			path, default_type_name(type));
	}
	else if (this->m_warned_overflow_announced == false)
	{
		this->m_warned_overflow_announced = true;

		KOTEK_MESSAGE_WARNING(
			"[embedded-defaults]: the loud-once dedupe set is full ({}) — "
			"further missing resources still resolve to their defaults but "
			"stop logging; raise ZIRCON_DEF_EMBEDDED_DEFAULTS_MAX_WARNED",
			ZIRCON_DEF_EMBEDDED_DEFAULTS_MAX_WARNED);
	}
}

kotek::uint32_t zircon_embedded_defaults::get_warned_count(void
) const noexcept
{
	return static_cast<kotek::uint32_t>(this->m_warned_pairs.size());
}

bool zircon_embedded_defaults::get_warned_overflow_announced(void
) const noexcept
{
	return this->m_warned_overflow_announced;
}

bool zircon_resolve_or_default(
	kotek::core::ktkIFileSystem* p_filesystem,
	zircon_embedded_defaults& defaults,
	const kotek::static_path_t& path,
	eZirconEmbeddedDefaultType type,
	kotek::uint8_t* p_buffer,
	kotek::size_t& in_out_buffer_size,
	zircon_resolved_or_default_t& out_result) noexcept
{
	KOTEK_ASSERT(p_filesystem, "passed a null filesystem");
	KOTEK_ASSERT(p_buffer, "passed a null buffer");
	KOTEK_ASSERT(
		static_cast<kotek::size_t>(type) <
			static_cast<kotek::size_t>(eZirconEmbeddedDefaultType::kCount),
		"unknown embedded default type: {}",
		static_cast<kotek::uint32_t>(type));

	if (p_filesystem == nullptr || p_buffer == nullptr)
		return false;

	// the chain: the dispatcher walks data_user/data_game/mounted packs per
	// the priority machinery (kAuto) — this file reimplements none of it
	kotek::uint8_t* p_read = p_buffer;
	kotek::size_t read_size = in_out_buffer_size;

	if (p_filesystem->Read_File(path, p_read, read_size))
	{
		in_out_buffer_size = read_size;
		out_result.m_p_bytes = p_buffer;
		out_result.m_size = read_size;
		out_result.m_is_default = false;
		return true;
	}

	if (read_size != 0)
	{
		// false with a non-zero size is the filesystem's too-small contract:
		// a REAL file exists and does not fit the caller's buffer — that is a
		// caller error (retry bigger), never a default case
		in_out_buffer_size = read_size;

		KOTEK_MESSAGE_WARNING(
			"[embedded-defaults]: '{}' exists but needs {} bytes (buffer too "
			"small) — NOT resolving to the default",
			path, read_size);

		return false;
	}

	// nothing resolved on the chain (size 0 + false is the missing/unreadable
	// contract) — the embedded default answers, loudly the first time
	const zircon_embedded_default_t& blob =
		zircon_embedded_defaults::get_embedded_default(type);

	defaults.log_default_used_once(type, path);

	in_out_buffer_size = blob.m_size;
	out_result.m_p_bytes = blob.m_p_bytes;
	out_result.m_size = blob.m_size;
	out_result.m_is_default = true;

	return true;
}
