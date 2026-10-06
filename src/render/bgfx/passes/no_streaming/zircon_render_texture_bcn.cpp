#include "zircon_render_texture_bcn.h"

#include <kotek.core.api/include/kotek_api.h>

// ---------------------------------------------------------------------------
// zircon_render_texture_bcn.cpp — the render-side BCn upload path (task
// Z24 B4). The no-decode contract: prepare reads + validates (never
// touches a block), create hands the blocks to bgfx AS-IS.
// ---------------------------------------------------------------------------

namespace no_streaming
{
	bgfx::TextureFormat::Enum zircon_render_texture_bcn_map_format(
		eZirconTextureBcnFormat format) noexcept
	{
		switch (format)
		{
		case eZirconTextureBcnFormat::kBC1:
			return bgfx::TextureFormat::BC1;
		case eZirconTextureBcnFormat::kBC3:
			return bgfx::TextureFormat::BC3;
		case eZirconTextureBcnFormat::kBC5:
			return bgfx::TextureFormat::BC5;
		case eZirconTextureBcnFormat::kBC7:
			return bgfx::TextureFormat::BC7;
		case eZirconTextureBcnFormat::kBC6H:
			return bgfx::TextureFormat::BC6H;
		default:
			return bgfx::TextureFormat::Unknown;
		}
	}

	bool zircon_render_texture_bcn_prepare(
		kotek::core::ktkIFileSystem* p_filesystem,
		const kotek::static_path_t& entry_path_relative_to_content_root,
		kotek::uint8_t* p_scratch, kotek::size_t scratch_capacity,
		zircon_render_texture_bcn_upload_t& out_upload) noexcept
	{
		if (p_filesystem == nullptr || p_scratch == nullptr)
		{
			KOTEK_MESSAGE_ERROR(
				"[texture_bcn] prepare: null filesystem or scratch");
			return false;
		}

		// the content-root resolution (cwd-independent, the folder
		// doctrine — game assets live under data_game/)
		kotek::static_path_t entry_path;
		p_filesystem->Make_Path(entry_path,
			kotek::core::eFolderIndex::kFolderIndex_DataGame);
		entry_path /= entry_path_relative_to_content_root;

		kotek::size_t file_size = 0;

		if (p_filesystem->Get_FileSize(entry_path, file_size) == false)
		{
			KOTEK_MESSAGE_WARNING(
				"[texture_bcn] prepare: '{}' does not resolve through the "
				"dispatcher (no pack entry, no native file)",
				entry_path_relative_to_content_root.c_str());
			return false;
		}

		if (file_size < zircon_texture_bcn_header_size ||
			file_size > ZIRCON_DEF_TEXTURE_BCN_MAX_PAYLOAD_BYTES +
				zircon_texture_bcn_header_size +
				ZIRCON_DEF_TEXTURE_BCN_MAX_MIP_COUNT *
					zircon_texture_bcn_mip_record_size)
		{
			KOTEK_MESSAGE_ERROR(
				"[texture_bcn] prepare: '{}' size {} breaks the format's "
				"bounds",
				entry_path_relative_to_content_root.c_str(),
				static_cast<kotek::uint32_t>(file_size));
			return false;
		}

		if (scratch_capacity < file_size)
		{
			KOTEK_MESSAGE_ERROR(
				"[texture_bcn] prepare: the scratch is too small for '{}' "
				"({} < {} bytes)",
				entry_path_relative_to_content_root.c_str(),
				static_cast<kotek::uint32_t>(scratch_capacity),
				static_cast<kotek::uint32_t>(file_size));
			return false;
		}

		kotek::size_t read_size = scratch_capacity;

		if (p_filesystem->Read_File(entry_path, p_scratch, read_size) ==
				false ||
			read_size != file_size)
		{
			KOTEK_MESSAGE_ERROR(
				"[texture_bcn] prepare: the dispatcher read of '{}' "
				"failed ({} of {} bytes)",
				entry_path_relative_to_content_root.c_str(),
				static_cast<kotek::uint32_t>(read_size),
				static_cast<kotek::uint32_t>(file_size));
			return false;
		}

		const eZirconTextureBcnStatus parsed =
			zircon_texture_bcn_parse(p_scratch, file_size,
				out_upload.m_desc);

		if (parsed != eZirconTextureBcnStatus::kSuccess)
		{
			KOTEK_MESSAGE_ERROR(
				"[texture_bcn] prepare: '{}' failed the format parse "
				"(status {} — the parse's line above names the rule)",
				entry_path_relative_to_content_root.c_str(),
				static_cast<kotek::uint32_t>(parsed));
			return false;
		}

		out_upload.m_width =
			static_cast<kotek::uint16_t>(out_upload.m_desc.m_width);
		out_upload.m_height =
			static_cast<kotek::uint16_t>(out_upload.m_desc.m_height);
		out_upload.m_mip_count = out_upload.m_desc.m_mip_count;
		out_upload.m_bgfx_format =
			zircon_render_texture_bcn_map_format(out_upload.m_desc.m_format);
		out_upload.m_p_payload =
			p_scratch + out_upload.m_desc.m_payload_file_offset;
		out_upload.m_payload_size = out_upload.m_desc.m_payload_size;
		out_upload.m_p_file_bytes = p_scratch;

		return true;
	}

	bgfx::TextureHandle zircon_render_texture_bcn_create(
		const zircon_render_texture_bcn_upload_t& upload) noexcept
	{
		if (upload.m_p_payload == nullptr ||
			upload.m_bgfx_format == bgfx::TextureFormat::Unknown ||
			upload.m_mip_count == 0)
		{
			KOTEK_MESSAGE_ERROR(
				"[texture_bcn] create: an unprepared/invalid upload "
				"descriptor");
			return BGFX_INVALID_HANDLE;
		}

		// bgfx sizes a hasMips=true texture as the FULL chain — a
		// partial-chain file cannot upload through this path today
		// (the bake defaults to full; partial is future streaming work)
		if (upload.m_mip_count !=
			zircon_texture_bcn_full_mip_count(
				upload.m_desc.m_width, upload.m_desc.m_height))
		{
			KOTEK_MESSAGE_ERROR(
				"[texture_bcn] create: a partial mip chain ({} of {}) is "
				"not uploadable through bgfx's hasMips path — rebake "
				"with the full chain",
				static_cast<kotek::uint32_t>(upload.m_mip_count),
				zircon_texture_bcn_full_mip_count(
					upload.m_desc.m_width, upload.m_desc.m_height));
			return BGFX_INVALID_HANDLE;
		}

		// THE no-decode upload: the blocks travel to the GPU untouched
		// (bgfx::copy hands bgfx its own copy of the payload region —
		// the .bcn payload layout IS bgfx's expected hasMips=true
		// memory layout by construction)
		const bgfx::TextureHandle handle = bgfx::createTexture2D(
			upload.m_width, upload.m_height, true, 1,
			upload.m_bgfx_format, BGFX_TEXTURE_NONE,
			bgfx::copy(upload.m_p_payload, upload.m_payload_size));

		if (bgfx::isValid(handle) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[texture_bcn] create: bgfx rejected the texture ({}x{} "
				"format {}, {} payload bytes) — a format/upload skew",
				upload.m_width, upload.m_height,
				static_cast<kotek::uint32_t>(upload.m_bgfx_format),
				upload.m_payload_size);
			return BGFX_INVALID_HANDLE;
		}

		return handle;
	}
} // namespace no_streaming
