#include "zircon_render_chunk_pool.h"

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
