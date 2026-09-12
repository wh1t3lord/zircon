#pragma once

#include "zircon_defs.h"

// ---------------------------------------------------------------------
// task Z23 (filesystem plan Part B4): the fault-tolerant DEFAULTS chain.
//
// The proven-by-time pattern (Source's magenta checkerboard, Unreal's
// default material, id's default entries): a resource request resolves
// through the priority chain — data_user overrides -> data_game dirs ->
// mounted packs (newest first; the dispatcher machinery already walks
// all of it, this file reimplements NONE of that) — and when NOTHING
// resolved, the EMBEDDED default for the resource's type answers. An
// unreadable/missing resource NEVER exit-faults/asserts: the embedded
// blob is returned instead with a loud-once log (missing content must
// be visible, not silent).
//
// Embedded defaults are the owner's "embed data as byte arrays"
// instinct: tiny constexpr byte blocks compiled into the engine — the
// only "cache" with zero runtime bookkeeping (house streaming-first
// posture: no runtime cache of file bytes; these are NOT a cache, they
// are the content of last resort).
//
// THE SHADER DECISION (documented per the plan): the shader default is
// a SEMANTIC TOKEN, not a compiled blob. A real shader binary is
// backend-specific (bgfx's d3d11/vulkan containers differ, NRI's dxil
// differs again) and the active backend is a RUNTIME choice — a core-
// compiled blob would be wrong for half the configurations, and bgfx/
// NRI types are forbidden in zircon core. So kShader_Unlit_Magenta
// carries the ASCII token below: the RENDER side recognizes it (via the
// descriptor / is_default) and binds its own built-in unlit-magenta
// material per backend (bgfx: the editor passes already own embedded
// shader pairs; NRI: the same token maps to its magenta path). The
// render-side mapping is future work — the token is the stable seam.
//
// DEDUPE SEMANTICS of the loud-once log: one warning per (type, path)
// pair per manager instance — each distinct missing resource is named
// exactly once (a repeated resolution of the same missing path does not
// re-log), bounded at ZIRCON_DEF_EMBEDDED_DEFAULTS_MAX_WARNED; on
// overflow one suppression notice fires and defaults keep resolving
// silently (never a fault). The set is member-level (no statics, house
// rule 1a) and NOT synchronized — the same single-threaded-at-a-time
// discipline as the filesystem it sits on (the resource manager's sync
// path and its worker never resolve concurrently today).
//
// WHO RIDES THE CHAIN (the plan's point 5, decisions):
// - the resource manager's text branch: a missing/unreadable/oversized/
//   unparseable json resolves to the embedded {} with is_default=true
//   (the editor badges it later — recorded, not built).
// - the localization manager: NO — a missing language file degrades to
//   the previous table + key echo (missing strings self-document on
//   screen), which is strictly better than an empty-entries {} locale;
//   an empty table is NOT the same as a missing one.
// - zircon_config: NO — its missing-file case already installs the
//   hardcoded defaults (initialize_default), the designed semantic
//   floor; an embedded {} would only reach the same defaults through
//   the absent-key path with a misleading "default used" log on top.
// - future texture/shader/audio consumers (the .ktx/.ogg format traps
//   are Z8's C4): the resolve entry points below are ready for them.
// ---------------------------------------------------------------------

/// one loud warning per missing (type, path) pair, deduped through this
/// member set (a plain static_vector scan — 16 entries, house rule 2);
/// when full, one suppression notice fires and later defaults resolve
/// silently
#define ZIRCON_DEF_EMBEDDED_DEFAULTS_MAX_WARNED 16

/// the embedded default kinds — one per resource-type fallback. The
/// enum is deliberately NOT eZirconResourceType: that enum has no
/// shader kind and mixes level/material/gui kinds that have no embedded
/// default; this set is exactly the critical few the engine can always
/// answer for
enum class eZirconEmbeddedDefaultType : kotek::uint8_t
{
	/// 2x2 magenta/black checker, uncompressed RGBA8 (Source-style)
	kTexture_Checker_Magenta = 0,
	/// the semantic token the render side maps to a built-in
	/// unlit-magenta material (see the header's shader decision)
	kShader_Unlit_Magenta,
	/// a tiny canonical PCM WAV (mono 8 kHz 16-bit) of zeroed samples
	kAudio_Silence,
	/// the two bytes "{}" — the json floor that keeps config-style
	/// loads valid
	kJson_Empty,
	kCount
};

/// pixel format of an embedded texture blob (kNone for non-textures)
enum class eZirconEmbeddedTextureFormat : kotek::uint8_t
{
	kNone = 0,
	/// 4 bytes per pixel, R-G-B-A order, 8 bits each, opaque alpha
	kRGBA8_Unorm
};

/// @brief \~english the POD descriptor every embedded default is handed
/// out as: the raw bytes + their size + the texture metadata (zero/
/// kNone for non-texture types — the blob is a pixel block, NOT a file
/// format container, so the dimensions must travel beside it)
struct zircon_embedded_default_t
{
	eZirconEmbeddedDefaultType m_type;
	const kotek::uint8_t* m_p_bytes;
	kotek::uint32_t m_size;
	kotek::uint16_t m_texture_width;
	kotek::uint16_t m_texture_height;
	eZirconEmbeddedTextureFormat m_texture_format;
};

/// @brief \~english the result of a resolve_or_default call: the bytes
/// point EITHER into the caller's buffer (a chain hit — the dispatcher
/// found real content) OR at the embedded blob (is_default=true).
/// m_p_bytes is never null when the call returned true
struct zircon_resolved_or_default_t
{
	const kotek::uint8_t* m_p_bytes;
	kotek::size_t m_size;
	bool m_is_default;
};

/// @brief \~english the embedded-defaults facade (task Z23): the blob
/// table accessor (pure data, static, stateless) + the loud-once log
/// mechanism (member warned set — one instance per resolving owner:
/// the resource manager owns one, the tests own fresh ones, a future
/// render-side consumer owns its own). Tiny and copy-free; safe to
/// place as a member anywhere
class zircon_embedded_defaults
{
public:
	zircon_embedded_defaults(void) = default;

	/// pure data lookup over the embedded table — no logging, no state,
	/// never fails (an out-of-range type is a programmer error: assert +
	/// the json floor as the safe answer)
	static const zircon_embedded_default_t& get_embedded_default(
		eZirconEmbeddedDefaultType type) noexcept;

	/// the loud-once log: ONE KOTEK_MESSAGE_WARNING per (type, path)
	/// pair per instance (see the header's dedupe semantics). Cheap on
	/// the repeat path (a 16-entry scan), allocation-free always
	void log_default_used_once(eZirconEmbeddedDefaultType type,
		const kotek::static_path_t& path) noexcept;

	/// diagnostics (the tests pin the loud-once contract through these;
	/// a future editor statistics view reads the same numbers)
	kotek::uint32_t get_warned_count(void) const noexcept;
	bool get_warned_overflow_announced(void) const noexcept;

private:
	kotek::static_vector_t<kotek::uint64_t,
		ZIRCON_DEF_EMBEDDED_DEFAULTS_MAX_WARNED>
		m_warned_pairs;
	bool m_warned_overflow_announced = false;
};

/// @brief \~english the resolution seam (task Z23): try the dispatcher
/// chain for path's bytes (data_user/data_game/mounted packs per the
/// priority machinery — ktkIFileSystem::Read_File with kAuto), else
/// answer the embedded default for type.
///
/// in_out_buffer_size: in = p_buffer capacity; out = the real size (the
/// bytes copied on a chain hit, the blob size on a default).
///
/// Returns false ONLY on a caller error: null arguments, or a REAL file
/// that does not fit p_buffer (the required size lands in
/// in_out_buffer_size per the filesystem's too-small contract — retry
/// bigger; a too-small real file is real content, never a default
/// case). A missing/unreadable file is never false: the default
/// resolves with out_result.m_is_default=true and the loud-once log
/// fires through defaults. A present 0-byte file is a chain hit with
/// size 0 (it IS the real content; policy like "empty means default"
/// belongs to the consumer — the resource manager's text branch applies
/// its own).
bool zircon_resolve_or_default(
	kotek::core::ktkIFileSystem* p_filesystem,
	zircon_embedded_defaults& defaults,
	const kotek::static_path_t& path,
	eZirconEmbeddedDefaultType type,
	kotek::uint8_t* p_buffer,
	kotek::size_t& in_out_buffer_size,
	zircon_resolved_or_default_t& out_result) noexcept;

/// the per-type entry points (thin named forwards over
/// zircon_resolve_or_default so call sites read by resource kind)
inline bool zircon_resolve_texture_or_default(
	kotek::core::ktkIFileSystem* p_filesystem,
	zircon_embedded_defaults& defaults,
	const kotek::static_path_t& path,
	kotek::uint8_t* p_buffer,
	kotek::size_t& in_out_buffer_size,
	zircon_resolved_or_default_t& out_result) noexcept
{
	return zircon_resolve_or_default(p_filesystem, defaults, path,
		eZirconEmbeddedDefaultType::kTexture_Checker_Magenta, p_buffer,
		in_out_buffer_size, out_result);
}

inline bool zircon_resolve_shader_or_default(
	kotek::core::ktkIFileSystem* p_filesystem,
	zircon_embedded_defaults& defaults,
	const kotek::static_path_t& path,
	kotek::uint8_t* p_buffer,
	kotek::size_t& in_out_buffer_size,
	zircon_resolved_or_default_t& out_result) noexcept
{
	return zircon_resolve_or_default(p_filesystem, defaults, path,
		eZirconEmbeddedDefaultType::kShader_Unlit_Magenta, p_buffer,
		in_out_buffer_size, out_result);
}

inline bool zircon_resolve_audio_or_default(
	kotek::core::ktkIFileSystem* p_filesystem,
	zircon_embedded_defaults& defaults,
	const kotek::static_path_t& path,
	kotek::uint8_t* p_buffer,
	kotek::size_t& in_out_buffer_size,
	zircon_resolved_or_default_t& out_result) noexcept
{
	return zircon_resolve_or_default(p_filesystem, defaults, path,
		eZirconEmbeddedDefaultType::kAudio_Silence, p_buffer,
		in_out_buffer_size, out_result);
}

inline bool zircon_resolve_json_or_default(
	kotek::core::ktkIFileSystem* p_filesystem,
	zircon_embedded_defaults& defaults,
	const kotek::static_path_t& path,
	kotek::uint8_t* p_buffer,
	kotek::size_t& in_out_buffer_size,
	zircon_resolved_or_default_t& out_result) noexcept
{
	return zircon_resolve_or_default(p_filesystem, defaults, path,
		eZirconEmbeddedDefaultType::kJson_Empty, p_buffer,
		in_out_buffer_size, out_result);
}
