// model_static.fs.slang — forward-Phong static geometry, fragment stage
// (task Z3 P2g). Conventions: see zircon_core.slang. One directional light
// + ambient floor + a modest Phong specular, clamped; the lit result
// modulates the interpolated vertex color so the unlit path's coloring
// still reads. The lighting math itself lives in zircon_core.slang
// (zircon_evaluate_phong — shared with model_static_gpu_driven.fs.slang,
// task Z24 B1) and is mirrored one-to-one by the C++ static
// zircon_render_graph_pass_model_static_bgfx::evaluate_phong (the unit
// tests pin it) — keep all three in sync.

import zircon_core;

// fragment-stage uniform buffer. The slot depends on the compile target:
// bgfx-vulkan expects the FS cbuffer at set 0 / binding 1
// (kSpirvFragmentBinding) and the NRI-dx12 path at register(b1, space0),
// but the bgfx d3d11 route (slang -target hlsl + fxc, compiled with
// -DZIRCON_SLANG_BGFX_FXC from cmake/zircon_slang_shaders.cmake) MUST sit
// at b0 — bgfx's d3d renderers bind the uniform cbuffer at slot 0 for BOTH
// stages (renderer_d3d11.cpp PSSetConstantBuffers(0, ...)), so a b1
// declaration there reads zeros (the 2026-09-03 "black screen" bug).
// Member offsets (cbuffer packing): u_lightDir = 0, u_lightColor = 16,
// u_ambient = 32, u_cameraPos = 48, u_lightClusterInfo = 64 — mirrored in
// cmake/zircon_slang_shaders.cmake (u_lightDir:vec4:0:1,
// u_lightColor:vec4:16:1, u_ambient:vec4:32:1, u_cameraPos:vec4:48:1,
// u_lightClusterInfo:vec4:64:1). Every member is pass-written (no bgfx
// predefined uniforms here): the pass fills the first four per frame from
// the zircon_DEF_RENDER_PASS_MODEL_STATIC_* defines (the FALLBACK light —
// the zero-light-components contract, task Z24 B5b), and
// u_lightClusterInfo from the frame's light collection.
//
// task Z24 B5b (clustered forward+): u_lightClusterInfo carries the tile
// grid + activation as u32 BIT PATTERNS (the B1 integer-lane discipline —
// asuint on this side): x = tiles_x, y = clustered-path active (0 = the
// fallback directional below answers; the pass sets it when the frame
// collected >= 1 light component), z = the tile edge in pixels
// (ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS's value, passed never hardcoded),
// w = the per-tile list cap.
//
// task Z24 B5c (the CSM sun shadows — the cbuffer extension):
//   u_shadowCascadeViewProj[4] at byte  80 (mat4 x4 = 256 B),
//   u_shadowSplitFar         at byte 336 (the per-cascade far view-z
//                            selection thresholds; lanes past the active
//                            count are FLT_MAX),
//   u_shadowMeta             at byte 352 — x = active cascade count as u32
//                            bits (0 = shadows OFF: the factor-1.0 path,
//                            the pre-B5c output byte-identical), y =
//                            1/map_size, z = the depth bias, w = the
//                            caster's light-table index as u32 bits,
//   u_view                   at byte 368 — a bgfx PREDEFINED (the camera
//                            view matrix, filled per draw from the view
//                            state; NEVER createUniform'd — the B5b
//                            u_viewProj trap's discipline). The fragment's
//                            camera view-z comes from it
#ifdef ZIRCON_SLANG_BGFX_FXC
cbuffer LightParams : register(b0, space0)
#else
[[vk::binding(1, 0)]]
cbuffer LightParams : register(b1, space0)
#endif
{
	float4 u_lightDir;   // xyz = direction the light travels (FROM the light), normalized
	float4 u_lightColor; // xyz = light rgb (w unused, 16-byte slot)
	float4 u_ambient;    // xyz = ambient floor rgb (w unused)
	float4 u_cameraPos;  // xyz = world camera position (the specular view vector)
	float4 u_lightClusterInfo; // the B5b cluster meta (see above)
	column_major float4x4 u_shadowCascadeViewProj[kZirconShadowCascadeCount]; // the B5c cascade VPs
	float4 u_shadowSplitFar; // the cascade selection thresholds
	float4 u_shadowMeta;       // the B5c shadow meta (see above)
	column_major float4x4 u_view; // bgfx predefined: the camera view (the cascade selection's view-z)
};

// task Z24 B5b: the frame's light table (4 float4 streams — the lanes are
// zircon_render_light_buffer.h's) + the binning output (per-tile counts +
// compacted index lists). Bound at stages 0..5 by the pass whenever the
// pass draws (bound in the fallback mode too — an unbound SRV is never
// read but the always-bound rule keeps the d3d11 debug layer quiet). The
// layout is byte-exact with the CPU table; the d3d11 route declares the
// TYPED forms (the compute-section comment in zircon_core.slang —
// renderer_d3d11.cpp creates typed SRVs only).
#ifdef ZIRCON_SLANG_BGFX_FXC
Buffer<float4> u_lightPositions : register(t0);
Buffer<float4> u_lightDirections : register(t1);
Buffer<float4> u_lightColors : register(t2);
Buffer<float4> u_lightParams : register(t3);
Buffer<uint> u_lightTileCounts : register(t4);
Buffer<uint> u_lightTileIndices : register(t5);
#else
[[vk::binding(2, 0)]]
StructuredBuffer<float4> u_lightPositions : register(t0, space0);
[[vk::binding(3, 0)]]
StructuredBuffer<float4> u_lightDirections : register(t1, space0);
[[vk::binding(4, 0)]]
StructuredBuffer<float4> u_lightColors : register(t2, space0);
[[vk::binding(5, 0)]]
StructuredBuffer<float4> u_lightParams : register(t3, space0);
[[vk::binding(6, 0)]]
StructuredBuffer<uint> u_lightTileCounts : register(t4, space0);
[[vk::binding(7, 0)]]
StructuredBuffer<uint> u_lightTileIndices : register(t5, space0);
#endif

// task Z24 B5c: the CSM depth map array (one slice per cascade), bound at
// stage 6 by the pass EVERY drawn frame (the always-bound rule — the
// factor-1.0 branches never sample it; this CPU-path pass binds the inert
// 1x1x4 dummy, the GPU-driven A/B pass binds the real array). Texture at
// vk binding 2+6 = 8, its sampler at 18+6 = 24 (the bgfx v11 binding
// model). POINT sampling: the 3x3 PCF below does its own per-texel
// compares — linear-filtered depth before the compare would be the wrong
// kernel
#ifdef ZIRCON_SLANG_BGFX_FXC
Texture2DArray<float> u_shadowMap : register(t6);
SamplerState u_shadowMapSampler : register(s6);
#else
[[vk::binding(8, 0)]]
Texture2DArray<float> u_shadowMap : register(t6, space0);
[[vk::binding(24, 0)]]
SamplerState u_shadowMapSampler : register(s6, space0);
#endif

// modest gloss (PBR is explicitly deferred — no material system yet)
static const float kShininess = 32.0;

// task Z24 B5c: the sun's shadow factor for a world position — the
// cascade picked by the camera view-z, the 3x3 PCF over the cascade's
// map slice. Factor 1.0 (lit) when: the meta is inactive (no caster /
// the rig inert), the fragment is past the last cascade, or the light
// clip lands outside the map (the contract — the C++ mirrors in
// zircon_render_shadow_cascade pin every step). Shared verbatim with
// model_static_gpu_driven.fs.slang
float zircon_sample_sun_shadow(float3 world_pos)
{
	const uint cascade_count = asuint(u_shadowMeta.x);

	if (cascade_count == 0u)
		return 1.0;

	const float view_z = mul(u_view, float4(world_pos, 1.0)).z;

	const uint cascade =
		zircon_shadow_select_cascade(view_z, u_shadowSplitFar);

	if (cascade >= cascade_count)
		return 1.0;

	const float4 clip =
		mul(u_shadowCascadeViewProj[cascade], float4(world_pos, 1.0));

	if (clip.w <= 0.0)
		return 1.0;

	const float3 ndc = clip.xyz / clip.w;

	// outside the map footprint — lit (the clamp sampler would smear the
	// edge texels otherwise)
	if (ndc.x < -1.0 || ndc.x > 1.0 || ndc.y < -1.0 || ndc.y > 1.0 ||
		ndc.z < 0.0 || ndc.z > 1.0)
		return 1.0;

	const float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	const float receiver = ndc.z;

	const float inv_map_size = u_shadowMeta.y;
	const float map_size = 1.0 / inv_map_size;
	const float bias = u_shadowMeta.z;

	// the 3x3 PCF: texel space, floor + {-1,0,1}^2 clamped to the map
	// edge, point-sampled at texel centers — the C++ mirror
	// (sample_shadow_pcf_3x3) pins the exact taps
	const float2 base = floor(uv * map_size);

	float lit_sum = 0.0;

	for (int offset_y = -1; offset_y <= 1; ++offset_y)
	{
		for (int offset_x = -1; offset_x <= 1; ++offset_x)
		{
			const float2 tap_texel = clamp(base + float2(offset_x, offset_y),
				0.0, map_size - 1.0);
			const float2 tap_uv = (tap_texel + 0.5) * inv_map_size;

			const float stored = u_shadowMap.SampleLevel(u_shadowMapSampler,
				float3(tap_uv, float(cascade)), 0);

			lit_sum += zircon_shadow_compare_depth(stored, receiver, bias);
		}
	}

	return lit_sum * (1.0 / 9.0);
}

// the clustered forward+ evaluation of this fragment's tile list (task
// Z24 B5b) — the body is shared verbatim by model_static.fs.slang and
// model_static_gpu_driven.fs.slang (the binding boilerplate is per-shader
// by necessity, the math is zircon_core.slang's). The fragment computes
// its tile from the pixel coordinate (SV_Position), iterates the tile's
// light list accumulating the diffuse terms, and the caller saturates
// ambient + the sum
float3 zircon_evaluate_clustered_lights(float3 normal, float3 world_pos,
	float2 pixel_position)
{
	const uint tiles_x = asuint(u_lightClusterInfo.x);
	const uint tile_pixels = asuint(u_lightClusterInfo.z);
	const uint max_per_tile = asuint(u_lightClusterInfo.w);

	const uint tile_x = uint(pixel_position.x) / tile_pixels;
	const uint tile_y = uint(pixel_position.y) / tile_pixels;
	const uint tile_index = tile_y * tiles_x + tile_x;

	const uint light_count_in_tile =
		min(u_lightTileCounts[tile_index], max_per_tile);

	float3 light_sum = float3(0.0, 0.0, 0.0);

	// no [unroll]: the dynamic trip count is the point (Slang's attributed
	// loops also trip fxc's X3511 on the d3d11 route — the cull compute's
	// lesson)
	for (uint list_index = 0; list_index < light_count_in_tile;
		 ++list_index)
	{
		const uint light_index =
			u_lightTileIndices[tile_index * max_per_tile + list_index];

		const float4 pos_range = u_lightPositions[light_index];
		const float4 params = u_lightParams[light_index];
		const uint light_type = asuint(params.z);
		const float3 light_color = u_lightColors[light_index].xyz;

		if (light_type == kZirconLightType_Directional)
		{
			float3 directional =
				zircon_evaluate_directional_light_diffuse(normal,
					u_lightDirections[light_index].xyz, light_color);

			// task Z24 B5c: the sun's shadow factor — only the caster (the
			// meta's table index) is shadowed; an inactive meta keeps the
			// factor at exactly 1.0 (the pre-B5c output byte-identical)
			if (asuint(u_shadowMeta.x) != 0u &&
				light_index == asuint(u_shadowMeta.w))
			{
				directional *= zircon_sample_sun_shadow(world_pos);
			}

			light_sum += directional;
		}
		else if (light_type == kZirconLightType_Point)
		{
			light_sum += zircon_evaluate_point_light_diffuse(normal,
				world_pos, pos_range.xyz, light_color, pos_range.w);
		}
		else
		{
			light_sum += zircon_evaluate_spot_light_diffuse(normal,
				world_pos, pos_range.xyz, u_lightDirections[light_index].xyz,
				light_color, pos_range.w, params.x, params.y);
		}
	}

	return saturate(u_ambient.xyz + light_sum);
}

[shader("fragment")]
float4 fs_main(ZirconVsToFs_model_static input) : SV_Target
{
	float3 normal = normalize(input.v_normal);

	float3 lighting;

	if (asuint(u_lightClusterInfo.y) != 0u)
	{
		// the clustered path (the frame collected >= 1 light component)
		lighting = zircon_evaluate_clustered_lights(
			normal, input.v_worldPos, input.position.xy);
	}
	else
	{
		// the pre-B5b fallback: the named-define directional + ambient +
		// Phong specular — the zero-light-components contract (the current
		// behavior never regresses)
		float3 light_dir = normalize(u_lightDir.xyz);
		float3 view_dir = normalize(u_cameraPos.xyz - input.v_worldPos);

		lighting = zircon_evaluate_phong(normal, light_dir,
			u_lightColor.xyz, u_ambient.xyz, view_dir, kShininess);
	}

	return float4(input.v_color0.rgb * lighting, input.v_color0.a);
}
