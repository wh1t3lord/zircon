#include "zircon_csg_bake.h"

#include <kotek.core.api/include/kotek_api.h>
#include <kotek.core.filesystem.pack/include/kotek_kpack_format.h>

// ---------------------------------------------------------------------------
// zircon_csg_bake.cpp — the CSG game bake (task Z25 phase A3). The
// implementation of the contract in zircon_csg_bake.h: evaluate every
// compound through the A1 core, split the welded meshes on the world
// grid (the centroid floor rule), quantize (u16/u8 positions, u8
// octahedral per-triangle normals, u32 chunk-local indices), and write
// the pack through the shared kpack encoder. The whole bake state is
// ONE heap-allocated context (the ~6 MB evaluation context plus the
// scratch — the fixture rule); no statics (rule 1a).
// ---------------------------------------------------------------------------

namespace
{
	// the maximum chunk-entry payload: header + the capped vertex /
	// index / triangle tables (see the format banner)
	constexpr kotek::uint32_t k_chunk_scratch_size =
		zircon_csg_bake_chunk_header_size +
		ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION * 3 *
			sizeof(zircon_csg_bake_position_quant_t) +
		ZIRCON_DEF_CSG_BAKE_MAX_TRIANGLES_PER_CHUNK * 2 +
		ZIRCON_DEF_CSG_BAKE_MAX_TRIANGLES_PER_CHUNK * 3 *
			sizeof(kotek::uint32_t) +
		ZIRCON_DEF_CSG_BAKE_MAX_TRIANGLES_PER_CHUNK *
			sizeof(kotek::uint16_t);

	// one grid cell's bucket into the shared triangle-order array
	struct bake_cell_t
	{
		kotek::int32_t m_cell_x;
		kotek::int32_t m_cell_y;
		kotek::int32_t m_cell_z;
		kotek::uint32_t m_triangle_start; // into m_triangle_order
		kotek::uint32_t m_triangle_count;
	};

	// the per-chunk manifest record while the bake accumulates it
	struct bake_record_t
	{
		kotek::uint32_t m_compound_index;
		kotek::uint32_t m_chunk_index;
		kotek::int32_t m_cell[3];
		kotek::uint32_t m_welded_vertex_count;
		kotek::uint32_t m_index_count;
		kotek::uint32_t m_triangle_count;
		kotek::uint16_t m_material_id_first;
		double m_aabb_min[3];
		double m_aabb_max[3];
		kotek::uint64_t m_entry_name_hash;
	};

	// the whole bake state, heap-allocated per bake call (no statics)
	struct bake_context_t
	{
		// the A1 evaluation core (~6 MB — reused across compounds)
		zircon_csg_evaluation_t<zircon_csg_scalar_t> m_evaluation;

		// the evaluated mesh converted to double (the exact boundary —
		// to_double is exact for every precision mode)
		kotek::static_vector_t<double,
			ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION * 3>
			m_positions;
		kotek::static_vector_t<double,
			ZIRCON_DEF_CSG_MAX_TRIANGLES_PER_EVALUATION * 3>
			m_normals;
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_CSG_MAX_INDICES_PER_EVALUATION>
			m_indices;
		kotek::static_vector_t<kotek::uint16_t,
			ZIRCON_DEF_CSG_MAX_TRIANGLES_PER_EVALUATION>
			m_materials;

		// the cell buckets (the counting sort over triangles)
		kotek::static_vector_t<bake_cell_t,
			ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_COMPOUND>
			m_cells;
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_CSG_MAX_TRIANGLES_PER_EVALUATION>
			m_triangle_order;

		// the per-chunk welded-vertex remap (generation-stamped: the
		// 65536-entry table is NOT cleared between chunks — an entry is
		// valid only when its stamp equals the current generation)
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION>
			m_remap_new_index;
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION>
			m_remap_generation;
		kotek::uint32_t m_remap_generation_counter;

		// the current chunk's welded vertex list (mesh indices,
		// first-use order — the deterministic emission order)
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION>
			m_chunk_vertices;

		// the chunk-entry bin scratch (reused per chunk)
		kotek::static_vector_t<kotek::uint8_t, k_chunk_scratch_size>
			m_chunk_scratch;

		// the manifest accumulator (header + records)
		kotek::static_vector_t<kotek::uint8_t,
			zircon_csg_bake_manifest_header_size +
				ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE *
				zircon_csg_bake_manifest_record_size>
			m_manifest;
		kotek::static_vector_t<bake_record_t,
			ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE>
			m_records;

		// the material table (sorted distinct ids) + the json text
		kotek::static_vector_t<kotek::uint16_t,
			ZIRCON_DEF_CSG_BAKE_MAX_MATERIALS>
			m_materials_sorted;
		kotek::static_vector_t<char,
			ZIRCON_DEF_CSG_BAKE_MATERIALS_JSON_MAX_SIZE>
			m_materials_json;

		// the entry table + the stable name storage (the encoder keeps
		// the name pointers until the write returns) + the payload
		// arena (the byte store the chunk bins are appended to)
		kotek::static_vector_t<kotek::static_cstring_t<
				ZIRCON_DEF_CSG_BAKE_ENTRY_NAME_MAX_LENGTH>,
			ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE + 2>
			m_entry_names;
		kotek::static_vector_t<kotek::core::kpack_writer_entry_t,
			ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE + 2>
			m_entries;
		kotek::uint8_t* m_p_payload;
		kotek::uint32_t m_payload_used;

		bake_context_t(void) :
			m_remap_generation_counter{0},
			m_p_payload{nullptr},
			m_payload_used{0}
		{
			m_remap_new_index.resize(
				ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION);
			m_remap_generation.resize(
				ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION);

			for (kotek::uint32_t& stamp : m_remap_generation)
			{
				stamp = 0u;
			}

			m_p_payload = new kotek::uint8_t
				[ZIRCON_DEF_CSG_BAKE_MAX_TOTAL_PAYLOAD];
		}

		~bake_context_t(void) { delete[] this->m_p_payload; }

		bake_context_t(const bake_context_t&) = delete;
		bake_context_t& operator=(const bake_context_t&) = delete;
	};

	// u32 -> decimal append (hand-rolled: deterministic, locale-free,
	// no CRT formatting behind the engine's back)
	void bake_append_u32(kotek::static_cstring_t<
			ZIRCON_DEF_CSG_BAKE_ENTRY_NAME_MAX_LENGTH>& out_string,
		kotek::uint32_t value) noexcept
	{
		char digits[10];
		kotek::uint32_t count = 0;

		do
		{
			digits[count++] =
				static_cast<char>('0' + (value % 10u));
			value /= 10u;
		} while (value != 0u);

		while (count > 0)
		{
			char symbol[2] = {digits[--count], '\0'};
			out_string += symbol;
		}
	}

	// "csg/<scene>/compound_<compound>/chunk_<chunk>.bin"
	void bake_build_entry_name(kotek::static_cstring_t<
			ZIRCON_DEF_CSG_BAKE_ENTRY_NAME_MAX_LENGTH>& out_name,
		const char* p_scene_name, kotek::uint32_t compound_index,
		kotek::uint32_t chunk_index) noexcept
	{
		out_name.assign("csg/");
		out_name += p_scene_name;
		out_name += "/compound_";
		bake_append_u32(out_name, compound_index);
		out_name += "/chunk_";
		bake_append_u32(out_name, chunk_index);
		out_name += ".bin";
	}

	bool bake_is_scene_name_valid(const char* p_scene_name
	) noexcept
	{
		if (p_scene_name == nullptr || p_scene_name[0] == '\0')
			return false;

		kotek::uint32_t length = 0;

		while (p_scene_name[length] != '\0')
		{
			const char symbol = p_scene_name[length];

			const bool is_valid =
				(symbol >= 'a' && symbol <= 'z') ||
				(symbol >= 'A' && symbol <= 'Z') ||
				(symbol >= '0' && symbol <= '9') || symbol == '_' ||
				symbol == '-';

			if (is_valid == false)
				return false;

			++length;

			if (length > ZIRCON_DEF_CSG_BAKE_MAX_SCENE_NAME_LENGTH)
				return false;
		}

		return true;
	}

	// the sorted distinct insert (lookup-table-on-vector discipline:
	// the table is <= 4096 and inserts happen per triangle — binary
	// search + shift-insert)
	bool bake_insert_material(bake_context_t& context,
		kotek::uint16_t material_id) noexcept
	{
		kotek::uint32_t low = 0;
		kotek::uint32_t high =
			static_cast<kotek::uint32_t>(context.m_materials_sorted.size());

		while (low < high)
		{
			const kotek::uint32_t middle = (low + high) / 2;

			if (context.m_materials_sorted[middle] < material_id)
				low = middle + 1;
			else
				high = middle;
		}

		if (low < context.m_materials_sorted.size() &&
			context.m_materials_sorted[low] == material_id)
		{
			return true;
		}

		if (context.m_materials_sorted.full())
		{
			KOTEK_MESSAGE_ERROR(
				"[csg_bake] the material table exceeds {} ids — the "
				"bake stops (raise ZIRCON_DEF_CSG_BAKE_MAX_MATERIALS)",
				ZIRCON_DEF_CSG_BAKE_MAX_MATERIALS);
			return false;
		}

		context.m_materials_sorted.insert(
			context.m_materials_sorted.begin() + low, material_id);
		return true;
	}

	// the lexicographic (x, y, z) cell order — the documented streaming
	// order. Insertion sort: deterministic, stable, and the cell count
	// is capped small (256)
	void bake_sort_cells(bake_context_t& context) noexcept
	{
		for (kotek::uint32_t index = 1;
			 index < context.m_cells.size(); ++index)
		{
			const bake_cell_t key = context.m_cells[index];

			kotek::uint32_t position = index;

			while (position > 0)
			{
				const bake_cell_t& previous =
					context.m_cells[position - 1];

				const bool is_previous_less =
					previous.m_cell_x < key.m_cell_x ||
					(previous.m_cell_x == key.m_cell_x &&
						previous.m_cell_y < key.m_cell_y) ||
					(previous.m_cell_x == key.m_cell_x &&
						previous.m_cell_y == key.m_cell_y &&
						previous.m_cell_z < key.m_cell_z);

				if (is_previous_less)
					break;

				context.m_cells[position] = previous;
				--position;
			}

			context.m_cells[position] = key;
		}
	}

	// evaluates + converts one compound to the double scratch. false =
	// the compound is skipped (the loud warning already emitted)
	bool bake_prepare_compound(bake_context_t& context,
		const zircon_csg_bake_compound_input_t& input,
		kotek::uint32_t compound_index,
		kotek::uint32_t& out_triangle_count) noexcept
	{
		out_triangle_count = 0;

		if (input.p_primitives == nullptr ||
			input.m_primitive_count == 0 ||
			input.m_primitive_count >
				ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND)
		{
			KOTEK_MESSAGE_WARNING(
				"[csg_bake] compound {} has an invalid primitive list "
				"({} primitives) — skipped",
				compound_index, input.m_primitive_count);
			return false;
		}

		const eZirconCsgEvaluationStatus status =
			zircon_csg_evaluate_compound(input.p_primitives,
				input.m_primitive_count, context.m_evaluation);

		if (status != eZirconCsgEvaluationStatus::kSuccess)
		{
			KOTEK_MESSAGE_WARNING(
				"[csg_bake] compound {} evaluated with status {} — "
				"skipped",
				compound_index, static_cast<kotek::uint32_t>(status));
			return false;
		}

		const auto& mesh = context.m_evaluation.m_mesh;

		const kotek::uint32_t triangle_count =
			static_cast<kotek::uint32_t>(mesh.m_indices.size() / 3);

		if (triangle_count == 0 || mesh.m_positions.empty() ||
			mesh.m_normals.size() < triangle_count ||
			mesh.m_materials.size() < triangle_count)
		{
			KOTEK_MESSAGE_WARNING(
				"[csg_bake] compound {} evaluated to an empty mesh — "
				"skipped",
				compound_index);
			return false;
		}

		using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_t>;

		context.m_positions.resize(mesh.m_positions.size() * 3);
		context.m_normals.resize(triangle_count * 3);
		context.m_indices.resize(mesh.m_indices.size());
		context.m_materials.resize(triangle_count);

		for (kotek::uint32_t vertex = 0;
			 vertex < mesh.m_positions.size(); ++vertex)
		{
			for (int axis = 0; axis < 3; ++axis)
			{
				context.m_positions[vertex * 3 + axis] =
					traits_t::to_double(mesh.m_positions[vertex].m[axis]);
			}
		}

		for (kotek::uint32_t triangle = 0; triangle < triangle_count;
			 ++triangle)
		{
			for (int axis = 0; axis < 3; ++axis)
			{
				context.m_normals[triangle * 3 + axis] =
					traits_t::to_double(mesh.m_normals[triangle].m[axis]);
			}

			context.m_materials[triangle] = mesh.m_materials[triangle];
		}

		for (kotek::uint32_t index = 0; index < mesh.m_indices.size();
			 ++index)
		{
			context.m_indices[index] = mesh.m_indices[index];
		}

		out_triangle_count = triangle_count;
		return true;
	}

	// buckets the converted mesh's triangles into grid cells (the
	// counting sort). false = the compound exceeds the per-compound
	// chunk cap (the loud warning already emitted)
	bool bake_bucket_compound(bake_context_t& context,
		kotek::uint32_t compound_index, kotek::uint32_t triangle_count
		) noexcept
	{
		context.m_cells.clear();

		for (kotek::uint32_t triangle = 0; triangle < triangle_count;
			 ++triangle)
		{
			const kotek::uint32_t base = triangle * 3;

			double centroid[3];

			for (int axis = 0; axis < 3; ++axis)
			{
				centroid[axis] =
					(context.m_positions
							[context.m_indices[base] * 3 + axis] +
						context.m_positions
							[context.m_indices[base + 1] * 3 + axis] +
						context.m_positions
							[context.m_indices[base + 2] * 3 + axis]) /
					3.0;
			}

			const kotek::int32_t cell_x =
				zircon_csg_bake_cell_for_coordinate(centroid[0]);
			const kotek::int32_t cell_y =
				zircon_csg_bake_cell_for_coordinate(centroid[1]);
			const kotek::int32_t cell_z =
				zircon_csg_bake_cell_for_coordinate(centroid[2]);

			// the lookup-table-on-vector scan (the cell count is
			// capped at 256)
			kotek::uint32_t cell_index = 0;

			for (; cell_index < context.m_cells.size(); ++cell_index)
			{
				bake_cell_t& cell = context.m_cells[cell_index];

				if (cell.m_cell_x == cell_x && cell.m_cell_y == cell_y &&
					cell.m_cell_z == cell_z)
				{
					++cell.m_triangle_count;
					break;
				}
			}

			if (cell_index == context.m_cells.size())
			{
				if (context.m_cells.full())
				{
					KOTEK_MESSAGE_WARNING(
						"[csg_bake] compound {} spans more than {} "
						"grid cells — skipped (raise "
						"ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_COMPOUND "
						"if the content is real)",
						compound_index,
						ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_COMPOUND);
					return false;
				}

				bake_cell_t cell{};
				cell.m_cell_x = cell_x;
				cell.m_cell_y = cell_y;
				cell.m_cell_z = cell_z;
				cell.m_triangle_start = 0;
				cell.m_triangle_count = 1;
				context.m_cells.push_back(cell);
			}
		}

		// the counting sort: prefix sums, then the fill pass (the
		// per-cell cursor rides the reset m_triangle_count)
		kotek::uint32_t running = 0;

		for (bake_cell_t& cell : context.m_cells)
		{
			cell.m_triangle_start = running;
			running += cell.m_triangle_count;
			cell.m_triangle_count = 0;
		}

		context.m_triangle_order.resize(running);

		for (kotek::uint32_t triangle = 0; triangle < triangle_count;
			 ++triangle)
		{
			const kotek::uint32_t base = triangle * 3;

			double centroid[3];

			for (int axis = 0; axis < 3; ++axis)
			{
				centroid[axis] =
					(context.m_positions
							[context.m_indices[base] * 3 + axis] +
						context.m_positions
							[context.m_indices[base + 1] * 3 + axis] +
						context.m_positions
							[context.m_indices[base + 2] * 3 + axis]) /
					3.0;
			}

			const kotek::int32_t cell_x =
				zircon_csg_bake_cell_for_coordinate(centroid[0]);
			const kotek::int32_t cell_y =
				zircon_csg_bake_cell_for_coordinate(centroid[1]);
			const kotek::int32_t cell_z =
				zircon_csg_bake_cell_for_coordinate(centroid[2]);

			for (bake_cell_t& cell : context.m_cells)
			{
				if (cell.m_cell_x == cell_x && cell.m_cell_y == cell_y &&
					cell.m_cell_z == cell_z)
				{
					context.m_triangle_order[cell.m_triangle_start +
						cell.m_triangle_count] = triangle;
					++cell.m_triangle_count;
					break;
				}
			}
		}

		bake_sort_cells(context);

		// the loadability pre-check: a chunk over the triangle cap
		// could never register in the pool — skip the compound loudly
		// BEFORE any of its chunks are emitted (no orphan entries)
		for (const bake_cell_t& cell : context.m_cells)
		{
			if (cell.m_triangle_count >
				ZIRCON_DEF_CSG_BAKE_MAX_TRIANGLES_PER_CHUNK)
			{
				KOTEK_MESSAGE_WARNING(
					"[csg_bake] compound {} has a cell with {} "
					"triangles (cap {} — the pool's u16 vertex "
					"count) — skipped",
					compound_index, cell.m_triangle_count,
					ZIRCON_DEF_CSG_BAKE_MAX_TRIANGLES_PER_CHUNK);
				return false;
			}
		}

		return true;
	}

	// emits one chunk's entry (the bin into the payload arena, the
	// entry table row, the manifest record). false = a hard bake
	// failure (the payload cap — the whole bake stops)
	bool bake_emit_chunk(bake_context_t& context,
		const char* p_scene_name, kotek::uint32_t compound_index,
		kotek::uint32_t chunk_index, const bake_cell_t& cell,
		kotek::uint32_t& in_out_welded_total,
		kotek::uint32_t& in_out_index_total) noexcept
	{
		// the scene's chunk budget (the pack reader's entry cap): a
		// hard failure — the bake must never emit an unloadable pack
		if (context.m_records.full())
		{
			KOTEK_MESSAGE_ERROR(
				"[csg_bake] the scene emitted {} chunks (cap {} — the "
				"pack reader's entry budget) — the bake stops",
				context.m_records.size(),
				ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE);
			return false;
		}

		// the welded vertex list (first-use order, the deterministic
		// emission order)
		context.m_chunk_vertices.clear();

		++context.m_remap_generation_counter;

		if (context.m_remap_generation_counter == 0u)
		{
			// the 32-bit generation wrapped: clear the stamp table
			// (2^32 chunks per bake is unreachable — the guard is
			// the wrap-safety discipline, not a reachable path)
			for (kotek::uint32_t& stamp : context.m_remap_generation)
			{
				stamp = 0u;
			}
			context.m_remap_generation_counter = 1u;
		}

		for (kotek::uint32_t order = 0; order < cell.m_triangle_count;
			 ++order)
		{
			const kotek::uint32_t triangle =
				context.m_triangle_order[cell.m_triangle_start + order];

			for (kotek::uint8_t corner = 0; corner < 3; ++corner)
			{
				const kotek::uint32_t old_index =
					context.m_indices[triangle * 3 + corner];

				if (context.m_remap_generation[old_index] !=
					context.m_remap_generation_counter)
				{
					context.m_remap_generation[old_index] =
						context.m_remap_generation_counter;
					context.m_remap_new_index[old_index] =
						static_cast<kotek::uint32_t>(
							context.m_chunk_vertices.size());
					context.m_chunk_vertices.push_back(old_index);
				}
			}
		}

		const kotek::uint32_t welded_count =
			static_cast<kotek::uint32_t>(context.m_chunk_vertices.size());
		const kotek::uint32_t index_count = cell.m_triangle_count * 3;

		// the chunk's exact world AABB in double (the manifest's
		// dequantization bounds AND the pool's expected bounds)
		double aabb_min[3];
		double aabb_max[3];

		for (kotek::uint32_t welded = 0; welded < welded_count; ++welded)
		{
			const kotek::uint32_t old_index =
				context.m_chunk_vertices[welded];

			for (int axis = 0; axis < 3; ++axis)
			{
				const double value =
					context.m_positions[old_index * 3 + axis];

				if (welded == 0 || value < aabb_min[axis])
					aabb_min[axis] = value;
				if (welded == 0 || value > aabb_max[axis])
					aabb_max[axis] = value;
			}
		}

		// the bin: header + positions + normals + indices + materials
		const kotek::uint32_t bin_size =
			zircon_csg_bake_chunk_header_size +
			welded_count * 3 *
				sizeof(zircon_csg_bake_position_quant_t) +
			cell.m_triangle_count * 2 + index_count * sizeof(kotek::uint32_t) +
			cell.m_triangle_count * sizeof(kotek::uint16_t);

		if (context.m_payload_used + bin_size >
			ZIRCON_DEF_CSG_BAKE_MAX_TOTAL_PAYLOAD)
		{
			KOTEK_MESSAGE_ERROR(
				"[csg_bake] the chunk payload exceeds {} bytes — the "
				"bake stops (raise "
				"ZIRCON_DEF_CSG_BAKE_MAX_TOTAL_PAYLOAD)",
				ZIRCON_DEF_CSG_BAKE_MAX_TOTAL_PAYLOAD);
			return false;
		}

		context.m_chunk_scratch.resize(bin_size);

		std::memcpy(context.m_chunk_scratch.data(),
			zircon_csg_bake_chunk_magic, 4);
		zircon_csg_bake_store_u32(context.m_chunk_scratch.data() + 4,
			welded_count);
		zircon_csg_bake_store_u32(context.m_chunk_scratch.data() + 8,
			index_count);
		zircon_csg_bake_store_u32(context.m_chunk_scratch.data() + 12,
			cell.m_triangle_count);

		kotek::uint32_t cursor = zircon_csg_bake_chunk_header_size;

		// the positions, quantized per axis against the chunk AABB
		for (kotek::uint32_t welded = 0; welded < welded_count; ++welded)
		{
			const kotek::uint32_t old_index =
				context.m_chunk_vertices[welded];

			for (int axis = 0; axis < 3; ++axis)
			{
				const double value =
					context.m_positions[old_index * 3 + axis];
				const double extent = aabb_max[axis] - aabb_min[axis];

				const zircon_csg_bake_position_quant_t quantized =
					zircon_csg_bake_quantize_position(
						value, aabb_min[axis], extent);

				if constexpr (sizeof(zircon_csg_bake_position_quant_t) ==
					2)
				{
					zircon_csg_bake_store_u16(
						context.m_chunk_scratch.data() + cursor,
						static_cast<kotek::uint16_t>(quantized));
					cursor += 2;
				}
				else
				{
					context.m_chunk_scratch.data()[cursor] =
						static_cast<kotek::uint8_t>(quantized);
					cursor += 1;
				}
			}
		}

		// the per-triangle octahedral normals
		for (kotek::uint32_t order = 0; order < cell.m_triangle_count;
			 ++order)
		{
			const kotek::uint32_t triangle =
				context.m_triangle_order[cell.m_triangle_start + order];

			zircon_csg_bake_encode_normal_oct_u8(
				context.m_normals.data() + triangle * 3,
				context.m_chunk_scratch.data() + cursor);
			cursor += 2;
		}

		// the chunk-local indices (the remapped weld)
		for (kotek::uint32_t order = 0; order < cell.m_triangle_count;
			 ++order)
		{
			const kotek::uint32_t triangle =
				context.m_triangle_order[cell.m_triangle_start + order];

			for (kotek::uint8_t corner = 0; corner < 3; ++corner)
			{
				const kotek::uint32_t old_index =
					context.m_indices[triangle * 3 + corner];

				zircon_csg_bake_store_u32(
					context.m_chunk_scratch.data() + cursor,
					context.m_remap_new_index[old_index]);
				cursor += 4;
			}
		}

		// the per-triangle materials
		for (kotek::uint32_t order = 0; order < cell.m_triangle_count;
			 ++order)
		{
			const kotek::uint32_t triangle =
				context.m_triangle_order[cell.m_triangle_start + order];

			zircon_csg_bake_store_u16(
				context.m_chunk_scratch.data() + cursor,
				context.m_materials[triangle]);
			cursor += 2;
		}

		// the payload copy + the entry table row
		std::memcpy(context.m_p_payload + context.m_payload_used,
			context.m_chunk_scratch.data(), bin_size);

		const kotek::uint32_t payload_offset = context.m_payload_used;
		context.m_payload_used += bin_size;

		context.m_entry_names.push_back(
			kotek::static_cstring_t<
				ZIRCON_DEF_CSG_BAKE_ENTRY_NAME_MAX_LENGTH>{});
		bake_build_entry_name(context.m_entry_names.back(), p_scene_name,
			compound_index, chunk_index);

		const char* p_entry_name = context.m_entry_names.back().c_str();

		kotek::core::kpack_writer_entry_t entry{};
		entry.p_name = p_entry_name;
		entry.p_data = context.m_p_payload + payload_offset;
		entry.data_size = bin_size;
		// the geometry payload compresses well (the quantized vertex
		// tables are highly repetitive); the manifest/materials ride
		// stored (tiny, and the reader's first contact)
		entry.compression = kotek::core::eKpackCompression::kZstd;
		context.m_entries.push_back(entry);

		// the manifest record
		bake_record_t record{};
		record.m_compound_index = compound_index;
		record.m_chunk_index = chunk_index;
		record.m_cell[0] = cell.m_cell_x;
		record.m_cell[1] = cell.m_cell_y;
		record.m_cell[2] = cell.m_cell_z;
		record.m_welded_vertex_count = welded_count;
		record.m_index_count = index_count;
		record.m_triangle_count = cell.m_triangle_count;

		const kotek::uint32_t first_triangle =
			context.m_triangle_order[cell.m_triangle_start];
		record.m_material_id_first = context.m_materials[first_triangle];

		for (int axis = 0; axis < 3; ++axis)
		{
			record.m_aabb_min[axis] = aabb_min[axis];
			record.m_aabb_max[axis] = aabb_max[axis];
		}

		record.m_entry_name_hash = kotek::core::kpack_hash_name(
			p_entry_name, std::strlen(p_entry_name));

		context.m_records.push_back(record);

		in_out_welded_total += welded_count;
		in_out_index_total += index_count;
		return true;
	}

	// the materials.json text: {"materials":[<id>,...]}
	bool bake_write_materials_json(bake_context_t& context) noexcept
	{
		context.m_materials_json.clear();

		const char* p_prefix = "{\"materials\":[";
		const kotek::uint32_t prefix_length =
			static_cast<kotek::uint32_t>(std::strlen(p_prefix));

		for (kotek::uint32_t index = 0; index < prefix_length; ++index)
		{
			context.m_materials_json.push_back(p_prefix[index]);
		}

		for (kotek::uint32_t index = 0;
			 index < context.m_materials_sorted.size(); ++index)
		{
			if (index > 0)
				context.m_materials_json.push_back(',');

			char digits[5];
			kotek::uint32_t count = 0;
			kotek::uint32_t value = context.m_materials_sorted[index];

			do
			{
				digits[count++] =
					static_cast<char>('0' + (value % 10u));
				value /= 10u;
			} while (value != 0u);

			while (count > 0)
			{
				context.m_materials_json.push_back(digits[--count]);
			}
		}

		const char* p_suffix = "]}";
		context.m_materials_json.push_back(p_suffix[0]);
		context.m_materials_json.push_back(p_suffix[1]);

		return context.m_materials_json.full() == false;
	}
} // namespace

bool zircon_csg_bake_pack(kotek::core::ktkIFileSystem* p_filesystem,
	const kotek::static_path_t& pack_path_relative_to_root,
	const zircon_csg_bake_compound_input_t* p_compounds,
	kotek::uint32_t compound_count, const char* p_scene_name,
	zircon_csg_bake_result_t& out_result) noexcept
{
	out_result = zircon_csg_bake_result_t{};

	KOTEK_ASSERT(
		p_filesystem, "the bake resolves the pack path through the filesystem"
	);
	KOTEK_ASSERT(p_compounds, "the bake needs the compound array");
	KOTEK_ASSERT(p_scene_name, "the bake needs the scene name");

	if (p_filesystem == nullptr || p_compounds == nullptr ||
		p_scene_name == nullptr)
	{
		return false;
	}

	if (compound_count == 0 ||
		compound_count > ZIRCON_DEF_CSG_BAKE_MAX_COMPOUNDS)
	{
		KOTEK_MESSAGE_ERROR(
			"[csg_bake] compound count {} is outside [1, {}]",
			compound_count, ZIRCON_DEF_CSG_BAKE_MAX_COMPOUNDS);
		return false;
	}

	if (bake_is_scene_name_valid(p_scene_name) == false)
	{
		KOTEK_MESSAGE_ERROR(
			"[csg_bake] scene name '{}' is invalid (1..{} of "
			"[a-zA-Z0-9_-])",
			p_scene_name, ZIRCON_DEF_CSG_BAKE_MAX_SCENE_NAME_LENGTH);
		return false;
	}

	// the pack path resolved against the filesystem root (cwd-
	// independent); the caller created the parent directory
	kotek::static_path_t pack_path;
	p_filesystem->Make_Path(
		pack_path, kotek::core::eFolderIndex::kFolderIndex_Root);
	pack_path /= pack_path_relative_to_root;

	bake_context_t* p_context = new bake_context_t();

	bool is_ok = true;

	// ---- per-compound evaluate + chunk + emit ----
	for (kotek::uint32_t compound_index = 0;
		 compound_index < compound_count && is_ok; ++compound_index)
	{
		kotek::uint32_t triangle_count = 0;

		if (bake_prepare_compound(*p_context,
				p_compounds[compound_index], compound_index,
				triangle_count) == false)
		{
			++out_result.m_skipped_compound_count;
			continue;
		}

		// the material table collects every triangle of the compound
		// (a capacity breach is a hard failure)
		for (kotek::uint32_t triangle = 0; triangle < triangle_count;
			 ++triangle)
		{
			if (bake_insert_material(*p_context,
					p_context->m_materials[triangle]) == false)
			{
				is_ok = false;
				break;
			}
		}

		if (is_ok == false)
			break;

		if (bake_bucket_compound(
				*p_context, compound_index, triangle_count) == false)
		{
			++out_result.m_skipped_compound_count;
			continue;
		}

		for (kotek::uint32_t cell_index = 0;
			 cell_index < p_context->m_cells.size() && is_ok;
			 ++cell_index)
		{
			// the per-compound emission index is the cell's position
			// in the sorted cell list (the documented streaming
			// order): every cell emits exactly one chunk, in order
			if (bake_emit_chunk(*p_context, p_scene_name,
					compound_index, cell_index,
					p_context->m_cells[cell_index],
					out_result.m_emitted_vertex_count,
					out_result.m_emitted_index_count) == false)
			{
				is_ok = false;
			}
		}

		if (is_ok == false)
			break;

		++out_result.m_baked_compound_count;
	}

	// ---- the manifest + materials.json + the pack write ----
	if (is_ok)
	{
		if (bake_write_materials_json(*p_context) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[csg_bake] the materials json exceeded its bound "
				"({} bytes)",
				ZIRCON_DEF_CSG_BAKE_MATERIALS_JSON_MAX_SIZE);
			is_ok = false;
		}
	}

	if (is_ok)
	{
		// the manifest: header + records in emission order
		const kotek::uint32_t record_count =
			static_cast<kotek::uint32_t>(p_context->m_records.size());
		const kotek::uint32_t manifest_size =
			zircon_csg_bake_manifest_header_size +
			record_count * zircon_csg_bake_manifest_record_size;

		p_context->m_manifest.resize(manifest_size);
		kotek::uint8_t* p_manifest = p_context->m_manifest.data();

		std::memcpy(p_manifest, zircon_csg_bake_manifest_magic, 8);

		float chunk_size_meters =
			static_cast<float>(ZIRCON_DEF_CSG_BAKE_CHUNK_SIZE_METERS);
		kotek::uint32_t chunk_size_bits = 0;
		std::memcpy(&chunk_size_bits, &chunk_size_meters,
			sizeof(chunk_size_bits));
		zircon_csg_bake_store_u32(p_manifest + 8, chunk_size_bits);

		zircon_csg_bake_store_u32(p_manifest + 12,
			out_result.m_baked_compound_count);
		zircon_csg_bake_store_u32(p_manifest + 16, record_count);
		zircon_csg_bake_store_u32(p_manifest + 20,
			out_result.m_emitted_vertex_count);
		zircon_csg_bake_store_u32(p_manifest + 24,
			out_result.m_emitted_index_count);
		zircon_csg_bake_store_u32(p_manifest + 28, 0u);

		for (kotek::uint32_t record_index = 0;
			 record_index < record_count; ++record_index)
		{
			const bake_record_t& record =
				p_context->m_records[record_index];
			kotek::uint8_t* p_record =
				p_manifest + zircon_csg_bake_manifest_header_size +
				record_index * zircon_csg_bake_manifest_record_size;

			zircon_csg_bake_store_u32(p_record + 0,
				record.m_compound_index);
			zircon_csg_bake_store_u32(p_record + 4,
				record.m_chunk_index);
			zircon_csg_bake_store_i32(p_record + 8, record.m_cell[0]);
			zircon_csg_bake_store_i32(p_record + 12, record.m_cell[1]);
			zircon_csg_bake_store_i32(p_record + 16, record.m_cell[2]);
			zircon_csg_bake_store_u32(p_record + 20,
				record.m_welded_vertex_count);
			zircon_csg_bake_store_u32(p_record + 24,
				record.m_index_count);
			zircon_csg_bake_store_u32(p_record + 28,
				record.m_triangle_count);
			zircon_csg_bake_store_u16(p_record + 32,
				record.m_material_id_first);
			zircon_csg_bake_store_u16(p_record + 34, 0u);

			for (int axis = 0; axis < 3; ++axis)
			{
				zircon_csg_bake_store_f64(p_record + 36 + axis * 8,
					record.m_aabb_min[axis]);
				zircon_csg_bake_store_f64(p_record + 60 + axis * 8,
					record.m_aabb_max[axis]);
			}

			zircon_csg_bake_store_u64(p_record + 84,
				record.m_entry_name_hash);
			zircon_csg_bake_store_u32(p_record + 92, 0u);
		}

		// the entry order = the load/streaming order: manifest,
		// materials, then the chunk bins in emission order (they were
		// appended per chunk already). The name strings live in this
		// scope until the encoder returns (the encoder keeps the
		// pointers)
		kotek::static_cstring_t<
			ZIRCON_DEF_CSG_BAKE_ENTRY_NAME_MAX_LENGTH>
			manifest_name;
		manifest_name.assign("csg/");
		manifest_name += p_scene_name;
		manifest_name += "/manifest.bin";

		kotek::static_cstring_t<
			ZIRCON_DEF_CSG_BAKE_ENTRY_NAME_MAX_LENGTH>
			materials_name;
		materials_name.assign("csg/");
		materials_name += p_scene_name;
		materials_name += "/materials.json";

		// rebuild the entry table: manifest + materials first, then
		// the chunk rows in emission order (heap: ~131 KB, the fixture
		// rule)
		auto* p_final_entries =
			new kotek::static_vector_t<kotek::core::kpack_writer_entry_t,
				ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE + 2>();

		kotek::core::kpack_writer_entry_t manifest_entry{};
		manifest_entry.p_name = manifest_name.c_str();
		manifest_entry.p_data = p_manifest;
		manifest_entry.data_size = manifest_size;
		manifest_entry.compression =
			kotek::core::eKpackCompression::kStored;
		p_final_entries->push_back(manifest_entry);

		kotek::core::kpack_writer_entry_t materials_entry{};
		materials_entry.p_name = materials_name.c_str();
		materials_entry.p_data =
			reinterpret_cast<const kotek::uint8_t*>(
				p_context->m_materials_json.data());
		materials_entry.data_size =
			p_context->m_materials_json.size();
		materials_entry.compression =
			kotek::core::eKpackCompression::kStored;
		p_final_entries->push_back(materials_entry);

		for (const auto& chunk_entry : p_context->m_entries)
		{
			p_final_entries->push_back(chunk_entry);
		}

		is_ok = kotek::core::kpack_write_file(pack_path.c_str(),
			p_final_entries->data(), p_final_entries->size());

		delete p_final_entries;

		if (is_ok == false)
		{
			// the encoder already reported the reason
			KOTEK_MESSAGE_ERROR(
				"[csg_bake] the pack write failed for '{}'",
				pack_path.c_str());
		}
	}

	out_result.m_emitted_chunk_count =
		static_cast<kotek::uint32_t>(p_context->m_records.size());
	out_result.m_material_count =
		static_cast<kotek::uint32_t>(p_context->m_materials_sorted.size());

	delete p_context;
	return is_ok;
}
