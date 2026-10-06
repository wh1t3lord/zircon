// meshlet_cluster_cull.cs.slang — the GPU cluster cull + LOD cut of the
// nanite path (task Z24 B3c), through the kotek geometry seam's compute
// extension (RWByteAddressBuffer storage — the seam's byte-address
// binding form, no element-type coupling). Conventions: see
// zircon_core.slang (the compute section) and the layout's single source
// of truth, src/core/zircon_meshlet_cull.h — the C++ mirror of this
// classification is zircon_meshlet_cull_classify_cluster there; the unit
// tests pin both, keep them 1:1.
//
// ONE kernel, two modes (u_cullMeta.y), dispatched back-to-back per frame
// by the pass:
//   mode 0 clear: thread 0 zeroes the visible counter (the compacted tail
//     of a previous frame is never read — the draw's count buffer governs
//     how many commands execute)
//   mode 1 cull:  one thread per cluster — the LOD cut (own projected
//     error vs ZIRCON_DEF_NRI_MESHLET_LOD_ERROR_THRESHOLD + the parent
//     comparison, the FLT_MAX sentinel making roots parent-less), then
//     the frustum p-vertex test, then the cone backface test; a visible
//     cluster claims a slot with one atomic and writes its 20-byte
//     indirect command (the NRI DrawIndexedDesc layout)
//
// Ordering: the two dispatches bind the same counter — the UAV contract
// sequences their accesses with no explicit barrier. The indirect buffer
// + the counter change ACCESS KIND after the cull (indirect-argument
// reads by the draw) — the pass inserts the seam's Barrier_Buffer there
// (D3D12 tracks buffer states; the validation layer is on).
//
// THE CULL-PARAMS CBUFFER (offsets mirrored in
// cmake/zircon_slang_shaders.cmake):
//   u_cullPlanes   0..95   six world-space planes, xyz normalized,
//                          dot(n,p)+d >= 0 inside (the [0,1]-depth
//                          Gribb/Hartmann combinations)
//   u_cullCamera   96..111 xyz = the camera position
//   u_cullMeta     112..127 x = cluster count, y = the kernel mode,
//                          z = the projection scale (0.5 * height /
//                          tan(fov/2)), w = the error threshold (px)

import zircon_core;

// the storage table (stage N = register tN/uN on dx12 — the READ-ONLY
// cluster table is the SRV form (t0, legal on host-uploaded memory),
// the compacted commands + the counter are UAVs (u0/u1, GPU-local only
// — the D3D12 upload-heap UAV rule); the vulkan container mirrors the
// bindings through the packer spec — the bgfx routes build the same
// container for pipeline uniformity, nothing bgfx binds this kernel:
// the NRI path is the only live consumer)
[[vk::binding(1, 0)]]
ByteAddressBuffer u_clusterTable : register(t0, space0);
[[vk::binding(2, 0)]]
RWByteAddressBuffer u_indirectCommands : register(u0, space0);
[[vk::binding(3, 0)]]
RWByteAddressBuffer u_visibleCount : register(u1, space0);

// the push-constant block — register(b0) on every target (the compute
// root-constant range, the NRI pipeline's COMPUTE-stage root constant)
#ifdef ZIRCON_SLANG_BGFX_FXC
cbuffer CullParams : register(b0, space0)
#else
[[vk::binding(0, 0)]]
cbuffer CullParams : register(b0, space0)
#endif
{
	float4 u_cullPlanes[6];
	float4 u_cullCamera;
	float4 u_cullMeta;
};

static const uint kCullMode_Clear = 0;
static const uint kCullMode_Cull = 1;

// the GPU cluster record's byte offsets (the 64-byte layout — the single
// source of truth is zircon_meshlet_cull.h)
static const uint kRecordStrideBytes = 64u;
static const uint kRecordConeCutoff = 12u;   // aabb_min.w
static const uint kRecordError = 28u;        // aabb_max.w
static const uint kRecordParentError = 44u;  // cone_axis.w
static const uint kRecordIndexCount = 48u;
static const uint kRecordBaseVertex = 52u;
static const uint kRecordStartIndex = 56u;

static const uint kCommandStrideBytes = 20u;

static const float kMinDistance = 0.01f; // the mirror's floor

// the frustum p-vertex test, the mirror's math verbatim
bool zircon_meshlet_is_aabb_visible(float3 aabb_min, float3 aabb_max)
{
	// no [unroll]: Slang lowers attributed loops to an infinite-for with a
	// break, which fxc's unroller rejects (X3511) on the d3d11 route —
	// the plain 6-iteration loop compiles on every target (the B1 lesson)
	for (int plane_index = 0; plane_index < 6; ++plane_index)
	{
		float4 plane = u_cullPlanes[plane_index];

		float3 positive_vertex = float3(
			plane.x >= 0.0 ? aabb_max.x : aabb_min.x,
			plane.y >= 0.0 ? aabb_max.y : aabb_min.y,
			plane.z >= 0.0 ? aabb_max.z : aabb_min.z);

		if (dot(plane.xyz, positive_vertex) + plane.w < 0.0)
			return false;
	}

	return true;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID)
{
	const uint cluster_count = asuint(u_cullMeta.x);
	const uint mode = asuint(u_cullMeta.y);
	const uint thread_index = dispatch_id.x;

	if (mode == kCullMode_Clear)
	{
		if (thread_index == 0)
			u_visibleCount.Store(0u, 0u);

		return;
	}

	if (thread_index >= cluster_count)
		return;

	const uint record_base = thread_index * kRecordStrideBytes;

	const float3 aabb_min =
		asfloat(u_clusterTable.Load3(record_base));
	const float cone_cutoff =
		asfloat(u_clusterTable.Load(record_base + kRecordConeCutoff));
	const float3 aabb_max =
		asfloat(u_clusterTable.Load3(record_base + 16u));
	const float error_metric =
		asfloat(u_clusterTable.Load(record_base + kRecordError));
	const float3 cone_axis =
		asfloat(u_clusterTable.Load3(record_base + 32u));
	const float parent_error =
		asfloat(u_clusterTable.Load(record_base + kRecordParentError));
	const uint index_count =
		u_clusterTable.Load(record_base + kRecordIndexCount);
	const uint base_vertex =
		u_clusterTable.Load(record_base + kRecordBaseVertex);
	const uint start_index =
		u_clusterTable.Load(record_base + kRecordStartIndex);

	// ---- 1. the LOD cut (zircon_meshlet_cull_is_on_cut) ----
	const float3 center = 0.5f * (aabb_min + aabb_max);
	const float3 to_center = center - u_cullCamera.xyz;
	const float distance = max(length(to_center), kMinDistance);

	const float proj_scale = u_cullMeta.z;
	const float error_threshold = u_cullMeta.w;

	// the own projected error must pass...
	if (error_metric * proj_scale / distance > error_threshold)
		return;

	// ...and the parent's must NOT (the FLT_MAX sentinel projects to
	// +inf — a root always passes this half)
	if (parent_error * proj_scale / distance <= error_threshold)
		return;

	// ---- 2. the frustum ----
	if (!zircon_meshlet_is_aabb_visible(aabb_min, aabb_max))
		return;

	// ---- 3. the cone backface (the B3a predicate, mirrored) ----
	if (cone_cutoff <= 1.0f)
	{
		const float3 dir = to_center / distance;

		if (dot(dir, cone_axis) > cone_cutoff)
			return;
	}

	// ---- visible: compact the indirect command ----
	uint slot;
	u_visibleCount.InterlockedAdd(0u, 1u, slot);

	const uint command_base = slot * kCommandStrideBytes;

	u_indirectCommands.Store(command_base + 0u, index_count);
	u_indirectCommands.Store(command_base + 4u, 1u);
	u_indirectCommands.Store(command_base + 8u, start_index);
	u_indirectCommands.Store(command_base + 12u, base_vertex);
	u_indirectCommands.Store(command_base + 16u, thread_index);
}
