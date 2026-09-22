#pragma once

// zircon_csg_bake.h — the CSG game BAKE (task Z25 phase A3, the final
// phase): the level-compilation step that turns the editor's evaluated
// CSG compounds into pack-hosted, GPU-ready chunked geometry. The game
// session NEVER evaluates CSG (the plan's game-session split): at bake
// time every compound's primitive list goes through the EXISTING A1
// evaluation core (zircon_csg_evaluate.h — never reimplemented here),
// the welded mesh is split on a world grid, quantized, and written as
// .kpack entries through the shared encoder (kpack_write_file — the
// same function the zircon_kpacker tool and the tests link); at runtime
// the B1 chunk pool reads the entries back through the filesystem
// dispatcher (zircon_render_chunk_pool::load_chunks_from_pack) and
// registers them as chunks for the GPU-culled path.
//
// ---------------------------------------------------------------------
// THE PACK LAYOUT (all little-endian, written field-by-field — no
// struct dumps, no padding; the kpack format's own posture)
//
// entries, in WRITER (= load/streaming) order:
//   csg/<scene>/manifest.bin      the metadata (below)
//   csg/<scene>/materials.json    the material table (below)
//   csg/<scene>/compound_<i>/chunk_<j>.bin   one per chunk, j in
//                                           streaming order
// <scene> is validated as a FILE name (1..32 of [a-zA-Z0-9_-], the
// locale language-tag rule — a path walk is rejected, loudly).
//
// manifest.bin:
//   [header] 32 bytes
//     +0   8   magic char[8] = {'Z','C','S','G','B','A','K','1'}
//     +8   4   chunk_size_meters f32 (the grid the bake split on)
//     +12  4   baked_compound_count u32
//     +16  4   chunk_count_total u32
//     +20  4   welded_vertex_count_total u32
//     +24  4   index_count_total u32
//     +28  4   flags u32 (reserved, must be 0)
//   [records] chunk_count_total x 96 bytes, in EMISSION order:
//     +0   4   compound_index u32 (the input array order)
//     +4   4   chunk_index u32 (per-compound emission index)
//     +8   4   cell_x i32 (the world-grid cell, floor rule below)
//     +12  4   cell_y i32
//     +16  4   cell_z i32
//     +20  4   welded_vertex_count u32
//     +24  4   index_count u32 (3 x triangle_count)
//     +28  4   triangle_count u32
//     +32  2   material_id_first u16 (the chunk's first triangle's
//              material — the pool's per-chunk material slot; the full
//              per-triangle table rides the chunk entry)
//     +34  2   reserved u16 (must be 0)
//     +36  24  aabb_min f64[3] (world space, PRE-quantization, exact
//              from the source triangles)
//     +60  24  aabb_max f64[3]
//     +84  8   entry_name_hash u64 — kpack_hash_name of the chunk's
//              entry name (a manifest/pack skew check at load)
//     +92  4   reserved u32 (must be 0)
//
// compound_<i>/chunk_<j>.bin:
//   [header] 16 bytes
//     +0   4   magic char[4] = {'C','H','K','1'}
//     +4   4   welded_vertex_count u32
//     +8   4   index_count u32
//     +12  4   triangle_count u32
//   [positions] welded_vertex_count x 3 x position_quant_t (u16 LE by
//     default, u8 under ZIRCON_DEF_CSG_BAKE_POSITION_QUANT_BITS=8) —
//     normalized against the chunk's aabb (the manifest record)
//   [normals] triangle_count x 2 x u8 — octahedral, PER TRIANGLE (the
//     brush's flat shading; the welded positions cannot carry
//     per-vertex normals across normal seams — the pool expands the
//     triangle soup at load, the same expansion the A2 editor pool
//     does at upsert)
//   [indices] index_count x u32 LE — chunk-local, addressing the
//     chunk's welded positions
//   [materials] triangle_count x u16 LE — per-triangle material ids
//
// materials.json: {"materials":[<id>,...]} — the sorted distinct
// material ids the bake observed (the id -> material resolution is the
// future material system's; the bake records what the content uses).
//
// ---------------------------------------------------------------------
// THE CHUNKING RULE (deterministic, documented for the tests):
//   triangle -> the grid cell of its CENTROID, cell = floor(coord /
//   CHUNK_SIZE_METERS) per axis (half-open cells [k*size, (k+1)*size) —
//   a centroid exactly ON a boundary lands in the HIGHER cell). Every
//   triangle lands in EXACTLY ONE chunk (no duplication at seams — a
//   straddling triangle is drawn whole by its owning chunk); the
//   chunk's cull AABB is computed from its member triangles, so it is
//   exact and may exceed the cell bounds. Vertices shared by triangles
//   of DIFFERENT chunks are duplicated per chunk (chunks are
//   independent cull/draw units — the documented trade-off); within a
//   chunk the weld is preserved (a generation-stamped remap table,
//   first-use order — deterministic).
//
// THE STREAMING ORDER: chunks are emitted in GRID order — cells sorted
// lexicographically by (cell_x, cell_y, cell_z); compounds in input
// array order. A camera-path order is content-dependent by contrast;
// the grid order is a fixed function of the geometry, which keeps the
// bake reproducible (the determinism contract below).
//
// ---------------------------------------------------------------------
// THE QUANTIZATION (the plan's u8/u16 GPU vertex format; the error
// analysis the tests pin):
//   positions: per chunk, per axis, value -> round((v - min) *
//     (max_store / extent)) clamped to [0, max_store], round-half-up
//     (floor(x + 0.5) — exact, platform-independent). The error is
//     bounded by HALF A LSB: extent / (2 * max_store) per axis (u16:
//     16 m cell / 131070 ~= 0.122 mm; u8: ~= 31.4 mm — the 8-bit mode
//     is the small-mode bandwidth option, lossy by design). A
//     degenerate axis (extent == 0 — a flat chunk, e.g. the
//     single-triangle case) stores 0 and dequantizes to min exactly.
//     Dequantization runs in double from the manifest's f64 bounds and
//     casts to float at the pool boundary; the extra f32 rounding is
//     ~extent * 2^-24, two orders below the u16 LSB.
//   normals: u8 octahedral (2 bytes per triangle — the u16
//     per-component alternative costs 3x the bytes for an error two
//     orders smaller; flat-shaded brush faces do not need it). The
//     octahedral map: n -> (x, y) / (|x|+|y|+|z|), the z < 0
//     hemisphere folded onto the square's back; stored u8 =
//     round((o + 1) * 127.5). Per-component error <= 0.5/127.5 =
//     1/255; the decode map's gradient is bounded by sqrt(6) in the
//     worst region (the fold edges, where the unnormalized vector's
//     length is ~0.707), giving a conservative angular bound of
//     sqrt(6)/255 / 0.707 ~= 0.0136 rad ~= 0.78 degrees — the test
//     pins observed error under 0.015 rad over a direction sweep.
//
// ---------------------------------------------------------------------
// THE DETERMINISM CONTRACT: the same inputs produce byte-identical bake
// output. The evaluation is the A1 core (u32f is bit-exact everywhere;
// f32/f64 are deterministic per binary); the bake's own math is integer
// or IEEE double with fixed rounding rules (no FPU-dependent steps),
// the chunk order is a fixed function of the geometry, and the pack
// writer is deterministic (zstd at a fixed level, no timestamps). The
// test bakes twice and memcmps the pack files.
//
// This header is the format's SINGLE SOURCE OF TRUTH: the bake
// (zircon.core) writes through these helpers and the game-side loader
// (the B1 chunk pool, zircon.render.passes.bgfx) reads through them.

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.containers.vector/include/kotek_core_containers_vector.h>
#include <kotek.core.containers.filesystem.path/include/kotek_core_containers_filesystem_path.h>

// std::floor / std::fabs / std::sqrt / std::memcpy in the pure-POD
// codec kernels (the chunk-pool precedent: plain C math inside the
// pure-POD kernels)
#include <cmath>
#include <cstring>

// the evaluation core the bake drives (never reimplemented — the A1
// plane-set boolean, reached through the same relative include the
// editor session uses)
#include "../ecs/zircon_csg_evaluate.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkIFileSystem;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

// ---------------------------------------------------------------------
// the named capacities (rule 9: named, sized by comment, raised by
// measurement)
// ---------------------------------------------------------------------
// the world-grid cell edge in meters (the plan's 16)
#define ZIRCON_DEF_CSG_BAKE_CHUNK_SIZE_METERS 16
// the baked position quantization: 16 (u16 normalized, the default) or
// 8 (u8 normalized, the small-mode bandwidth option — see the error
// analysis above)
#ifndef ZIRCON_DEF_CSG_BAKE_POSITION_QUANT_BITS
	#define ZIRCON_DEF_CSG_BAKE_POSITION_QUANT_BITS 16
#endif
#if ZIRCON_DEF_CSG_BAKE_POSITION_QUANT_BITS != 8 && \
	ZIRCON_DEF_CSG_BAKE_POSITION_QUANT_BITS != 16
	#error \
		"ZIRCON_DEF_CSG_BAKE_POSITION_QUANT_BITS must be 8 or 16"
#endif
// the scene name segment of every entry name (the locale tag rule)
#define ZIRCON_DEF_CSG_BAKE_MAX_SCENE_NAME_LENGTH 32
// compounds per bake (a level's compound count; the manifest carries
// per-compound aggregates only, so this bounds the input, not a file)
#define ZIRCON_DEF_CSG_BAKE_MAX_COMPOUNDS 256
// chunks per compound: a compound sprawling over more than 64 cells
// (a 1 km row at 16 m cells) is pathological content for ONE compound
// — the scene-level budget is the pool's 1024 chunk slots — the
// compound is SKIPPED, loudly (the bake continues; the result reports
// the skip)
#define ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_COMPOUND 64
// chunks per scene: the pack reader's entry cap
// (KOTEK_DEF_FILESYSTEM_PACK_MAX_ENTRIES = 4096) minus the manifest and
// materials entries — a pack over the cap is rejected at mount, so the
// bake refuses to emit one
#define ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE 4090
// triangles per chunk: the pool's register_chunk takes a u16 vertex
// count and the load expands the soup (3 pool vertices per triangle),
// so 3 * triangles must fit u16 — a chunk over the cap could never
// load; the bake skips the compound loudly instead of emitting it
#define ZIRCON_DEF_CSG_BAKE_MAX_TRIANGLES_PER_CHUNK 21845
// the material table (u16 ids; a level's real material count is in the
// low hundreds — 4096 is the format ceiling, the json stays ~24 KB)
#define ZIRCON_DEF_CSG_BAKE_MAX_MATERIALS 4096
// the total chunk-payload residency of one bake: the kpack encoder API
// is entry-resident (every entry's bytes must live until
// kpack_write_file returns), so the payload is accumulated in one
// bounded heap store — 64 MB is ~4k average chunks, a whole level;
// exceeding it is a loud hard failure (a content-size error the caller
// must see, never a silent truncation). A streaming encoder is the
// documented future work if levels outgrow the cap
#define ZIRCON_DEF_CSG_BAKE_MAX_TOTAL_PAYLOAD (64u * 1024u * 1024u)
// the materials.json text bound (4096 ids x <=5 digits + separators)
#define ZIRCON_DEF_CSG_BAKE_MATERIALS_JSON_MAX_SIZE 32768
// one entry name ("csg/<32>/compound_<255>/chunk_<4089>.bin" plus NUL)
#define ZIRCON_DEF_CSG_BAKE_ENTRY_NAME_MAX_LENGTH 96

// the stored position quantum type
#if ZIRCON_DEF_CSG_BAKE_POSITION_QUANT_BITS == 8
using zircon_csg_bake_position_quant_t = kotek::uint8_t;
#else
using zircon_csg_bake_position_quant_t = kotek::uint16_t;
#endif

// the format's magic values (the version rides inside, the kpack
// posture: a format change bumps the digits)
inline constexpr char zircon_csg_bake_manifest_magic[8] = {
	'Z', 'C', 'S', 'G', 'B', 'A', 'K', '1'};
inline constexpr char zircon_csg_bake_chunk_magic[4] = {'C', 'H', 'K',
	'1'};

// the fixed record sizes (the layouts in the banner)
inline constexpr kotek::uint32_t zircon_csg_bake_manifest_header_size =
	32;
inline constexpr kotek::uint32_t zircon_csg_bake_manifest_record_size =
	96;
inline constexpr kotek::uint32_t zircon_csg_bake_chunk_header_size = 16;

// ---------------------------------------------------------------------
// the little-endian field helpers (the shared writer/reader cursor
// discipline — every multi-byte field is assembled byte-by-byte, so
// the layout never depends on the host's struct packing)
// ---------------------------------------------------------------------
inline void zircon_csg_bake_store_u16(
	kotek::uint8_t* p, kotek::uint16_t value) noexcept
{
	p[0] = static_cast<kotek::uint8_t>(value & 0xFFu);
	p[1] = static_cast<kotek::uint8_t>((value >> 8) & 0xFFu);
}

inline void zircon_csg_bake_store_u32(
	kotek::uint8_t* p, kotek::uint32_t value) noexcept
{
	p[0] = static_cast<kotek::uint8_t>(value & 0xFFu);
	p[1] = static_cast<kotek::uint8_t>((value >> 8) & 0xFFu);
	p[2] = static_cast<kotek::uint8_t>((value >> 16) & 0xFFu);
	p[3] = static_cast<kotek::uint8_t>((value >> 24) & 0xFFu);
}

inline void zircon_csg_bake_store_i32(
	kotek::uint8_t* p, kotek::int32_t value) noexcept
{
	zircon_csg_bake_store_u32(p, static_cast<kotek::uint32_t>(value));
}

inline void zircon_csg_bake_store_u64(
	kotek::uint8_t* p, kotek::uint64_t value) noexcept
{
	zircon_csg_bake_store_u32(p, static_cast<kotek::uint32_t>(
								 value & 0xFFFFFFFFull));
	zircon_csg_bake_store_u32(p + 4, static_cast<kotek::uint32_t>(
									 value >> 32));
}

inline void zircon_csg_bake_store_f64(
	kotek::uint8_t* p, double value) noexcept
{
	static_assert(sizeof(double) == sizeof(kotek::uint64_t));
	kotek::uint64_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	zircon_csg_bake_store_u64(p, bits);
}

inline kotek::uint16_t zircon_csg_bake_load_u16(
	const kotek::uint8_t* p) noexcept
{
	return static_cast<kotek::uint16_t>(
		static_cast<kotek::uint16_t>(p[0]) |
		(static_cast<kotek::uint16_t>(p[1]) << 8));
}

inline kotek::uint32_t zircon_csg_bake_load_u32(
	const kotek::uint8_t* p) noexcept
{
	return static_cast<kotek::uint32_t>(p[0]) |
		(static_cast<kotek::uint32_t>(p[1]) << 8) |
		(static_cast<kotek::uint32_t>(p[2]) << 16) |
		(static_cast<kotek::uint32_t>(p[3]) << 24);
}

inline kotek::int32_t zircon_csg_bake_load_i32(
	const kotek::uint8_t* p) noexcept
{
	return static_cast<kotek::int32_t>(zircon_csg_bake_load_u32(p));
}

inline kotek::uint64_t zircon_csg_bake_load_u64(
	const kotek::uint8_t* p) noexcept
{
	return static_cast<kotek::uint64_t>(zircon_csg_bake_load_u32(p)) |
		(static_cast<kotek::uint64_t>(zircon_csg_bake_load_u32(p + 4))
			<< 32);
}

inline double zircon_csg_bake_load_f64(const kotek::uint8_t* p) noexcept
{
	const kotek::uint64_t bits = zircon_csg_bake_load_u64(p);
	double value = 0.0;
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}

// ---------------------------------------------------------------------
// the quantization primitives (the shared bake/load math — the tests
// drive them directly; all double, fixed rounding rules, so the bits
// are a deterministic function of the input)
// ---------------------------------------------------------------------
// the highest storable quantum value (65535 / 255)
inline constexpr kotek::uint32_t zircon_csg_bake_position_max_store =
	(1u << ZIRCON_DEF_CSG_BAKE_POSITION_QUANT_BITS) - 1u;

// the documented position error bound for one axis: half an LSB of the
// quantum grid (the dequantization's f64 -> f32 cast at the pool
// boundary adds ~extent * 2^-24, two orders below the u16 LSB — see
// the banner)
inline double zircon_csg_bake_position_error_bound(double extent
) noexcept
{
	return extent * 0.5 /
		static_cast<double>(zircon_csg_bake_position_max_store);
}

// quantize one axis value against [axis_min, axis_min + axis_extent]:
// round((v - min) * (max_store / extent)) clamped to [0, max_store],
// round-half-up (floor(x + 0.5) — exact and platform-independent). A
// degenerate axis (extent <= 0) stores 0 (dequantizes to min exactly)
inline zircon_csg_bake_position_quant_t zircon_csg_bake_quantize_position(
	double value, double axis_min, double axis_extent) noexcept
{
	if (!(axis_extent > 0.0))
		return static_cast<zircon_csg_bake_position_quant_t>(0);

	const double scaled =
		(value - axis_min) *
		(static_cast<double>(zircon_csg_bake_position_max_store) /
			axis_extent);

	// floor(x + 0.5) is round-half-up; the clamp covers the +-1 ulp
	// wobble of the multiply at the far endpoint (p == max can land a
	// hair past max_store)
	double rounded = std::floor(scaled + 0.5);

	if (rounded < 0.0)
		rounded = 0.0;

	if (rounded >
		static_cast<double>(zircon_csg_bake_position_max_store))
	{
		rounded =
			static_cast<double>(zircon_csg_bake_position_max_store);
	}

	return static_cast<zircon_csg_bake_position_quant_t>(rounded);
}

// dequantize: min + (q / max_store) * extent (a degenerate axis yields
// min — the exact value every point of that axis shares)
inline double zircon_csg_bake_dequantize_position(
	zircon_csg_bake_position_quant_t value, double axis_min,
	double axis_extent) noexcept
{
	if (!(axis_extent > 0.0))
		return axis_min;

	return axis_min +
		(static_cast<double>(value) /
			static_cast<double>(zircon_csg_bake_position_max_store)) *
			axis_extent;
}

// the grid cell of one world coordinate (the documented floor rule:
// half-open cells, a boundary value lands in the HIGHER cell)
inline kotek::int32_t zircon_csg_bake_cell_for_coordinate(
	double value) noexcept
{
	return static_cast<kotek::int32_t>(
		std::floor(value / static_cast<double>(
					   ZIRCON_DEF_CSG_BAKE_CHUNK_SIZE_METERS)));
}

// the octahedral normal codec (u8 x2 — see the banner for the error
// analysis). encode: n -> (x, y) / (|x|+|y|+|z|), the z < 0
// hemisphere folded onto the square's back; sign(0) = +1 by
// definition (determinism at the fold seams)
inline void zircon_csg_bake_encode_normal_oct_u8(
	const double* p_normal_xyz, kotek::uint8_t* p_out_2) noexcept
{
	const double x = p_normal_xyz[0];
	const double y = p_normal_xyz[1];
	const double z = p_normal_xyz[2];

	const double l1 = std::fabs(x) + std::fabs(y) + std::fabs(z);

	if (!(l1 > 0.0))
	{
		// a zero normal is degenerate content; the deterministic
		// encoding is the +Z pole, oct (0, 0) -> 128 (loudness is the
		// evaluation's job — it never emits one)
		p_out_2[0] = 128u;
		p_out_2[1] = 128u;
		return;
	}

	double oct_x = x / l1;
	double oct_y = y / l1;

	if (z < 0.0)
	{
		const double sign_x = oct_x >= 0.0 ? 1.0 : -1.0;
		const double sign_y = oct_y >= 0.0 ? 1.0 : -1.0;
		const double folded_x = (1.0 - std::fabs(oct_y)) * sign_x;
		const double folded_y = (1.0 - std::fabs(oct_x)) * sign_y;

		oct_x = folded_x;
		oct_y = folded_y;
	}

	// round((o + 1) * 127.5), round-half-up
	const double scaled_x = (oct_x + 1.0) * 127.5;
	const double scaled_y = (oct_y + 1.0) * 127.5;

	p_out_2[0] = static_cast<kotek::uint8_t>(std::floor(scaled_x + 0.5));
	p_out_2[1] = static_cast<kotek::uint8_t>(std::floor(scaled_y + 0.5));
}

// decode: the stored bytes -> oct in [-1, 1] -> the unfolded
// (unnormalized) vector -> normalized FLOAT output (the pool's vertex
// layout)
inline void zircon_csg_bake_decode_normal_oct_u8(
	const kotek::uint8_t* p_in_2, float* p_out_xyz) noexcept
{
	const double oct_x =
		static_cast<double>(p_in_2[0]) / 127.5 - 1.0;
	const double oct_y =
		static_cast<double>(p_in_2[1]) / 127.5 - 1.0;

	double x = oct_x;
	double y = oct_y;
	double z = 1.0 - std::fabs(oct_x) - std::fabs(oct_y);

	if (z < 0.0)
	{
		const double sign_x = x >= 0.0 ? 1.0 : -1.0;
		const double sign_y = y >= 0.0 ? 1.0 : -1.0;
		const double unfolded_x = (1.0 - std::fabs(y)) * sign_x;
		const double unfolded_y = (1.0 - std::fabs(x)) * sign_y;

		x = unfolded_x;
		y = unfolded_y;
	}

	const double length = std::sqrt(x * x + y * y + z * z);

	if (!(length > 0.0))
	{
		p_out_xyz[0] = 0.0f;
		p_out_xyz[1] = 0.0f;
		p_out_xyz[2] = 1.0f;
		return;
	}

	p_out_xyz[0] = static_cast<float>(x / length);
	p_out_xyz[1] = static_cast<float>(y / length);
	p_out_xyz[2] = static_cast<float>(z / length);
}

// ---------------------------------------------------------------------
// the bake API
// ---------------------------------------------------------------------
// one compound's bake input: the primitive list in the CONFIGURED
// precision (the same descriptor the components fill — the bake
// evaluates it through the A1 core; the compound's identity in the
// pack is its INPUT ARRAY INDEX, documented)
struct zircon_csg_bake_compound_input_t
{
	const zircon_csg_primitive_desc_t<zircon_csg_scalar_t>* p_primitives;
	kotek::uint32_t m_primitive_count;
};

// the bake's observability (the tests pin these; a skipped compound is
// a LOUD degradation — the bake itself still succeeds when at least
// the pack write does)
struct zircon_csg_bake_result_t
{
	kotek::uint32_t m_baked_compound_count;
	kotek::uint32_t m_skipped_compound_count;
	kotek::uint32_t m_emitted_chunk_count;
	kotek::uint32_t m_emitted_vertex_count; // welded, across chunks
	kotek::uint32_t m_emitted_index_count;
	kotek::uint32_t m_material_count;
};

// evaluates every compound through the A1 core, chunks + quantizes the
// meshes and writes the pack (the banner's layout) through the shared
// kpack encoder. p_pack_path is RELATIVE TO THE FILESYSTEM ROOT (the
// engine-root resolution, cwd-independent — the caller creates the
// parent directory). p_scene_name is the entry-name segment (the FILE
// name rule). Hard failures (invalid arguments, the payload/material
// caps, the encoder) return false with a loud error; a compound whose
// evaluation fails, comes out empty, or exceeds the per-compound
// chunk/triangle caps is SKIPPED with a loud warning and counted in
// the result — one broken compound never loses the level bake.
bool zircon_csg_bake_pack(kotek::core::ktkIFileSystem* p_filesystem,
	const kotek::static_path_t& pack_path_relative_to_root,
	const zircon_csg_bake_compound_input_t* p_compounds,
	kotek::uint32_t compound_count, const char* p_scene_name,
	zircon_csg_bake_result_t& out_result) noexcept;
