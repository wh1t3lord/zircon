#include "zircon_render_csg_editor_pool.h"

#include <cstring>

void zircon_render_csg_editor_pool::initialize(void) noexcept
{
	this->m_vertex_allocator.initialize(
		zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_VERTICES);
	this->m_index_allocator.initialize(
		zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_INDICES);

	this->clear();
}

void zircon_render_csg_editor_pool::clear(void) noexcept
{
	this->m_compound_ids.clear();
	this->m_ranges.clear();
	this->m_live_flags.clear();
	this->m_vertex_shadow.clear();
	this->m_index_shadow.clear();
	this->m_dirty_spans.clear();

	this->m_vertex_high_water = 0;
	this->m_index_high_water = 0;
	this->m_live_compound_count = 0;
	this->m_is_full_upload_pending = false;
}

bool zircon_render_csg_editor_pool::upsert_compound_mesh(
	kotek::uint64_t compound_id,
	const float* p_positions_xyz,
	kotek::uint32_t position_count,
	const float* p_normals_xyz_per_triangle,
	const kotek::uint32_t* p_indices,
	kotek::uint32_t triangle_count
) noexcept
{
	const kotek::uint32_t vertex_need = triangle_count * 3;
	const kotek::uint32_t index_need = triangle_count * 3;

	if (vertex_need > zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_VERTICES ||
		index_need > zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_INDICES)
	{
		KOTEK_MESSAGE_ERROR(
			"[csg_pool] upsert: compound {} needs {} vertices / "
			"{} indices, beyond the pool caps",
			static_cast<kotek::uint32_t>(compound_id),
			vertex_need,
			index_need);
		return false;
	}

	// a zero-triangle evaluation (an all-subtractive compound, a
	// fully-carved result) has nothing to draw — the upsert is a
	// removal
	if (triangle_count == 0)
	{
		this->remove_compound(compound_id);
		return true;
	}

	if (p_positions_xyz == nullptr || p_indices == nullptr ||
		p_normals_xyz_per_triangle == nullptr)
	{
		KOTEK_MESSAGE_ERROR(
			"[csg_pool] upsert: null mesh arguments");
		return false;
	}

	if (position_count == 0)
	{
		KOTEK_MESSAGE_ERROR(
			"[csg_pool] upsert: indices without positions");
		return false;
	}

	const kotek::uint32_t existing_slot =
		this->find_compound_slot(compound_id);

	if (existing_slot != this->m_compound_ids.size())
		this->release_ranges(existing_slot);

	// the amortized defrag trigger: a failed fit with enough TOTAL
	// free space defragments once, then the allocation retries (the
	// chunk-pool contract)
	kotek::uint32_t vertex_offset = 0;
	kotek::uint32_t index_offset = 0;

	bool is_fit = this->m_vertex_allocator.allocate(
		vertex_need, vertex_offset);

	if (is_fit == false &&
		this->m_vertex_allocator.get_free_total() >= vertex_need)
	{
		this->defrag_pools();
		is_fit = this->m_vertex_allocator.allocate(
			vertex_need, vertex_offset);
	}

	if (is_fit == false)
	{
		KOTEK_MESSAGE_ERROR(
			"[csg_pool] vertex pool overflow: compound {} needs "
			"{} vertices, {} free in total",
			static_cast<kotek::uint32_t>(compound_id),
			vertex_need,
			this->m_vertex_allocator.get_free_total());
		return false;
	}

	bool is_index_fit = this->m_index_allocator.allocate(
		index_need, index_offset);

	if (is_index_fit == false &&
		this->m_index_allocator.get_free_total() >= index_need)
	{
		this->defrag_pools();
		is_index_fit = this->m_index_allocator.allocate(
			index_need, index_offset);
	}

	if (is_index_fit == false)
	{
		this->m_vertex_allocator.free(vertex_offset, vertex_need);

		KOTEK_MESSAGE_ERROR(
			"[csg_pool] index pool overflow: compound {} needs "
			"{} indices, {} free in total",
			static_cast<kotek::uint32_t>(compound_id),
			index_need,
			this->m_index_allocator.get_free_total());
		return false;
	}

	// expand the triangle soup into the vertex shadow (the positions
	// array is the evaluation's WELDED set — every index looks it
	// up; the normal is the triangle's flat normal; the color is the
	// neutral modulator until the material system lands)
	const kotek::uint32_t kColor =
		zircon_DEF_RENDER_CSG_EDITOR_POOL_VERTEX_COLOR_ABGR;

	if (this->m_vertex_shadow.size() < vertex_offset + vertex_need)
		this->m_vertex_shadow.resize(vertex_offset + vertex_need);

	if (this->m_index_shadow.size() < index_offset + index_need)
		this->m_index_shadow.resize(index_offset + index_need);

	for (kotek::uint32_t triangle = 0; triangle < triangle_count;
		 ++triangle)
	{
		const float* p_triangle_normal =
			p_normals_xyz_per_triangle + triangle * 3;

		for (kotek::uint8_t corner = 0; corner < 3; ++corner)
		{
			const kotek::uint32_t soup_vertex =
				triangle * 3 + corner;
			const kotek::uint32_t position_index =
				p_indices[triangle * 3 + corner];

			if (position_index >= position_count)
			{
				KOTEK_MESSAGE_ERROR(
					"[csg_pool] upsert: index {} addresses "
					"position {} of {} — caller error",
					triangle * 3 + corner,
					position_index,
					position_count);
				this->m_vertex_allocator.free(
					vertex_offset, vertex_need);
				this->m_index_allocator.free(
					index_offset, index_need);
				return false;
			}

			zircon_model_static_vertex_t& vertex =
				this->m_vertex_shadow[vertex_offset + soup_vertex];

			vertex.m_position[0] =
				p_positions_xyz[position_index * 3 + 0];
			vertex.m_position[1] =
				p_positions_xyz[position_index * 3 + 1];
			vertex.m_position[2] =
				p_positions_xyz[position_index * 3 + 2];
			vertex.m_normal[0] = p_triangle_normal[0];
			vertex.m_normal[1] = p_triangle_normal[1];
			vertex.m_normal[2] = p_triangle_normal[2];
			vertex.m_color_abgr = kColor;

			this->m_index_shadow[index_offset + soup_vertex] =
				vertex_offset + soup_vertex;
		}
	}

	if (this->m_vertex_high_water < vertex_offset + vertex_need)
		this->m_vertex_high_water = vertex_offset + vertex_need;

	if (this->m_index_high_water < index_offset + index_need)
		this->m_index_high_water = index_offset + index_need;

	if (existing_slot != this->m_compound_ids.size())
	{
		kotek::uint32_t* p_ranges =
			&this->m_ranges[existing_slot * 4];
		p_ranges[0] = vertex_offset;
		p_ranges[1] = vertex_need;
		p_ranges[2] = index_offset;
		p_ranges[3] = index_need;
		this->m_live_flags[existing_slot] = 1;
	}
	else
	{
		if (this->m_compound_ids.full())
		{
			this->m_vertex_allocator.free(vertex_offset, vertex_need);
			this->m_index_allocator.free(index_offset, index_need);

			KOTEK_MESSAGE_ERROR(
				"[csg_pool] compound table is full ({}), "
				"increase zircon_DEF_RENDER_CSG_EDITOR_POOL_"
				"MAX_COMPOUNDS",
				zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_COMPOUNDS);
			return false;
		}

		this->m_compound_ids.push_back(compound_id);
		this->m_ranges.push_back(vertex_offset);
		this->m_ranges.push_back(vertex_need);
		this->m_ranges.push_back(index_offset);
		this->m_ranges.push_back(index_need);
		this->m_live_flags.push_back(1);
	}

	++this->m_live_compound_count;

	this->append_dirty_span(false, vertex_offset, vertex_need);
	this->append_dirty_span(true, index_offset, index_need);

	return true;
}

bool zircon_render_csg_editor_pool::remove_compound(
	kotek::uint64_t compound_id) noexcept
{
	const kotek::uint32_t slot = this->find_compound_slot(compound_id);

	if (slot == this->m_compound_ids.size())
	{
		KOTEK_MESSAGE_WARNING(
			"[csg_pool] remove: unknown compound {}",
			static_cast<kotek::uint32_t>(compound_id));
		return false;
	}

	this->release_ranges(slot);

	return true;
}

void zircon_render_csg_editor_pool::defrag_pools(void) noexcept
{
	// vertices: relocate every live span towards zero
	zircon_pool_range_allocator::relocation_t vertex_moves
		[zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_COMPOUNDS];
	kotek::uint32_t vertex_new_end = 0;

	const kotek::uint32_t vertex_move_count =
		this->m_vertex_allocator.compute_defrag_relocations(
			vertex_moves,
			zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_COMPOUNDS,
			vertex_new_end);

	for (kotek::uint32_t move_index = 0;
		 move_index < vertex_move_count; ++move_index)
	{
		const auto& move = vertex_moves[move_index];

		std::memmove(
			this->m_vertex_shadow.data() + move.m_new_offset,
			this->m_vertex_shadow.data() + move.m_old_offset,
			move.m_count *
				sizeof(zircon_model_static_vertex_t));
	}

	// indices: same relocation + the freed tail re-zeroed (the holes
	// between compounds must read as degenerate triangles for the
	// single draw)
	zircon_pool_range_allocator::relocation_t index_moves
		[zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_COMPOUNDS];
	kotek::uint32_t index_new_end = 0;

	const kotek::uint32_t index_move_count =
		this->m_index_allocator.compute_defrag_relocations(
			index_moves,
			zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_COMPOUNDS,
			index_new_end);

	for (kotek::uint32_t move_index = 0;
		 move_index < index_move_count; ++move_index)
	{
		const auto& move = index_moves[move_index];

		std::memmove(
			this->m_index_shadow.data() + move.m_new_offset,
			this->m_index_shadow.data() + move.m_old_offset,
			move.m_count * sizeof(kotek::uint32_t));
	}

	this->m_vertex_allocator.apply_defrag_layout(vertex_new_end);
	this->m_index_allocator.apply_defrag_layout(index_new_end);

	// rewrite the records from the relocation plans: a relocation
	// covers one whole live SEGMENT (many compounds can share it —
	// the allocator emits one entry per segment, not per record),
	// so each record finds the entry CONTAINING its old offset and
	// shifts by that entry's delta
	for (kotek::uint32_t slot = 0; slot < this->m_compound_ids.size();
		 ++slot)
	{
		if (this->m_live_flags[slot] == 0)
			continue;

		kotek::uint32_t* p_ranges = &this->m_ranges[slot * 4];

		for (kotek::uint32_t move_index = 0;
			 move_index < vertex_move_count; ++move_index)
		{
			const auto& move = vertex_moves[move_index];

			if (p_ranges[0] >= move.m_old_offset &&
				p_ranges[0] < move.m_old_offset + move.m_count)
			{
				p_ranges[0] -= (move.m_old_offset -
					move.m_new_offset);
				break;
			}
		}

		for (kotek::uint32_t move_index = 0;
			 move_index < index_move_count; ++move_index)
		{
			const auto& move = index_moves[move_index];

			if (p_ranges[2] >= move.m_old_offset &&
				p_ranges[2] < move.m_old_offset + move.m_count)
			{
				p_ranges[2] -= (move.m_old_offset -
					move.m_new_offset);
				break;
			}
		}
	}

	// degenerate-fill the freed index tail (from the new live end to
	// the old high water)
	if (this->m_index_high_water > index_new_end)
	{
		kotek::ktk::memory::memset(
			this->m_index_shadow.data() + index_new_end,
			0,
			(this->m_index_high_water - index_new_end) *
				sizeof(kotek::uint32_t));
	}

	this->m_vertex_high_water = vertex_new_end;
	this->m_index_high_water = index_new_end;

	// the whole used range re-uploads
	this->m_dirty_spans.clear();
	this->m_is_full_upload_pending = true;
}

kotek::uint32_t zircon_render_csg_editor_pool::take_dirty_spans(
	zircon_csg_editor_pool_dirty_span_t* p_out_spans,
	kotek::uint32_t out_capacity) noexcept
{
	if (p_out_spans == nullptr || out_capacity == 0)
	{
		this->m_dirty_spans.clear();
		this->m_is_full_upload_pending = false;
		return 0;
	}

	kotek::uint32_t written = 0;

	if (this->m_is_full_upload_pending)
	{
		// the overflow degradation: one full-range span per pool
		if (out_capacity >= 2)
		{
			p_out_spans[0] = {0, this->m_vertex_high_water, 0};
			p_out_spans[1] = {0, this->m_index_high_water, 1};
			written = 2;
		}
	}
	else if (this->m_dirty_spans.size() <= out_capacity)
	{
		for (const auto& span : this->m_dirty_spans)
			p_out_spans[written++] = span;
	}
	else if (out_capacity >= 2)
	{
		p_out_spans[0] = {0, this->m_vertex_high_water, 0};
		p_out_spans[1] = {0, this->m_index_high_water, 1};
		written = 2;
	}

	this->m_dirty_spans.clear();
	this->m_is_full_upload_pending = false;

	return written;
}

kotek::uint32_t zircon_render_csg_editor_pool::find_compound_slot(
	kotek::uint64_t compound_id) const noexcept
{
	for (kotek::uint32_t slot = 0; slot < this->m_compound_ids.size();
		 ++slot)
	{
		if (this->m_compound_ids[slot] == compound_id)
			return slot;
	}

	return this->m_compound_ids.size();
}

void zircon_render_csg_editor_pool::append_dirty_span(
	bool is_index_pool,
	kotek::uint32_t offset,
	kotek::uint32_t count) noexcept
{
	if (count == 0)
		return;

	if (this->m_dirty_spans.full())
	{
		// the overflow degradation: the next take re-uploads the
		// whole used range
		this->m_is_full_upload_pending = true;
		return;
	}

	this->m_dirty_spans.push_back(
		{offset, count, static_cast<kotek::uint8_t>(
				is_index_pool ? 1 : 0)});
}

void zircon_render_csg_editor_pool::release_ranges(
	kotek::uint32_t slot) noexcept
{
	if (slot >= this->m_compound_ids.size() ||
		this->m_live_flags[slot] == 0)
		return;

	kotek::uint32_t* p_ranges = &this->m_ranges[slot * 4];

	// the freed index span re-reads as degenerate triangles; the
	// vertex span stays (unreferenced bytes nobody reads)
	if (p_ranges[3])
	{
		kotek::ktk::memory::memset(
			this->m_index_shadow.data() + p_ranges[2],
			0,
			p_ranges[3] * sizeof(kotek::uint32_t));

		this->append_dirty_span(true, p_ranges[2], p_ranges[3]);
	}

	this->m_vertex_allocator.free(p_ranges[0], p_ranges[1]);
	this->m_index_allocator.free(p_ranges[2], p_ranges[3]);

	this->m_live_flags[slot] = 0;
	--this->m_live_compound_count;

	if (this->m_live_compound_count == 0)
	{
		// a clean slate: the next registration starts at zero
		this->m_vertex_high_water = 0;
		this->m_index_high_water = 0;
	}
}

kotek::uint64_t zircon_render_csg_editor_pool::get_compound_id(
	kotek::uint32_t slot) const noexcept
{
	if (slot >= this->m_compound_ids.size())
		return 0;

	return this->m_compound_ids[slot];
}

bool zircon_render_csg_editor_pool::is_compound_live(
	kotek::uint32_t slot) const noexcept
{
	if (slot >= this->m_live_flags.size())
		return false;

	return this->m_live_flags[slot] != 0;
}

kotek::uint32_t zircon_render_csg_editor_pool::get_record_count(
	void) const noexcept
{
	return this->m_compound_ids.size();
}

kotek::uint32_t zircon_render_csg_editor_pool::get_live_compound_count(
	void) const noexcept
{
	return this->m_live_compound_count;
}

const zircon_model_static_vertex_t*
zircon_render_csg_editor_pool::get_vertex_shadow(void) const noexcept
{
	return this->m_vertex_shadow.data();
}

const kotek::uint32_t*
zircon_render_csg_editor_pool::get_index_shadow(void) const noexcept
{
	return this->m_index_shadow.data();
}

kotek::uint32_t zircon_render_csg_editor_pool::get_vertex_high_water(
	void) const noexcept
{
	return this->m_vertex_high_water;
}

kotek::uint32_t zircon_render_csg_editor_pool::get_index_high_water(
	void) const noexcept
{
	return this->m_index_high_water;
}

void zircon_render_csg_editor_pool::get_record_ranges(
	kotek::uint32_t slot,
	kotek::uint32_t& out_vertex_offset,
	kotek::uint32_t& out_vertex_count,
	kotek::uint32_t& out_index_offset,
	kotek::uint32_t& out_index_count) const noexcept
{
	out_vertex_offset = 0;
	out_vertex_count = 0;
	out_index_offset = 0;
	out_index_count = 0;

	if (slot >= this->m_compound_ids.size())
		return;

	const kotek::uint32_t* p_ranges = &this->m_ranges[slot * 4];

	out_vertex_offset = p_ranges[0];
	out_vertex_count = p_ranges[1];
	out_index_offset = p_ranges[2];
	out_index_count = p_ranges[3];
}

const zircon_pool_range_allocator&
zircon_render_csg_editor_pool::get_vertex_allocator(void) const noexcept
{
	return this->m_vertex_allocator;
}

const zircon_pool_range_allocator&
zircon_render_csg_editor_pool::get_index_allocator(void) const noexcept
{
	return this->m_index_allocator;
}
