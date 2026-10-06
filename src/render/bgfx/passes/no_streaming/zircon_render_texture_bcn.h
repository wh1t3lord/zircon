#pragma once

// zircon_render_texture_bcn.h — the RENDER-side BCn texture upload
// path (task Z24 phase B4): reads a baked .bcn (the zircon_texture_bcn.h
// format — textures/<scene>/<name>.bcn pack entries) through the
// filesystem dispatcher and builds the GPU texture with the blocks
// AS-IS. THE NO-DECODE CONTRACT (the plan's hard rule): nothing on
// this path ever decompresses, transcodes or re-encodes — the BCn
// blocks travel file -> dispatcher -> bgfx::copy -> the GPU texture
// untouched and the HARDWARE decodes in the sampler. Decoding exists
// only as a test proof (the vendored adapter's decode seam).
//
// The split is the house's headless-testable shape:
//   - zircon_render_texture_bcn_prepare: IO + validation, fills an
//     upload DESCRIPTOR viewing the caller's scratch (no bgfx calls —
//     the tests drive it headlessly and pin the byte-identity of the
//     upload layout against the bake's blocks);
//   - zircon_render_texture_bcn_create: the ONE bgfx call
//     (createTexture2D with the matching compressed format enum and
//     bgfx::copy of the payload region — the .bcn payload layout IS
//     bgfx's expected hasMips=true memory layout by construction).
// bgfx's createTexture2D sizes hasMips=true textures as the FULL chain,
// so a partial-chain .bcn is rejected loudly at create (the bake
// defaults to full; the format can represent partial for future
// streaming work).

#include <kotek.core.defines.static.render.bgfx/include/kotek_core_defines_static_render_bgfx.h>
#include <kotek.core.containers.filesystem.path/include/kotek_core_containers_filesystem_path.h>

#include <bgfx/bgfx.h>

#include "../../../../core/zircon_texture_bcn.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkIFileSystem;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

namespace no_streaming
{
	// the upload descriptor: a validated .bcn's GPU-relevant view. The
	// pointers reference the caller's scratch buffer (prepare's
	// p_scratch) — the scratch must outlive the create call
	// (bgfx::copy hands bgfx its own copy, so nothing outlives create).
	struct zircon_render_texture_bcn_upload_t
	{
		kotek::uint16_t m_width;
		kotek::uint16_t m_height;
		kotek::uint8_t m_mip_count;
		bgfx::TextureFormat::Enum m_bgfx_format;
		const kotek::uint8_t* m_p_payload; // ALL mips, mip 0 first
		kotek::uint32_t m_payload_size;
		// the validated format view (per-mip records ride the scratch
		// through zircon_texture_bcn_read_mip_record)
		zircon_texture_bcn_desc_t m_desc;
		const kotek::uint8_t* m_p_file_bytes; // the scratch base
	};

	// the format mapping (pure — headless-testable): our file enum ->
	// bgfx's compressed texture formats (the vendored bgfx carries the
	// BC1/BC2/BC3/BC4/BC5/BC6H/BC7 create paths — bgfx.h TextureFormat).
	// An out-of-enum value maps to Unknown (create rejects it).
	bgfx::TextureFormat::Enum zircon_render_texture_bcn_map_format(
		eZirconTextureBcnFormat format) noexcept;

	// reads entry_path_relative_to_root ("textures/<scene>/<name>.bcn")
	// through the dispatcher (the pack-first override chain —
	// kFolderIndex_Root resolution keeps the read cwd-independent) into
	// p_scratch, validates it through the format's parse and fills the
	// upload view. false + a loud log on every failure: a missing entry
	// (the read's one B0 warning + this context line), a scratch
	// smaller than the file (the required size is logged), a corrupt
	// payload (the parse's reason). Never an assert — user content is
	// not a programmer error.
	bool zircon_render_texture_bcn_prepare(
		kotek::core::ktkIFileSystem* p_filesystem,
		const kotek::static_path_t& entry_path_relative_to_root,
		kotek::uint8_t* p_scratch, kotek::size_t scratch_capacity,
		zircon_render_texture_bcn_upload_t& out_upload) noexcept;

	// the no-decode upload: creates the GPU texture from the payload
	// blocks AS-IS (hasMips=true — the payload is the full chain; a
	// partial chain is rejected loudly, see the banner). Returns
	// BGFX_INVALID_HANDLE on a caller-error argument (loud, never an
	// assert). The caller owns the handle (destroy with bgfx::destroy).
	bgfx::TextureHandle zircon_render_texture_bcn_create(
		const zircon_render_texture_bcn_upload_t& upload) noexcept;
} // namespace no_streaming
