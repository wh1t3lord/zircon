// model_static_gpu_driven_cull.cs.slang — the GPU frustum cull of the
// chunked GPU-driven path (task Z24 B1). Conventions: see
// zircon_core.slang (the compute section). ONE kernel, three modes
// (u_cullMeta.y), dispatched back-to-back per frame by the pass:
//   mode 0 clear: zero the visible counter + every command slot (the
//     compacted tail of a previous, fuller frame must never draw)
//   mode 1 cull:  one thread per chunk — AABB-vs-frustum; a visible
//     chunk claims a slot with one atomic on the counter and writes its
//     compacted draw command there
//   mode 2 stats: one thread copies the final counter into the 1x1 R32U
//     stats image (the CPU readback — bgfx has no buffer readback, only
//     readTexture, so the count hops through a texture)
// Ordering: the three dispatches hit the same UAVs in submission order;
// d3d11 sequences UAV accesses between Dispatch calls by the API
// contract. (bgfx's vulkan renderer inserts no barriers between
// dispatches — a known bgfx gap, not zircon's to fix here; the boot's
// active backend is d3d11.)
//
// The C++ mirror of the mode-1 classification lives in
// zircon_render_chunk_pool.h (test_aabb_against_frustum +
// cull_chunks_against_frustum + the plane extraction static) — the unit
// tests pin it; keep the two in sync.

import zircon_core;

// ---------------------------------------------------------------------------
// GPU data layouts (byte-exact with the C++ side — the static asserts in
// zircon_render_chunk_pool.h pin them). AoSoA per the house DOD rule: the
// hot cull stream is the bounds bundle (2 x float4 per chunk, one 32-byte
// AoSoA record — coalesced SIMD plane tests); the draw ranges are their
// own narrow uint4 records, only touched by the threads whose chunk is
// visible; material ids stay in a CPU-side u16 array (no material system
// in B1 — they join the GPU streams when materials land).
// ---------------------------------------------------------------------------
// bounds bundle (stage 0): 2 float4 elements per chunk
//   [2i]   = (aabb_min.x, aabb_min.y, aabb_min.z, live-flag bits) — the
//            flag rides the spare lane: 0 = dead (removed), else live;
//            the cull treats a dead chunk as culled
//   [2i+1] = (aabb_max.x, aabb_max.y, aabb_max.z, material-id bits)
static const uint kBoundsStrideF32x4 = 2;

// ranges record (stage 1): 1 uint4 element per chunk =
//   (vertex_offset, vertex_count, index_offset, index_count) — expanded
//   from the CPU's narrow 12-byte record at upload
// indirect draw command (stage 2): 32 B = 2 uint4 elements. The FIRST 20
// bytes are the D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS /
// VkDrawIndexedIndirectCommand layout (identical 5-word shapes — d3d11
// reads them via DrawIndexedInstancedIndirect, renderer_d3d11.cpp:3677;
// vulkan via vkCmdDrawIndexedIndirect, renderer_vk.cpp:9323):
//   +0  index_count      (IndexCountPerInstance / indexCount)
//   +4  instance_count   (InstanceCount / instanceCount) — 1 per chunk
//   +8  start_index      (StartIndexLocation / firstIndex)
//   +12 base_vertex      (BaseVertexLocation / vertexOffset, int32 — the
//                        uint bits are the same value, pools stay < 2^31)
//   +16 start_instance   (StartInstanceLocation / firstInstance) — the
//                        chunk index (reserved for per-chunk fetch; the
//                        B1 draw does not read it: d3d11 does not expose
//                        StartInstanceLocation to the shader, and baked
//                        transforms + shared material make it
//                        unnecessary — documented in the pass header)
//   +20..+31 padding     (bgfx strides commands at
//                        BGFX_CONFIG_DRAW_INDIRECT_STRIDE = 32 —
//                        bgfx/src/config.h:359, passed as the stride to
//                        both renderers' indirect calls)
static const uint kCommandStrideU32x4 = 2;

// ---------------------------------------------------------------------------
// resources (stage N = register tN/uN on d3d11/dx12, [[vk::binding(2+N,0)]]
// on vulkan — the .bin storage table mirrors these, see
// cmake/zircon_slang_shaders.cmake)
// ---------------------------------------------------------------------------
#ifdef ZIRCON_SLANG_BGFX_FXC
// the bgfx d3d11 route declares the TYPED forms: bgfx's d3d11 renderer
// creates typed UAV/SRV views, never structured
// (renderer_d3d11.cpp BufferD3D11::create — the tables are
// 16-byte-element vertex-buffer-backed (R32G32B32A32_FLOAT / _UINT), the
// counter is 32-bit index-buffer-backed (R32_UINT), the indirect buffer
// is R32G32B32A32_UINT)
Buffer<float4> u_chunkBounds : register(t0);
Buffer<uint4> u_chunkRanges : register(t1);
RWBuffer<uint4> u_indirectCommands : register(u2);
RWBuffer<uint> u_visibleCount : register(u3);
#else
[[vk::binding(2, 0)]]
StructuredBuffer<float4> u_chunkBounds : register(t0, space0);
[[vk::binding(3, 0)]]
StructuredBuffer<uint4> u_chunkRanges : register(t1, space0);
[[vk::binding(4, 0)]]
RWStructuredBuffer<uint4> u_indirectCommands : register(u2, space0);
[[vk::binding(5, 0)]]
RWStructuredBuffer<uint> u_visibleCount : register(u3, space0);
#endif

// the 1x1 R32U stats image (stage 4): mode 2 stores the count here
[[vk::binding(6, 0)]]
RWTexture2D<uint> u_statsImage : register(u4, space0);

// cbuffer — register(b0) on every target (the compute UBO slot; vulkan
// binding 0). Member offsets: u_cullPlanes = 0..95, u_cullMeta = 96 —
// mirrored in cmake/zircon_slang_shaders.cmake.
#ifdef ZIRCON_SLANG_BGFX_FXC
cbuffer CullParams : register(b0, space0)
#else
[[vk::binding(0, 0)]]
cbuffer CullParams : register(b0, space0)
#endif
{
	float4 u_cullPlanes[6]; // world-space, normalized, dot(n,p)+d >= 0 inside
	float4 u_cullMeta;      // x = chunk count (bits), y = kernel mode (bits)
};

static const uint kCullMode_Clear = 0;
static const uint kCullMode_Cull = 1;
static const uint kCullMode_Stats = 2;

// AABB vs the six-plane frustum, positive-vertex test (the p-vertex is
// the corner farthest along the plane normal; if IT is outside, the whole
// box is) — mirrored in C++ by
// zircon_render_chunk_pool::test_aabb_against_frustum
bool zircon_is_aabb_visible(float3 aabb_min, float3 aabb_max)
{
	// no [unroll]: Slang lowers attributed loops to an infinite-for with a
	// break, which fxc's unroller rejects (X3511) on the d3d11 route —
	// the plain 6-iteration loop compiles on every target
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
	const uint chunk_count = asuint(u_cullMeta.x);
	const uint mode = asuint(u_cullMeta.y);
	const uint thread_index = dispatch_id.x;

	if (mode == kCullMode_Clear)
	{
		if (thread_index == 0)
			u_visibleCount[0] = 0;

		if (thread_index < chunk_count)
		{
			// a zeroed command is a legal no-op draw
			// (IndexCountPerInstance = 0) — the submit always issues
			// chunkCount commands, so every slot must be written each
			// frame
			u_indirectCommands[thread_index * kCommandStrideU32x4] =
				uint4(0, 0, 0, 0);
			u_indirectCommands[thread_index * kCommandStrideU32x4 + 1] =
				uint4(0, 0, 0, 0);
		}

		return;
	}

	if (mode == kCullMode_Cull)
	{
		if (thread_index >= chunk_count)
			return;

		const float4 bounds_min = u_chunkBounds[
			thread_index * kBoundsStrideF32x4];

		// a removed chunk (the dead flag in the spare lane) is culled
		// without a plane test
		if (asuint(bounds_min.w) == 0u)
			return;

		const float4 bounds_max = u_chunkBounds[
			thread_index * kBoundsStrideF32x4 + 1];

		if (zircon_is_aabb_visible(bounds_min.xyz, bounds_max.xyz))
		{
			const uint4 ranges = u_chunkRanges[thread_index];

			uint slot;
			InterlockedAdd(u_visibleCount[0], 1, slot);

			// the compacted command (see the layout table above):
			// instance_count 1, start_instance = the chunk index
			u_indirectCommands[slot * kCommandStrideU32x4] = uint4(
				ranges.w,      // index_count
				1,             // instance_count
				ranges.z,      // start_index
				ranges.x       // base_vertex
				);
			u_indirectCommands[slot * kCommandStrideU32x4 + 1] =
				uint4(thread_index, 0, 0, 0);
		}

		return;
	}

	// kCullMode_Stats — a single thread (the pass dispatches 1x1x1)
	if (thread_index == 0)
		u_statsImage[uint2(0, 0)] = u_visibleCount[0];
}
