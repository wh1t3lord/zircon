// meshlet_cluster_visibility.fs.slang — writes the nonzero cluster ID that
// the shared meshlet_cluster.vs.slang forwards without interpolation.
// Depth testing keeps the nearest ID; zero is the cleared background.

import zircon_core;

struct ZirconVsToFs_meshlet_cluster_visibility
{
	float4 position : SV_Position;
	float3 v_normal : NORMAL;
	nointerpolation uint v_cluster_id : TEXCOORD1;
};

[shader("fragment")]
uint fs_main(ZirconVsToFs_meshlet_cluster_visibility input) : SV_Target
{
	return input.v_cluster_id;
}
