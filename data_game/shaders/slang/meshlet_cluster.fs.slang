// meshlet_cluster.fs.slang — visibility-buffered clustered forward+.
// The visibility pass writes nonzero cluster IDs + D32 depth. This stage
// resolves that ID (plus EQUAL depth) and evaluates only the lights binned
// into the fragment's 32 px tile. Zero collected lights keeps the original
// directional fallback, not a second shader program.

import zircon_core;

[[vk::binding(1, 0)]] Texture2D<uint> u_visibility : register(t0, space0);

#ifdef ZIRCON_SLANG_BGFX_FXC
Buffer<float4> u_lightPositions : register(t1);
Buffer<float4> u_lightDirections : register(t2);
Buffer<float4> u_lightColors : register(t3);
Buffer<float4> u_lightParams : register(t4);
Buffer<uint> u_lightTileCounts : register(t5);
Buffer<uint> u_lightTileIndices : register(t6);
#else
[[vk::binding(2, 0)]] StructuredBuffer<float4> u_lightPositions : register(t1, space0);
[[vk::binding(3, 0)]] StructuredBuffer<float4> u_lightDirections : register(t2, space0);
[[vk::binding(4, 0)]] StructuredBuffer<float4> u_lightColors : register(t3, space0);
[[vk::binding(5, 0)]] StructuredBuffer<float4> u_lightParams : register(t4, space0);
[[vk::binding(6, 0)]] StructuredBuffer<uint> u_lightTileCounts : register(t5, space0);
[[vk::binding(7, 0)]] StructuredBuffer<uint> u_lightTileIndices : register(t6, space0);
#endif

[[vk::binding(0, 0)]]
cbuffer MeshletParams : register(b0, space0)
{
	column_major float4x4 u_viewProj;
	float4 u_lightClusterInfo;
};

struct ZirconVsToFs_meshlet_cluster
{
	float4 position : SV_Position;
	float3 v_normal : NORMAL;
	nointerpolation uint v_cluster_id : TEXCOORD1;
	float3 v_worldPos : TEXCOORD2;
};

static const float3 kMeshletLightDir = float3(-0.4, -1.0, -0.3);
static const float3 kMeshletLightColor = float3(1.0, 0.98, 0.95);
static const float3 kMeshletAmbient = float3(0.15, 0.15, 0.18);
static const float3 kMeshletAlbedo = float3(0.62, 0.66, 0.72);

float3 zircon_meshlet_evaluate_clustered_lights(float3 normal,
	float3 world_pos, float2 pixel_position)
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
	for (uint list_index = 0; list_index < light_count_in_tile; ++list_index)
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
			light_sum += zircon_evaluate_point_light_diffuse(normal, world_pos,
				pos_range.xyz, light_color, pos_range.w);
		}
		else
		{
			light_sum += zircon_evaluate_spot_light_diffuse(normal, world_pos,
				pos_range.xyz, u_lightDirections[light_index].xyz, light_color,
				pos_range.w, params.x, params.y);
		}
	}

	return saturate(kMeshletAmbient + light_sum);
}

[shader("fragment")]
float4 fs_main(ZirconVsToFs_meshlet_cluster input) : SV_Target
{
	const uint visible_id = u_visibility.Load(
		int3(int2(input.position.xy), 0));
	if (visible_id == 0 || visible_id != input.v_cluster_id)
		discard;

	float3 normal = normalize(input.v_normal);
	float3 lighting;
	if (asuint(u_lightClusterInfo.y) != 0u)
	{
		lighting = zircon_meshlet_evaluate_clustered_lights(
			normal, input.v_worldPos, input.position.xy);
	}
	else
	{
		float3 light_dir = normalize(kMeshletLightDir);
		lighting = kMeshletAmbient +
			kMeshletLightColor * saturate(dot(normal, -light_dir));
	}

	return float4(kMeshletAlbedo * saturate(lighting), 1.0);
}
