#pragma once

// zircon_meshlet_clusterize.h — the nanite-style MESHLET CLUSTERIZER
// (task Z24 phase B3a, the CONTENT side of the nanite path — no GPU work
// this phase). One triangle soup in (positions + per-triangle normals +
// indices + per-triangle materials — the SAME shape the CSG bake emits,
// zircon_csg_bake.h), the clusterized LOD hierarchy out, written as
// .kpack entries through the shared encoder (kpack_write_file — the same
// function the zircon_kpacker tool, the CSG bake and the tests link).
// The future game-side loader (B3b: GPU cluster culling + per-cluster
// LOD selection) reads the pack back through the filesystem dispatcher;
// this header is the format's SINGLE SOURCE OF TRUTH.
//
// ---------------------------------------------------------------------
// THE CLUSTERIZER (the industry's converged approach, the v1 choices
// documented):
//
// LOD0 = SEEDED REGION GROWTH over the shared-vertex adjacency graph.
// The frontier is a LEVEL-GLOBAL FIFO of candidate triangles (the
// meshlette discipline): a cluster pops candidates — breadth-first in
// discovery order, adjacency hop from the seed (the v1 "geodesic-ish"
// metric: unweighted hop count over shared-vertex adjacency; ties
// break by mesh index order through the CSR walk) — while it stays
// inside BOTH budgets, <= ZIRCON_DEF_MESHLET_MAX_TRIANGLES (=124, the
// nanite-converged size) triangles AND <=
// ZIRCON_DEF_MESHLET_MAX_VERTICES (=62, the classic budget) welded
// vertices. 62 vertices keep the LOD0 cluster indices in u8 (the
// budget's reason). A candidate that would break the vertex budget is
// HELD, never lost: the close re-offers the held set to the next
// cluster (a young cluster always fits any single triangle), so the
// next cluster grows from the previous boundary. When the frontier
// runs dry the cluster's own vertices are rescanned through resumable
// per-vertex cursors (each vertex's adjacency list is walked once per
// level in total — the amortized linear bound); a dry refill closes
// the cluster. The seed order is the frontier first, then the primary
// mesh-index cursor over the untouched region — every decision is
// index-ordered (the determinism contract). Region growth — not a
// naive strip order — because compact clusters give tight cull bounds
// and LOD parent locality (the nanite argument; a strip can zig-zag
// across the mesh and produce a loose AABB for the same triangle
// count).
//
// THE LOD HIERARCHY (the DECIMATION-FREE placeholder — the real
// simplifier is explicitly DEFERRED): a real nanite pipeline SIMPLIFIES
// the children into the parent at the same 124/62 budget. B3a proves
// the hierarchy machinery (parent/child links, per-LOD cluster sets,
// the error-metric slot) WITHOUT vertex removal: level L+1 re-groups
// level L's clusters into half as many parents by GREEDY ADJACENCY
// PAIRING — each level-L cluster pairs with its first (level-L emission
// order) unpaired vertex-adjacent neighbor, falling back to the next
// unpaired cluster in emission order; a lone leftover becomes a
// one-child parent. The parent inherits the children's geometry
// UNCHANGED (so the per-cluster error metric is exactly 0.0f this
// phase — the field exists in the format as the SIMPLIFIER'S SLOT; a
// real simplifier fills it with the screen-space error it introduces).
// Pairing guarantees the halving is EXACT (ceil(n/2) parents) and keeps
// parents compact (adjacency-first). Levels build until the cluster
// count reaches 1, the halving stops helping, or
// ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS (=6) is hit.
//
// THE NORMAL CONE (the nanite backface-culling trick): per cluster,
// the axis = the normalized mean of the member triangles' normals; the
// half-angle theta = the largest angular deviation of any member
// normal from the axis; the stored cutoff = sin(theta). Derived
// predicate (the exact math the test pins): with d = the view direction
// (unit, pointing from the camera into the scene), a triangle with
// geometric normal n is front-facing iff dot(d, n) < 0; the cluster's
// whole normal cone lies in the back-facing hemisphere iff
// dot(d, axis) > sin(theta) — so the cluster is culled iff
// dot(d, axis) > cutoff. A cluster whose normals span >= 90 degrees
// (cone mean degenerate, e.g. a closed cube) can never be cone-culled:
// the documented never-cull sentinel is cutoff = 2.0f (with axis 0) —
// the predicate checks cutoff > 1.0f first. Runtime use is B3b; the
// predicate + sentinel live here as testable content-side math.
//
// ---------------------------------------------------------------------
// THE PACK LAYOUT (all little-endian, field-by-field — no struct dumps;
// the LE field codec + the octahedral normal codec are the SHARED
// content codecs from zircon_csg_bake.h — one codec home, reused, never
// forked)
//
// entries, in WRITER (= load/streaming) order:
//   meshlets/<scene>/manifest.bin            the metadata (below)
//   meshlets/<scene>/lod_<l>/cluster_<i>.bin one per cluster, emitted
//       level-DESCENDING (the coarsest level first — see the streaming
//       order below), i = the cluster's index inside its level
// <scene> is validated as a FILE name (1..32 of [a-zA-Z0-9_-], the
// locale language-tag / CSG-bake rule — a path walk is rejected loudly).
//
// manifest.bin:
//   [header] 32 bytes
//     +0   8   magic char[8] = {'Z','M','S','H','L','E','T','1'}
//     +8   4   flags u32 (reserved, must be 0)
//     +12  4   lod_level_count u32 (<= ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS)
//     +16  4   cluster_count_total u32 (across ALL levels)
//     +20  4   mesh_triangle_count u32 (the input; every level's
//              cluster triangle counts sum to it — geometry unchanged)
//     +24  4   child_link_count_total u32
//     +28  4   reserved u32 (must be 0)
//   [level table] lod_level_count x 8 bytes, level-ASCENDING (LOD0
//     first — the natural hierarchy read; the pack ENTRY order is the
//     opposite, see below):
//     +0   4   first_cluster u32 (the level-major cluster index)
//     +4   4   cluster_count u32
//   [cluster records] cluster_count_total x 88 bytes, level-major
//     ascending (= the LOD0 clusters first), each:
//     +0   4   triangle_count u32
//     +4   4   vertex_count u32 (the welded per-cluster count)
//     +8   4   first_child_link u32 (into the link table; 0 for the
//              leaf level — leaves have child_count 0)
//     +12  4   child_count u32 (0 at LOD0, 1..2 above)
//     +16  4   error_metric f32 bits (0.0f this phase — the deferred
//              simplifier's slot, see above)
//     +20  4   cone_cutoff f32 bits (sin(theta); 2.0f = never cull)
//     +24  12  cone_axis f32[3] (0 when never-cull)
//     +36  24  aabb_min f64[3] (world/exact from the source positions)
//     +60  24  aabb_max f64[3]
//     +84  2   material_min u16 (the cluster's material span)
//     +86  2   material_max u16
//   [child link table] child_link_count_total x 4 bytes u32 — the
//     GLOBAL (level-major) cluster indices of every parent's children,
//     each parent's run stored contiguously in parent order
//
// lod_<l>/cluster_<i>.bin:
//   [header] 20 bytes
//     +0   4   magic char[4] = {'M','L','E','T'}
//     +4   4   vertex_count u32 (<= ZIRCON_DEF_MESHLET_MAX_VERTICES <<
//              level — 62 at LOD0, doubling per level)
//     +8   4   triangle_count u32 (<= ZIRCON_DEF_MESHLET_MAX_TRIANGLES
//              << level)
//     +12  4   index_width u32 (1 = u8 at LOD0 — the 62-vert budget
//              guarantees the fit; 2 = u16 at every level above — a
//              deterministic function of the level, stored for the
//              load-side validation)
//     +16  4   reserved u32 (must be 0)
//   [positions] vertex_count x 3 x u16 LE (the meshlet position
//     quantum, ZIRCON_DEF_MESHLET_POSITION_QUANT_BITS = 16 by default /
//     8 the small mode — normalized against the CLUSTER's AABB with
//     the same fixed round-half-up rule + half-LSB error bound the CSG
//     bake documents; a cluster is small so the bound is tight)
//   [normals] triangle_count x 2 x u8 — octahedral per-triangle (the
//     shared codec; the CSG-bake shape)
//   [indices] triangle_count x 3 x index_width — cluster-local,
//     addressing the cluster's own welded positions
//   [materials] triangle_count x u16 LE — per-triangle material ids
//
// ---------------------------------------------------------------------
// THE STREAMING ORDER (the documented choice): pack entries go
// level-DESCENDING — the COARSEST level first, the leaf level last —
// and cluster order inside a level is the emission (growth/pairing)
// order. Rationale: the coarse level is the cheap whole-mesh coverage;
// streaming it first makes the mesh visible everywhere early and the
// finer levels refine (coarse-to-fine is the progressive-loading
// posture). A camera-path refinement order is RUNTIME state, not pack
// order; the pack order stays a fixed function of the content, which
// the determinism contract requires. The manifest's own tables are
// level-ASCENDING (the hierarchy read) — the two orders are deliberate
// and both pinned by tests.
//
// ---------------------------------------------------------------------
// THE DETERMINISM CONTRACT (the same contract as the CSG bake — this
// content goes into packs with hashes): the same input soup clusterizes
// to byte-identical pack output. Every decision is index-ordered (seed
// scan, CSR walk order, pairing scan), all math is IEEE double/float
// with fixed evaluation order (no FPU-dependent reductions), the
// quantization uses the fixed round-half-up rule, and the pack writer is
// deterministic (zstd at a fixed level, no timestamps). The test
// clusterizes twice and memcmps the packs.

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.vector/include/kotek_core_containers_vector.h>
#include <kotek.core.containers.filesystem.path/include/kotek_core_containers_filesystem_path.h>

// std::floor / std::sqrt in the pure-POD quantization + cone kernels
// (the chunk-pool / CSG-bake precedent: plain C math inside the
// pure-POD kernels)
#include <cmath>

// the shared content codecs: the little-endian field store/load helpers
// and the u8 octahedral normal codec (one codec home — reused here,
// never forked)
#include "zircon_csg_bake.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkIFileSystem;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

// ---------------------------------------------------------------------
// the named capacities (rule 9: named, sized by comment, raised by
// measurement)
// ---------------------------------------------------------------------
// the nanite-converged cluster triangle budget (every level's budget is
// this << level — the placeholder pairing bounds the parent sizes by
// construction: two children at level L never exceed 2x the L budget)
#define ZIRCON_DEF_MESHLET_MAX_TRIANGLES 124
// the classic welded-vertex budget: 62 vertices keep the LOD0 cluster
// indices in u8 (the budget's reason); parents double it per level
#define ZIRCON_DEF_MESHLET_MAX_VERTICES 62
// the LOD hierarchy depth cap (LOD0 = full detail + 5 placeholder
// levels)
#define ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS 6
// the TOTAL cluster count across ALL levels of one scene: the pack
// reader's entry budget (KOTEK_DEF_FILESYSTEM_PACK_MAX_ENTRIES = 4096)
// minus the manifest entry — a pack over the cap is rejected at mount,
// so the clusterizer refuses to emit one (a LOD0 overflow is a HARD
// failure; a mid-hierarchy overflow truncates the remaining LEVELS
// loudly — the pack stays loadable, just shallower)
#define ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE 4090
// the input mesh caps: the LOD0 cluster count is bounded by half the
// scene budget (every level re-groups the SAME geometry — the
// placeholder's cost — so the total across levels is < 2x the LOD0
// count), and a LOD0 cluster never exceeds the 124-triangle budget
#define ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES \
	((ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE / 2) * \
		ZIRCON_DEF_MESHLET_MAX_TRIANGLES)
// a fully disconnected triangle soup carries 3 vertices per triangle
// (the adjacency tables size from this)
#define ZIRCON_DEF_MESHLET_MAX_MESH_VERTICES \
	(ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES * 3)
#define ZIRCON_DEF_MESHLET_MAX_MESH_INDICES \
	(ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES * 3)
// the meshlet position quantization: 16 (u16 normalized, the default)
// or 8 (u8 normalized, the small-mode bandwidth option — the same error
// analysis the CSG bake documents applies: half an LSB of the quantum
// grid per axis)
#ifndef ZIRCON_DEF_MESHLET_POSITION_QUANT_BITS
	#define ZIRCON_DEF_MESHLET_POSITION_QUANT_BITS 16
#endif
#if ZIRCON_DEF_MESHLET_POSITION_QUANT_BITS != 8 && \
	ZIRCON_DEF_MESHLET_POSITION_QUANT_BITS != 16
	#error "ZIRCON_DEF_MESHLET_POSITION_QUANT_BITS must be 8 or 16"
#endif
// the scene name segment of every entry name (the locale tag rule)
#define ZIRCON_DEF_MESHLET_MAX_SCENE_NAME_LENGTH 32
// the total cluster-bin payload residency of one bake: the kpack
// encoder API is entry-resident (every entry's bytes must live until
// kpack_write_file returns), so the bins are accumulated in one bounded
// heap store — 64 MB is the CSG-bake precedent and covers the worst
// case here (every level re-groups the full mesh: ~253k triangles x 6
// levels x ~28 bytes of quantized tables); exceeding it is a loud hard
// failure (a content-size error the caller must see)
#define ZIRCON_DEF_MESHLET_MAX_TOTAL_PAYLOAD (64u * 1024u * 1024u)
// one entry name ("meshlets/<32>/lod_<l>/cluster_<4089>.bin" plus NUL)
#define ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH 96

// the stored position quantum type
#if ZIRCON_DEF_MESHLET_POSITION_QUANT_BITS == 8
using zircon_meshlet_position_quant_t = kotek::uint8_t;
#else
using zircon_meshlet_position_quant_t = kotek::uint16_t;
#endif

// the format's magic values (the version rides inside, the kpack
// posture: a format change bumps the digits)
inline constexpr char zircon_meshlet_manifest_magic[8] = {
	'Z', 'M', 'S', 'H', 'L', 'E', 'T', '1'};
inline constexpr char zircon_meshlet_cluster_magic[4] = {
	'M', 'L', 'E', 'T'};

// the fixed record sizes (the layouts in the banner)
inline constexpr kotek::uint32_t zircon_meshlet_manifest_header_size = 32;
inline constexpr kotek::uint32_t zircon_meshlet_manifest_level_size = 8;
inline constexpr kotek::uint32_t zircon_meshlet_manifest_record_size = 88;
inline constexpr kotek::uint32_t zircon_meshlet_cluster_header_size = 20;

// the level budgets (the pairing bounds the parents by construction —
// the shifts stay inside u32 for the level cap of 6)
inline kotek::uint32_t zircon_meshlet_max_tris_for_level(
	kotek::uint32_t level) noexcept
{
	return static_cast<kotek::uint32_t>(ZIRCON_DEF_MESHLET_MAX_TRIANGLES)
		<< level;
}

inline kotek::uint32_t zircon_meshlet_max_verts_for_level(
	kotek::uint32_t level) noexcept
{
	return static_cast<kotek::uint32_t>(ZIRCON_DEF_MESHLET_MAX_VERTICES)
		<< level;
}

// ---------------------------------------------------------------------
// the meshlet position quantum (the same fixed rounding rule + error
// analysis the CSG bake documents: round-half-up
// (floor(x + 0.5)), clamped, half-LSB bound per axis; a degenerate axis
// (extent 0) stores 0 and dequantizes to the bound exactly)
// ---------------------------------------------------------------------
inline constexpr kotek::uint32_t zircon_meshlet_position_max_store =
	(1u << ZIRCON_DEF_MESHLET_POSITION_QUANT_BITS) - 1u;

inline zircon_meshlet_position_quant_t zircon_meshlet_quantize_position(
	double value, double axis_min, double axis_extent) noexcept
{
	if (!(axis_extent > 0.0))
		return static_cast<zircon_meshlet_position_quant_t>(0);

	const double scaled =
		(value - axis_min) *
		(static_cast<double>(zircon_meshlet_position_max_store) /
			axis_extent);

	double rounded = std::floor(scaled + 0.5);

	if (rounded < 0.0)
		rounded = 0.0;

	if (rounded > static_cast<double>(zircon_meshlet_position_max_store))
	{
		rounded =
			static_cast<double>(zircon_meshlet_position_max_store);
	}

	return static_cast<zircon_meshlet_position_quant_t>(rounded);
}

inline double zircon_meshlet_dequantize_position(
	zircon_meshlet_position_quant_t value, double axis_min,
	double axis_extent) noexcept
{
	if (!(axis_extent > 0.0))
		return axis_min;

	return axis_min +
		(static_cast<double>(value) /
			static_cast<double>(zircon_meshlet_position_max_store)) *
			axis_extent;
}

inline double zircon_meshlet_position_error_bound(double extent
) noexcept
{
	return extent * 0.5 /
		static_cast<double>(zircon_meshlet_position_max_store);
}

// ---------------------------------------------------------------------
// the normal-cone backface predicate (the derivation in the banner):
// culled iff dot(view_direction, axis) > cutoff; the never-cull
// sentinel is cutoff > 1.0f (stored as 2.0f). d = the unit view
// direction, pointing from the camera into the scene
// ---------------------------------------------------------------------
inline bool zircon_meshlet_cone_culls(float view_direction_x,
	float view_direction_y, float view_direction_z,
	const float* p_cone_axis, float cone_cutoff) noexcept
{
	if (cone_cutoff > 1.0f)
		return false; // the never-cull sentinel

	return (view_direction_x * p_cone_axis[0] +
			   view_direction_y * p_cone_axis[1] +
			   view_direction_z * p_cone_axis[2]) > cone_cutoff;
}

// ---------------------------------------------------------------------
// the clusterizer API
// ---------------------------------------------------------------------
// one mesh's clusterizer input: the triangle soup in EXACT doubles with
// per-triangle flat normals + materials — the same shape the CSG bake
// emits (a caller holding the CSG-bake chunk data builds this directly;
// the glTF loader's welded meshes fit the same descriptor)
struct zircon_meshlet_mesh_input_t
{
	const double* p_positions; // m_vertex_count x 3 (world space)
	kotek::uint32_t m_vertex_count;
	const kotek::uint32_t* p_indices; // m_triangle_count x 3, mesh-local
	const double* p_normals; // m_triangle_count x 3 (per-triangle flat)
	const kotek::uint16_t* p_materials; // m_triangle_count
	kotek::uint32_t m_triangle_count;
};

// one cluster's content-side metadata (the manifest record's mirror;
// the level-major index space — the level table maps level -> range)
struct zircon_meshlet_cluster_t
{
	kotek::uint32_t m_triangle_count;
	kotek::uint32_t m_vertex_count;
	kotek::uint32_t m_first_triangle; // into m_cluster_triangles
	kotek::uint32_t m_first_child_link; // into m_child_links
	kotek::uint32_t m_child_count;
	float m_error_metric; // 0.0f — the deferred simplifier's slot
	float m_cone_cutoff; // sin(theta); 2.0f = never cull
	float m_cone_axis[3];
	double m_aabb_min[3];
	double m_aabb_max[3];
	kotek::uint16_t m_material_min;
	kotek::uint16_t m_material_max;
	kotek::uint8_t m_level;
};

// the clusterized LOD hierarchy (the AoSoA tables per rule 9; owned by
// the caller — heap-allocated, the fixture rule: the triangle-reference
// table is ~6 MB at the caps)
struct zircon_meshlet_lod_set_t
{
	kotek::static_vector_t<zircon_meshlet_cluster_t,
		ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE>
		m_clusters;
	// level -> the level-major first cluster index; the sentinel
	// entry [lod_level_count] == the cluster total (a level's count is
	// the adjacent difference — the manifest writes explicit pairs)
	kotek::static_vector_t<kotek::uint32_t,
		ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS + 1>
		m_level_first_cluster;
	kotek::static_vector_t<kotek::uint32_t,
		ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE>
		m_child_links;
	// every cluster's mesh-local triangle list, contiguous per cluster
	// in cluster order (the placeholder re-groups the SAME geometry per
	// level — the per-level sum is exactly the mesh triangle count)
	kotek::static_vector_t<kotek::uint32_t,
		ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES *
		ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS>
		m_cluster_triangles;
};

// the bake's observability (the tests pin these)
struct zircon_meshlet_bake_result_t
{
	kotek::uint32_t m_emitted_cluster_count; // across all levels
	kotek::uint32_t m_emitted_lod_level_count;
	kotek::uint32_t m_emitted_child_link_count;
	kotek::uint32_t m_mesh_triangle_count;
};

// the pure clusterizer (no filesystem): the mesh -> the LOD hierarchy.
// Hard failures (invalid input, the cluster-table caps) return false
// with a loud error; a mid-hierarchy cluster-table overflow TRUNCATES
// the remaining levels loudly and keeps the shallower-but-loadable
// hierarchy (the graceful half of the capacity contract)
bool zircon_meshlet_clusterize_lod_hierarchy(
	const zircon_meshlet_mesh_input_t& mesh,
	zircon_meshlet_lod_set_t& out_set) noexcept;

// clusterizes + writes the pack (the banner's layout) through the
// shared kpack encoder. p_pack_path is RELATIVE TO THE FILESYSTEM ROOT
// (the engine-root resolution, cwd-independent — the caller creates the
// parent directory). p_scene_name is the entry-name segment (the FILE
// name rule). Hard failures (invalid arguments, a LOD0 cluster-table
// overflow, the payload cap, the encoder) return false with a loud
// error
bool zircon_meshlet_clusterize_pack(kotek::core::ktkIFileSystem* p_filesystem,
	const kotek::static_path_t& pack_path_relative_to_root,
	const zircon_meshlet_mesh_input_t& mesh, const char* p_scene_name,
	zircon_meshlet_bake_result_t& out_result) noexcept;
