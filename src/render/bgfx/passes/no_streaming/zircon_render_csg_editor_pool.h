#pragma once

// zircon_render_csg_editor_pool — the ONE-draw-call dynamic geometry
// pool of the editor CSG pass (task Z25 A2). ALL editor compounds
// share ONE dynamic vertex pool and ONE dynamic index pool (the
// plan's "one draw call total": the pass submits the index buffer's
// used range once, the compounds are pool ranges inside it). The
// class is bgfx-FREE on purpose (the chunk-pool precedent): it owns
// the CPU shadows, the range allocators, the per-compound records
// and the changed-range bookkeeping; the pass mirrors the changed
// ranges into bgfx dynamic buffers (createDynamicVertexBuffer/
// createDynamicIndexBuffer + bgfx::update of the CHANGED spans only)
// and issues the single submit.
//
// Layout decisions (rule 9, the DOD bar):
//   - the CSG mesh arrives as a WELDED position set + per-triangle
//     flat normals + indices (the evaluation core's natural output —
//     brush shading IS flat). Welded positions cannot carry
//     per-triangle normals, so the pool stores a TRIANGLE SOUP:
//     3 vertices per triangle, each with the triangle's normal.
//     The vertex pool capacity therefore equals the evaluation's
//     index cap (3 x the triangle cap), not its welded vertex cap;
//     positions are looked up per index at upsert time.
//   - indices are pool-GLOBAL u32 (one draw call over the whole
//     pool — the u32 index pool, the pass sets BGFX_BUFFER_INDEX32);
//     the records keep both spans in u32.
//   - the per-compound record is the narrow 16-byte pair of spans;
//     compound ids are a separate u64 array, live flags a u8 array.
//   - freed index spans are re-filled with ZERO indices — degenerate
//     (0,0,0) triangles rasterize nothing, so the holes between the
//     live compounds stay invisible inside the single draw. Vertices
//     of freed spans are simply unreferenced (never cleared — a
//     stale vertex is bytes nobody reads).
//
// Rebuild semantics: upsert_compound_mesh writes ONLY the rebuilt
// compound's spans (byte-pinned against the other compounds — a
// test-pinned contract); every shadow write appends a dirty span;
// the pass takes the spans (take_dirty_spans), uploads them and the
// pool clears the bookkeeping. Span-list overflow degrades to ONE
// full-range span per pool (a complete re-upload), never a growth.
//
// Defragmentation is explicit and amortized (the chunk-pool
// contract): a failed range fit with enough TOTAL free space
// triggers ONE defrag (compaction towards zero + record rewrite +
// the freed index tail re-zeroed + a full-range re-upload), then the
// allocation retries; a second failure is a loud overflow error.
// The allocators are zircon_pool_range_allocator reused from
// zircon_render_chunk_pool.h — its static free-list capacity is the
// chunk pool's (slots + 1) bound, which amply covers this pool's
// smaller compound table (documented, never forked).

#include "zircon_render_chunk_pool.h"

// the evaluation caps the pool capacities are derived from
#include "../../../../ecs/zircon_csg_defs.h"

// pool capacities — named per the memory-budget rule, sized by
// comment: the editor's live CSG compound count is far below the
// compound cap; the vertex/index caps equal the evaluation core's
// per-compound output caps (65536 triangles x 3) so ANY single
// compound evaluation fits the pool; several compounds of that size
// cannot coexist (an upsert past the fit is the loud overflow class)
#define zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_VERTICES \
	ZIRCON_DEF_CSG_MAX_INDICES_PER_EVALUATION
#define zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_INDICES \
	ZIRCON_DEF_CSG_MAX_INDICES_PER_EVALUATION
// the editor's live compound count with headroom (the chunk pool's
// 1024 is the baked game scale — the editor works in dozens)
#define zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_COMPOUNDS 64
// the changed-span bookkeeping between two uploads: an edit burst
// appends a handful of spans; overflow degrades to the full-range
// re-upload (see the header)
#define zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_DIRTY_SPANS 256

// the neutral vertex color of the soup until the material system
// lands (white ABGR — the lighting contract's modulator)
#define zircon_DEF_RENDER_CSG_EDITOR_POOL_VERTEX_COLOR_ABGR \
	0xffffffffu

// one pending GPU upload: a half-open element span of one pool
struct zircon_csg_editor_pool_dirty_span_t
{
	kotek::uint32_t m_offset; // elements
	kotek::uint32_t m_count;
	kotek::uint8_t m_is_index_pool; // 0 = vertices, 1 = indices
};

class zircon_render_csg_editor_pool
{
public:
	zircon_render_csg_editor_pool(void) = default;
	~zircon_render_csg_editor_pool(void) = default;

	// (re)arms the allocators and drops every record
	void initialize(void) noexcept;
	void clear(void) noexcept;

	// installs (or replaces) one compound's evaluated mesh: expands
	// the triangle soup into the shadow, allocates the two ranges
	// (defragmenting once on the documented amortized trigger) and
	// appends the dirty spans. The OTHER compounds' shadow bytes are
	// untouched (byte-pinned). A zero-triangle mesh removes the
	// compound (nothing to draw). false + loud log on a caller error
	// or an overflow
	bool upsert_compound_mesh(
		kotek::uint64_t compound_id,
		const float* p_positions_xyz,
		kotek::uint32_t position_count,
		const float* p_normals_xyz_per_triangle,
		const kotek::uint32_t* p_indices,
		kotek::uint32_t triangle_count
	) noexcept;

	// frees the compound's ranges; the freed index span is re-filled
	// with degenerate zeros so the single draw stays correct. false
	// = unknown id (loud no-op)
	bool remove_compound(kotek::uint64_t compound_id) noexcept;

	// the explicit defragmentation (the upsert trigger calls it
	// internally; the tests drive it directly): compacts both pools
	// towards zero, rewrites the records, re-zeros the freed index
	// tail, marks the used ranges fully dirty (one re-upload)
	void defrag_pools(void) noexcept;

	// the upload bookkeeping: copies the pending spans (capped —
	// overflow degrades to one full-range span per pool), then clears
	// the list. Returns the span count written
	kotek::uint32_t take_dirty_spans(
		zircon_csg_editor_pool_dirty_span_t* p_out_spans,
		kotek::uint32_t out_capacity
	) noexcept;

	// ---- pass/test accessors (read-only views of the tables)
	kotek::uint64_t get_compound_id(kotek::uint32_t slot) const noexcept;
	bool is_compound_live(kotek::uint32_t slot) const noexcept;
	kotek::uint32_t get_record_count(void) const noexcept;
	kotek::uint32_t get_live_compound_count(void) const noexcept;

	const zircon_model_static_vertex_t* get_vertex_shadow(
		void) const noexcept;
	const kotek::uint32_t* get_index_shadow(void) const noexcept;
	kotek::uint32_t get_vertex_high_water(void) const noexcept;
	kotek::uint32_t get_index_high_water(void) const noexcept;

	// the record spans (vertex_offset/count, index_offset/count) of
	// a slot — the draw/byte-pin contract
	void get_record_ranges(kotek::uint32_t slot,
		kotek::uint32_t& out_vertex_offset,
		kotek::uint32_t& out_vertex_count,
		kotek::uint32_t& out_index_offset,
		kotek::uint32_t& out_index_count) const noexcept;

	// the allocators, exposed read-only for the allocator unit proofs
	const zircon_pool_range_allocator& get_vertex_allocator(
		void) const noexcept;
	const zircon_pool_range_allocator& get_index_allocator(
		void) const noexcept;

private:
	kotek::uint32_t find_compound_slot(
		kotek::uint64_t compound_id) const noexcept;
	void append_dirty_span(bool is_index_pool,
		kotek::uint32_t offset, kotek::uint32_t count) noexcept;
	void release_ranges(kotek::uint32_t slot) noexcept;

private:
	zircon_pool_range_allocator m_vertex_allocator;
	zircon_pool_range_allocator m_index_allocator;

	// the SoA compound tables (slot-indexed)
	kotek::static_vector_t<kotek::uint64_t,
		zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_COMPOUNDS>
		m_compound_ids;
	kotek::static_vector_t<kotek::uint32_t,
		zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_COMPOUNDS * 4>
		m_ranges; // 4 per slot: v_offset, v_count, i_offset, i_count
	kotek::static_vector_t<kotek::uint8_t,
		zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_COMPOUNDS>
		m_live_flags;

	// the CPU shadows of the two GPU pools (capacity-bounded per
	// rule 9 — the documented memory cost of the single-draw
	// dynamic pool; ~6 MB total)
	kotek::static_vector_t<zircon_model_static_vertex_t,
		zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_VERTICES>
		m_vertex_shadow;
	kotek::static_vector_t<kotek::uint32_t,
		zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_INDICES>
		m_index_shadow;

	kotek::static_vector_t<zircon_csg_editor_pool_dirty_span_t,
		zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_DIRTY_SPANS>
		m_dirty_spans;

	kotek::uint32_t m_vertex_high_water{0};
	kotek::uint32_t m_index_high_water{0};
	kotek::uint32_t m_live_compound_count{0};
	bool m_is_full_upload_pending{false};
};
