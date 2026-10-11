#include "zircon_render_terrain_chunks.h"

#include <kotek.core.api/include/kotek_api.h>

#include <cmath>

// ---------------------------------------------------------------------------
// zircon_render_terrain_chunks.cpp — the static heightfield terrain's
// chunk builder (task Z24 B5a). See the header for the intake format,
// the chunking/seam policy and the capacity story.
// ---------------------------------------------------------------------------

float zircon_terrain_sample_height_meters(
	const zircon_terrain_heightfield_view_t& heightfield,
	kotek::uint16_t texel_x, kotek::uint16_t texel_y,
	float height_scale) noexcept
{
	const kotek::uint32_t sample_index = static_cast<kotek::uint32_t>(
		texel_y) * heightfield.m_resolution_x + texel_x;

	float normalized = 0.0f;

	if (heightfield.m_format == eZirconTerrainHeightmapFormat::kGray16LE)
	{
		// explicit little-endian decode (never a reinterpret_cast — the
		// LE-codec discipline: endianness- and alignment-safe on every
		// platform)
		const kotek::uint8_t lo =
			heightfield.m_p_samples[sample_index * 2];
		const kotek::uint8_t hi =
			heightfield.m_p_samples[sample_index * 2 + 1];

		normalized = static_cast<float>(
						 static_cast<kotek::uint16_t>(lo) |
						 (static_cast<kotek::uint16_t>(hi) << 8)) /
			65535.0f;
	}
	else
	{
		normalized =
			static_cast<float>(heightfield.m_p_samples[sample_index]) /
			255.0f;
	}

	return normalized * height_scale;
}

void zircon_terrain_compute_vertex(
	const zircon_terrain_heightfield_view_t& heightfield,
	kotek::uint16_t texel_x, kotek::uint16_t texel_y, float world_size_x,
	float world_size_z, float height_scale,
	zircon_model_static_vertex_t& out_vertex) noexcept
{
	const float step_x =
		world_size_x / static_cast<float>(heightfield.m_resolution_x - 1);
	const float step_z =
		world_size_z / static_cast<float>(heightfield.m_resolution_y - 1);

	out_vertex.m_position[0] =
		static_cast<float>(texel_x) * step_x - world_size_x * 0.5f;
	out_vertex.m_position[1] = zircon_terrain_sample_height_meters(
		heightfield, texel_x, texel_y, height_scale);
	out_vertex.m_position[2] =
		static_cast<float>(texel_y) * step_z - world_size_z * 0.5f;

	// the central-difference normal over the GLOBAL heightfield (edge
	// texels clamp the stencil — the two chunks sharing a boundary vertex
	// compute the identical normal, the no-shading-seam property):
	// n = normalize(-dH/dx, 1, -dH/dz) of the surface
	// S(x,z) = (x, H(x,z), z)
	const kotek::uint16_t left = texel_x > 0
		? static_cast<kotek::uint16_t>(texel_x - 1)
		: texel_x;
	const kotek::uint16_t right = texel_x + 1 < heightfield.m_resolution_x
		? static_cast<kotek::uint16_t>(texel_x + 1)
		: texel_x;
	const kotek::uint16_t up = texel_y > 0
		? static_cast<kotek::uint16_t>(texel_y - 1)
		: texel_y;
	const kotek::uint16_t down = texel_y + 1 < heightfield.m_resolution_y
		? static_cast<kotek::uint16_t>(texel_y + 1)
		: texel_y;

	const float dHdx = (zircon_terrain_sample_height_meters(
							heightfield, right, texel_y, height_scale) -
						   zircon_terrain_sample_height_meters(
							   heightfield, left, texel_y, height_scale)) /
		(static_cast<float>(right - left) * step_x);
	const float dHdz = (zircon_terrain_sample_height_meters(
							heightfield, texel_x, down, height_scale) -
						   zircon_terrain_sample_height_meters(
							   heightfield, texel_x, up, height_scale)) /
		(static_cast<float>(down - up) * step_z);

	const float normal_length =
		std::sqrt(dHdx * dHdx + 1.0f + dHdz * dHdz);

	out_vertex.m_normal[0] = -dHdx / normal_length;
	out_vertex.m_normal[1] = 1.0f / normal_length;
	out_vertex.m_normal[2] = -dHdz / normal_length;

	out_vertex.m_color_abgr = 0xffffffffu;
}

kotek::uint16_t zircon_terrain_cells_per_side(
	kotek::uint16_t resolution) noexcept
{
	// the lattice clamps the define when the resolution is smaller (a 4x4
	// heightfield has 3 quads -> 3 cells of one quad)
	const kotek::uint16_t quads =
		resolution > 1 ? static_cast<kotek::uint16_t>(resolution - 1) : 0;

	return quads < zircon_DEF_TERRAIN_CHUNK_CELLS
		? quads
		: static_cast<kotek::uint16_t>(zircon_DEF_TERRAIN_CHUNK_CELLS);
}

void zircon_terrain_cell_quad_range(kotek::uint16_t quads_per_side,
	kotek::uint16_t cells_per_side, kotek::uint16_t cell,
	kotek::uint16_t& out_quad_begin, kotek::uint16_t& out_quad_count) noexcept
{
	// front-loaded: the first (quads % cells) cells carry one extra quad
	const kotek::uint16_t base = quads_per_side / cells_per_side;
	const kotek::uint16_t remainder = quads_per_side % cells_per_side;

	out_quad_count = static_cast<kotek::uint16_t>(
		base + (cell < remainder ? 1 : 0));
	out_quad_begin = static_cast<kotek::uint16_t>(
		cell * base + (cell < remainder ? cell : remainder));
}

kotek::uint32_t zircon_terrain_chunk_count_total(
	kotek::uint16_t resolution_x, kotek::uint16_t resolution_y) noexcept
{
	return static_cast<kotek::uint32_t>(
			   zircon_terrain_cells_per_side(resolution_x)) *
		zircon_terrain_cells_per_side(resolution_y);
}

bool zircon_terrain_load_heightfield(
	kotek::core::ktkIFileSystem* p_filesystem,
	const kotek::static_path_t& heightmap_path,
	eZirconTerrainHeightmapFormat format, kotek::uint16_t resolution_x,
	kotek::uint16_t resolution_y, kotek::uint8_t* p_scratch,
	kotek::size_t scratch_capacity,
	zircon_terrain_heightfield_view_t& out_heightfield) noexcept
{
	out_heightfield.m_p_samples = nullptr;
	out_heightfield.m_resolution_x = 0;
	out_heightfield.m_resolution_y = 0;
	out_heightfield.m_format = format;

	if (p_filesystem == nullptr || p_scratch == nullptr)
	{
		KOTEK_MESSAGE_ERROR(
			"[terrain] load_heightfield: null filesystem or scratch — "
			"caller error");
		return false;
	}

	if (format != eZirconTerrainHeightmapFormat::kGray8 &&
		format != eZirconTerrainHeightmapFormat::kGray16LE)
	{
		KOTEK_MESSAGE_ERROR(
			"[terrain] load_heightfield: unknown heightmap format {} — the "
			"component descriptor is corrupt",
			static_cast<kotek::uint32_t>(format));
		return false;
	}

	if (resolution_x < 2 || resolution_y < 2 ||
		resolution_x > zircon_DEF_TERRAIN_MAX_RESOLUTION ||
		resolution_y > zircon_DEF_TERRAIN_MAX_RESOLUTION)
	{
		KOTEK_MESSAGE_ERROR(
			"[terrain] load_heightfield: resolution {}x{} is outside [2, "
			"{}] — rejected before any read",
			resolution_x, resolution_y, zircon_DEF_TERRAIN_MAX_RESOLUTION);
		return false;
	}

	const kotek::uint32_t bytes_per_sample =
		format == eZirconTerrainHeightmapFormat::kGray16LE ? 2u : 1u;

	const kotek::size_t expected_size =
		static_cast<kotek::size_t>(resolution_x) * resolution_y *
		bytes_per_sample;

	if (scratch_capacity < expected_size)
	{
		KOTEK_MESSAGE_ERROR(
			"[terrain] load_heightfield: the scratch is too small for "
			"{}x{}x{} ({} < {} bytes) — caller error, retriable",
			resolution_x, resolution_y, bytes_per_sample,
			static_cast<kotek::uint32_t>(scratch_capacity),
			static_cast<kotek::uint32_t>(expected_size));
		return false;
	}

	kotek::size_t file_size = 0;

	if (p_filesystem->Get_FileSize(heightmap_path, file_size) == false)
	{
		// the filesystem logged the absence once (the B0 contract); a
		// missing heightmap is the designed loud-graceful degradation —
		// the terrain is simply absent, never a fault (see the header's
		// Z23 note)
		KOTEK_MESSAGE_WARNING(
			"[terrain] heightmap '{}' does not resolve through the "
			"dispatcher (no pack entry, no native file) — no terrain is "
			"built",
			heightmap_path.c_str());
		return false;
	}

	if (file_size != expected_size)
	{
		KOTEK_MESSAGE_ERROR(
			"[terrain] heightmap '{}' is {} bytes, the descriptor expects "
			"{}x{}x{} = {} — the size match IS the integrity check of the "
			"headerless format, fix the content or the component",
			heightmap_path.c_str(),
			static_cast<kotek::uint32_t>(file_size), resolution_x,
			resolution_y, bytes_per_sample,
			static_cast<kotek::uint32_t>(expected_size));
		return false;
	}

	kotek::size_t read_size = scratch_capacity;

	if (p_filesystem->Read_File(heightmap_path, p_scratch, read_size) ==
			false ||
		read_size != expected_size)
	{
		KOTEK_MESSAGE_ERROR(
			"[terrain] the dispatcher read of '{}' failed ({} of {} "
			"bytes)",
			heightmap_path.c_str(), static_cast<kotek::uint32_t>(read_size),
			static_cast<kotek::uint32_t>(expected_size));
		return false;
	}

	out_heightfield.m_p_samples = p_scratch;
	out_heightfield.m_resolution_x = resolution_x;
	out_heightfield.m_resolution_y = resolution_y;

	return true;
}

kotek::uint32_t zircon_terrain_build_chunks(zircon_render_chunk_pool& pool,
	const zircon_terrain_heightfield_view_t& heightfield,
	float world_size_x, float world_size_z, float height_scale,
	kotek::uint16_t material_id, const float* p_model_matrix_or_null,
	zircon_model_static_vertex_t* p_vertex_scratch,
	kotek::uint16_t* p_index_scratch) noexcept
{
	if (heightfield.m_p_samples == nullptr ||
		p_vertex_scratch == nullptr || p_index_scratch == nullptr)
	{
		KOTEK_MESSAGE_ERROR(
			"[terrain] build_chunks: null heightfield or scratch — caller "
			"error");
		return 0;
	}

	if (heightfield.m_resolution_x < 2 || heightfield.m_resolution_y < 2 ||
		heightfield.m_resolution_x > zircon_DEF_TERRAIN_MAX_RESOLUTION ||
		heightfield.m_resolution_y > zircon_DEF_TERRAIN_MAX_RESOLUTION)
	{
		KOTEK_MESSAGE_ERROR(
			"[terrain] build_chunks: resolution {}x{} is outside [2, {}]",
			heightfield.m_resolution_x, heightfield.m_resolution_y,
			zircon_DEF_TERRAIN_MAX_RESOLUTION);
		return 0;
	}

	if (world_size_x <= 0.0f || world_size_z <= 0.0f)
	{
		KOTEK_MESSAGE_ERROR(
			"[terrain] build_chunks: non-positive world size {}x{} — the "
			"descriptor is unconfigured",
			world_size_x, world_size_z);
		return 0;
	}

	const kotek::uint16_t cells_x =
		zircon_terrain_cells_per_side(heightfield.m_resolution_x);
	const kotek::uint16_t cells_y =
		zircon_terrain_cells_per_side(heightfield.m_resolution_y);

	const kotek::uint16_t quads_x = heightfield.m_resolution_x - 1;
	const kotek::uint16_t quads_y = heightfield.m_resolution_y - 1;

	// the all-or-nothing pre-flight (the exact totals — see the header):
	// every cell re-samples its boundary texel row+column, so the vertex
	// sum factorizes: (quads + cells) per side; every quad emits its 6
	// indices exactly once
	const kotek::uint32_t total_chunks =
		static_cast<kotek::uint32_t>(cells_x) * cells_y;
	const kotek::uint32_t total_vertices =
		static_cast<kotek::uint32_t>(quads_x + cells_x) *
		(quads_y + cells_y);
	const kotek::uint32_t total_indices =
		static_cast<kotek::uint32_t>(quads_x) * quads_y * 6u;

	if (total_chunks > pool.get_free_chunk_slot_count() ||
		total_vertices > pool.get_vertex_allocator().get_free_total() ||
		total_indices > pool.get_index_allocator().get_free_total())
	{
		KOTEK_MESSAGE_ERROR(
			"[terrain] build_chunks: {} chunks / {} vertices / {} indices "
			"do not fit the pool's free space ({} slots / {} vertices / {} "
			"indices) — the terrain is absent, never half-built (raise the "
			"pool caps or shrink the terrain)",
			total_chunks, total_vertices, total_indices,
			pool.get_free_chunk_slot_count(),
			pool.get_vertex_allocator().get_free_total(),
			pool.get_index_allocator().get_free_total());
		return 0;
	}

	kotek::uint32_t registered_count = 0;

	for (kotek::uint16_t cell_y = 0; cell_y < cells_y; ++cell_y)
	{
		kotek::uint16_t quad_begin_y = 0;
		kotek::uint16_t quad_count_y = 0;

		zircon_terrain_cell_quad_range(
			quads_y, cells_y, cell_y, quad_begin_y, quad_count_y);

		for (kotek::uint16_t cell_x = 0; cell_x < cells_x; ++cell_x)
		{
			kotek::uint16_t quad_begin_x = 0;
			kotek::uint16_t quad_count_x = 0;

			zircon_terrain_cell_quad_range(
				quads_x, cells_x, cell_x, quad_begin_x, quad_count_x);

			const kotek::uint32_t vertices_per_row = quad_count_x + 1;
			const kotek::uint32_t vertex_count =
				vertices_per_row * (quad_count_y + 1);
			const kotek::uint32_t index_count =
				static_cast<kotek::uint32_t>(quad_count_x) * quad_count_y * 6u;

			// the defines guarantee the scratch fit by construction (the
			// front-loaded first cell is the worst case)
			KOTEK_ASSERT(vertex_count <= zircon_DEF_TERRAIN_CHUNK_MAX_VERTICES &&
					index_count <= zircon_DEF_TERRAIN_CHUNK_MAX_INDICES,
				"the terrain chunk scratch is undersized for the shipped "
				"defines — the derived caps are wrong");

			for (kotek::uint16_t local_y = 0; local_y <= quad_count_y;
				 ++local_y)
			{
				for (kotek::uint16_t local_x = 0; local_x <= quad_count_x;
					 ++local_x)
				{
					zircon_terrain_compute_vertex(heightfield,
						static_cast<kotek::uint16_t>(quad_begin_x + local_x),
						static_cast<kotek::uint16_t>(quad_begin_y + local_y),
						world_size_x, world_size_z, height_scale,
						p_vertex_scratch[local_y * vertices_per_row +
							local_x]);
				}
			}

			kotek::uint32_t index_cursor = 0;

			for (kotek::uint16_t local_y = 0; local_y < quad_count_y;
				 ++local_y)
			{
				for (kotek::uint16_t local_x = 0; local_x < quad_count_x;
					 ++local_x)
				{
					const kotek::uint16_t v00 = static_cast<kotek::uint16_t>(
						local_y * vertices_per_row + local_x);
					const kotek::uint16_t v10 =
						static_cast<kotek::uint16_t>(v00 + 1);
					const kotek::uint16_t v01 = static_cast<kotek::uint16_t>(
						v00 + vertices_per_row);
					const kotek::uint16_t v11 =
						static_cast<kotek::uint16_t>(v01 + 1);

					// the up-facing winding (the header's derivation:
					// (v00, v01, v10) x (v10, v01, v11) point +y on a
					// flat field)
					p_index_scratch[index_cursor++] = v00;
					p_index_scratch[index_cursor++] = v01;
					p_index_scratch[index_cursor++] = v10;
					p_index_scratch[index_cursor++] = v10;
					p_index_scratch[index_cursor++] = v01;
					p_index_scratch[index_cursor++] = v11;
				}
			}

			kotek::uint32_t chunk_id =
				zircon_render_chunk_pool::kInvalidChunkId;

			if (pool.register_chunk(p_vertex_scratch, vertex_count,
					p_index_scratch, index_count, p_model_matrix_or_null,
					material_id, chunk_id))
			{
				++registered_count;
			}
			else
			{
				// unreachable past the exact pre-flight (registration
				// defragments the pools internally on a fragmented failed
				// fit) — loud, and the partial terrain stays visible
				// rather than silently dropped
				KOTEK_MESSAGE_ERROR(
					"[terrain] build_chunks: cell ({},{}) failed to "
					"register past a successful pre-flight — {} of {} "
					"chunks built",
					cell_x, cell_y, registered_count, total_chunks);
				return registered_count;
			}
		}
	}

	return registered_count;
}
