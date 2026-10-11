#pragma once

// zircon_render_terrain_chunks — the static heightfield terrain's chunk
// builder (task Z24 B5a): the terrain component
// (zircon_component_terrain_static, ecs-side descriptor) is turned into
// chunk content for the EXISTING B1 chunk pool — one chunk per
// world-grid cell, drawn by the GPU-driven pass like every other chunk
// (reuse — never a forked draw path). bgfx-FREE on purpose (the chunk
// pool's own discipline): the module owns the heightmap intake, the
// grid-mesh construction and the registration loop; the pass drives it
// from its one-shot world collect.
//
// THE HEIGHTMAP INTAKE (the simplest honest binary — no image-format
// dependency): a headerless little-endian gray RAW file, row-major
// samples, u8 (kGray8) or u16-LE (kGray16LE) per sample, the size must
// equal res_x * res_y * (1|2) EXACTLY (the component carries the
// dimensions, so the size match IS the integrity check). Per-texel
// height = value/255 (u8) or value/65535 (u16) * height_scale. A
// MISSING or unreadable heightmap fails loud-but-graceful (one warning,
// the terrain is simply absent — the Z23 embedded defaults carry no
// heightfield kind: a heightfield is scene content, not one of the four
// always-answerable engine types, and the terrain-absent degradation is
// the designed answer). The whole file reads in one bounded shot this
// phase (the caps keep it <= 128 KB); chunked streaming of big
// heightfields is the B5b+ concern that arrives with the tessellation
// upgrade.
//
// THE CHUNKING (the seam policy — NO CRACKS BY CONSTRUCTION): the
// heightfield's texel lattice is the vertex lattice (vertex (i,j) sits
// at world x = i*step_x - world_size_x/2, z = j*step_z - world_size_z/2,
// y = height — the terrain is centered on its local XZ origin, the
// entity transform places it). The lattice splits into up to
// zircon_DEF_TERRAIN_CHUNK_CELLS x CELLS grid cells; every chunk owns
// its cells' QUADS and samples the SHARED boundary texel row/column, so
// adjacent chunks compute the boundary vertices from the same texels
// with the same arithmetic — bit-exact shared edge positions (seam
// vertices are duplicated per chunk by design, the CSG-bake precedent).
// Normals are central differences over the GLOBAL heightfield (edges
// clamp), so a boundary vertex's normal is identical in both chunks too
// — no shading seam either. The last cell row/column absorbs the
// remainder when (res-1) % cells != 0 (front-loaded distribution — the
// layout is a fixed function of the resolution, deterministic
// everywhere).
//
// The draw-side consequences are the pool's own: transforms bake at
// registration (a runtime transform edit needs re-registration), the
// AABB is computed tight per chunk by the pool from the baked
// positions, and the GPU frustum cull classifies the chunks like any
// other content.

#include "zircon_render_chunk_pool.h"

#include "../../../../ecs/zircon_component_terrain_static.h"

// the terrain's grid split: up to this many chunk CELLS per side (the
// lattice clamps it when the resolution is smaller). Sized against the
// B1 pool budget: the practical reference heightfield (129x129) splits
// into 8x8 = 64 chunks of 17x17 vertices — 18496 vertices total (~28%
// of the 65536-vertex pool, leaving the shared pool its other content);
// the per-chunk records stay tiny (289 verts / 1536 indices). The
// define, not the component, owns the split (rule 9: scale by
// preprocessor capacities, not by code)
#define zircon_DEF_TERRAIN_CHUNK_CELLS 8

// the heightfield resolution cap per side (texels). Sized by the pool:
// the vertex pool holds 65536, so the largest all-terrain heightfield is
// ~241x241 (61504 baked vertices with the 8x8 edge duplication);
// 256 keeps the intake file <= 128 KB (256*256*2) and the per-chunk
// budgets below small and fixed (the derived caps). Bigger terrains are
// the B5b+ tessellation upgrade (CBT), not a bigger define
#define zircon_DEF_TERRAIN_MAX_RESOLUTION 256

// one chunk's worst-case quad side: the front-loaded distribution gives
// the first cells ceil((res-1)/cells) quads
#define zircon_DEF_TERRAIN_CHUNK_CELL_MAX_QUADS \
	((zircon_DEF_TERRAIN_MAX_RESOLUTION - 1 + zircon_DEF_TERRAIN_CHUNK_CELLS - 1) / \
		zircon_DEF_TERRAIN_CHUNK_CELLS)

// the per-chunk build scratch caps (the caller's scratch is sized by
// these): (quads+1)^2 vertices, quads^2 * 6 indices — 33x33 = 1089
// vertices and 32*32*6 = 6144 indices at the shipped defines, both far
// under the pool's per-chunk u16 caps by construction
#define zircon_DEF_TERRAIN_CHUNK_MAX_VERTICES \
	((zircon_DEF_TERRAIN_CHUNK_CELL_MAX_QUADS + 1) * \
		(zircon_DEF_TERRAIN_CHUNK_CELL_MAX_QUADS + 1))
#define zircon_DEF_TERRAIN_CHUNK_MAX_INDICES \
	(zircon_DEF_TERRAIN_CHUNK_CELL_MAX_QUADS * \
		zircon_DEF_TERRAIN_CHUNK_CELL_MAX_QUADS * 6)

// the intake file cap (256*256 samples * 2 bytes) — the read scratch
// bound, named per rule 9
#define zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES \
	(zircon_DEF_TERRAIN_MAX_RESOLUTION * zircon_DEF_TERRAIN_MAX_RESOLUTION * 2)

// the validated heightfield: a VIEW over the caller's scratch (the
// samples are never copied out — the build consumes them in place and
// the pool copies the produced vertices/indices at registration, so the
// scratch is free once the build returns)
struct zircon_terrain_heightfield_view_t
{
	const kotek::uint8_t* m_p_samples; // res_x * res_y * (1|2) bytes
	kotek::uint16_t m_resolution_x;
	kotek::uint16_t m_resolution_y;
	eZirconTerrainHeightmapFormat m_format;
};

// the per-texel height in meters (the documented mapping: value/255 or
// value/65535 * height_scale; the u16 leg decodes little-endian
// explicitly — never a reinterpret_cast, the LE-codec discipline).
// The coordinate is a programmer contract (clamped by the lattice
// loops, never by this function)
float zircon_terrain_sample_height_meters(
	const zircon_terrain_heightfield_view_t& heightfield,
	kotek::uint16_t texel_x, kotek::uint16_t texel_y,
	float height_scale) noexcept;

// one lattice vertex of the grid mesh: the centered-XZ position (see
// the banner) and the central-difference normal over the GLOBAL
// heightfield (edge texels clamp the stencil — the boundary vertices of
// two adjacent chunks compute identical positions AND normals, the
// no-cracks/no-shading-seam property). The vertex color is opaque white
// (the model_static vertex contract; a terrain texture/material lands
// with the material system, not here)
void zircon_terrain_compute_vertex(
	const zircon_terrain_heightfield_view_t& heightfield,
	kotek::uint16_t texel_x, kotek::uint16_t texel_y, float world_size_x,
	float world_size_z, float height_scale,
	zircon_model_static_vertex_t& out_vertex) noexcept;

// the cell layout: the quad count per side splits into
// min(zircon_DEF_TERRAIN_CHUNK_CELLS, quads) cells, front-loaded (cell c
// gets base + (c < remainder) quads) — the fixed, deterministic layout
// function the chunk count and the build share
kotek::uint16_t zircon_terrain_cells_per_side(
	kotek::uint16_t resolution) noexcept;
void zircon_terrain_cell_quad_range(kotek::uint16_t quads_per_side,
	kotek::uint16_t cells_per_side, kotek::uint16_t cell,
	kotek::uint16_t& out_quad_begin, kotek::uint16_t& out_quad_count) noexcept;

// the total chunk count a resolution produces (cells_x * cells_y) — the
// pass's log and the tests' expectation
kotek::uint32_t zircon_terrain_chunk_count_total(
	kotek::uint16_t resolution_x, kotek::uint16_t resolution_y) noexcept;

// reads + validates the heightmap through the filesystem dispatcher
// (the pack-first priority chain; the caller resolves the path — the
// pass hands a data_game-resolved path, the tests theirs). false on
// EVERY failure, loud, never an assert — user content is not a
// programmer error: null/too-small scratch is the caller's (the
// required size is logged), a missing/unreadable file is one warning
// (the filesystem's own) + this context line, a size mismatch (the file
// is not res_x*res_y*(1|2) bytes), a resolution outside [2,
// zircon_DEF_TERRAIN_MAX_RESOLUTION] or an unknown format are rejected
// before any read
bool zircon_terrain_load_heightfield(
	kotek::core::ktkIFileSystem* p_filesystem,
	const kotek::static_path_t& heightmap_path,
	eZirconTerrainHeightmapFormat format, kotek::uint16_t resolution_x,
	kotek::uint16_t resolution_y, kotek::uint8_t* p_scratch,
	kotek::size_t scratch_capacity,
	zircon_terrain_heightfield_view_t& out_heightfield) noexcept;

// builds the whole heightfield into the chunk pool: one chunk per grid
// cell (the layout above), positions/normals per
// zircon_terrain_compute_vertex, the optional model matrix baked by the
// pool (the entity transform — nullptr is the identity). ALL-OR-NOTHING
// per call: a pre-flight capacity check (exact totals — chunks, the
// factorized vertex sum (res-1+cells) per side, quads*6 indices)
// against the pool's free space rejects the build BEFORE any
// registration, so an over-budget terrain is loud-absent, never
// half-built. The scratches are caller-owned and reused per chunk (the
// derived caps size them; the defines guarantee the fit by
// construction). Returns the registered chunk count (0 on every
// rejection/failure — the reason is always logged)
kotek::uint32_t zircon_terrain_build_chunks(zircon_render_chunk_pool& pool,
	const zircon_terrain_heightfield_view_t& heightfield,
	float world_size_x, float world_size_z, float height_scale,
	kotek::uint16_t material_id, const float* p_model_matrix_or_null,
	zircon_model_static_vertex_t* p_vertex_scratch,
	kotek::uint16_t* p_index_scratch) noexcept;
