#pragma once

// zircon_meshlet_cull.h — the GPU-CLUSTER-CULL + LOD-CUT LOGIC of the
// nanite path (task Z24 B3c, the GPU-side finish — the runtime consumer of
// the B3a manifest through the kotek geometry seam's compute extension).
// Pure statics over caller-owned storage, byte-pinned layouts: the Slang
// kernel (meshlet_cluster_cull.cs.slang) mirrors the classification 1:1,
// this header is the C++ mirror the unit tests pin and the NRI pass runs
// as its CPU reference for the A/B readback evidence (the B1 discipline).
//
// ---------------------------------------------------------------------
// THE CULL, three predicates per cluster (the kernel and the mirror both,
// in this order):
//
//   1. THE LOD CUT (the nanite per-cluster formulation, parallel over ALL
//      clusters — no tree walk): a cluster C is ON THE CUT iff its own
//      projected screen-space error is acceptable AND its parent P's is
//      NOT (if the parent were acceptable, the parent would represent C).
//      The root's parent error is the FLT_MAX sentinel, whose projection
//      is +inf — never acceptable, so a root is on the cut iff its own
//      error passes. With the B3a placeholder hierarchy (geometry
//      unchanged, every error_metric 0.0f) the degenerate-but-correct cut
//      is "the coarsest level everywhere" — the machinery (threshold,
//      projection, parent comparison) is what this phase proves; a real
//      simplifier's nonzero errors exercise it (the tests drive both).
//      The parent error rides the GPU record — the B3a format's
//      parent->child links are inverted ONCE at load into the
//      child->parent error the predicate needs.
//   2. THE FRUSTUM TEST: the AABB-vs-six-planes positive-vertex test,
//      the exact chunk-pool math (zircon_render_chunk_pool.h's statics —
//      same Gribb/Hartmann combinations, same bx row-major column read),
//      re-homed here because zircon.core cannot include the bgfx passes
//      project. Planes: world space, xyz normalized, dot(n,p)+d >= 0
//      inside, the [0,1]-depth convention (near = c2, far = c3-c2).
//   3. THE CONE BACKFACE TEST: the B3a predicate
//      (zircon_meshlet_cone_culls — reused, never forked): culled iff
//      dot(d, axis) > cutoff with d = the unit view direction from the
//      camera into the scene (the cluster-center direction this phase,
//      the documented v1 approximation), the never-cull sentinel
//      cutoff > 1.0f checked first.
//
// A visible cluster compacts ONE indirect draw command into the argument
// buffer with a single atomic InterlockedAdd on the counter — the
// classic GPU compaction, the count consumed by ExecuteIndirect.
//
// ---------------------------------------------------------------------
// THE GPU CLUSTER RECORD (the storage-buffer table the kernel reads,
// 64 bytes, 4 x uint4 loads — byte-exact with the upload and the kernel):
//
//   +0  16  float4 aabb_min   (w = cone_cutoff — the B3a never-cull
//                             sentinel 2.0f rides the spare lane)
//   +16 16  float4 aabb_max   (w = error_metric — the simplifier's slot)
//   +32 16  float4 cone_axis  (w = parent_error_metric, FLT_MAX = no
//                             parent — the LOD-cut comparison operand)
//   +48 4   u32 index_count   (triangle_count * 3 — the soup expansion)
//   +52 4   u32 base_vertex   (the cluster's soup-vertex offset in the
//                             pooled dynamic VB)
//   +56 4   u32 start_index   (the cluster's element offset in the
//                             pooled identity IB)
//   +60 4   u32 reserved      (0 — the 64-byte alignment pad)
//
// ---------------------------------------------------------------------
// THE INDIRECT COMMAND (the compacted record, 20 bytes — field-for-field
// NRI's nri::DrawIndexedDesc, external/nri/Include/NRIDescs.h:1659, which
// is layout-identical to D3D12's D3D12_DRAW_INDEXED_ARGUMENTS
// (IndexCountPerInstance, InstanceCount, StartIndexLocation,
// BaseVertexLocation, StartInstanceLocation) — the D3D12 backend builds
// the ExecuteIndirect command signature from this layout with the
// caller's stride (Source/D3D12/DeviceD3D12.hpp:1466
// GetDrawIndexedCommandSignature -> CreateCommandSignature(
// D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED, stride, ...)), so the
// kernel's 20-byte commands are consumed with stride 20, NOT bgfx's
// 32-byte stride (the B1 layout proof lives with the bgfx container;
// this is the NRI layout proof the test pins byte-exact):
//
//   +0  4  index_count    u32
//   +4  4  instance_count u32 (1)
//   +8  4  start_index    u32
//   +12 4  base_vertex    i32 (uint bits — the pools stay < 2^31)
//   +16 4  start_instance u32 (the cluster index — diagnostic only; like
//                             B1, d3d12 does not expose it to the shader)

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>

// std::sqrt in the pure-POD kernels (the chunk-pool / clusterizer
// precedent: plain C math inside the pure-POD kernels)
#include <cmath>
#include <cstring>

#include "zircon_meshlet_clusterize.h" // the cone predicate + the manifest types

/// the LOD cut's projected-error threshold in PIXELS (task Z24 B3c) —
/// the nanite-style "up to this much screen-space error" target; with
/// the B3a placeholder hierarchy every error is 0.0f so the cut is always
/// satisfied from the cluster side (the parent comparison decides); a
/// real simplifier's errors compete against this
#define ZIRCON_DEF_NRI_MESHLET_LOD_ERROR_THRESHOLD 1.0f

/// the distance denominator's floor — a degenerate (on-camera) cluster
/// must not divide by zero; at 1 cm the projected error saturates
#define ZIRCON_DEF_MESHLET_CULL_MIN_DISTANCE 0.01f

/// the root's parent-error sentinel: no parent exists, the FLT_MAX
/// projection is +inf, never acceptable — the root is on the cut iff its
/// own error passes (the natural arithmetic, no special case)
#define ZIRCON_DEF_MESHLET_CULL_NO_PARENT_ERROR 3.402823466e+38f // FLT_MAX

namespace zircon_meshlet_cull_layout
{
	/// the GPU cluster record, 64 bytes (the banner's layout — the pass
	/// uploads an array of these, the kernel loads 4 uint4s per cluster)
	struct gpu_record_t
	{
		float m_aabb_min[4];  // w = cone_cutoff
		float m_aabb_max[4];  // w = error_metric
		float m_cone_axis[4]; // w = parent_error_metric
		kotek::uint32_t m_index_count;
		kotek::uint32_t m_base_vertex;
		kotek::uint32_t m_start_index;
		kotek::uint32_t m_reserved;
	};

	/// the compacted indirect draw command, 20 bytes (the banner's
	/// layout — the NRI DrawIndexedDesc / D3D12_DRAW_INDEXED_ARGUMENTS
	/// field order)
	struct indirect_command_t
	{
		kotek::uint32_t m_index_count;
		kotek::uint32_t m_instance_count;
		kotek::uint32_t m_start_index;
		kotek::int32_t m_base_vertex;
		kotek::uint32_t m_start_instance;
	};
} // namespace zircon_meshlet_cull_layout

// the layout pins (a format change updates the banner, the kernel and
// these asserts in one commit)
static_assert(sizeof(zircon_meshlet_cull_layout::gpu_record_t) == 64u,
	"the meshlet GPU cluster record must stay 64 bytes (4 x uint4)");
static_assert(sizeof(zircon_meshlet_cull_layout::indirect_command_t) ==
		20u,
	"the meshlet indirect command must stay 20 bytes (the NRI "
	"DrawIndexedDesc / D3D12_DRAW_INDEXED_ARGUMENTS layout)");

/// \~english the cull pass's view-side inputs (the push-constant block
/// the kernel mirrors + the CPU mirror consumes): the six world-space
/// frustum planes, the camera position, the projection scale and the LOD
/// threshold
struct zircon_meshlet_cull_view_t
{
	/// 6 x float4, xyz normalized, dot(n,p)+d >= 0 inside (the
	/// [0,1]-depth Gribb/Hartmann combinations)
	float m_planes[24];
	float m_camera_position[3];
	/// 0.5 * viewport_height / tan(fov_y * 0.5) — the world->pixel error
	/// projection scale (the perspective divide at unit distance)
	float m_proj_scale;
	float m_error_threshold;
};

/// \~english extracts the six world-space frustum planes from a
/// view-projection matrix — the chunk-pool math verbatim (bx stores
/// row-major with row-vector math: component i of column j is
/// m[i*4+j]; the Gribb/Hartmann combinations for -w<=x<=w, -w<=y<=w,
/// 0<=z<=w), each plane xyz-normalized. Output plane k = out[k*4..k*4+3]
/// in the left/right/bottom/top/near/far order
inline void zircon_meshlet_cull_extract_frustum_planes(
	const float* p_view_projection, float* p_out_planes_6x4) noexcept
{
	if (p_view_projection == nullptr || p_out_planes_6x4 == nullptr)
		return;

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

	for (int plane_index = 0; plane_index < 6; ++plane_index)
	{
		float* p_plane = p_out_planes_6x4 + plane_index * 4;

		const float length_squared = p_plane[0] * p_plane[0] +
			p_plane[1] * p_plane[1] + p_plane[2] * p_plane[2];

		if (length_squared <= 0.0f)
			continue;

		const float length = std::sqrt(length_squared);

		p_plane[0] /= length;
		p_plane[1] /= length;
		p_plane[2] /= length;
		p_plane[3] /= length;
	}
}

/// \~english the AABB-vs-six-planes positive-vertex test (the p-vertex is
/// the corner farthest along the plane normal; if IT is outside, the
/// whole box is) — the chunk-pool / B1-shader math verbatim
inline bool zircon_meshlet_cull_test_aabb(const float* p_planes_6x4,
	const float* p_aabb_min, const float* p_aabb_max) noexcept
{
	if (p_planes_6x4 == nullptr || p_aabb_min == nullptr ||
		p_aabb_max == nullptr)
	{
		return false;
	}

	for (int plane_index = 0; plane_index < 6; ++plane_index)
	{
		const float* p_plane = p_planes_6x4 + plane_index * 4;

		const float positive_vertex_x =
			p_plane[0] >= 0.0f ? p_aabb_max[0] : p_aabb_min[0];
		const float positive_vertex_y =
			p_plane[1] >= 0.0f ? p_aabb_max[1] : p_aabb_min[1];
		const float positive_vertex_z =
			p_plane[2] >= 0.0f ? p_aabb_max[2] : p_aabb_min[2];

		if (p_plane[0] * positive_vertex_x +
				p_plane[1] * positive_vertex_y +
				p_plane[2] * positive_vertex_z + p_plane[3] <
			0.0f)
		{
			return false;
		}
	}

	return true;
}

/// \~english the world-space error's projection to PIXELS at the given
/// camera distance (the nanite formulation: err * proj_scale / distance,
/// proj_scale = 0.5 * viewport_height / tan(fov/2) — the perspective
/// divide). The distance floors at ZIRCON_DEF_MESHLET_CULL_MIN_DISTANCE
inline float zircon_meshlet_cull_projected_error(float error_metric,
	float distance, float proj_scale) noexcept
{
	const float safe_distance =
		distance > ZIRCON_DEF_MESHLET_CULL_MIN_DISTANCE
		? distance
		: ZIRCON_DEF_MESHLET_CULL_MIN_DISTANCE;

	return error_metric * proj_scale / safe_distance;
}

/// \~english the LOD-cut predicate's parent half: true when the PARENT's
/// projected error is acceptable (so the parent represents this cluster
/// and the cluster itself is NOT on the cut). The no-parent sentinel
/// (FLT_MAX) projects to +inf — never acceptable
inline bool zircon_meshlet_cull_parent_acceptable(
	float parent_error_metric, float distance, float proj_scale,
	float error_threshold) noexcept
{
	return zircon_meshlet_cull_projected_error(parent_error_metric,
			   distance, proj_scale) <= error_threshold;
}

/// \~english the LOD-cut predicate: the cluster is ON THE CUT iff its own
/// projected error passes the threshold AND its parent's does not
inline bool zircon_meshlet_cull_is_on_cut(float error_metric,
	float parent_error_metric, float distance, float proj_scale,
	float error_threshold) noexcept
{
	if (zircon_meshlet_cull_projected_error(error_metric, distance,
			proj_scale) > error_threshold)
	{
		return false;
	}

	return zircon_meshlet_cull_parent_acceptable(parent_error_metric,
			   distance, proj_scale, error_threshold) == false;
}

/// \~english the full per-cluster classification (the kernel mirrors this
/// 1:1 — keep the order): the LOD cut, then the frustum, then the cone.
/// The cluster-center direction is the cone test's view direction and the
/// center distance is the LOD projection's distance (both the documented
/// v1 approximations). Returns the visibility; the distance the decision
/// used rides out for the caller's diagnostics
inline bool zircon_meshlet_cull_classify_cluster(
	const zircon_meshlet_cull_layout::gpu_record_t& record,
	const zircon_meshlet_cull_view_t& view, float& out_distance) noexcept
{
	const float center_x =
		0.5f * (record.m_aabb_min[0] + record.m_aabb_max[0]);
	const float center_y =
		0.5f * (record.m_aabb_min[1] + record.m_aabb_max[1]);
	const float center_z =
		0.5f * (record.m_aabb_min[2] + record.m_aabb_max[2]);

	const float to_center_x = center_x - view.m_camera_position[0];
	const float to_center_y = center_y - view.m_camera_position[1];
	const float to_center_z = center_z - view.m_camera_position[2];

	out_distance = std::sqrt(to_center_x * to_center_x +
		to_center_y * to_center_y + to_center_z * to_center_z);

	// 1. the LOD cut (the w lanes carry cone_cutoff / error /
	// parent_error — the record layout)
	if (zircon_meshlet_cull_is_on_cut(record.m_aabb_max[3],
			record.m_cone_axis[3], out_distance, view.m_proj_scale,
			view.m_error_threshold) == false)
	{
		return false;
	}

	// 2. the frustum
	if (zircon_meshlet_cull_test_aabb(view.m_planes, record.m_aabb_min,
			record.m_aabb_max) == false)
	{
		return false;
	}

	// 3. the cone backface (the B3a predicate, reused)
	const float safe_distance =
		out_distance > ZIRCON_DEF_MESHLET_CULL_MIN_DISTANCE
		? out_distance
		: ZIRCON_DEF_MESHLET_CULL_MIN_DISTANCE;

	const float dir_x = to_center_x / safe_distance;
	const float dir_y = to_center_y / safe_distance;
	const float dir_z = to_center_z / safe_distance;

	if (zircon_meshlet_cone_culls(dir_x, dir_y, dir_z,
			record.m_cone_axis, record.m_aabb_min[3]))
	{
		return false;
	}

	return true;
}

/// \~english the CPU mirror of the whole cull+compact (the A/B proof's
/// cpu side — the GPU counter must equal this count and the readback
/// commands must match): walks every record in table order, classifies,
/// compacts one indirect command per visible cluster. The caller sizes
/// the command buffer to the record count (capacity >= count — the pass
/// contract), so a capacity guard is a programmer error, not truncation
inline kotek::uint32_t zircon_meshlet_cull_compact(
	const zircon_meshlet_cull_layout::gpu_record_t* p_records,
	kotek::uint32_t record_count, const zircon_meshlet_cull_view_t& view,
	zircon_meshlet_cull_layout::indirect_command_t* p_out_commands,
	kotek::uint32_t command_capacity) noexcept
{
	if (p_records == nullptr)
		return 0;

	kotek::uint32_t visible_count = 0;

	for (kotek::uint32_t index = 0; index < record_count; ++index)
	{
		float distance = 0.0f;

		if (zircon_meshlet_cull_classify_cluster(
				p_records[index], view, distance) == false)
		{
			continue;
		}

		if (visible_count >= command_capacity)
		{
			KOTEK_ASSERT(false,
				"the meshlet cull mirror ran out of command capacity "
				"{} — the caller must size the buffer to the cluster "
				"count", command_capacity);

			break;
		}

		p_out_commands[visible_count].m_index_count =
			p_records[index].m_index_count;
		p_out_commands[visible_count].m_instance_count = 1u;
		p_out_commands[visible_count].m_start_index =
			p_records[index].m_start_index;
		p_out_commands[visible_count].m_base_vertex = static_cast<
			kotek::int32_t>(p_records[index].m_base_vertex);
		p_out_commands[visible_count].m_start_instance = index;

		++visible_count;
	}

	return visible_count;
}
