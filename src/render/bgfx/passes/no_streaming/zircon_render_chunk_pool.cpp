#include "zircon_render_chunk_pool.h"

#include <kotek.core.api/include/kotek_api.h>
#include <kotek.core.filesystem.pack/include/kotek_kpack_format.h>

// the A3 bake format (task Z25): the pack-loaded chunk variant reads
// the manifest/chunk entries through the shared format helpers
#include "../../../../core/zircon_csg_bake.h"

#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
// zircon_pool_range_allocator
// ---------------------------------------------------------------------------

void zircon_pool_range_allocator::initialize(
	kotek::uint32_t capacity) noexcept
{
	this->m_capacity = capacity;
	this->m_free_ranges.clear();

	if (capacity)
	{
		this->m_free_ranges.push_back(range_t{0, capacity});
	}
}

bool zircon_pool_range_allocator::allocate(
	kotek::uint32_t count, kotek::uint32_t& out_offset) noexcept
{
	KOTEK_ASSERT(count, "allocate(0) is a caller error");

	out_offset = 0;

	if (count == 0 || count > this->m_capacity)
		return false;

	for (kotek::uint32_t range_index = 0;
		 range_index < this->m_free_ranges.size(); ++range_index)
	{
		range_t& range = this->m_free_ranges[range_index];

		if (range.m_count < count)
			continue;

		out_offset = range.m_offset;
		range.m_offset += count;
		range.m_count -= count;

		if (range.m_count == 0)
		{
			this->m_free_ranges.erase(
				this->m_free_ranges.begin() + range_index);
		}

		return true;
	}

	return false;
}

bool zircon_pool_range_allocator::free(
	kotek::uint32_t offset, kotek::uint32_t count) noexcept
{
	KOTEK_ASSERT(count, "free(0) is a caller error");
	KOTEK_ASSERT(offset + count <= this->m_capacity,
		"the freed range escapes the pool");

	if (count == 0 || offset + count > this->m_capacity)
		return false;

	// sorted insert by offset, then coalesce with both neighbours
	kotek::uint32_t insert_index = 0;

	while (insert_index < this->m_free_ranges.size() &&
		this->m_free_ranges[insert_index].m_offset < offset)
	{
		++insert_index;
	}

	range_t inserted{offset, count};

	// merge with the previous range when adjacent
	if (insert_index > 0)
	{
		range_t& previous = this->m_free_ranges[insert_index - 1];

		if (previous.m_offset + previous.m_count == inserted.m_offset)
		{
			previous.m_count += inserted.m_count;
			inserted = previous;
			this->m_free_ranges.erase(
				this->m_free_ranges.begin() + (insert_index - 1));
			--insert_index;
		}
	}

	// merge with the next range when adjacent
	if (insert_index < this->m_free_ranges.size())
	{
		range_t& next = this->m_free_ranges[insert_index];

		if (inserted.m_offset + inserted.m_count == next.m_offset)
		{
			inserted.m_count += next.m_count;
			this->m_free_ranges.erase(
				this->m_free_ranges.begin() + insert_index);
		}
	}

	if (insert_index >= this->m_free_ranges.size())
	{
		this->m_free_ranges.push_back(inserted);
	}
	else
	{
		this->m_free_ranges.insert(
			this->m_free_ranges.begin() + insert_index, inserted);
	}

	return true;
}

kotek::uint32_t zircon_pool_range_allocator::compute_defrag_relocations(
	relocation_t* p_out_relocations, kotek::uint32_t out_relocations_capacity,
	kotek::uint32_t& out_new_used_end) const noexcept
{
	out_new_used_end = 0;

	kotek::uint32_t written_count = 0;
	kotek::uint32_t shift = 0;
	kotek::uint32_t live_segment_begin = 0;

	for (const range_t& free_range : this->m_free_ranges)
	{
		if (free_range.m_offset > live_segment_begin && shift > 0)
		{
			// a live segment that must move
			if (p_out_relocations &&
				written_count < out_relocations_capacity)
			{
				p_out_relocations[written_count] =
					relocation_t{live_segment_begin,
						live_segment_begin - shift,
						free_range.m_offset - live_segment_begin};
			}

			++written_count;
		}

		shift += free_range.m_count;
		live_segment_begin = free_range.m_offset + free_range.m_count;
	}

	// the tail live segment past the last free range
	if (this->m_capacity > live_segment_begin && shift > 0)
	{
		if (p_out_relocations && written_count < out_relocations_capacity)
		{
			p_out_relocations[written_count] =
				relocation_t{live_segment_begin, live_segment_begin - shift,
					this->m_capacity - live_segment_begin};
		}

		++written_count;
	}

	out_new_used_end = this->m_capacity - shift;

	return written_count;
}

void zircon_pool_range_allocator::apply_defrag_layout(
	kotek::uint32_t new_used_end) noexcept
{
	KOTEK_ASSERT(new_used_end <= this->m_capacity,
		"the defrag layout escapes the pool");

	this->m_free_ranges.clear();

	if (new_used_end < this->m_capacity)
	{
		this->m_free_ranges.push_back(
			range_t{new_used_end, this->m_capacity - new_used_end});
	}
}

void zircon_pool_range_allocator::clear(void) noexcept
{
	this->m_capacity = 0;
	this->m_free_ranges.clear();
}

kotek::uint32_t zircon_pool_range_allocator::get_capacity(
	void) const noexcept
{
	return this->m_capacity;
}

kotek::uint32_t zircon_pool_range_allocator::get_free_total(
	void) const noexcept
{
	kotek::uint32_t total = 0;

	for (const range_t& range : this->m_free_ranges)
	{
		total += range.m_count;
	}

	return total;
}

kotek::uint32_t zircon_pool_range_allocator::get_free_range_count(
	void) const noexcept
{
	return static_cast<kotek::uint32_t>(this->m_free_ranges.size());
}

// ---------------------------------------------------------------------------
// zircon_render_chunk_pool
// ---------------------------------------------------------------------------

zircon_render_chunk_pool::zircon_render_chunk_pool(void)
{
	this->clear();
}

bool zircon_render_chunk_pool::register_chunk(
	const zircon_model_static_vertex_t* p_vertices,
	kotek::uint32_t vertex_count, const kotek::uint16_t* p_indices,
	kotek::uint32_t index_count, const float* p_model_matrix_or_null,
	kotek::uint16_t material_id, kotek::uint32_t& out_chunk_id) noexcept
{
	out_chunk_id = kInvalidChunkId;

	if (p_vertices == nullptr || p_indices == nullptr ||
		vertex_count == 0 || index_count == 0)
	{
		KOTEK_MESSAGE_ERROR(
			"[chunk_pool] register_chunk: null spans or zero counts — "
			"caller error");
		return false;
	}

	// the narrow ranges record keeps these in u16 — a chunk's 16-bit
	// indices already cap it below 65536 (the -1 headroom keeps the
	// count itself representable)
	if (vertex_count > 65535 || index_count > 65535)
	{
		KOTEK_MESSAGE_ERROR(
			"[chunk_pool] register_chunk: {} vertices / {} indices exceed "
			"the per-chunk u16 cap — split the mesh first",
			vertex_count, index_count);
		return false;
	}

	kotek::uint32_t vertex_offset = 0;
	kotek::uint32_t index_offset = 0;

	bool vertex_fit =
		this->m_vertex_allocator.allocate(vertex_count, vertex_offset);
	bool index_fit =
		this->m_index_allocator.allocate(index_count, index_offset);

	if ((vertex_fit && index_fit) == false)
	{
		// roll back the half that fit, defragment both pools (the
		// documented amortized trigger: a failed fit with enough total
		// free), retry once
		if (vertex_fit)
			this->m_vertex_allocator.free(vertex_offset, vertex_count);
		if (index_fit)
			this->m_index_allocator.free(index_offset, index_count);

		const bool worth_defrag =
			(this->m_vertex_allocator.get_free_total() >= vertex_count) &&
			(this->m_index_allocator.get_free_total() >= index_count);

		if (worth_defrag)
		{
			KOTEK_MESSAGE(
				"[chunk_pool] fragmented past a failed fit — defragmenting "
				"the pools ({} free vertex ranges, {} free index ranges)",
				this->m_vertex_allocator.get_free_range_count(),
				this->m_index_allocator.get_free_range_count());

			this->defrag_pools();

			vertex_fit = this->m_vertex_allocator.allocate(
				vertex_count, vertex_offset);
			index_fit = this->m_index_allocator.allocate(
				index_count, index_offset);
		}
	}

	if ((vertex_fit && index_fit) == false)
	{
		KOTEK_MESSAGE_ERROR(
			"[chunk_pool] register_chunk OVERFLOW: {} + {} vertices, "
			"{} + {} indices do not fit (capacities {} / {} — raise "
			"zircon_DEF_RENDER_CHUNK_POOL_MAX_VERTICES/INDICES)",
			vertex_count, this->m_vertex_high_water, index_count,
			this->m_index_high_water,
			zircon_DEF_RENDER_CHUNK_POOL_MAX_VERTICES,
			zircon_DEF_RENDER_CHUNK_POOL_MAX_INDICES);
		return false;
	}

	return this->register_chunk_with_fit(p_vertices,
		static_cast<kotek::uint16_t>(vertex_count), p_indices,
		static_cast<kotek::uint16_t>(index_count), p_model_matrix_or_null,
		material_id, vertex_offset, index_offset, out_chunk_id);
}

bool zircon_render_chunk_pool::register_chunk_with_fit(
	const zircon_model_static_vertex_t* p_vertices,
	kotek::uint16_t vertex_count, const kotek::uint16_t* p_indices,
	kotek::uint16_t index_count, const float* p_model_matrix_or_null,
	kotek::uint16_t material_id, kotek::uint32_t vertex_offset,
	kotek::uint32_t index_offset, kotek::uint32_t& out_chunk_id) noexcept
{
	// the chunk slot: recycle or grow
	kotek::uint32_t slot;

	if (this->m_free_chunk_slots.empty() == false)
	{
		slot = this->m_free_chunk_slots.back();
		this->m_free_chunk_slots.pop_back();
	}
	else
	{
		if (this->m_bounds.size() >= zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS)
		{
			// roll the ranges back — the slot budget is spent
			this->m_vertex_allocator.free(vertex_offset, vertex_count);
			this->m_index_allocator.free(index_offset, index_count);

			KOTEK_MESSAGE_ERROR(
				"[chunk_pool] register_chunk OVERFLOW: the chunk table is "
				"full ({} slots — raise "
				"zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS)",
				zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS);
			return false;
		}

		slot = static_cast<kotek::uint32_t>(this->m_bounds.size());

		this->m_bounds.push_back(zircon_chunk_bounds_t{});
		this->m_ranges.push_back(zircon_chunk_ranges_t{});
		this->m_material_ids.push_back(0);
		this->m_flags.push_back(0);
	}

	// bake the model matrix into the shadow copy (identity when the
	// caller passes none — the phase's fixture path); normals take the
	// 3x3 only (the model_static no-inverse-transpose approximation —
	// the fragment stage normalizes). The matrix is bx storage —
	// row-major with row-vector math (translation at [12..14]), the
	// exact byte layout the shaders' column_major mul() reads:
	// out_i = sum_j(v_j * m[j*4+i]) + m[12+i]
	if (this->m_vertex_shadow.size() < vertex_offset + vertex_count)
	{
		this->m_vertex_shadow.resize(vertex_offset + vertex_count);
	}
	if (this->m_index_shadow.size() < index_offset + index_count)
	{
		this->m_index_shadow.resize(index_offset + index_count);
	}

	const float* p_matrix = p_model_matrix_or_null;

	for (kotek::uint32_t vertex_index = 0; vertex_index < vertex_count;
		 ++vertex_index)
	{
		const zircon_model_static_vertex_t& source =
			p_vertices[vertex_index];

		zircon_model_static_vertex_t& target =
			this->m_vertex_shadow[vertex_offset + vertex_index];

		if (p_matrix)
		{
			for (int component = 0; component < 3; ++component)
			{
				target.m_position[component] =
					source.m_position[0] * p_matrix[0 * 4 + component] +
					source.m_position[1] * p_matrix[1 * 4 + component] +
					source.m_position[2] * p_matrix[2 * 4 + component] +
					p_matrix[12 + component];

				target.m_normal[component] =
					source.m_normal[0] * p_matrix[0 * 4 + component] +
					source.m_normal[1] * p_matrix[1 * 4 + component] +
					source.m_normal[2] * p_matrix[2 * 4 + component];
			}
		}
		else
		{
			target.m_position[0] = source.m_position[0];
			target.m_position[1] = source.m_position[1];
			target.m_position[2] = source.m_position[2];
			target.m_normal[0] = source.m_normal[0];
			target.m_normal[1] = source.m_normal[1];
			target.m_normal[2] = source.m_normal[2];
		}

		target.m_color_abgr = source.m_color_abgr;
	}

	for (kotek::uint32_t index_index = 0; index_index < index_count;
		 ++index_index)
	{
		this->m_index_shadow[index_offset + index_index] =
			p_indices[index_index];
	}

	// the world AABB over the INDEXED vertices (the submesh case: a chunk
	// may share its vertex span with siblings — the bounds must wrap the
	// drawn triangles, not the whole span)
	float aabb_min[3] = {0.0f, 0.0f, 0.0f};
	float aabb_max[3] = {0.0f, 0.0f, 0.0f};

	for (kotek::uint32_t index_index = 0; index_index < index_count;
		 ++index_index)
	{
		const kotek::uint16_t vertex_index = p_indices[index_index];

		KOTEK_ASSERT(vertex_index < vertex_count,
			"an index escapes the chunk's vertex span");

		if (vertex_index >= vertex_count)
			continue;

		const float* p_position =
			this->m_vertex_shadow[vertex_offset + vertex_index].m_position;

		for (int axis = 0; axis < 3; ++axis)
		{
			if (index_index == 0 || p_position[axis] < aabb_min[axis])
				aabb_min[axis] = p_position[axis];
			if (index_index == 0 || p_position[axis] > aabb_max[axis])
				aabb_max[axis] = p_position[axis];
		}
	}

	// the tables (the bounds bundle's spare lanes carry the live flag and
	// the material id — bit-punned floats, read back with asuint/asfloat
	// on the GPU side)
	zircon_chunk_bounds_t& bounds = this->m_bounds[slot];

	bounds.m_aabb_min[0] = aabb_min[0];
	bounds.m_aabb_min[1] = aabb_min[1];
	bounds.m_aabb_min[2] = aabb_min[2];
	bounds.m_aabb_max[0] = aabb_max[0];
	bounds.m_aabb_max[1] = aabb_max[1];
	bounds.m_aabb_max[2] = aabb_max[2];

	const kotek::uint32_t live_bits = 1u;
	const kotek::uint32_t material_bits = material_id;

	std::memcpy(&bounds.m_aabb_min[3], &live_bits, sizeof(float));
	std::memcpy(&bounds.m_aabb_max[3], &material_bits, sizeof(float));

	zircon_chunk_ranges_t& ranges = this->m_ranges[slot];

	ranges.m_vertex_offset = static_cast<kotek::uint16_t>(vertex_offset);
	ranges.m_vertex_count = vertex_count;
	ranges.m_index_offset = index_offset;
	ranges.m_index_count = index_count;
	ranges.m_spare = 0;

	this->m_material_ids[slot] = material_id;
	this->m_flags[slot] = kFlagLive;

	if (this->m_vertex_high_water < vertex_offset + vertex_count)
		this->m_vertex_high_water = vertex_offset + vertex_count;
	if (this->m_index_high_water < index_offset + index_count)
		this->m_index_high_water = index_offset + index_count;

	++this->m_live_chunk_count;

	this->m_pools_dirty = true;
	this->m_tables_dirty = true;

	out_chunk_id = slot;
	return true;
}

bool zircon_render_chunk_pool::remove_chunk(
	kotek::uint32_t chunk_id) noexcept
{
	if (chunk_id >= this->m_bounds.size() ||
		(this->m_flags[chunk_id] & kFlagLive) == 0)
	{
		KOTEK_MESSAGE_WARNING(
			"[chunk_pool] remove_chunk: stale or invalid id {} — no-op",
			chunk_id);
		return false;
	}

	const zircon_chunk_ranges_t& ranges = this->m_ranges[chunk_id];

	this->m_vertex_allocator.free(
		ranges.m_vertex_offset, ranges.m_vertex_count);
	this->m_index_allocator.free(
		ranges.m_index_offset, ranges.m_index_count);

	this->m_flags[chunk_id] = 0;

	// the GPU-side dead flag: zero the bounds bundle's live lane
	const kotek::uint32_t dead_bits = 0u;
	std::memcpy(&this->m_bounds[chunk_id].m_aabb_min[3], &dead_bits,
		sizeof(float));

	this->m_free_chunk_slots.push_back(
		static_cast<kotek::uint16_t>(chunk_id));

	--this->m_live_chunk_count;

	this->m_pools_dirty = true;
	this->m_tables_dirty = true;

	return true;
}

void zircon_render_chunk_pool::clear(void) noexcept
{
	this->m_vertex_allocator.initialize(
		zircon_DEF_RENDER_CHUNK_POOL_MAX_VERTICES);
	this->m_index_allocator.initialize(
		zircon_DEF_RENDER_CHUNK_POOL_MAX_INDICES);

	this->m_bounds.clear();
	this->m_ranges.clear();
	this->m_material_ids.clear();
	this->m_flags.clear();
	this->m_free_chunk_slots.clear();

	this->m_vertex_shadow.clear();
	this->m_index_shadow.clear();

	this->m_vertex_high_water = 0;
	this->m_index_high_water = 0;
	this->m_live_chunk_count = 0;
	this->m_pools_dirty = true;
	this->m_tables_dirty = true;
}

void zircon_render_chunk_pool::defrag_pools(void) noexcept
{
	defrag_one_pool(this->m_vertex_allocator, this->m_vertex_shadow.data(),
		this->m_vertex_high_water, this->m_ranges.data(),
		static_cast<kotek::uint32_t>(this->m_ranges.size()), true);
	defrag_one_pool(this->m_index_allocator, this->m_index_shadow.data(),
		this->m_index_high_water, this->m_ranges.data(),
		static_cast<kotek::uint32_t>(this->m_ranges.size()), false);

	this->m_pools_dirty = true;
	this->m_tables_dirty = true;
}

template <typename Element>
void zircon_render_chunk_pool::defrag_one_pool(
	zircon_pool_range_allocator& allocator, Element* p_shadow,
	kotek::uint32_t& io_high_water, zircon_chunk_ranges_t* p_ranges,
	kotek::uint32_t range_count, bool is_vertex_pool) noexcept
{
	zircon_pool_range_allocator::relocation_t relocations
		[zircon_DEF_RENDER_CHUNK_POOL_MAX_FREE_RANGES];

	kotek::uint32_t new_used_end = 0;

	const kotek::uint32_t relocation_count =
		allocator.compute_defrag_relocations(relocations,
			zircon_DEF_RENDER_CHUNK_POOL_MAX_FREE_RANGES, new_used_end);

	if (relocation_count == 0)
		return;

	// move the live segments down (relocations come ordered by ascending
	// old offset and every move is towards zero — a forward walk never
	// overwrites data a later move still needs)
	for (kotek::uint32_t relocation_index = 0;
		 relocation_index < relocation_count; ++relocation_index)
	{
		const zircon_pool_range_allocator::relocation_t& relocation =
			relocations[relocation_index];

		std::memmove(p_shadow + relocation.m_new_offset,
			p_shadow + relocation.m_old_offset,
			relocation.m_count * sizeof(Element));
	}

	// rewrite the live chunks' range fields (a chunk's range lies wholly
	// inside exactly one live segment)
	for (kotek::uint32_t chunk_index = 0; chunk_index < range_count;
		 ++chunk_index)
	{
		zircon_chunk_ranges_t& ranges = p_ranges[chunk_index];

		const kotek::uint32_t chunk_offset = is_vertex_pool
			? static_cast<kotek::uint32_t>(ranges.m_vertex_offset)
			: ranges.m_index_offset;
		const kotek::uint32_t chunk_count = is_vertex_pool
			? static_cast<kotek::uint32_t>(ranges.m_vertex_count)
			: static_cast<kotek::uint32_t>(ranges.m_index_count);

		if (chunk_count == 0)
			continue;

		for (kotek::uint32_t relocation_index = 0;
			 relocation_index < relocation_count; ++relocation_index)
		{
			const zircon_pool_range_allocator::relocation_t& relocation =
				relocations[relocation_index];

			if (chunk_offset >= relocation.m_old_offset &&
				chunk_offset + chunk_count <=
					relocation.m_old_offset + relocation.m_count)
			{
				const kotek::uint32_t new_offset = relocation.m_new_offset +
					(chunk_offset - relocation.m_old_offset);

				if (is_vertex_pool)
				{
					ranges.m_vertex_offset =
						static_cast<kotek::uint16_t>(new_offset);
				}
				else
				{
					ranges.m_index_offset = new_offset;
				}

				break;
			}
		}
	}

	allocator.apply_defrag_layout(new_used_end);
	io_high_water = new_used_end;
}

kotek::uint32_t zircon_render_chunk_pool::cull_chunks_against_frustum(
	const float* p_planes_6x4, kotek::uint32_t* p_out_visible_chunk_ids,
	kotek::uint32_t out_visible_capacity) const noexcept
{
	KOTEK_ASSERT(p_planes_6x4, "must be valid");
	KOTEK_ASSERT(p_out_visible_chunk_ids, "must be valid storage");

	if (p_planes_6x4 == nullptr || p_out_visible_chunk_ids == nullptr)
		return 0;

	kotek::uint32_t written_count = 0;

	for (kotek::uint32_t slot = 0; slot < this->m_bounds.size(); ++slot)
	{
		if ((this->m_flags[slot] & kFlagLive) == 0)
			continue;

		const zircon_chunk_bounds_t& bounds = this->m_bounds[slot];

		if (test_aabb_against_frustum(p_planes_6x4, bounds.m_aabb_min,
				bounds.m_aabb_max) == false)
		{
			continue;
		}

		if (written_count < out_visible_capacity)
		{
			p_out_visible_chunk_ids[written_count] = slot;
		}

		++written_count;
	}

	return written_count;
}

void zircon_render_chunk_pool::extract_frustum_planes(
	const float* p_view_projection, float* p_out_planes_6x4) noexcept
{
	KOTEK_ASSERT(p_view_projection, "must be valid");
	KOTEK_ASSERT(p_out_planes_6x4, "must be valid storage");

	if (p_view_projection == nullptr || p_out_planes_6x4 == nullptr)
		return;

	// the matrix columns c_j = (m[j], m[4+j], m[8+j], m[12+j]) — bx
	// stores row-major with row-vector math (M[r][c] = m[r*4+c],
	// translation at [12..14], the byte layout the shaders' column_major
	// mul() reads), so component i of column j is m[i*4+j] and the clip
	// components are x' = dot(p, c0) ... w' = dot(p, c3); the
	// Gribb/Hartmann combinations for -w<=x<=w, -w<=y<=w, 0<=z<=w (the
	// [0,1] depth backends) are:
	for (int component = 0; component < 4; ++component)
	{
		const float column0 = p_view_projection[component * 4 + 0];
		const float column1 = p_view_projection[component * 4 + 1];
		const float column2 = p_view_projection[component * 4 + 2];
		const float column3 = p_view_projection[component * 4 + 3];

		p_out_planes_6x4[0 * 4 + component] = column3 + column0; // left
		p_out_planes_6x4[1 * 4 + component] = column3 - column0; // right
		p_out_planes_6x4[2 * 4 + component] = column3 + column1; // bottom
		p_out_planes_6x4[3 * 4 + component] = column3 - column1; // top
		p_out_planes_6x4[4 * 4 + component] = column2;           // near
		p_out_planes_6x4[5 * 4 + component] = column3 - column2; // far
	}

	// normalize to unit xyz length (the sign of every plane test is
	// scale-invariant, but the mirror and the shader compare dot values
	// against zero — normalization keeps the two float paths numerically
	// aligned)
	for (int plane_index = 0; plane_index < 6; ++plane_index)
	{
		float* p_plane = p_out_planes_6x4 + plane_index * 4;

		const float length_squared = p_plane[0] * p_plane[0] +
			p_plane[1] * p_plane[1] + p_plane[2] * p_plane[2];

		if (length_squared <= 0.0f)
			continue;

		const float length = std::sqrt(length_squared);

		for (int component = 0; component < 4; ++component)
		{
			p_plane[component] /= length;
		}
	}
}

bool zircon_render_chunk_pool::test_aabb_against_frustum(
	const float* p_planes_6x4, const float* p_aabb_min_xyz,
	const float* p_aabb_max_xyz) noexcept
{
	KOTEK_ASSERT(p_planes_6x4, "must be valid");
	KOTEK_ASSERT(p_aabb_min_xyz, "must be valid");
	KOTEK_ASSERT(p_aabb_max_xyz, "must be valid");

	if (p_planes_6x4 == nullptr || p_aabb_min_xyz == nullptr ||
		p_aabb_max_xyz == nullptr)
	{
		return false;
	}

	// one-to-one with the compute shader's zircon_is_aabb_visible
	// (model_static_gpu_driven_cull.cs.slang) — keep the two in sync
	for (int plane_index = 0; plane_index < 6; ++plane_index)
	{
		const float* p_plane = p_planes_6x4 + plane_index * 4;

		const float positive_vertex[3] = {
			p_plane[0] >= 0.0f ? p_aabb_max_xyz[0] : p_aabb_min_xyz[0],
			p_plane[1] >= 0.0f ? p_aabb_max_xyz[1] : p_aabb_min_xyz[1],
			p_plane[2] >= 0.0f ? p_aabb_max_xyz[2] : p_aabb_min_xyz[2],
		};

		const float distance = p_plane[0] * positive_vertex[0] +
			p_plane[1] * positive_vertex[1] +
			p_plane[2] * positive_vertex[2] + p_plane[3];

		if (distance < 0.0f)
			return false;
	}

	return true;
}

const zircon_chunk_bounds_t* zircon_render_chunk_pool::get_bounds(
	void) const noexcept
{
	return this->m_bounds.data();
}

const zircon_chunk_ranges_t* zircon_render_chunk_pool::get_ranges(
	void) const noexcept
{
	return this->m_ranges.data();
}

const kotek::uint16_t* zircon_render_chunk_pool::get_material_ids(
	void) const noexcept
{
	return this->m_material_ids.data();
}

kotek::uint32_t zircon_render_chunk_pool::get_chunk_slot_count(
	void) const noexcept
{
	return static_cast<kotek::uint32_t>(this->m_bounds.size());
}

kotek::uint32_t zircon_render_chunk_pool::get_live_chunk_count(
	void) const noexcept
{
	return this->m_live_chunk_count;
}

bool zircon_render_chunk_pool::is_chunk_live(
	kotek::uint32_t chunk_id) const noexcept
{
	return chunk_id < this->m_flags.size() &&
		(this->m_flags[chunk_id] & kFlagLive) != 0;
}

const zircon_model_static_vertex_t*
zircon_render_chunk_pool::get_vertex_shadow(void) const noexcept
{
	return this->m_vertex_shadow.data();
}

const kotek::uint16_t* zircon_render_chunk_pool::get_index_shadow(
	void) const noexcept
{
	return this->m_index_shadow.data();
}

kotek::uint32_t zircon_render_chunk_pool::get_vertex_high_water(
	void) const noexcept
{
	return this->m_vertex_high_water;
}

kotek::uint32_t zircon_render_chunk_pool::get_index_high_water(
	void) const noexcept
{
	return this->m_index_high_water;
}

bool zircon_render_chunk_pool::is_pools_dirty(void) const noexcept
{
	return this->m_pools_dirty;
}

bool zircon_render_chunk_pool::is_tables_dirty(void) const noexcept
{
	return this->m_tables_dirty;
}

void zircon_render_chunk_pool::clear_dirty_flags(void) noexcept
{
	this->m_pools_dirty = false;
	this->m_tables_dirty = false;
}

const zircon_pool_range_allocator&
zircon_render_chunk_pool::get_vertex_allocator(void) const noexcept
{
	return this->m_vertex_allocator;
}

const zircon_pool_range_allocator&
zircon_render_chunk_pool::get_index_allocator(void) const noexcept
{
	return this->m_index_allocator;
}

kotek::uint32_t zircon_render_chunk_pool::get_free_chunk_slot_count(
	void) const noexcept
{
	return static_cast<kotek::uint32_t>(
		zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS -
		this->m_bounds.size() + this->m_free_chunk_slots.size());
}

// ---------------------------------------------------------------------------
// the A3 pack-loaded variant (task Z25): the baked CSG chunk set through
// the filesystem dispatcher into the B1 GPU-culled path
// ---------------------------------------------------------------------------

namespace
{
	// u32 -> decimal append (the bake-side helper's twin — the entry
	// name must hash identically to the bake's)
	void chunk_pool_append_u32(kotek::static_cstring_t<
			ZIRCON_DEF_CSG_BAKE_ENTRY_NAME_MAX_LENGTH>& out_string,
		kotek::uint32_t value) noexcept
	{
		char digits[10];
		kotek::uint32_t count = 0;

		do
		{
			digits[count++] = static_cast<char>('0' + (value % 10u));
			value /= 10u;
		} while (value != 0u);

		while (count > 0)
		{
			char symbol[2] = {digits[--count], '\0'};
			out_string += symbol;
		}
	}

	// <prefix>/compound_<i>/chunk_<j>.bin — byte-identical to the
	// bake's entry name (the manifest's name-hash check pins it)
	void chunk_pool_build_entry_name(kotek::static_cstring_t<
			ZIRCON_DEF_CSG_BAKE_ENTRY_NAME_MAX_LENGTH>& out_name,
		const kotek::static_path_t& prefix, kotek::uint32_t compound_index,
		kotek::uint32_t chunk_index) noexcept
	{
		out_name.assign(prefix.c_str());
		out_name += "/compound_";
		chunk_pool_append_u32(out_name, compound_index);
		out_name += "/chunk_";
		chunk_pool_append_u32(out_name, chunk_index);
		out_name += ".bin";
	}

	// the per-chunk entry's exact byte size (the bake's layout)
	kotek::uint32_t chunk_pool_bin_size(kotek::uint32_t welded_count,
		kotek::uint32_t triangle_count, kotek::uint32_t index_count
		) noexcept
	{
		return zircon_csg_bake_chunk_header_size +
			welded_count * 3 *
				sizeof(zircon_csg_bake_position_quant_t) +
			triangle_count * 2 +
			index_count * static_cast<kotek::uint32_t>(
				sizeof(kotek::uint32_t)) +
			triangle_count * static_cast<kotek::uint32_t>(
				sizeof(kotek::uint16_t));
	}
} // namespace

bool zircon_render_chunk_pool::load_chunks_from_pack(
	kotek::core::ktkIFileSystem* p_filesystem,
	const kotek::static_path_t& pack_path_prefix_relative_to_root,
	kotek::uint32_t& out_loaded_chunk_count) noexcept
{
	out_loaded_chunk_count = 0;

	KOTEK_ASSERT(
		p_filesystem, "the pack load reads through the filesystem"
	);

	if (p_filesystem == nullptr)
		return false;

	// the root resolution (cwd-independent), the bake's twin
	kotek::static_path_t root_path;
	p_filesystem->Make_Path(
		root_path, kotek::core::eFolderIndex::kFolderIndex_Root);

	kotek::static_path_t manifest_path = root_path;
	manifest_path /= pack_path_prefix_relative_to_root;
	manifest_path /= "manifest.bin";

	kotek::size_t manifest_size = 0;

	if (p_filesystem->Get_FileSize(manifest_path, manifest_size) == false)
		return false; // the probe's one B0 warning

	if (manifest_size < zircon_csg_bake_manifest_header_size ||
		(manifest_size - zircon_csg_bake_manifest_header_size) %
				zircon_csg_bake_manifest_record_size !=
			0)
	{
		KOTEK_MESSAGE_ERROR(
			"[chunk_pool] load_chunks_from_pack: the manifest size {} "
			"does not fit the record grid — corrupt",
			static_cast<kotek::uint32_t>(manifest_size));
		return false;
	}

	const kotek::uint32_t record_count =
		static_cast<kotek::uint32_t>(
			(manifest_size - zircon_csg_bake_manifest_header_size) /
			zircon_csg_bake_manifest_record_size);

	if (record_count > ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE)
	{
		KOTEK_MESSAGE_ERROR(
			"[chunk_pool] load_chunks_from_pack: the manifest declares "
			"{} chunks (cap {}) — corrupt",
			record_count, ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE);
		return false;
	}

	if (record_count == 0)
	{
		KOTEK_MESSAGE_WARNING(
			"[chunk_pool] load_chunks_from_pack: the manifest declares "
			"no chunks — nothing to load");
		return true;
	}

	// the manifest read (heap: up to ~384 KB at the cap; +1 for the
	// read path's '\0' terminator)
	kotek::uint8_t* p_manifest = new kotek::uint8_t[manifest_size + 1];
	kotek::uint8_t* p_manifest_cursor = p_manifest;
	kotek::size_t manifest_read_size = manifest_size + 1;

	const bool is_manifest_read = p_filesystem->Read_File(
		manifest_path, p_manifest_cursor, manifest_read_size);

	if (is_manifest_read == false ||
		manifest_read_size != manifest_size)
	{
		KOTEK_MESSAGE_ERROR(
			"[chunk_pool] load_chunks_from_pack: the manifest read "
			"failed ({} of {} bytes)",
			static_cast<kotek::uint32_t>(manifest_read_size),
			static_cast<kotek::uint32_t>(manifest_size));
		delete[] p_manifest;
		return false;
	}

	// the header validation
	if (std::memcmp(p_manifest, zircon_csg_bake_manifest_magic, 8) != 0)
	{
		KOTEK_MESSAGE_ERROR(
			"[chunk_pool] load_chunks_from_pack: the manifest magic "
			"does not match — not a CSG bake");
		delete[] p_manifest;
		return false;
	}

	const kotek::uint32_t chunk_size_bits =
		zircon_csg_bake_load_u32(p_manifest + 8);
	float chunk_size_meters = 0.0f;
	std::memcpy(&chunk_size_meters, &chunk_size_bits,
		sizeof(chunk_size_meters));

	if (chunk_size_meters !=
		static_cast<float>(ZIRCON_DEF_CSG_BAKE_CHUNK_SIZE_METERS))
	{
		KOTEK_MESSAGE_ERROR(
			"[chunk_pool] load_chunks_from_pack: the manifest's grid "
			"cell is {} m, this build bakes {} m — format skew",
			static_cast<double>(chunk_size_meters),
			static_cast<double>(
				ZIRCON_DEF_CSG_BAKE_CHUNK_SIZE_METERS));
		delete[] p_manifest;
		return false;
	}

	if (zircon_csg_bake_load_u32(p_manifest + 16) != record_count ||
		zircon_csg_bake_load_u32(p_manifest + 28) != 0u)
	{
		KOTEK_MESSAGE_ERROR(
			"[chunk_pool] load_chunks_from_pack: the manifest header "
			"disagrees with its size — corrupt");
		delete[] p_manifest;
		return false;
	}

	// ---- pass 1: per-record validation + the capacity pre-check ----
	kotek::uint64_t sum_index_count = 0;
	kotek::uint32_t max_bin_size = 0;
	kotek::uint32_t max_index_count = 0;

	for (kotek::uint32_t record_index = 0; record_index < record_count;
		 ++record_index)
	{
		const kotek::uint8_t* p_record =
			p_manifest + zircon_csg_bake_manifest_header_size +
			record_index * zircon_csg_bake_manifest_record_size;

		const kotek::uint32_t welded_count =
			zircon_csg_bake_load_u32(p_record + 20);
		const kotek::uint32_t index_count =
			zircon_csg_bake_load_u32(p_record + 24);
		const kotek::uint32_t triangle_count =
			zircon_csg_bake_load_u32(p_record + 28);

		if (welded_count == 0 || welded_count >
				ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION ||
			index_count == 0 || index_count % 3 != 0 ||
			triangle_count != index_count / 3 ||
			triangle_count > ZIRCON_DEF_CSG_BAKE_MAX_TRIANGLES_PER_CHUNK ||
			zircon_csg_bake_load_u16(p_record + 34) != 0u ||
			zircon_csg_bake_load_u32(p_record + 92) != 0u)
		{
			KOTEK_MESSAGE_ERROR(
				"[chunk_pool] load_chunks_from_pack: record {} fails "
				"validation ({} welded / {} indices / {} triangles) — "
				"corrupt",
				record_index, welded_count, index_count, triangle_count);
			delete[] p_manifest;
			return false;
		}

		// the manifest/pack skew check: the record's name hash must be
		// the hash of the entry name this record addresses
		kotek::static_cstring_t<
			ZIRCON_DEF_CSG_BAKE_ENTRY_NAME_MAX_LENGTH>
			entry_name;
		chunk_pool_build_entry_name(entry_name,
			pack_path_prefix_relative_to_root,
			zircon_csg_bake_load_u32(p_record + 0),
			zircon_csg_bake_load_u32(p_record + 4));

		if (kotek::core::kpack_hash_name(entry_name.c_str(),
				std::strlen(entry_name.c_str())) !=
			zircon_csg_bake_load_u64(p_record + 84))
		{
			KOTEK_MESSAGE_ERROR(
				"[chunk_pool] load_chunks_from_pack: record {}'s entry "
				"name hash does not match '{}' — manifest/pack skew",
				record_index, entry_name.c_str());
			delete[] p_manifest;
			return false;
		}

		const kotek::uint32_t bin_size = chunk_pool_bin_size(
			welded_count, triangle_count, index_count);

		if (bin_size > max_bin_size)
			max_bin_size = bin_size;
		if (index_count > max_index_count)
			max_index_count = index_count;

		sum_index_count += index_count;
	}

	// the soup expansion costs 3 pool vertices + 3 pool indices per
	// triangle (index_count of each) — the whole set must fit BEFORE
	// any registration (no partial state on a capacity breach)
	if (sum_index_count >
			this->m_vertex_allocator.get_free_total() ||
		sum_index_count > this->m_index_allocator.get_free_total() ||
		record_count > this->get_free_chunk_slot_count())
	{
		KOTEK_MESSAGE_ERROR(
			"[chunk_pool] load_chunks_from_pack: the scene needs {} "
			"pool vertices/indices in {} chunks, the pool has {} / {} "
			"free in {} slots — raise the pool capacities or bake "
			"smaller scenes",
			static_cast<kotek::uint32_t>(sum_index_count), record_count,
			this->m_vertex_allocator.get_free_total(),
			this->m_index_allocator.get_free_total(),
			this->get_free_chunk_slot_count());
		delete[] p_manifest;
		return false;
	}

	// ---- pass 2: per-record read + dequantize + register ----
	kotek::uint8_t* p_bin = new kotek::uint8_t[max_bin_size + 1];
	zircon_model_static_vertex_t* p_soup =
		new zircon_model_static_vertex_t[max_index_count];
	float* p_positions = new float[max_index_count * 3];
	kotek::uint16_t* p_soup_indices = new kotek::uint16_t[max_index_count];

	bool is_ok = true;

	for (kotek::uint32_t record_index = 0;
		 record_index < record_count && is_ok; ++record_index)
	{
		const kotek::uint8_t* p_record =
			p_manifest + zircon_csg_bake_manifest_header_size +
			record_index * zircon_csg_bake_manifest_record_size;

		const kotek::uint32_t welded_count =
			zircon_csg_bake_load_u32(p_record + 20);
		const kotek::uint32_t index_count =
			zircon_csg_bake_load_u32(p_record + 24);
		const kotek::uint32_t triangle_count =
			zircon_csg_bake_load_u32(p_record + 28);

		double aabb_min[3];
		double aabb_max[3];

		for (int axis = 0; axis < 3; ++axis)
		{
			aabb_min[axis] = zircon_csg_bake_load_f64(
				p_record + 36 + axis * 8);
			aabb_max[axis] = zircon_csg_bake_load_f64(
				p_record + 60 + axis * 8);
		}

		kotek::static_cstring_t<
			ZIRCON_DEF_CSG_BAKE_ENTRY_NAME_MAX_LENGTH>
			entry_name;
		chunk_pool_build_entry_name(entry_name,
			pack_path_prefix_relative_to_root,
			zircon_csg_bake_load_u32(p_record + 0),
			zircon_csg_bake_load_u32(p_record + 4));

		kotek::static_path_t entry_path = root_path;
		entry_path /= entry_name.c_str();

		const kotek::uint32_t expected_bin_size = chunk_pool_bin_size(
			welded_count, triangle_count, index_count);

		kotek::size_t entry_size = 0;

		if (p_filesystem->Get_FileSize(entry_path, entry_size) == false ||
			entry_size != expected_bin_size)
		{
			KOTEK_MESSAGE_ERROR(
				"[chunk_pool] load_chunks_from_pack: entry '{}' is "
				"missing or sized {} (expected {}) — corrupt",
				entry_name.c_str(), static_cast<kotek::uint32_t>(
					entry_size), expected_bin_size);
			is_ok = false;
			break;
		}

		kotek::uint8_t* p_bin_cursor = p_bin;
		kotek::size_t bin_read_size = max_bin_size + 1;

		if (p_filesystem->Read_File(
				entry_path, p_bin_cursor, bin_read_size) == false ||
			bin_read_size != expected_bin_size)
		{
			KOTEK_MESSAGE_ERROR(
				"[chunk_pool] load_chunks_from_pack: entry '{}' read "
				"failed",
				entry_name.c_str());
			is_ok = false;
			break;
		}

		if (std::memcmp(p_bin, zircon_csg_bake_chunk_magic, 4) != 0 ||
			zircon_csg_bake_load_u32(p_bin + 4) != welded_count ||
			zircon_csg_bake_load_u32(p_bin + 8) != index_count ||
			zircon_csg_bake_load_u32(p_bin + 12) != triangle_count)
		{
			KOTEK_MESSAGE_ERROR(
				"[chunk_pool] load_chunks_from_pack: entry '{}' fails "
				"its header check — corrupt",
				entry_name.c_str());
			is_ok = false;
			break;
		}

		// dequantize the welded positions (double against the
		// manifest's f64 bounds, float at the pool boundary)
		const kotek::uint8_t* p_quant =
			p_bin + zircon_csg_bake_chunk_header_size;

		for (kotek::uint32_t welded = 0; welded < welded_count; ++welded)
		{
			for (int axis = 0; axis < 3; ++axis)
			{
				zircon_csg_bake_position_quant_t quantized{};

				if constexpr (sizeof(
								  zircon_csg_bake_position_quant_t) ==
					2)
				{
					quantized = static_cast<
						zircon_csg_bake_position_quant_t>(
						zircon_csg_bake_load_u16(p_quant));
					p_quant += 2;
				}
				else
				{
					quantized = static_cast<
						zircon_csg_bake_position_quant_t>(*p_quant);
					p_quant += 1;
				}

				const double extent = aabb_max[axis] - aabb_min[axis];

				p_positions[welded * 3 + axis] = static_cast<float>(
					zircon_csg_bake_dequantize_position(
						quantized, aabb_min[axis], extent));
			}
		}

		// the per-triangle octahedral normals
		const kotek::uint8_t* p_oct = p_quant;
		p_quant += triangle_count * 2;

		// the chunk-local indices
		const kotek::uint8_t* p_indices_u32 = p_quant;
		p_quant += index_count * static_cast<kotek::uint32_t>(
			sizeof(kotek::uint32_t));

		// the triangle-soup expansion (the A2 editor pool's contract):
		// every corner is its own pool vertex carrying the triangle's
		// flat normal; the color is the neutral modulator until the
		// material system lands (the editor pool's white)
		for (kotek::uint32_t triangle = 0; triangle < triangle_count;
			 ++triangle)
		{
			float normal[3];
			zircon_csg_bake_decode_normal_oct_u8(
				p_oct + triangle * 2, normal);

			for (kotek::uint8_t corner = 0; corner < 3; ++corner)
			{
				const kotek::uint32_t soup_vertex = triangle * 3 + corner;
				const kotek::uint32_t local_index =
					zircon_csg_bake_load_u32(
						p_indices_u32 + soup_vertex * 4);

				if (local_index >= welded_count)
				{
					KOTEK_MESSAGE_ERROR(
						"[chunk_pool] load_chunks_from_pack: entry "
						"'{}' index {} addresses welded vertex {} of "
						"{} — corrupt",
						entry_name.c_str(), soup_vertex, local_index,
						welded_count);
					is_ok = false;
					break;
				}

				zircon_model_static_vertex_t& vertex =
					p_soup[soup_vertex];

				vertex.m_position[0] =
					p_positions[local_index * 3 + 0];
				vertex.m_position[1] =
					p_positions[local_index * 3 + 1];
				vertex.m_position[2] =
					p_positions[local_index * 3 + 2];
				vertex.m_normal[0] = normal[0];
				vertex.m_normal[1] = normal[1];
				vertex.m_normal[2] = normal[2];
				vertex.m_color_abgr = 0xffffffffu;

				p_soup_indices[soup_vertex] =
					static_cast<kotek::uint16_t>(soup_vertex);
			}

			if (is_ok == false)
				break;
		}

		if (is_ok == false)
			break;

		// the per-triangle materials ride the entry; the pool's
		// per-chunk slot takes the manifest's first-material record
		// (the full table is the future material stream's)
		const kotek::uint16_t material_id =
			zircon_csg_bake_load_u16(p_record + 32);

		kotek::uint32_t chunk_id = kInvalidChunkId;

		if (this->register_chunk(p_soup,
				static_cast<kotek::uint16_t>(index_count),
				p_soup_indices,
				static_cast<kotek::uint16_t>(index_count), nullptr,
				material_id, chunk_id) == false)
		{
			// the pre-check makes this unreachable (defensive: a
			// rollback is register_chunk's own contract)
			is_ok = false;
			break;
		}

		++out_loaded_chunk_count;
	}

	delete[] p_soup_indices;
	delete[] p_positions;
	delete[] p_soup;
	delete[] p_bin;
	delete[] p_manifest;

	return is_ok;
}
