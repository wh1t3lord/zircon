// model_static_gpu_driven.fs.slang — the GPU-driven chunked path's draw,
// fragment stage (task Z24 B1). Conventions: see zircon_core.slang. The
// lighting is the model_static forward-Phong verbatim — the SHARED
// zircon_evaluate_phong from zircon_core.slang (never forked), mirrored
// in C++ by zircon_render_graph_pass_model_static_bgfx::evaluate_phong
// (the unit tests pin it — keep all three in sync). The LightParams
// cbuffer contract is model_static's: same layout, same binding
// discipline (b1/vk::binding(1,0), dropping to b0 under
// ZIRCON_SLANG_BGFX_FXC — the 2026-09-03 rule), same four pass-written
// uniforms.

import zircon_core;

// fragment-stage uniform buffer — see the slot comment in
// model_static.fs.slang (identical contract). Member offsets:
// u_lightDir = 0, u_lightColor = 16, u_ambient = 32, u_cameraPos = 48,
// u_lightClusterInfo = 64 — mirrored in cmake/zircon_slang_shaders.cmake.
// u_lightClusterInfo (task Z24 B5b) carries the tile grid + activation as
// u32 BIT PATTERNS: x = tiles_x, y = clustered-path active (0 = the
// fallback directional answers — the zero-light-components contract), z =
// the tile edge in pixels, w = the per-tile list cap
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
};

// task Z24 B5b: the frame's light table + the binning output, bound at
// stages 0..5 by the pass whenever it draws (the always-bound rule — see
// model_static.fs.slang). Byte-exact with zircon_render_light_buffer.h's
// AoSoA table; typed Buffer forms on the d3d11 route (zircon_core.slang's
// compute-section comment)
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

// the same modest gloss as model_static
static const float kShininess = 32.0;

// the clustered forward+ evaluation of this fragment's tile list (task
// Z24 B5b) — shared verbatim with model_static.fs.slang (the math is
// zircon_core.slang's)
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
			light_sum += zircon_evaluate_directional_light_diffuse(normal,
				u_lightDirections[light_index].xyz, light_color);
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
		// Phong specular — the zero-light-components contract
		float3 light_dir = normalize(u_lightDir.xyz);
		float3 view_dir = normalize(u_cameraPos.xyz - input.v_worldPos);

		lighting = zircon_evaluate_phong(normal, light_dir,
			u_lightColor.xyz, u_ambient.xyz, view_dir, kShininess);
	}

	return float4(input.v_color0.rgb * lighting, input.v_color0.a);
}
