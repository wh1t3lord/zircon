// meshlet_cluster.fs.slang — the NRI visibility-buffered forward stage.
// The visibility prepass writes a nonzero cluster ID and depth; this stage
// shades only fragments whose ID won that depth test. One VS and two FS
// programs serve the whole path — there is no per-material shader family.

import zircon_core;

[[vk::binding(0, 0)]] Texture2D<uint> u_visibility : register(t0, space0);

struct ZirconVsToFs_meshlet_cluster
{
	float4 position : SV_Position;
	float3 v_normal : NORMAL;
	nointerpolation uint v_cluster_id : TEXCOORD1;
};

// the hardcoded directional light (FROM the light, the phong convention)
static const float3 kMeshletLightDir = float3(-0.4, -1.0, -0.3);
static const float3 kMeshletLightColor = float3(1.0, 0.98, 0.95);
static const float3 kMeshletAmbient = float3(0.15, 0.15, 0.18);
static const float3 kMeshletAlbedo = float3(0.62, 0.66, 0.72);

[shader("fragment")]
float4 fs_main(ZirconVsToFs_meshlet_cluster input) : SV_Target
{
	const uint visible_id = u_visibility.Load(
		int3(int2(input.position.xy), 0));
	if (visible_id == 0 || visible_id != input.v_cluster_id)
		discard;

	float3 normal = normalize(input.v_normal);
	float3 light_dir = normalize(kMeshletLightDir);

	// lambert + ambient floor (the phong specular's view vector needs a
	// camera uniform — same deferred descriptor story)
	float3 lighting =
		kMeshletAmbient + kMeshletLightColor * saturate(dot(normal,
			-light_dir));

	return float4(kMeshletAlbedo * saturate(lighting), 1.0);
}
