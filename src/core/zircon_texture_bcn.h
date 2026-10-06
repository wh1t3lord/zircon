#pragma once

// zircon_texture_bcn.h — the BCn TEXTURE pipeline (task Z24 phase B4):
// block-compressed textures encoded OFFLINE at bake time, packed as
// .kpack entries, uploaded to the GPU with the blocks AS-IS. The plan's
// hard rules: the hardware decodes in the sampler (zero CPU on the
// runtime path — the runtime NEVER transcodes and NEVER decodes; Basis/
// UASTC-style transcode-on-load is rejected), and encoding is a
// bake/tool concern, never engine runtime code.
//
// This header is the format's SINGLE SOURCE OF TRUTH: the bake driver
// (below, driven by the tests and by the zircon_bcn_bake host tool), the
// vendored-encoder adapter (src/tools/zircon_bcn_encoder/) and the
// render-side uploader
// (src/render/bgfx/passes/no_streaming/zircon_render_texture_bcn.h) all
// read/write through these helpers.
//
// ---------------------------------------------------------------------
// THE CLASS -> FORMAT MAPPING (the approved plan, Part B "texture
// pipeline"): BC7 for albedo/normal/AO (perceptually near-lossless at
// 4:1 vs RGBA8), BC5 for tangent-space normal maps (two full channels,
// no RGB cross-talk), BC1 for masks/ramps (8:1), BC6H for HDR skies.
// BC6H ENCODING IS EXPLICITLY DEFERRED this phase: no license-clean
// single-file BC6H encoder exists (ispc-texcomp needs the ISPC
// toolchain, DirectXTex is a library-scale dep), and the plan's HDR use
// is skies — a later intake task. The enum value + the format's block
// math cover it, the bake rejects kBC6H loudly with the deferral
// reason, and the render uploader maps it (bgfx has the BC6H create
// path) for the day the encoder lands. BC3 (DXT5) sits in the enum for
// the classic alpha-gradient content class; no default class selects
// it, the --format override encodes it.
//
// ---------------------------------------------------------------------
// THE .bcn FILE LAYOUT (all little-endian, written field-by-field — no
// struct dumps, no padding; the kpack format's own posture). One
// texture per pack entry: textures/<scene>/<name>.bcn
//
//   [header] 32 bytes
//     +0   8   magic char[8] = {'Z','B','C','N','0','1','\0','\0'}
//     +8   4   width u32 (texels; v1 REQUIRES a multiple of 4 — see the
//              dimension rule below)
//     +12  4   height u32 (texels; multiple of 4)
//     +16  1   format u8 (eZirconTextureBcnFormat)
//     +17  1   mip_count u8 (1..ZIRCON_DEF_TEXTURE_BCN_MAX_MIP_COUNT)
//     +18  2   flags u16 (must be 0 in v1; bit 0 is RESERVED for a
//              future sRGB declaration — no consumer this phase)
//     +20  4   block_payload_size u32 (the sum of every mip level's
//              block bytes — the parse validates it against the
//              dimensions/format/mip count)
//     +24  1   content_class u8 (eZirconTextureBcnClass — the bake's
//              class option, informational for tooling/badge UI)
//     +25  1   encoder_quality_tier u8 (the tier the bake encoded at —
//              informational; the tiers are defined adapter-side)
//     +26  6   reserved (must be 0)
//   [mip table] mip_count x 8 bytes, mip 0 first:
//     +0   4   payload_offset u32 (relative to the PAYLOAD's start, so
//              the table is file-position independent)
//     +4   4   size u32 (that level's block bytes)
//   [payload] block_payload_size bytes: every mip level's BCn blocks,
//     mip 0 first, each level = ceil(level_w/4) * ceil(level_h/4) *
//     block_bytes(format) tightly packed, blocks in row-major order.
//     This is EXACTLY the memory layout bgfx's createTexture2D expects
//     for a hasMips=true texture, so the uploader hands the payload
//     region to bgfx::copy without touching a byte (the no-decode
//     contract).
//
// THE DIMENSION RULE (v1): width and height must be multiples of 4 and
// inside [4, ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION]. BCn encodes 4x4
// blocks; partial edge blocks are legal in hardware but every
// encoder/decoder/mip step special-cases them — rejected loudly at bake
// for now (a later intake task lifts it). A multiple-of-4 source keeps
// every mip level even down to 2x2, so the box filter's edge clamp
// (below) never engages on bake-legal content.
//
// ---------------------------------------------------------------------
// THE MIP POLICY (the industry's standard starting point, documented):
// the full chain down to 1x1 by default (mip_count 0 = full at the
// bake API; floor(log2(max(w,h))) + 1 levels, capped at
// ZIRCON_DEF_TEXTURE_BCN_MAX_MIP_COUNT). Each level is the previous
// level BOX-FILTERED 2x2: out = (a + b + c + d + 2) >> 2 per channel
// (integer round-nearest — deterministic on every platform, no FPU
// steps), the 2x2 taps edge-clamped (min(2x+1, w-1) — only reachable on
// non-bake-legal odd sizes). The filter runs in the ENCODED (sRGB)
// space for albedo — the documented v1 approximation; sRGB-aware
// (linear-light) downsampling is a recorded refinement, not this phase.
// Partial chains are representable in the format but the bgfx upload
// path REQUIRES the full chain (bgfx's createTexture2D hasMips=true
// always sizes the full chain — a partial .bcn is rejected loudly at
// upload prepare).
//
// ---------------------------------------------------------------------
// THE ENCODE SEAM (the owner's rule: the vendored encoder is TOOL-side,
// never engine/runtime code): this module takes the per-level block
// encoder as an INJECTED function pointer
// (zircon_texture_bcn_encode_level_fn + a void* user context) — it
// never names the vendored library. The zircon_bcn_bake host tool and
// the unit tests inject the adapter
// (src/tools/zircon_bcn_encoder/zircon_bcn_encoder_adapter.h, which
// drives rgbcx for BC1/BC3/BC5 and bc7enc for BC7); a user bake tool
// injects its own. The runtime render path injects NOTHING — it only
// ever parses + uploads.
//
// THE DECODE SEAM: decoding exists ONLY as a test proof (the PSNR
// quality pins) through the same adapter (bc7decomp for BC7, bcdec for
// BC1/BC3/BC5 — both vendored tool-side). No runtime code path decodes.
//
// ---------------------------------------------------------------------
// THE .zraw INTAKE (the bake's raw input container — OUR minimal
// format, so the phase adds NO PNG/TGA dependency; real-format intake
// is the recorded later task):
//   +0   8   magic char[8] = {'Z','R','A','W','0','1','\0','\0'}
//   +8   4   width u32, +12  4   height u32, then width*height*4 bytes
//   of RGBA8 (R first, row-major, no padding).
//
// ---------------------------------------------------------------------
// THE SHIPPED BOOT PROBE: data_game/textures/boot/boot_checker.bcn (the
// folder doctrine — game assets live under data_game/, the parent's
// meshlet move): a 64x64 BC7 magenta/black checker (8 texel cells — the
// embedded-default magenta theme, zircon_embedded_defaults.h) with the
// FULL mip chain, synthesized by
// zircon_texture_bcn_make_boot_checker_rgba (the ONE recipe home — the
// tool, the fixture and the tests can never drift). The B1 gpu-driven
// pass uploads it at create when present (the no-decode upload's live
// proof). Regenerate after a format/encoder change with:
// zircon_bcn_bake --synthesize_boot_checker (the test
// BootFixtureMatchesEncoder pins the bytes).
//
// THE NAMESPACE RULE (the pack-resolution subtlety, the Z23 marker
// precedent): the format's LOGICAL entry names are content-root-relative
// ("textures/<scene>/<name>.bcn"); the filesystem dispatcher resolves
// them via kFolderIndex_DataGame (native: <root>/data_game/textures/...),
// and the pack backend relativizes absolute read paths against the
// FILESYSTEM root (the repo root) — so a pack entry resolving the same
// texture is named "data_game/textures/<scene>/<name>.bcn" (the
// repo-root-relative form). The roundtrip test pins both shapes.
//
// ---------------------------------------------------------------------
// THE DETERMINISM CONTRACT: the same RGBA input + format + quality tier
// produces byte-identical .bcn output — the box filter is integer math,
// the vendored encoders are single-threaded with fixed parameters per
// (format, tier), and no step reads wall-clock or thread state. The
// boot-fixture test re-encodes and memcmps.

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>

// the shared little-endian field codec (one codec home — the CSG
// bake's, the meshlet bake's and now the texture bake's; never forked).
// Split out of zircon_csg_bake.h so THIS module stays free of the CSG
// evaluation chain — the zircon_bcn_bake host tool compiles this module
// standalone (no lowercase-alias umbrella there)
#include "zircon_le_field_codec.h"

// ---------------------------------------------------------------------
// the named capacities (rule 9: named, sized by comment, raised by
// measurement)
// ---------------------------------------------------------------------
// the largest texture edge in texels (4K albedo is the practical ceiling
// for the phase's content classes; the payload cap below is the real
// bound)
#define ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION 4096
// the smallest texture edge (one full BCn block; the dimension rule)
#define ZIRCON_DEF_TEXTURE_BCN_MIN_DIMENSION 4
// the mip-chain cap: 4096 -> 1 is 13 levels (2^12 = 4096, +1)
#define ZIRCON_DEF_TEXTURE_BCN_MAX_MIP_COUNT 13
// the total block-payload cap: a 4096^2 BC7/BC6H texture is
// 1024^2 blocks x 16 B = 16 MB at mip 0 and the geometric chain sums to
// ~22.3 MB — 24 MB covers the largest legal texture's full chain with
// headroom; over-cap is a loud rejection, never a truncation
#define ZIRCON_DEF_TEXTURE_BCN_MAX_PAYLOAD_BYTES (24u * 1024u * 1024u)
// the .zraw intake cap: the 16-byte header + 4096^2 RGBA8 = 64 MB
#define ZIRCON_DEF_TEXTURE_BCN_ZRAW_MAX_BYTES \
	(16u + ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION * \
			ZIRCON_DEF_TEXTURE_BCN_MAX_DIMENSION * 4u)
// the name segment rules (the locale language-tag / CSG-bake file-name
// discipline — a path walk is rejected, loudly)
#define ZIRCON_DEF_TEXTURE_BCN_MAX_SCENE_NAME_LENGTH 32
#define ZIRCON_DEF_TEXTURE_BCN_MAX_TEXTURE_NAME_LENGTH 64
// one entry name ("textures/<32>/<64>.bcn" plus NUL)
#define ZIRCON_DEF_TEXTURE_BCN_ENTRY_NAME_MAX_LENGTH 128
// the boot probe fixture's edge (a small always-shipped texture — the
// upload proof must not cost boot time)
#define ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_SIZE 64
// the boot checker's cell edge in texels
#define ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_CELL 8

// the format's magic values (the version rides inside, the kpack
// posture: a format change bumps the digits)
inline constexpr char zircon_texture_bcn_magic[8] = {'Z', 'B', 'C', 'N',
	'0', '1', '\0', '\0'};
inline constexpr char zircon_texture_bcn_zraw_magic[8] = {'Z', 'R', 'A',
	'W', '0', '1', '\0', '\0'};

// the fixed record sizes (the layouts in the banner)
inline constexpr kotek::uint32_t zircon_texture_bcn_header_size = 32;
inline constexpr kotek::uint32_t zircon_texture_bcn_mip_record_size = 8;
inline constexpr kotek::uint32_t zircon_texture_bcn_zraw_header_size = 16;

// the GPU block-compression format of one .bcn payload (the enum values
// are the file format — append only, never renumber)
enum class eZirconTextureBcnFormat : kotek::uint8_t
{
	kBC1 = 0, // DXT1 — 8 B/block, masks/ramps (8:1 vs RGBA8)
	kBC3 = 1, // DXT5 — 16 B/block, the classic alpha-gradient class
	kBC5 = 2, // LATC2/ATI2 — 16 B/block, tangent-space normal maps
	kBC7 = 3, // 16 B/block, albedo/normal/AO (the near-lossless class)
	kBC6H = 4, // 16 B/block, HDR (ENCODE DEFERRED — see the banner)
	kEndOfEnum
};

// the content class (the bake's --class option; selects the DEFAULT
// format per the plan's mapping, recorded in the header for tooling)
enum class eZirconTextureBcnClass : kotek::uint8_t
{
	kAlbedo = 0,
	kNormal = 1,
	kAO = 2,
	kMask = 3,
	kHDR = 4,
	kEndOfEnum
};

// the status enum of every driver below (the gltf loader's loud-graceful
// posture: user content is not a programmer error — a failure is a
// status + a KOTEK_MESSAGE, never an assert)
enum class eZirconTextureBcnStatus : kotek::uint8_t
{
	kSuccess = 0,
	kInvalidArgument, // null pointers, zero sizes, bad enum values
	kDimensionNotMultipleOf4, // the v1 dimension rule
	kDimensionOutOfRange, // outside [MIN, MAX]_DIMENSION
	kMipCountInvalid, // 0 where a count is required, or > the full chain
	kPayloadOverCap, // ZIRCON_DEF_TEXTURE_BCN_MAX_PAYLOAD_BYTES
	kFormatNotSupportedByEncoder, // kBC6H this phase (the deferral)
	kBufferTooSmall, // the out capacity is short (the required size is
					 // reported where the API has the out-param)
	kCorruptData, // the parse: magic/size/table/bounds skew
	kNameInvalid, // the entry-name rule (a path walk, bad characters)
	kEncodeFailed // the injected encoder returned false
};

// the plan's class -> default-format mapping (the bake option's default;
// --format overrides)
inline constexpr eZirconTextureBcnFormat
	zircon_texture_bcn_default_format_for_class(
		eZirconTextureBcnClass content_class) noexcept
{
	switch (content_class)
	{
	case eZirconTextureBcnClass::kAlbedo:
	case eZirconTextureBcnClass::kAO:
		return eZirconTextureBcnFormat::kBC7;
	case eZirconTextureBcnClass::kNormal:
		return eZirconTextureBcnFormat::kBC5;
	case eZirconTextureBcnClass::kMask:
		return eZirconTextureBcnFormat::kBC1;
	case eZirconTextureBcnClass::kHDR:
		return eZirconTextureBcnFormat::kBC6H;
	default:
		return eZirconTextureBcnFormat::kEndOfEnum;
	}
}

// the on-disk block stride of one 4x4 block (BC1 = 8, everything else
// 16); kEndOfEnum/unknown yields 0
inline constexpr kotek::uint32_t zircon_texture_bcn_block_bytes(
	eZirconTextureBcnFormat format) noexcept
{
	switch (format)
	{
	case eZirconTextureBcnFormat::kBC1:
		return 8;
	case eZirconTextureBcnFormat::kBC3:
	case eZirconTextureBcnFormat::kBC5:
	case eZirconTextureBcnFormat::kBC7:
	case eZirconTextureBcnFormat::kBC6H:
		return 16;
	default:
		return 0;
	}
}

// the mip level's texel dimensions: max(1, dimension >> level)
inline constexpr kotek::uint32_t zircon_texture_bcn_mip_dimension(
	kotek::uint32_t dimension, kotek::uint32_t level) noexcept
{
	return (dimension >> level) ? (dimension >> level) : 1;
}

// the FULL chain length (floor(log2(max(w,h))) + 1), uncapped by the
// format's MAX_MIP_COUNT (the source validation applies the cap)
inline constexpr kotek::uint32_t zircon_texture_bcn_full_mip_count(
	kotek::uint32_t width, kotek::uint32_t height) noexcept
{
	kotek::uint32_t largest = width > height ? width : height;
	kotek::uint32_t count = 1;

	while (largest > 1)
	{
		largest >>= 1;
		++count;
	}

	return count;
}

// one mip level's block bytes: ceil(w/4) * ceil(h/4) * block_bytes
inline constexpr kotek::uint32_t zircon_texture_bcn_level_block_bytes(
	kotek::uint32_t width, kotek::uint32_t height,
	eZirconTextureBcnFormat format) noexcept
{
	const kotek::uint32_t blocks_x = (width + 3) / 4;
	const kotek::uint32_t blocks_y = (height + 3) / 4;
	return blocks_x * blocks_y * zircon_texture_bcn_block_bytes(format);
}

// the summed payload of mip_count levels starting at (width, height)
inline constexpr kotek::uint32_t zircon_texture_bcn_total_payload_bytes(
	kotek::uint32_t width, kotek::uint32_t height,
	eZirconTextureBcnFormat format, kotek::uint32_t mip_count) noexcept
{
	kotek::uint32_t total = 0;

	for (kotek::uint32_t level = 0; level < mip_count; ++level)
	{
		total += zircon_texture_bcn_level_block_bytes(
			zircon_texture_bcn_mip_dimension(width, level),
			zircon_texture_bcn_mip_dimension(height, level), format);
	}

	return total;
}

// the whole .bcn file size: header + table + payload
inline constexpr kotek::uint32_t zircon_texture_bcn_total_file_bytes(
	kotek::uint32_t width, kotek::uint32_t height,
	eZirconTextureBcnFormat format, kotek::uint32_t mip_count) noexcept
{
	return zircon_texture_bcn_header_size +
		mip_count * zircon_texture_bcn_mip_record_size +
		zircon_texture_bcn_total_payload_bytes(
			width, height, format, mip_count);
}

// the .zraw file size for one RGBA8 image
inline constexpr kotek::uint32_t zircon_texture_bcn_zraw_file_bytes(
	kotek::uint32_t width, kotek::uint32_t height) noexcept
{
	return zircon_texture_bcn_zraw_header_size + width * height * 4;
}

// ---------------------------------------------------------------------
// the encode seam (see the banner): the per-level block encoder is
// INJECTED. Contract: p_rgba_pixels is width*height*4 RGBA8 (both
// dimensions multiples of 4 on bake-legal content); the implementation
// writes ceil(w/4)*ceil(h/4)*block_bytes(format) bytes into
// p_out_blocks (out_block_capacity >= that, validated by the caller)
// and returns true; false = the level failed (the build aborts with
// kEncodeFailed). Called per mip level, mip 0 first, single-threaded.
using zircon_texture_bcn_encode_level_fn = bool (*)(void* p_user,
	const kotek::uint8_t* p_rgba_pixels, kotek::uint32_t width,
	kotek::uint32_t height, eZirconTextureBcnFormat format,
	kotek::uint8_t* p_out_blocks, kotek::size_t out_block_capacity) noexcept;

// ---------------------------------------------------------------------
// the bake driver
// ---------------------------------------------------------------------
// validates the source parameters against the dimension/cap rules.
// mip_count 0 means "the full chain" (the default). Loud on failure
// (a KOTEK_MESSAGE names the rule that rejected the content).
eZirconTextureBcnStatus zircon_texture_bcn_validate_source(
	kotek::uint32_t width, kotek::uint32_t height,
	eZirconTextureBcnFormat format, kotek::uint32_t mip_count) noexcept;

// the mip chain's box filter (the documented 2x2, integer round-nearest,
// edge-clamped): p_src is width*height*4 RGBA8, p_dst receives
// mip_dimension(w,1) * mip_dimension(h,1) * 4 bytes. Exposed for the
// tests (the filter output is pinned on a known 8x8); the build calls
// it per level. A 1-wide/tall axis degenerates to a 2-tap/1-tap average
// through the same clamp.
void zircon_texture_bcn_downsample_box(const kotek::uint8_t* p_src,
	kotek::uint32_t width, kotek::uint32_t height,
	kotek::uint8_t* p_dst) noexcept;

// THE BAKE: builds one .bcn file (header + mip table + the encoded
// blocks of every level) from one RGBA8 image. mip_count 0 = the full
// chain. The encoder is the INJECTED p_encode (+ p_encode_user — the
// adapter context); encoder_quality_tier is recorded in the header
// (informational). p_out_file_bytes receives the file;
// out_file_size the written size; on kBufferTooSmall out_file_size
// carries the REQUIRED size (the B0 contract's shape). The downsample
// scratch is a bounded heap allocation inside (the house idiom for
// MB-scale scratch), freed before return. Loud on every failure path.
eZirconTextureBcnStatus zircon_texture_bcn_build(
	const kotek::uint8_t* p_rgba_pixels, kotek::uint32_t width,
	kotek::uint32_t height, eZirconTextureBcnFormat format,
	eZirconTextureBcnClass content_class, kotek::uint32_t mip_count,
	kotek::uint8_t encoder_quality_tier,
	zircon_texture_bcn_encode_level_fn p_encode, void* p_encode_user,
	kotek::uint8_t* p_out_file_bytes, kotek::size_t out_capacity,
	kotek::size_t& out_file_size) noexcept;

// ---------------------------------------------------------------------
// the load-side parse (the render uploader + the tests; pure — the
// caller owns the bytes and the IO)
// ---------------------------------------------------------------------
struct zircon_texture_bcn_desc_t
{
	kotek::uint32_t m_width;
	kotek::uint32_t m_height;
	eZirconTextureBcnFormat m_format;
	eZirconTextureBcnClass m_content_class;
	kotek::uint8_t m_mip_count;
	kotek::uint8_t m_encoder_quality_tier;
	kotek::uint32_t m_payload_size; // the summed block bytes
	kotek::uint32_t m_mip_table_file_offset; // header_size, always
	kotek::uint32_t m_payload_file_offset; // header + table, always
};

// validates the header + the mip table against every format rule
// (magic, the dimension rule, the caps, the table's offsets/sizes
// covering exactly the payload, the file size matching exactly) and
// fills out_desc. p_file_bytes must hold file_size bytes.
eZirconTextureBcnStatus zircon_texture_bcn_parse(
	const kotek::uint8_t* p_file_bytes, kotek::size_t file_size,
	zircon_texture_bcn_desc_t& out_desc) noexcept;

// one mip record (the parse-validated table): the level's payload-
// relative offset + size. false = index past the chain (a caller error,
// loud)
bool zircon_texture_bcn_read_mip_record(const kotek::uint8_t* p_file_bytes,
	const zircon_texture_bcn_desc_t& desc, kotek::uint32_t mip_index,
	kotek::uint32_t& out_payload_offset, kotek::uint32_t& out_size) noexcept;

// ---------------------------------------------------------------------
// the entry name + the .zraw intake + the boot probe recipe
// ---------------------------------------------------------------------
// builds "textures/<scene>/<name>.bcn" with the file-name rule on both
// segments ([a-zA-Z0-9_-], 1..32 scene / 1..64 texture — a path walk or
// a bad character is kNameInvalid, loud)
eZirconTextureBcnStatus zircon_texture_bcn_make_entry_name(
	const char* p_scene_name, const char* p_texture_name,
	kotek::static_cstring_t<ZIRCON_DEF_TEXTURE_BCN_ENTRY_NAME_MAX_LENGTH>&
		out_entry_name) noexcept;

// validates a .zraw image (magic, the dimension caps, the exact file
// size) and returns the texel view (width/height + the pixel offset —
// the pixels stay inside the caller's buffer, zero-copy)
eZirconTextureBcnStatus zircon_texture_bcn_zraw_parse(
	const kotek::uint8_t* p_zraw_bytes, kotek::size_t zraw_size,
	kotek::uint32_t& out_width, kotek::uint32_t& out_height,
	kotek::uint32_t& out_pixel_offset) noexcept;

// writes the .zraw container for one RGBA8 image (the fixture/
// roundtrip side of the intake format; out_size carries the written
// size, or the REQUIRED size on kBufferTooSmall)
eZirconTextureBcnStatus zircon_texture_bcn_zraw_write(
	const kotek::uint8_t* p_rgba_pixels, kotek::uint32_t width,
	kotek::uint32_t height, kotek::uint8_t* p_out_bytes,
	kotek::size_t out_capacity, kotek::size_t& out_size) noexcept;

// THE boot probe recipe (the banner's single home): fills
// p_out_rgba_pixels (ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_SIZE^2 x 4
// bytes) with the 8-texel-cell magenta/black checker. Deterministic by
// construction — the tool, the shipped fixture and the tests all take
// the bytes from here.
void zircon_texture_bcn_make_boot_checker_rgba(
	kotek::uint8_t* p_out_rgba_pixels) noexcept;
