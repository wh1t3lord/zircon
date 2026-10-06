#include "zircon_texture_bcn.h"

#include <kotek.core.defines_dependent.message/include/kotek_core_defines_dependent_message.h>

// std::memcpy in the pure-POD codec kernels (the chunk-pool / CSG-bake
// precedent: plain C math inside the pure-POD kernels); new[]/delete[]
// for the bounded downsample scratch (the house heap idiom for MB-scale
// scratch — the CSG bake's context/arena precedent)
#include <cstring>
#include <new>

// ---------------------------------------------------------------------------
// zircon_texture_bcn.cpp — the BCn texture bake + parse drivers (task
// Z24 phase B4). The format contract lives in zircon_texture_bcn.h (the
// single source of truth); this file implements it: the source
// validation, the box-filter mip chain, the build (header + table +
// injected per-level encode), the load-side parse, the entry-name and
// .zraw intake helpers and the boot-probe recipe. Pure content: no
// filesystem, no kpack, no render, no vendored-encoder references (the
// encoder is the injected fnptr — see the header banner). No statics
// (rule 1a).
// ---------------------------------------------------------------------------

namespace
{
	constexpr char k_log_prefix[] = "[texture_bcn]";

	// the file-name rule shared by the entry-name segments (the locale
	// language-tag / CSG-bake discipline): 1..max_length of
	// [a-zA-Z0-9_-] — anything else (incl. every separator) is a path
	// walk / invalid content
	bool texture_bcn_is_name_segment_valid(
		const char* p_name, kotek::uint32_t max_length) noexcept
	{
		if (p_name == nullptr)
			return false;

		kotek::uint32_t length = 0;

		for (const char* p = p_name; *p != '\0'; ++p)
		{
			const char symbol = *p;

			const bool valid =
				(symbol >= 'a' && symbol <= 'z') ||
				(symbol >= 'A' && symbol <= 'Z') ||
				(symbol >= '0' && symbol <= '9') || symbol == '_' ||
				symbol == '-';

			if (valid == false)
				return false;

			++length;

			if (length > max_length)
				return false;
		}

		return length > 0;
	}

	// gathers the rejected rule's log line and the status in one place
	eZirconTextureBcnStatus texture_bcn_reject(
		eZirconTextureBcnStatus status, const char* p_reason) noexcept
	{
		KOTEK_MESSAGE_ERROR("{} bake rejected: {}", k_log_prefix, p_reason);
		return status;
	}
} // namespace

eZirconTextureBcnStatus zircon_texture_bcn_validate_source(
	kotek::uint32_t width, kotek::uint32_t height,
	eZirconTextureBcnFormat format, kotek::uint32_t mip_count) noexcept
{
	if (format >= eZirconTextureBcnFormat::kEndOfEnum)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kInvalidArgument,
			"the format enum is out of range");
	}

	if (width < ZIRCON_DEF_TEXTURE_BCN_MIN_DIMENSION ||
		height < ZIRCON_DEF_TEXTURE_BCN_MIN_DIMENSION ||
		width > ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION ||
		height > ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION)
	{
		return texture_bcn_reject(
			eZirconTextureBcnStatus::kDimensionOutOfRange,
			"a dimension is outside [4, 4096]");
	}

	if ((width % 4) != 0 || (height % 4) != 0)
	{
		return texture_bcn_reject(
			eZirconTextureBcnStatus::kDimensionNotMultipleOf4,
			"a dimension is not a multiple of 4 (the v1 dimension rule)");
	}

	const kotek::uint32_t full_count =
		zircon_texture_bcn_full_mip_count(width, height);

	const kotek::uint32_t resolved_mip_count =
		mip_count == 0 ? full_count : mip_count;

	if (resolved_mip_count < 1 || resolved_mip_count > full_count ||
		resolved_mip_count > ZIRCON_DEF_TEXTURE_BCN_MAX_MIP_COUNT)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kMipCountInvalid,
			"the mip count is past the source's full chain (or the "
			"format cap)");
	}

	if (zircon_texture_bcn_total_payload_bytes(
			width, height, format, resolved_mip_count) >
		ZIRCON_DEF_TEXTURE_BCN_MAX_PAYLOAD_BYTES)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kPayloadOverCap,
			"the block payload exceeds the 24 MB format cap");
	}

	return eZirconTextureBcnStatus::kSuccess;
}

void zircon_texture_bcn_downsample_box(const kotek::uint8_t* p_src,
	kotek::uint32_t width, kotek::uint32_t height,
	kotek::uint8_t* p_dst) noexcept
{
	const kotek::uint32_t dst_width =
		zircon_texture_bcn_mip_dimension(width, 1);
	const kotek::uint32_t dst_height =
		zircon_texture_bcn_mip_dimension(height, 1);

	for (kotek::uint32_t y = 0; y < dst_height; ++y)
	{
		// the 2x2 taps, edge-clamped (the clamp only engages on
		// non-bake-legal odd sources — bake-legal multiples of 4 stay
		// even down to 2x2)
		const kotek::uint32_t src_y0 = 2 * y;
		const kotek::uint32_t src_y1 =
			(src_y0 + 1) < height ? (src_y0 + 1) : (height - 1);

		for (kotek::uint32_t x = 0; x < dst_width; ++x)
		{
			const kotek::uint32_t src_x0 = 2 * x;
			const kotek::uint32_t src_x1 =
				(src_x0 + 1) < width ? (src_x0 + 1) : (width - 1);

			const kotek::uint8_t* p_tap00 =
				p_src + (src_y0 * width + src_x0) * 4;
			const kotek::uint8_t* p_tap01 =
				p_src + (src_y0 * width + src_x1) * 4;
			const kotek::uint8_t* p_tap10 =
				p_src + (src_y1 * width + src_x0) * 4;
			const kotek::uint8_t* p_tap11 =
				p_src + (src_y1 * width + src_x1) * 4;

			kotek::uint8_t* p_out = p_dst + (y * dst_width + x) * 4;

			for (kotek::uint32_t channel = 0; channel < 4; ++channel)
			{
				// integer round-nearest: (a + b + c + d + 2) >> 2 —
				// deterministic on every platform (no FPU steps)
				const kotek::uint32_t sum =
					static_cast<kotek::uint32_t>(p_tap00[channel]) +
					static_cast<kotek::uint32_t>(p_tap01[channel]) +
					static_cast<kotek::uint32_t>(p_tap10[channel]) +
					static_cast<kotek::uint32_t>(p_tap11[channel]);

				p_out[channel] =
					static_cast<kotek::uint8_t>((sum + 2u) >> 2);
			}
		}
	}
}

eZirconTextureBcnStatus zircon_texture_bcn_build(
	const kotek::uint8_t* p_rgba_pixels, kotek::uint32_t width,
	kotek::uint32_t height, eZirconTextureBcnFormat format,
	eZirconTextureBcnClass content_class, kotek::uint32_t mip_count,
	kotek::uint8_t encoder_quality_tier,
	zircon_texture_bcn_encode_level_fn p_encode, void* p_encode_user,
	kotek::uint8_t* p_out_file_bytes, kotek::size_t out_capacity,
	kotek::size_t& out_file_size) noexcept
{
	out_file_size = 0;

	if (p_rgba_pixels == nullptr || p_out_file_bytes == nullptr ||
		p_encode == nullptr || content_class >= eZirconTextureBcnClass::kEndOfEnum)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kInvalidArgument,
			"a null source/output/encoder or a bad content class");
	}

	const eZirconTextureBcnStatus validation =
		zircon_texture_bcn_validate_source(width, height, format, mip_count);

	if (validation != eZirconTextureBcnStatus::kSuccess)
		return validation;

	const kotek::uint32_t resolved_mip_count =
		mip_count == 0 ? zircon_texture_bcn_full_mip_count(width, height)
					   : mip_count;

	const kotek::uint32_t payload_size = zircon_texture_bcn_total_payload_bytes(
		width, height, format, resolved_mip_count);

	const kotek::uint32_t required_size = zircon_texture_bcn_header_size +
		resolved_mip_count * zircon_texture_bcn_mip_record_size +
		payload_size;

	if (out_capacity < required_size)
	{
		out_file_size = required_size;

		KOTEK_MESSAGE_ERROR(
			"{} bake: the output buffer is too small ({} < {} — the "
			"required size is reported)",
			k_log_prefix, out_capacity, required_size);

		return eZirconTextureBcnStatus::kBufferTooSmall;
	}

	// ---- the header
	kotek::uint8_t* p_header = p_out_file_bytes;

	std::memcpy(p_header, zircon_texture_bcn_magic, 8);
	zircon_csg_bake_store_u32(p_header + 8, width);
	zircon_csg_bake_store_u32(p_header + 12, height);
	p_header[16] = static_cast<kotek::uint8_t>(format);
	p_header[17] = static_cast<kotek::uint8_t>(resolved_mip_count);
	zircon_csg_bake_store_u16(p_header + 18, 0);
	zircon_csg_bake_store_u32(p_header + 20, payload_size);
	p_header[24] = static_cast<kotek::uint8_t>(content_class);
	p_header[25] = encoder_quality_tier;
	std::memset(p_header + 26, 0, 6);

	kotek::uint8_t* p_mip_table =
		p_out_file_bytes + zircon_texture_bcn_header_size;
	kotek::uint8_t* p_payload = p_mip_table +
		resolved_mip_count * zircon_texture_bcn_mip_record_size;

	// the downsample ping-pong scratch (levels 1+ only — level 0 encodes
	// the source directly). Both buffers take the LEVEL-1 size: every
	// deeper level is smaller, so the pair never grows
	const kotek::uint32_t level1_width =
		zircon_texture_bcn_mip_dimension(width, 1);
	const kotek::uint32_t level1_height =
		zircon_texture_bcn_mip_dimension(height, 1);
	const kotek::size_t scratch_bytes =
		static_cast<kotek::size_t>(level1_width) * level1_height * 4u;

	kotek::uint8_t* p_level_a = nullptr;
	kotek::uint8_t* p_level_b = nullptr;

	if (resolved_mip_count > 1)
	{
		p_level_a = new (std::nothrow) kotek::uint8_t[scratch_bytes];
		p_level_b = new (std::nothrow) kotek::uint8_t[scratch_bytes];

		if (p_level_a == nullptr || p_level_b == nullptr)
		{
			delete[] p_level_a;
			delete[] p_level_b;

			return texture_bcn_reject(
				eZirconTextureBcnStatus::kInvalidArgument,
				"the downsample scratch allocation failed");
		}
	}

	kotek::uint32_t payload_offset = 0;
	eZirconTextureBcnStatus status = eZirconTextureBcnStatus::kSuccess;

	for (kotek::uint32_t level = 0; level < resolved_mip_count; ++level)
	{
		const kotek::uint32_t level_width =
			zircon_texture_bcn_mip_dimension(width, level);
		const kotek::uint32_t level_height =
			zircon_texture_bcn_mip_dimension(height, level);

		// the level's source pixels
		const kotek::uint8_t* p_level_pixels = p_rgba_pixels;

		if (level == 1)
		{
			zircon_texture_bcn_downsample_box(
				p_rgba_pixels, width, height, p_level_a);
			p_level_pixels = p_level_a;
		}
		else if (level > 1)
		{
			// ping-pong: the previous level sits in one buffer, the
			// next is written into the other (level 2 reads A writes B,
			// level 3 reads B writes A, ...)
			kotek::uint8_t* p_prev =
				(level % 2 == 0) ? p_level_a : p_level_b;
			kotek::uint8_t* p_next =
				(level % 2 == 0) ? p_level_b : p_level_a;

			const kotek::uint32_t prev_width =
				zircon_texture_bcn_mip_dimension(width, level - 1);
			const kotek::uint32_t prev_height =
				zircon_texture_bcn_mip_dimension(height, level - 1);

			zircon_texture_bcn_downsample_box(
				p_prev, prev_width, prev_height, p_next);
			p_level_pixels = p_next;
		}

		const kotek::uint32_t level_bytes =
			zircon_texture_bcn_level_block_bytes(
				level_width, level_height, format);

		// the mip record (payload-relative — the table is file-position
		// independent)
		kotek::uint8_t* p_record =
			p_mip_table + level * zircon_texture_bcn_mip_record_size;
		zircon_csg_bake_store_u32(p_record, payload_offset);
		zircon_csg_bake_store_u32(p_record + 4, level_bytes);

		const bool encoded = p_encode(p_encode_user, p_level_pixels,
			level_width, level_height, format, p_payload + payload_offset,
			level_bytes);

		if (encoded == false)
		{
			KOTEK_MESSAGE_ERROR(
				"{} bake: the injected encoder failed on mip level {} "
				"({}x{}, format {})",
				k_log_prefix, level, level_width, level_height,
				static_cast<kotek::uint32_t>(format));

			status = eZirconTextureBcnStatus::kEncodeFailed;
			break;
		}

		payload_offset += level_bytes;
	}

	delete[] p_level_a;
	delete[] p_level_b;

	if (status != eZirconTextureBcnStatus::kSuccess)
		return status;

	out_file_size = required_size;

	KOTEK_MESSAGE(
		"{} baked: {}x{} format {} class {} mips {} -> {} block bytes "
		"({} file bytes)",
		k_log_prefix, width, height,
		static_cast<kotek::uint32_t>(format),
		static_cast<kotek::uint32_t>(content_class), resolved_mip_count,
		payload_size, required_size);

	return eZirconTextureBcnStatus::kSuccess;
}

eZirconTextureBcnStatus zircon_texture_bcn_parse(
	const kotek::uint8_t* p_file_bytes, kotek::size_t file_size,
	zircon_texture_bcn_desc_t& out_desc) noexcept
{
	if (p_file_bytes == nullptr)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kInvalidArgument,
			"parse: null bytes");
	}

	if (file_size < zircon_texture_bcn_header_size)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kCorruptData,
			"parse: the file is shorter than the 32-byte header");
	}

	if (std::memcmp(p_file_bytes, zircon_texture_bcn_magic, 8) != 0)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kCorruptData,
			"parse: the magic is not ZBCN01 (a wrong-version or foreign "
			"file)");
	}

	const kotek::uint32_t width =
		zircon_csg_bake_load_u32(p_file_bytes + 8);
	const kotek::uint32_t height =
		zircon_csg_bake_load_u32(p_file_bytes + 12);
	const kotek::uint8_t format_value = p_file_bytes[16];
	const kotek::uint8_t mip_count = p_file_bytes[17];
	const kotek::uint32_t payload_size =
		zircon_csg_bake_load_u32(p_file_bytes + 20);

	if (format_value >=
		static_cast<kotek::uint8_t>(eZirconTextureBcnFormat::kEndOfEnum))
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kCorruptData,
			"parse: the format field is out of range");
	}

	const eZirconTextureBcnFormat format =
		static_cast<eZirconTextureBcnFormat>(format_value);

	// the same rules the bake enforces (a hand-crafted file is held to
	// the bake's contract)
	if (width < ZIRCON_DEF_TEXTURE_BCN_MIN_DIMENSION ||
		height < ZIRCON_DEF_TEXTURE_BCN_MIN_DIMENSION ||
		width > ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION ||
		height > ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION ||
		(width % 4) != 0 || (height % 4) != 0)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kCorruptData,
			"parse: the dimensions break the bake's dimension rule");
	}

	const kotek::uint32_t full_count =
		zircon_texture_bcn_full_mip_count(width, height);

	if (mip_count < 1 || mip_count > full_count ||
		mip_count > ZIRCON_DEF_TEXTURE_BCN_MAX_MIP_COUNT)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kCorruptData,
			"parse: the mip count is past the full chain");
	}

	const kotek::uint32_t expected_payload =
		zircon_texture_bcn_total_payload_bytes(
			width, height, format, mip_count);

	if (payload_size != expected_payload ||
		payload_size > ZIRCON_DEF_TEXTURE_BCN_MAX_PAYLOAD_BYTES)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kCorruptData,
			"parse: the payload size disagrees with the dimensions/"
			"format/mip count");
	}

	const kotek::uint32_t table_bytes =
		mip_count * zircon_texture_bcn_mip_record_size;

	const kotek::uint64_t expected_file_size =
		static_cast<kotek::uint64_t>(zircon_texture_bcn_header_size) +
		table_bytes + payload_size;

	if (file_size != expected_file_size)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kCorruptData,
			"parse: the file size is not header + table + payload "
			"(trailing garbage or a truncated file)");
	}

	// the table must tile the payload exactly: sequential offsets, in
	// bounds, summing to payload_size
	const kotek::uint8_t* p_table =
		p_file_bytes + zircon_texture_bcn_header_size;

	kotek::uint32_t expected_offset = 0;

	for (kotek::uint32_t level = 0; level < mip_count; ++level)
	{
		const kotek::uint32_t offset =
			zircon_csg_bake_load_u32(p_table + level * 8);
		const kotek::uint32_t size =
			zircon_csg_bake_load_u32(p_table + level * 8 + 4);

		const kotek::uint32_t expected_level_size =
			zircon_texture_bcn_level_block_bytes(
				zircon_texture_bcn_mip_dimension(width, level),
				zircon_texture_bcn_mip_dimension(height, level), format);

		if (offset != expected_offset || size != expected_level_size)
		{
			return texture_bcn_reject(
				eZirconTextureBcnStatus::kCorruptData,
				"parse: a mip record disagrees with the level layout");
		}

		expected_offset += size;
	}

	out_desc.m_width = width;
	out_desc.m_height = height;
	out_desc.m_format = format;
	out_desc.m_content_class =
		static_cast<eZirconTextureBcnClass>(p_file_bytes[24]);
	out_desc.m_mip_count = mip_count;
	out_desc.m_encoder_quality_tier = p_file_bytes[25];
	out_desc.m_payload_size = payload_size;
	out_desc.m_mip_table_file_offset = zircon_texture_bcn_header_size;
	out_desc.m_payload_file_offset =
		zircon_texture_bcn_header_size + table_bytes;

	return eZirconTextureBcnStatus::kSuccess;
}

bool zircon_texture_bcn_read_mip_record(const kotek::uint8_t* p_file_bytes,
	const zircon_texture_bcn_desc_t& desc, kotek::uint32_t mip_index,
	kotek::uint32_t& out_payload_offset, kotek::uint32_t& out_size) noexcept
{
	if (p_file_bytes == nullptr || mip_index >= desc.m_mip_count)
	{
		KOTEK_MESSAGE_ERROR(
			"{} read_mip_record: null bytes or an index past the chain "
			"({} vs {})",
			k_log_prefix, mip_index,
			static_cast<kotek::uint32_t>(desc.m_mip_count));
		return false;
	}

	const kotek::uint8_t* p_record =
		p_file_bytes + desc.m_mip_table_file_offset +
		mip_index * zircon_texture_bcn_mip_record_size;

	out_payload_offset = zircon_csg_bake_load_u32(p_record);
	out_size = zircon_csg_bake_load_u32(p_record + 4);

	return true;
}

eZirconTextureBcnStatus zircon_texture_bcn_make_entry_name(
	const char* p_scene_name, const char* p_texture_name,
	kotek::static_cstring_t<ZIRCON_DEF_TEXTURE_BCN_ENTRY_NAME_MAX_LENGTH>&
		out_entry_name) noexcept
{
	if (texture_bcn_is_name_segment_valid(p_scene_name,
			ZIRCON_DEF_TEXTURE_BCN_MAX_SCENE_NAME_LENGTH) == false)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kNameInvalid,
			"the scene segment breaks the file-name rule (a path walk or "
			"a bad character)");
	}

	if (texture_bcn_is_name_segment_valid(p_texture_name,
			ZIRCON_DEF_TEXTURE_BCN_MAX_TEXTURE_NAME_LENGTH) == false)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kNameInvalid,
			"the texture-name segment breaks the file-name rule");
	}

	out_entry_name = "textures/";
	out_entry_name += p_scene_name;
	out_entry_name += "/";
	out_entry_name += p_texture_name;
	out_entry_name += ".bcn";

	return eZirconTextureBcnStatus::kSuccess;
}

eZirconTextureBcnStatus zircon_texture_bcn_zraw_parse(
	const kotek::uint8_t* p_zraw_bytes, kotek::size_t zraw_size,
	kotek::uint32_t& out_width, kotek::uint32_t& out_height,
	kotek::uint32_t& out_pixel_offset) noexcept
{
	if (p_zraw_bytes == nullptr)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kInvalidArgument,
			"zraw parse: null bytes");
	}

	if (zraw_size < zircon_texture_bcn_zraw_header_size ||
		zraw_size > ZIRCON_DEF_TEXTURE_BCN_ZRAW_MAX_BYTES)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kCorruptData,
			"zraw parse: the size is outside the format's bounds");
	}

	if (std::memcmp(p_zraw_bytes, zircon_texture_bcn_zraw_magic, 8) != 0)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kCorruptData,
			"zraw parse: the magic is not ZRAW01");
	}

	const kotek::uint32_t width =
		zircon_csg_bake_load_u32(p_zraw_bytes + 8);
	const kotek::uint32_t height =
		zircon_csg_bake_load_u32(p_zraw_bytes + 12);

	if (width < 1 || height < 1 ||
		width > ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION ||
		height > ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kCorruptData,
			"zraw parse: a dimension is outside [1, 4096]");
	}

	const kotek::uint64_t expected_size =
		static_cast<kotek::uint64_t>(zircon_texture_bcn_zraw_file_bytes(
			width, height));

	if (zraw_size != expected_size)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kCorruptData,
			"zraw parse: the file size is not 16 + width*height*4");
	}

	out_width = width;
	out_height = height;
	out_pixel_offset = zircon_texture_bcn_zraw_header_size;

	return eZirconTextureBcnStatus::kSuccess;
}

eZirconTextureBcnStatus zircon_texture_bcn_zraw_write(
	const kotek::uint8_t* p_rgba_pixels, kotek::uint32_t width,
	kotek::uint32_t height, kotek::uint8_t* p_out_bytes,
	kotek::size_t out_capacity, kotek::size_t& out_size) noexcept
{
	out_size = 0;

	if (p_rgba_pixels == nullptr || p_out_bytes == nullptr || width < 1 ||
		height < 1 || width > ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION ||
		height > ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION)
	{
		return texture_bcn_reject(eZirconTextureBcnStatus::kInvalidArgument,
			"zraw write: null args or a dimension outside [1, 4096]");
	}

	const kotek::uint32_t required =
		zircon_texture_bcn_zraw_file_bytes(width, height);

	if (out_capacity < required)
	{
		out_size = required;
		return eZirconTextureBcnStatus::kBufferTooSmall;
	}

	std::memcpy(p_out_bytes, zircon_texture_bcn_zraw_magic, 8);
	zircon_csg_bake_store_u32(p_out_bytes + 8, width);
	zircon_csg_bake_store_u32(p_out_bytes + 12, height);
	std::memcpy(p_out_bytes + zircon_texture_bcn_zraw_header_size,
		p_rgba_pixels,
		static_cast<kotek::size_t>(width) * height * 4u);

	out_size = required;

	return eZirconTextureBcnStatus::kSuccess;
}

void zircon_texture_bcn_make_boot_checker_rgba(
	kotek::uint8_t* p_out_rgba_pixels) noexcept
{
	// the embedded-default theme (zircon_embedded_defaults.h): magenta
	// vs near-black in 8-texel cells — a missing/placeholder texture is
	// meant to be VISIBLE
	constexpr kotek::uint8_t k_magenta[4] = {255, 0, 255, 255};
	constexpr kotek::uint8_t k_black[4] = {16, 16, 16, 255};

	for (kotek::uint32_t y = 0; y < ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_SIZE;
		++y)
	{
		for (kotek::uint32_t x = 0;
			x < ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_SIZE; ++x)
		{
			const kotek::uint32_t cell_x =
				x / ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_CELL;
			const kotek::uint32_t cell_y =
				y / ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_CELL;

			const kotek::uint8_t* p_color =
				((cell_x + cell_y) & 1u) ? k_magenta : k_black;

			kotek::uint8_t* p_out = p_out_rgba_pixels +
				(y * ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_SIZE + x) * 4;

			p_out[0] = p_color[0];
			p_out[1] = p_color[1];
			p_out[2] = p_color[2];
			p_out[3] = p_color[3];
		}
	}
}
