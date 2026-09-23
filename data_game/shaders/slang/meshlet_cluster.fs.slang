// meshlet_cluster.fs.slang — the NRI meshlet cluster draw, fragment stage
// (task Z24 B3b). Conventions: see zircon_core.slang. The cluster bins
// carry no albedo (positions + flat normals + materials only), and the v1
// seam binds no descriptor tables — so the light is a HARDCODED
// directional lambert (the shared zircon_evaluate_phong contract needs
// bound LightParams cbuffers; it lands with the material/lighting phase,
// and the descriptor seam grows additively then). The albedo is a fixed
// neutral — this draw proves the geometry path, not the material system.

import zircon_core;

struct ZirconVsToFs_meshlet_cluster
{
	float4 position : SV_Position;
	float3 v_normal : NORMAL;
};

// the hardcoded directional light (FROM the light, the phong convention)
static const float3 kMeshletLightDir = float3(-0.4, -1.0, -0.3);
static const float3 kMeshletLightColor = float3(1.0, 0.98, 0.95);
static const float3 kMeshletAmbient = float3(0.15, 0.15, 0.18);
static const float3 kMeshletAlbedo = float3(0.62, 0.66, 0.72);

[shader("fragment")]
float4 fs_main(ZirconVsToFs_meshlet_cluster input) : SV_Target
{
	float3 normal = normalize(input.v_normal);
	float3 light_dir = normalize(kMeshletLightDir);

	// lambert + ambient floor (the phong specular's view vector needs a
	// camera uniform — same deferred descriptor story)
	float3 lighting =
		kMeshletAmbient + kMeshletLightColor * saturate(dot(normal,
			-light_dir));

	return float4(kMeshletAlbedo * saturate(lighting), 1.0);
}
