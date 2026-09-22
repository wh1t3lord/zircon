#pragma once

// zircon_render_chunk_pool — the shared chunk pools of the GPU-driven
// chunked-static-geometry path (task Z24 B1). A chunk = a mesh subrange
// (vertex range + index range) in ONE shared static vertex pool and ONE
// shared static index pool, plus its culling/material record. The class
// is bgfx-FREE on purpose (the gltf-loader pattern): it owns the CPU
// shadows, the range allocators and the cull mirror; the pass
// (zircon_render_graph_pass_model_static_gpu_driven_bgfx) mirrors the
// shadows into bgfx dynamic buffers and drives the GPU cull.
//
// Layout follows the house DOD rule (§2.9) — AoSoA, never one fat AoS
// POD:
//   - the HOT cull stream is the bounds bundle (zircon_chunk_bounds_t =
//     2 x float4 per chunk, one 32-byte AoSoA record — coalesced SIMD
//     plane tests, byte-exact with the GPU upload);
//   - the draw ranges are their own narrow 12-byte records (u16 wherever
//     the pool capacities provably fit — see the capacity comments);
//   - material ids live in a separate u16 array (B1 has no material
//     system — the array is the registration contract; it joins the GPU
//     streams when materials land);
//   - per-chunk state flags are u8 (bit 0 = live).
// Transforms are BAKED at registration (chunks are static; the phase's
// fixtures are identity transforms — a bake with a real matrix is the
// same code path): the pool copy holds world-space positions/normals and
// a world-space AABB, so the draw needs no model matrix and the AABB is
// exact. A runtime transform edit requires re-registration (remove +
// register) — the per-frame dynamic path is the B2 concern.

#include "zircon_render_graph_pass_model_static.h"

#include <kotek.core.containers.filesystem.path/include/kotek_core_containers_filesystem_path.h>

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkIFileSystem;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

// pool capacities — named per the memory-budget rule, sized by comment:
// the vertex pool matches the gltf loader's single-mesh cap
// (zircon_DEF_GLTF_MAX_VERTEX_COUNT = 16384) times four so several
// cached meshes + the synthetic fixture coexist; 65536 vertices keep
// every vertex offset/count addressable in u16 (the narrow ranges
// record) AND keep the pool's 16-bit chunk-local indices exact
#define zircon_DEF_RENDER_CHUNK_POOL_MAX_VERTICES 65536
// the index pool: 3x the vertex cap (the loader's 49152-index mesh is
// 3x its 16384 vertices; a cube chunk is 36/24 = 1.5x) — the 12-byte
// ranges record keeps the index OFFSET in u32 (this pool is deeper
// than 65536), the COUNT stays u16 (a chunk's 16-bit indices can never
// address more)
#define zircon_DEF_RENDER_CHUNK_POOL_MAX_INDICES 196608
// chunk slots: the synthetic grid fixture (8^3 = 512) + world-entity
// chunks with headroom; the free-range lists below are provably bounded
// by slots + 1 (every live allocation came from one split)
#define zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS 1024
#define zircon_DEF_RENDER_CHUNK_POOL_MAX_FREE_RANGES \
	(zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS + 1)

// the AoSoA cull bundle — 2 float4 per chunk, 32 bytes, byte-exact with
// the GPU bounds array (the compute shader reads the same layout; the
// static asserts at the bottom of this header pin the contract):
//   m_aabb_min = (min.x, min.y, min.z, live-flag bits) — the flag rides
//     the spare lane so the GPU cull skips removed chunks with one
//     scalar test (1.0f bits = live, 0 = dead);
//   m_aabb_max = (max.x, max.y, max.z, material-id bits) — the second
//     spare lane pre-wires the material stream
struct zircon_chunk_bounds_t
{
	float m_aabb_min[4];
	float m_aabb_max[4];
};

// the narrow draw-range record (12 bytes): u16 for every field the pool
// capacities bound (vertex offset/count under 65536; index count — a
// chunk's 16-bit indices cap it), u32 only for the deep pool's index
// offset. The GPU upload expands this to one uint4 per chunk
struct zircon_chunk_ranges_t
{
	kotek::uint16_t m_vertex_offset;
	kotek::uint16_t m_vertex_count;
	kotek::uint32_t m_index_offset;
	kotek::uint16_t m_index_count;
	kotek::uint16_t m_spare;
};

// a bounded free-list range allocator over one pool's [0, capacity)
// element space (member of the pool, no statics — one per vertex/index
// pool). First-fit allocation with splitting; free re-inserts sorted and
// coalesces both neighbours. Defragmentation is explicit and amortized:
// compute_defrag_relocations hands the owner the old->new range moves
// (the owner memmoves the shadow + rewrites chunk ranges), then
// apply_defrag_layout collapses the free list to the single tail range.
// The free list can never exceed live-allocations + 1 entries (every
// free range is born from one split or one free), so the static
// capacity is exact, never a guess
class zircon_pool_range_allocator
{
public:
	struct range_t
	{
		kotek::uint32_t m_offset;
		kotek::uint32_t m_count;
	};

	// one live segment's move under defragmentation: the elements
	// [m_old_offset, m_old_offset + m_count) must be relocated to
	// m_new_offset (always m_new_offset < m_old_offset — defrag compacts
	// towards zero)
	struct relocation_t
	{
		kotek::uint32_t m_old_offset;
		kotek::uint32_t m_new_offset;
		kotek::uint32_t m_count;
	};

	zircon_pool_range_allocator(void) = default;

	// (re)arms the allocator: one free range spanning the whole capacity
	void initialize(kotek::uint32_t capacity) noexcept;

	// first-fit: the first free range large enough donates the range from
	// its front (splits when larger). false = no single free range fits
	// (the owner decides: defrag+retry when get_free_total() suffices, or
	// loud overflow). count == 0 is a caller error
	bool allocate(kotek::uint32_t count,
		kotek::uint32_t& out_offset) noexcept;

	// returns a range to the free list (sorted insert + coalescing). The
	// owner guarantees the range is currently allocated (the allocator
	// cannot detect double-frees — documented contract); offset + count
	// past the capacity is a caller error
	bool free(kotek::uint32_t offset, kotek::uint32_t count) noexcept;

	// the defragmentation plan: walks the sorted free list and emits one
	// relocation per LIVE segment (the complement of the free list),
	// shifting each down by the free space before it. Returns the
	// relocation count (0 = already compact); out_new_used_end receives
	// the post-defrag live end (capacity - free_total)
	kotek::uint32_t compute_defrag_relocations(relocation_t* p_out_relocations,
		kotek::uint32_t out_relocations_capacity,
		kotek::uint32_t& out_new_used_end) const noexcept;

	// commits the post-defrag layout after the owner moved the data: the
	// free list collapses to the single tail range [new_used_end,
	// capacity)
	void apply_defrag_layout(kotek::uint32_t new_used_end) noexcept;

	void clear(void) noexcept;

	kotek::uint32_t get_capacity(void) const noexcept;
	// the live-element sum (capacity - free_total) and the fragmentation
	// metric (free range count) for tests/diagnostics
	kotek::uint32_t get_free_total(void) const noexcept;
	kotek::uint32_t get_free_range_count(void) const noexcept;

private:
	kotek::uint32_t m_capacity{0};
	kotek::static_vector_t<range_t,
		zircon_DEF_RENDER_CHUNK_POOL_MAX_FREE_RANGES>
		m_free_ranges; // sorted by m_offset at all times
};

// the chunk pool itself: CPU shadows + allocators + the AoSoA chunk
// tables + the cull mirror. register_chunk bakes the (optional) model
// matrix into the pool copy and computes the world AABB from the baked
// positions; remove_chunk frees the ranges and kills the live flag;
// clear() resets everything. Chunk ids are stable slot indices (u32 in
// the API, u16 slots inside) recycled through a free list; a stale id's
// remove is a loud no-op (the live flag guards it — generations are a
// later refinement, not needed for the B1 lifecycle)
class zircon_render_chunk_pool
{
public:
	// the invalid chunk id (register_chunk's failure value; tests compare
	// against it)
	static constexpr kotek::uint32_t kInvalidChunkId = 0xffffffffu;
	// the live flag's bit pattern in the bounds' spare lane (the shader
	// tests asuint(bounds_min.w) != 0)
	static constexpr kotek::uint8_t kFlagLive = 1 << 0;

	zircon_render_chunk_pool(void);
	~zircon_render_chunk_pool(void) = default;

	// registers one mesh subrange as a chunk: allocates the pool ranges
	// (defragmenting once when fragmented past a failed fit with enough
	// total free — the documented amortized trigger), bakes the model
	// matrix into the shadow copy (nullptr = identity — the phase's
	// fixture path), computes the world AABB from the baked positions.
	// false + loud log on overflow or a caller-error argument (null
	// spans, zero counts, counts past the per-chunk u16/16-bit-index
	// caps); out_chunk_id receives kInvalidChunkId then
	bool register_chunk(const zircon_model_static_vertex_t* p_vertices,
		kotek::uint32_t vertex_count, const kotek::uint16_t* p_indices,
		kotek::uint32_t index_count, const float* p_model_matrix_or_null,
		kotek::uint16_t material_id,
		kotek::uint32_t& out_chunk_id) noexcept;

	// frees the chunk's ranges and marks it dead (the GPU cull skips it
	// from the next table upload). false = stale/invalid id (loud no-op)
	bool remove_chunk(kotek::uint32_t chunk_id) noexcept;

	void clear(void) noexcept;

	// the explicit defragmentation entry (the tests drive it directly;
	// register_chunk calls it internally on the failed-fit trigger).
	// Compacts both pools' live ranges towards zero, rewrites the chunk
	// ranges and marks the pools fully dirty for re-upload
	void defrag_pools(void) noexcept;

	// ---- the A3 pack-loaded variant (task Z25): reads a baked CSG
	// chunk set (the zircon_csg_bake.h format — manifest + per-chunk
	// entries) through the filesystem dispatcher (the priority chain
	// resolves packs vs dirs), dequantizes into the pool's layout
	// (positions float from the u16/u8 normalized quanta against the
	// manifest's f64 per-chunk bounds, normals from the u8 octahedral
	// per-triangle code, the triangle-soup expansion per chunk) and
	// registers every chunk for the B1 GPU-culled path — the game
	// session does ZERO CSG evaluation. pack_path_prefix is RELATIVE
	// TO THE FILESYSTEM ROOT ("csg/<scene>"; the root resolution keeps
	// the reads cwd-independent). All-or-nothing per call: a manifest
	// that fails validation, a corrupt/oversized entry, or a pool
	// capacity breach aborts BEFORE any registration (the capacity
	// pre-check sums the manifest) or at the offending chunk (loud,
	// false; the chunks registered so far stay — clear() resets).
	// out_loaded_chunk_count receives the registered count on success.
	bool load_chunks_from_pack(kotek::core::ktkIFileSystem* p_filesystem,
		const kotek::static_path_t& pack_path_prefix_relative_to_root,
		kotek::uint32_t& out_loaded_chunk_count) noexcept;

	// ---- the CPU mirror of the GPU cull (the unit-test seam AND the
	// pass's cpu-side visible count for the A/B proof): walks the live
	// chunks, tests each AABB against the six planes and returns the
	// visible ids in registration order. The classification is
	// one-to-one with the compute shader's zircon_is_aabb_visible —
	// keep the two in sync
	kotek::uint32_t cull_chunks_against_frustum(const float* p_planes_6x4,
		kotek::uint32_t* p_out_visible_chunk_ids,
		kotek::uint32_t out_visible_capacity) const noexcept;

	// extracts the six world-space frustum planes from a view-projection
	// matrix (column-major float[16], the bx/bgfx convention — row-vector
	// math, clip volume -w<=x<=w, -w<=y<=w, 0<=z<=w for the [0,1] depth
	// backends): left = c3+c0, right = c3-c0, bottom = c3+c1,
	// top = c3-c1, near = c2, far = c3-c2 over the matrix columns c_j =
	// m[j*4..j*4+3], each normalized to unit xyz length. Output plane =
	// (n.x, n.y, n.z, d) with dot(n, p) + d >= 0 meaning inside
	static void extract_frustum_planes(const float* p_view_projection,
		float* p_out_planes_6x4) noexcept;

	// the AABB-vs-six-planes positive-vertex test (the p-vertex is the
	// corner farthest along the plane normal; if IT is outside, the whole
	// box is) — mirrored one-to-one by the compute shader's
	// zircon_is_aabb_visible (model_static_gpu_driven_cull.cs.slang)
	static bool test_aabb_against_frustum(const float* p_planes_6x4,
		const float* p_aabb_min_xyz, const float* p_aabb_max_xyz) noexcept;

	// ---- pass/test accessors (read-only views of the tables; the upload
	// spans are [0, slot_count) for the tables and [0, *_high_water) for
	// the pool shadows)
	const zircon_chunk_bounds_t* get_bounds(void) const noexcept;
	const zircon_chunk_ranges_t* get_ranges(void) const noexcept;
	const kotek::uint16_t* get_material_ids(void) const noexcept;
	kotek::uint32_t get_chunk_slot_count(void) const noexcept;
	kotek::uint32_t get_live_chunk_count(void) const noexcept;
	bool is_chunk_live(kotek::uint32_t chunk_id) const noexcept;

	const zircon_model_static_vertex_t* get_vertex_shadow(void) const noexcept;
	const kotek::uint16_t* get_index_shadow(void) const noexcept;
	kotek::uint32_t get_vertex_high_water(void) const noexcept;
	kotek::uint32_t get_index_high_water(void) const noexcept;

	// the free slot budget (the pack load's capacity pre-check and the
	// tests): live-capable slots still available
	kotek::uint32_t get_free_chunk_slot_count(void) const noexcept;

	// the upload bookkeeping: the pass re-uploads the used ranges when
	// dirty, then clears the flags (registration batches and defrag set
	// them; uploads are rare and bounded — never per-frame)
	bool is_pools_dirty(void) const noexcept;
	bool is_tables_dirty(void) const noexcept;
	void clear_dirty_flags(void) noexcept;

	// the allocators, exposed read-only for the allocator unit proofs
	// (they are members per the no-statics rule)
	const zircon_pool_range_allocator& get_vertex_allocator(void) const noexcept;
	const zircon_pool_range_allocator& get_index_allocator(void) const noexcept;

private:
	// the register_chunk core after a successful range fit
	bool register_chunk_with_fit(const zircon_model_static_vertex_t* p_vertices,
		kotek::uint16_t vertex_count, const kotek::uint16_t* p_indices,
		kotek::uint16_t index_count, const float* p_model_matrix_or_null,
		kotek::uint16_t material_id, kotek::uint32_t vertex_offset,
		kotek::uint32_t index_offset, kotek::uint32_t& out_chunk_id) noexcept;

	// one pool's defrag half (vertices or indices): moves the shadow per
	// the relocation plan and rewrites the matching chunk-range fields
	template <typename Element>
	static void defrag_one_pool(zircon_pool_range_allocator& allocator,
		Element* p_shadow, kotek::uint32_t& io_high_water,
		zircon_chunk_ranges_t* p_ranges, kotek::uint32_t range_count,
		bool is_vertex_pool) noexcept;

private:
	zircon_pool_range_allocator m_vertex_allocator;
	zircon_pool_range_allocator m_index_allocator;

	// the AoSoA chunk tables (slot-indexed; slots recycle through
	// m_free_chunk_slots)
	kotek::static_vector_t<zircon_chunk_bounds_t,
		zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS>
		m_bounds;
	kotek::static_vector_t<zircon_chunk_ranges_t,
		zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS>
		m_ranges;
	kotek::static_vector_t<kotek::uint16_t,
		zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS>
		m_material_ids;
	kotek::static_vector_t<kotek::uint8_t,
		zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS>
		m_flags;
	kotek::static_vector_t<kotek::uint16_t,
		zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS>
		m_free_chunk_slots;

	// the CPU shadows of the two GPU pools (the upload source AND the
	// defrag staging — capacity-bounded per rule 9, this is the
	// documented memory cost of GPU-side defragmentation without a
	// readback path)
	kotek::static_vector_t<zircon_model_static_vertex_t,
		zircon_DEF_RENDER_CHUNK_POOL_MAX_VERTICES>
		m_vertex_shadow;
	kotek::static_vector_t<kotek::uint16_t,
		zircon_DEF_RENDER_CHUNK_POOL_MAX_INDICES>
		m_index_shadow;

	kotek::uint32_t m_vertex_high_water{0};
	kotek::uint32_t m_index_high_water{0};
	kotek::uint32_t m_live_chunk_count{0};
	bool m_pools_dirty{false};
	bool m_tables_dirty{false};
};

// the GPU/CPU layout contract (the compute shader's table mirrors this):
// the bounds bundle is 2 x float4, the ranges record expands to one uint4
static_assert(sizeof(zircon_chunk_bounds_t) == 32,
	"the bounds AoSoA bundle is 2 x float4 per chunk");
static_assert(sizeof(zircon_chunk_ranges_t) == 12,
	"the narrow ranges record is 12 bytes (u16 x 4 + u32 + u16 spare)");
