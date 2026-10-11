// light_cluster_bin.cs.slang — the clustered forward+ light binning of
// task Z24 B5b (the plan's "bindless light lists per tile"). Conventions:
// see zircon_core.slang (the compute section). ONE kernel, three modes
// (u_binMeta.y), dispatched back-to-back per frame by the pass's light
// rig (zircon_render_light_cluster_rig):
//   mode 0 clear: zero the per-tile counts + the two bin counters (the
//     index lists are gated by the counts — they never need clearing)
//   mode 1 bin:   one thread per light — classify the light's volume
//     against the tile grid (below) and append the light index to every
//     covered tile's list (one atomic per tile on the tile's count, the
//     B1 counter+compaction discipline; a full tile's count still
//     increments so the FS's min(count, cap) clamp reads a consistent
//     list, and the overflow lands in the dropped counter)
//   mode 2 stats: one thread copies the counters into the 2x1 R32U stats
//     image (the CPU readback — the B1 hop: bgfx has no buffer readback)
// Ordering: the dispatches hit the same UAVs in submission order; d3d11
// sequences UAV accesses between Dispatch calls by the API contract
// (bgfx's vulkan renderer inserts no barriers between dispatches — the
// known bgfx gap, inherited unchanged; the boot's active backend is
// d3d11).
//
// THE CLASSIFICATION (mirrored one-to-one by the C++ statics in
// zircon_render_light_buffer.{h,cpp} — the unit tests pin them, keep the
// two in sync):
//   - directional: the whole grid (no volume);
//   - point: the sphere (center, range) — the six-plane frustum reject,
//     then the conservative screen-tile range of the sphere's world AABB
//     (8 corner projections; a corner with w <= 0 — the sphere crosses
//     the camera plane — widens to the FULL grid, never a miss);
//   - spot: the cone classifies through its conservative bounding sphere
//     (the tighter of the mid-cone sphere and the apex sphere — see
//     spot_bounding_sphere's comment).
// The projection formula clip[j] = dot((x,y,z,1), column j of the
// uploaded view-projection) is the bx-byte-layout transform the column
// -major mul() computes (translation at [12..14]) — the B1 plane-
// extraction convention; pixel rows run DOWN (ndc +y up -> py =
// (0.5-ndc.y*0.5)*height), matching the fragment stage's SV_Position.

import zircon_core;

// ---------------------------------------------------------------------------
// resources (stage N = register tN/uN on d3d11/dx12, [[vk::binding(2+N,0)]]
// on vulkan — the .bin storage table mirrors these, see
// cmake/zircon_slang_shaders.cmake)
// ---------------------------------------------------------------------------
#ifdef ZIRCON_SLANG_BGFX_FXC
// the bgfx d3d11 route declares the TYPED forms (the compute-section
// comment in zircon_core.slang — renderer_d3d11.cpp creates typed
// UAV/SRVs only): the light streams are 16-byte-element
// vertex-buffer-backed (R32G32B32A32_FLOAT), the counts/indices/counters
// are 32-bit index-buffer-backed (R32_UINT)
Buffer<float4> u_lightPositions : register(t0);
Buffer<float4> u_lightDirections : register(t1);
Buffer<float4> u_lightParams : register(t2);
RWBuffer<uint> u_lightTileCounts : register(u3);
RWBuffer<uint> u_lightTileIndices : register(u4);
RWBuffer<uint> u_binCounters : register(u5);
#else
[[vk::binding(2, 0)]]
StructuredBuffer<float4> u_lightPositions : register(t0, space0);
[[vk::binding(3, 0)]]
StructuredBuffer<float4> u_lightDirections : register(t1, space0);
[[vk::binding(4, 0)]]
StructuredBuffer<float4> u_lightParams : register(t2, space0);
[[vk::binding(5, 0)]]
RWStructuredBuffer<uint> u_lightTileCounts : register(u3, space0);
[[vk::binding(6, 0)]]
RWStructuredBuffer<uint> u_lightTileIndices : register(u4, space0);
[[vk::binding(7, 0)]]
RWStructuredBuffer<uint> u_binCounters : register(u5, space0);
#endif

// the 2x1 R32U stats image (stage 6): mode 2 stores the counters here
// (texel (0,0) = assignments, texel (1,0) = per-tile-cap drops)
[[vk::binding(8, 0)]]
RWTexture2D<uint> u_statsImage : register(u6, space0);

// cbuffer — register(b0) on every target (the compute UBO slot; vulkan
// binding 0). Member offsets: u_cullPlanes = 0..95, u_viewProj = 96..159,
// u_screenMeta = 160, u_binMeta = 176 — mirrored in
// cmake/zircon_slang_shaders.cmake.
#ifdef ZIRCON_SLANG_BGFX_FXC
cbuffer LightBinParams : register(b0, space0)
#else
[[vk::binding(0, 0)]]
cbuffer LightBinParams : register(b0, space0)
#endif
{
	float4 u_cullPlanes[6]; // world-space, normalized, dot(n,p)+d >= 0 inside
	float4 u_lightViewProj[4]; // the bx byte layout's 4 float4 groups (see the
	                        // projection formula above). NOT named u_viewProj:
	                        // that name is a bgfx PREDEFINED uniform
	                        // (bgfx.cpp s_predefinedName) — the renderer would
	                        // fill it from the view state instead of this
	                        // cbuffer, and createUniform-ing predefined names
	                        // is the documented trap
	float4 u_screenMeta;    // x = screen width px, y = screen height px
	                        // (floats), z = tiles_x bits, w = tiles_y bits
	float4 u_binMeta;       // x = light count bits, y = kernel mode bits,
	                        // z = tile pixels bits, w = per-tile cap bits
};

static const uint kBinMode_Clear = 0;
static const uint kBinMode_Bin = 1;
static const uint kBinMode_Stats = 2;

// sphere-vs-six-planes — mirrored in C++ by
// zircon_render_light_buffer::sphere_vs_frustum
bool zircon_light_sphere_vs_frustum(float3 center, float radius)
{
	for (int plane_index = 0; plane_index < 6; ++plane_index)
	{
		float4 plane = u_cullPlanes[plane_index];

		if (dot(plane.xyz, center) + plane.w < -radius)
			return false;
	}

	return true;
}

// the spot cone's bounding sphere — mirrored in C++ by
// zircon_render_light_buffer::spot_bounding_sphere (the tighter of the
// mid-cone sphere and the apex sphere; the mid-cone formula is used only
// below a 90-degree outer angle)
void zircon_light_spot_bounding_sphere(float3 apex, float3 dir_from,
	float range, float cos_outer, out float3 out_center,
	out float out_radius)
{
	float cos_sq = cos_outer * cos_outer;
	float sin_outer = sqrt(max(1.0 - cos_sq, 0.0));
	float tan_outer = sin_outer / cos_outer;

	float half_range = range * 0.5;
	float rim = range * tan_outer;
	float mid_radius = sqrt(half_range * half_range + rim * rim);

	if (cos_outer > 0.0 && mid_radius < range)
	{
		out_center = apex + dir_from * half_range;
		out_radius = mid_radius;
	}
	else
	{
		out_center = apex;
		out_radius = range;
	}
}

// the sphere's conservative screen-tile range — mirrored in C++ by
// zircon_render_light_buffer::project_sphere_to_tile_range
void zircon_light_project_sphere_tile_range(float3 center, float radius,
	uint tiles_x, uint tiles_y, uint tile_pixels, out uint out_min_x,
	out uint out_min_y, out uint out_max_x, out uint out_max_y)
{
	float min_px = 0.0;
	float max_px = 0.0;
	float min_py = 0.0;
	float max_py = 0.0;

	for (uint corner = 0; corner < 8; ++corner)
	{
		float3 corner_xyz = center + float3((corner & 1) ? radius : -radius,
			(corner & 2) ? radius : -radius,
			(corner & 4) ? radius : -radius);

		// clip[j] = dot((x,y,z,1), (vp[j], vp[4+j], vp[8+j], vp[12+j]))
		float4 v = float4(corner_xyz, 1.0);
		float4 clip = float4(
			dot(v, float4(u_lightViewProj[0].x, u_lightViewProj[1].x,
				u_lightViewProj[2].x, u_lightViewProj[3].x)),
			dot(v, float4(u_lightViewProj[0].y, u_lightViewProj[1].y,
				u_lightViewProj[2].y, u_lightViewProj[3].y)),
			dot(v, float4(u_lightViewProj[0].z, u_lightViewProj[1].z,
				u_lightViewProj[2].z, u_lightViewProj[3].z)),
			dot(v, float4(u_lightViewProj[0].w, u_lightViewProj[1].w,
				u_lightViewProj[2].w, u_lightViewProj[3].w)));

		// the sphere crosses the camera plane: the whole grid
		if (clip.w <= 0.0)
		{
			out_min_x = 0;
			out_min_y = 0;
			out_max_x = tiles_x - 1;
			out_max_y = tiles_y - 1;
			return;
		}

		float ndc_x = clip.x / clip.w;
		float ndc_y = clip.y / clip.w;

		float pixel_x = (ndc_x * 0.5 + 0.5) * u_screenMeta.x;
		// ndc +y is up, pixel rows run down — the flip
		float pixel_y = (0.5 - ndc_y * 0.5) * u_screenMeta.y;

		if (corner == 0)
		{
			min_px = max_px = pixel_x;
			min_py = max_py = pixel_y;
		}
		else
		{
			min_px = min(min_px, pixel_x);
			max_px = max(max_px, pixel_x);
			min_py = min(min_py, pixel_y);
			max_py = max(max_py, pixel_y);
		}
	}

	float inv_tile = 1.0 / float(tile_pixels);

	int raw_min_x = int(floor(min_px * inv_tile));
	int raw_max_x = int(floor(max_px * inv_tile));
	int raw_min_y = int(floor(min_py * inv_tile));
	int raw_max_y = int(floor(max_py * inv_tile));

	out_min_x = uint(clamp(raw_min_x, 0, int(tiles_x) - 1));
	out_max_x = uint(clamp(raw_max_x, 0, int(tiles_x) - 1));
	out_min_y = uint(clamp(raw_min_y, 0, int(tiles_y) - 1));
	out_max_y = uint(clamp(raw_max_y, 0, int(tiles_y) - 1));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID)
{
	const uint tiles_x = asuint(u_screenMeta.z);
	const uint tiles_y = asuint(u_screenMeta.w);
	const uint light_count = asuint(u_binMeta.x);
	const uint mode = asuint(u_binMeta.y);
	const uint tile_pixels = asuint(u_binMeta.z);
	const uint max_per_tile = asuint(u_binMeta.w);
	const uint thread_index = dispatch_id.x;

	if (mode == kBinMode_Clear)
	{
		if (thread_index < tiles_x * tiles_y)
			u_lightTileCounts[thread_index] = 0;

		if (thread_index == 0)
		{
			u_binCounters[0] = 0;
			u_binCounters[1] = 0;
		}

		return;
	}

	if (mode == kBinMode_Bin)
	{
		if (thread_index >= light_count)
			return;

		const uint light_index = thread_index;
		const uint light_type =
			asuint(u_lightParams[light_index].z);

		uint tile_min_x = 0;
		uint tile_min_y = 0;
		uint tile_max_x = 0;
		uint tile_max_y = 0;

		if (light_type == kZirconLightType_Directional)
		{
			// the directional touches every tile
			tile_max_x = tiles_x - 1;
			tile_max_y = tiles_y - 1;
		}
		else
		{
			float3 center = u_lightPositions[light_index].xyz;
			float radius = u_lightPositions[light_index].w;

			if (light_type == kZirconLightType_Spot)
			{
				// the cone classifies through its bounding sphere
				float3 sphere_center;
				float sphere_radius;

				zircon_light_spot_bounding_sphere(center,
					u_lightDirections[light_index].xyz, radius,
					u_lightParams[light_index].y, sphere_center,
					sphere_radius);

				center = sphere_center;
				radius = sphere_radius;
			}

			// a light fully outside the frustum touches nothing
			if (zircon_light_sphere_vs_frustum(center, radius) == false)
				return;

			zircon_light_project_sphere_tile_range(center, radius, tiles_x,
				tiles_y, tile_pixels, tile_min_x, tile_min_y, tile_max_x,
				tile_max_y);
		}

		for (uint tile_y = tile_min_y; tile_y <= tile_max_y; ++tile_y)
		{
			for (uint tile_x = tile_min_x; tile_x <= tile_max_x; ++tile_x)
			{
				const uint tile = tile_y * tiles_x + tile_x;

				uint slot;
				InterlockedAdd(u_lightTileCounts[tile], 1, slot);

				if (slot < max_per_tile)
				{
					u_lightTileIndices[tile * max_per_tile + slot] =
						light_index;
					InterlockedAdd(u_binCounters[0], 1);
				}
				else
				{
					InterlockedAdd(u_binCounters[1], 1);
				}
			}
		}

		return;
	}

	// kBinMode_Stats — a single thread (the pass dispatches 1x1x1)
	if (thread_index == 0)
	{
		u_statsImage[uint2(0, 0)] = u_binCounters[0];
		u_statsImage[uint2(1, 0)] = u_binCounters[1];
	}
}
