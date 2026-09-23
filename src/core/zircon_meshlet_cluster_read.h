#pragma once

// zircon_meshlet_cluster_read.h — the B3b READER of the B3a meshlet pack
// format (task Z24 B3b, the nanite path's first runtime consumer). Pure
// statics over caller-owned byte buffers: the caller reads the entries
// through the filesystem dispatcher (packs-first, native fallback — the
// data flow the CSG loader established), this header parses + dequantizes
// + expands them into the interleaved vertex-buffer layout the NRI
// meshlet pass draws. The FORMAT's single source of truth stays
// zircon_meshlet_clusterize.h — the layouts there and here are the same
// fields; a format change updates both in one commit.
//
// THE UPLOAD LAYOUT (the v1 draw's vertex contract): the cluster stores
// WELDED positions + PER-TRIANGLE flat normals — a welded vertex cannot
// carry a flat normal, so the reader EXPANDS to a triangle soup (the
// editor CSG pool's exact discipline): every triangle contributes 3
// vertices [pos.xyz (float3) | normal.xyz (float3)] = 24 bytes,
// interleaved, and the index buffer is the IDENTITY (vertex i is soup
// vertex i, u32). The baked cluster-local indices still VALIDATE the
// read (every index < the welded vertex count) but do not shape the
// output. Capacities come from the B3a budgets: a LOD0 cluster is
// <= 124 triangles -> <= 372 soup vertices (parents double per level —
// the reader is level-agnostic through explicit capacities).

#include <kotek.core.containers.vector/include/kotek_core_containers_vector.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>

#include <cstring>

#include "zircon_meshlet_clusterize.h"

/// the soup vertex: 2 x float3 (the interleaved stream stride)
#define ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_FLOATS 6
#define ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_BYTES \
	(ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_FLOATS * sizeof(float))

/// the one-cluster read output (caller-owned storage; the byte sizes
/// below are the exact required capacities)
struct zircon_meshlet_cluster_content_t
{
	kotek::uint32_t m_welded_vertex_count{}; // the bin's own count
	kotek::uint32_t m_triangle_count{};
	/// the soup vertex count — exactly 3 x the triangle count
	kotek::uint32_t m_soup_vertex_count{};
	/// the identity index count — exactly 3 x the triangle count
	kotek::uint32_t m_index_count{};
};

/// \~english the exact required interleaved VB capacity in FLOATS for a
/// cluster of the given triangle count (3 soup vertices x 6 floats)
inline constexpr kotek::uint32_t zircon_meshlet_read_vb_capacity_floats(
	kotek::uint32_t triangle_count) noexcept
{
	return triangle_count * 3u *
		ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_FLOATS;
}

/// \~english the exact required IB capacity in u32 elements
inline constexpr kotek::uint32_t zircon_meshlet_read_ib_capacity(
	kotek::uint32_t triangle_count) noexcept
{
	return triangle_count * 3u;
}

/// the manifest header fields the runtime consumes (the level table and
/// the cluster records follow — read by index below)
struct zircon_meshlet_read_manifest_header_t
{
	kotek::uint32_t m_lod_level_count;
	kotek::uint32_t m_cluster_count_total;
	kotek::uint32_t m_mesh_triangle_count;
	kotek::uint32_t m_child_link_count_total;
};

/// \~english parses + validates the manifest header (32 bytes + magic +
/// the reserved words); false on any skew — the caller logs, a corrupt
/// user content file is never a crash
inline bool zircon_meshlet_read_manifest_header(
	const kotek::uint8_t* p_manifest, kotek::size_t manifest_size,
	zircon_meshlet_read_manifest_header_t& out_header) noexcept
{
	if (p_manifest == nullptr ||
		manifest_size < zircon_meshlet_manifest_header_size)
	{
		return false;
	}

	if (std::memcmp(p_manifest, zircon_meshlet_manifest_magic, 8) != 0)
		return false;

	// the flags + the reserved word must be 0 (the version rides in the
	// magic digits — an unknown magic is rejected above)
	if (zircon_csg_bake_load_u32(p_manifest + 8) != 0u ||
		zircon_csg_bake_load_u32(p_manifest + 28) != 0u)
	{
		return false;
	}

	out_header.m_lod_level_count =
		zircon_csg_bake_load_u32(p_manifest + 12);
	out_header.m_cluster_count_total =
		zircon_csg_bake_load_u32(p_manifest + 16);
	out_header.m_mesh_triangle_count =
		zircon_csg_bake_load_u32(p_manifest + 20);
	out_header.m_child_link_count_total =
		zircon_csg_bake_load_u32(p_manifest + 24);

	if (out_header.m_lod_level_count == 0u ||
		out_header.m_lod_level_count > ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS ||
		out_header.m_cluster_count_total == 0u ||
		out_header.m_cluster_count_total >
			ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE)
	{
		return false;
	}

	// the whole declared manifest must fit the bytes the caller read
	const kotek::uint64_t declared_size =
		static_cast<kotek::uint64_t>(zircon_meshlet_manifest_header_size) +
		static_cast<kotek::uint64_t>(out_header.m_lod_level_count) *
			zircon_meshlet_manifest_level_size +
		static_cast<kotek::uint64_t>(out_header.m_cluster_count_total) *
			zircon_meshlet_manifest_record_size +
		static_cast<kotek::uint64_t>(out_header.m_child_link_count_total) *
			4u;

	return declared_size <=
		static_cast<kotek::uint64_t>(manifest_size);
}

/// \~english the level table entry (level-ASCENDING): the level-major
/// [first_cluster, cluster_count) range of the given level; false on a
/// bad level index or a truncated table
inline bool zircon_meshlet_read_level_range(const kotek::uint8_t* p_manifest,
	kotek::size_t manifest_size, kotek::uint32_t level,
	kotek::uint32_t& out_first_cluster,
	kotek::uint32_t& out_cluster_count) noexcept
{
	if (p_manifest == nullptr || level >= ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS)
		return false;

	// the header must at least declare the level we read (the full header
	// validation is the caller's job through
	// zircon_meshlet_read_manifest_header)
	const kotek::uint32_t declared_levels =
		zircon_csg_bake_load_u32(p_manifest + 12);

	if (level >= declared_levels)
		return false;

	const kotek::uint64_t level_offset =
		static_cast<kotek::uint64_t>(zircon_meshlet_manifest_header_size) +
		static_cast<kotek::uint64_t>(level) *
			zircon_meshlet_manifest_level_size;

	if (level_offset + zircon_meshlet_manifest_level_size >
		static_cast<kotek::uint64_t>(manifest_size))
	{
		return false;
	}

	const kotek::uint8_t* p_entry = p_manifest + level_offset;

	out_first_cluster = zircon_csg_bake_load_u32(p_entry);
	out_cluster_count = zircon_csg_bake_load_u32(p_entry + 4);

	// the range must stay inside the declared cluster table
	const kotek::uint32_t cluster_total =
		zircon_csg_bake_load_u32(p_manifest + 16);

	return out_cluster_count > 0u &&
		out_first_cluster <= cluster_total &&
		out_cluster_count <= cluster_total - out_first_cluster;
}

/// \~english one cluster record (the 88-byte manifest record at the
/// level-major index); the cone + the AABB ride along — the draw's debug
/// camera frames the cluster through the AABB. False on a bad index or a
/// truncated table
inline bool zircon_meshlet_read_cluster_record(
	const kotek::uint8_t* p_manifest, kotek::size_t manifest_size,
	kotek::uint32_t cluster_index,
	zircon_meshlet_cluster_t& out_cluster) noexcept
{
	if (p_manifest == nullptr)
		return false;

	const kotek::uint32_t level_count =
		zircon_csg_bake_load_u32(p_manifest + 12);
	const kotek::uint32_t cluster_total =
		zircon_csg_bake_load_u32(p_manifest + 16);

	if (cluster_index >= cluster_total ||
		level_count > ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS ||
		cluster_total > ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE)
	{
		return false;
	}

	const kotek::uint64_t record_offset =
		static_cast<kotek::uint64_t>(zircon_meshlet_manifest_header_size) +
		static_cast<kotek::uint64_t>(level_count) *
			zircon_meshlet_manifest_level_size +
		static_cast<kotek::uint64_t>(cluster_index) *
			zircon_meshlet_manifest_record_size;

	if (record_offset + zircon_meshlet_manifest_record_size >
		static_cast<kotek::uint64_t>(manifest_size))
	{
		return false;
	}

	const kotek::uint8_t* p_record = p_manifest + record_offset;

	out_cluster.m_triangle_count = zircon_csg_bake_load_u32(p_record + 0);
	out_cluster.m_vertex_count = zircon_csg_bake_load_u32(p_record + 4);
	out_cluster.m_first_triangle = 0; // the runtime does not walk the
									  // mesh-local triangle table
	out_cluster.m_first_child_link =
		zircon_csg_bake_load_u32(p_record + 8);
	out_cluster.m_child_count = zircon_csg_bake_load_u32(p_record + 12);

	kotek::uint32_t error_bits = zircon_csg_bake_load_u32(p_record + 16);
	kotek::uint32_t cutoff_bits = zircon_csg_bake_load_u32(p_record + 20);

	std::memcpy(&out_cluster.m_error_metric, &error_bits, sizeof(float));
	std::memcpy(&out_cluster.m_cone_cutoff, &cutoff_bits, sizeof(float));

	for (int axis = 0; axis < 3; ++axis)
	{
		const kotek::uint32_t axis_bits =
			zircon_csg_bake_load_u32(p_record + 24 + axis * 4);
		std::memcpy(&out_cluster.m_cone_axis[axis], &axis_bits,
			sizeof(float));

		const kotek::uint64_t min_part =
			static_cast<kotek::uint64_t>(
				zircon_csg_bake_load_u32(p_record + 36 + axis * 8)) |
			(static_cast<kotek::uint64_t>(
				zircon_csg_bake_load_u32(p_record + 40 + axis * 8))
				<< 32u);
		const kotek::uint64_t max_part =
			static_cast<kotek::uint64_t>(
				zircon_csg_bake_load_u32(p_record + 60 + axis * 8)) |
			(static_cast<kotek::uint64_t>(
				zircon_csg_bake_load_u32(p_record + 64 + axis * 8))
				<< 32u);

		std::memcpy(&out_cluster.m_aabb_min[axis], &min_part,
			sizeof(double));
		std::memcpy(&out_cluster.m_aabb_max[axis], &max_part,
			sizeof(double));
	}

	out_cluster.m_material_min = zircon_csg_bake_load_u16(p_record + 84);
	out_cluster.m_material_max = zircon_csg_bake_load_u16(p_record + 86);

	// the record's own level: found through the level table (the level of
	// the range holding this level-major index); a level-major index no
	// range claims is corrupt content
	kotek::uint32_t level = 0;
	bool is_level_found = false;

	for (kotek::uint32_t candidate = 0; candidate < level_count;
		 ++candidate)
	{
		kotek::uint32_t first = 0;
		kotek::uint32_t count = 0;

		if (zircon_meshlet_read_level_range(
				p_manifest, manifest_size, candidate, first, count) &&
			cluster_index >= first && cluster_index < first + count)
		{
			level = candidate;
			is_level_found = true;
			break;
		}
	}

	if (is_level_found == false)
		return false;

	out_cluster.m_level = static_cast<kotek::uint8_t>(level);

	// the per-level budgets bound the stored counts (the writer's
	// contract — a skew here means corrupt content)
	if (out_cluster.m_triangle_count == 0u ||
		out_cluster.m_triangle_count >
			zircon_meshlet_max_tris_for_level(level) ||
		out_cluster.m_vertex_count == 0u ||
		out_cluster.m_vertex_count >
			zircon_meshlet_max_verts_for_level(level))
	{
		return false;
	}

	for (int axis = 0; axis < 3; ++axis)
	{
		if (!(out_cluster.m_aabb_min[axis] <= out_cluster.m_aabb_max[axis]))
			return false;
	}

	return true;
}

/// \~english the cluster entry name of the pack layout
/// ("meshlets/<scene>/lod_<level>/cluster_<index>.bin") — the
/// ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH capacity is the format's own
inline bool zircon_meshlet_read_entry_name(char* p_out_name,
	kotek::size_t capacity, const char* p_scene_name,
	kotek::uint32_t level, kotek::uint32_t cluster_index) noexcept
{
	if (p_out_name == nullptr || p_scene_name == nullptr ||
		capacity < ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH)
	{
		return false;
	}

	// the scene name rule is the writer's (a FILE name); revalidate here
	// so a runtime-provided scene name cannot walk the path either
	const kotek::size_t scene_length = std::strlen(p_scene_name);

	if (scene_length == 0 ||
		scene_length > ZIRCON_DEF_MESHLET_MAX_SCENE_NAME_LENGTH)
	{
		return false;
	}

	for (kotek::size_t index = 0; index < scene_length; ++index)
	{
		const char c = p_scene_name[index];

		const bool is_valid = (c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
			c == '-';

		if (!is_valid)
			return false;
	}

	kotek::static_cstring_t<ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH> name;

	// the u32 -> decimal append (hand-rolled: deterministic, locale-free
	// — the writer's discipline; its helper is file-local there)
	auto append_u32 = [&name](kotek::uint32_t value) noexcept {
		char digits[10];
		kotek::uint32_t count = 0;

		do
		{
			digits[count++] = static_cast<char>('0' + (value % 10u));
			value /= 10u;
		} while (value != 0u);

		while (count > 0)
		{
			const char symbol[2] = {digits[--count], '\0'};
			name += symbol;
		}
	};

	name = "meshlets/";
	name += p_scene_name;
	name += "/lod_";
	append_u32(level);
	name += "/cluster_";
	append_u32(cluster_index);
	name += ".bin";

	if (name.size() + 1 > capacity)
		return false;

	std::memcpy(p_out_name, name.c_str(), name.size() + 1);

	return true;
}

/// \~english parses + dequantizes + expands one cluster bin into the
/// interleaved soup VB + the identity u32 IB. p_aabb_min/max are the
/// CLUSTER's own AABB from the manifest record (the position quantum is
/// normalized against it — the B3a contract). Caller-owned outputs with
/// the exact capacities from zircon_meshlet_read_*_capacity; false on any
/// skew (bad magic, truncated tables, an index outside the welded vertex
/// count) — the caller logs, never crashes on content.
inline bool zircon_meshlet_read_cluster(const kotek::uint8_t* p_bin,
	kotek::size_t bin_size, const double* p_aabb_min,
	const double* p_aabb_max, float* p_out_vb_floats,
	kotek::uint32_t vb_capacity_floats, kotek::uint32_t* p_out_indices,
	kotek::uint32_t ib_capacity,
	zircon_meshlet_cluster_content_t& out_content) noexcept
{
	out_content = zircon_meshlet_cluster_content_t{};

	if (p_bin == nullptr || p_aabb_min == nullptr || p_aabb_max == nullptr ||
		p_out_vb_floats == nullptr || p_out_indices == nullptr)
	{
		return false;
	}

	if (bin_size < zircon_meshlet_cluster_header_size ||
		std::memcmp(p_bin, zircon_meshlet_cluster_magic, 4) != 0)
	{
		return false;
	}

	const kotek::uint32_t vertex_count = zircon_csg_bake_load_u32(p_bin + 4);
	const kotek::uint32_t triangle_count =
		zircon_csg_bake_load_u32(p_bin + 8);
	const kotek::uint32_t index_width = zircon_csg_bake_load_u32(p_bin + 12);

	if (zircon_csg_bake_load_u32(p_bin + 16) != 0u ||
		vertex_count == 0u || triangle_count == 0u ||
		vertex_count > ZIRCON_DEF_MESHLET_MAX_VERTICES *
				(1u << (ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS - 1u)) ||
		(index_width != 1u && index_width != 2u))
	{
		return false;
	}

	// the counts against the per-level budgets: the width implies the level
	// exactly at LOD0 (u8); above it the exact level is not decodable from
	// the bin alone, so the widest level's budget bounds the stored counts
	// (the manifest record's own budget check is the tight one — the
	// reader reads ONE bin by its record's AABB anyway)
	const kotek::uint32_t budget_level = (index_width == 1u)
		? 0u
		: (ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS - 1u);

	if (triangle_count > zircon_meshlet_max_tris_for_level(budget_level) ||
		vertex_count > zircon_meshlet_max_verts_for_level(budget_level))
	{
		return false;
	}

	const kotek::uint32_t quant_stride =
		(sizeof(zircon_meshlet_position_quant_t) == 1u) ? 1u : 2u;
	const kotek::uint64_t positions_bytes =
		static_cast<kotek::uint64_t>(vertex_count) * 3u * quant_stride;
	const kotek::uint64_t normals_bytes =
		static_cast<kotek::uint64_t>(triangle_count) * 2u;
	const kotek::uint64_t indices_bytes =
		static_cast<kotek::uint64_t>(triangle_count) * 3u * index_width;
	const kotek::uint64_t materials_bytes =
		static_cast<kotek::uint64_t>(triangle_count) * 2u;

	const kotek::uint64_t required_size =
		static_cast<kotek::uint64_t>(zircon_meshlet_cluster_header_size) +
		positions_bytes + normals_bytes + indices_bytes + materials_bytes;

	if (required_size > static_cast<kotek::uint64_t>(bin_size))
		return false;

	// the output capacities must fit the exact soup expansion
	if (vb_capacity_floats <
			zircon_meshlet_read_vb_capacity_floats(triangle_count) ||
		ib_capacity < zircon_meshlet_read_ib_capacity(triangle_count))
	{
		return false;
	}

	const kotek::uint8_t* p_positions = p_bin +
		zircon_meshlet_cluster_header_size;
	const kotek::uint8_t* p_normals = p_positions + positions_bytes;
	const kotek::uint8_t* p_indices = p_normals + normals_bytes;

	// dequantize the welded positions against the cluster AABB (the exact
	// codec + half-LSB bound the format documents)
	auto dequantize_axis = [p_aabb_min, p_aabb_max](
							   zircon_meshlet_position_quant_t value,
							   int axis) -> double {
		return zircon_meshlet_dequantize_position(value,
			p_aabb_min[axis],
			p_aabb_max[axis] - p_aabb_min[axis]);
	};

	// the soup expansion: triangle t's corner c lands at soup vertex
	// t*3+c, carrying the welded corner position + the triangle's flat
	// normal; the index buffer is the identity
	for (kotek::uint32_t triangle = 0; triangle < triangle_count;
		 ++triangle)
	{
		float normal[3]{};
		zircon_csg_bake_decode_normal_oct_u8(
			p_normals + triangle * 2u, normal);

		for (kotek::uint32_t corner = 0; corner < 3u; ++corner)
		{
			const kotek::uint8_t* p_index =
				p_indices + (triangle * 3u + corner) * index_width;

			kotek::uint32_t welded_index = 0;

			if (index_width == 1u)
			{
				welded_index = p_index[0];
			}
			else
			{
				welded_index = zircon_csg_bake_load_u16(p_index);
			}

			if (welded_index >= vertex_count)
				return false;

			float* p_soup_vertex =
				p_out_vb_floats +
				(triangle * 3u + corner) *
					ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_FLOATS;

			for (int axis = 0; axis < 3; ++axis)
			{
				zircon_meshlet_position_quant_t value{};

				const kotek::uint8_t* p_quant =
					p_positions +
					(static_cast<kotek::uint64_t>(welded_index) * 3u +
						axis) *
						quant_stride;

				if (quant_stride == 1u)
				{
					value =
						static_cast<zircon_meshlet_position_quant_t>(p_quant[0]);
				}
				else
				{
					value = static_cast<zircon_meshlet_position_quant_t>(
						zircon_csg_bake_load_u16(p_quant));
				}

				p_soup_vertex[axis] = static_cast<float>(
					dequantize_axis(value, axis));
			}

			p_soup_vertex[3] = normal[0];
			p_soup_vertex[4] = normal[1];
			p_soup_vertex[5] = normal[2];

			p_out_indices[triangle * 3u + corner] = triangle * 3u + corner;
		}
	}

	out_content.m_welded_vertex_count = vertex_count;
	out_content.m_triangle_count = triangle_count;
	out_content.m_soup_vertex_count = triangle_count * 3u;
	out_content.m_index_count = triangle_count * 3u;

	return true;
}
