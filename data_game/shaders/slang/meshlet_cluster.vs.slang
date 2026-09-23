// meshlet_cluster.vs.slang — the NRI meshlet cluster draw, vertex stage
// (task Z24 B3b: the first real NRI draw — one baked LOD0 cluster through
// the kotek geometry seam). Conventions: see zircon_core.slang. The pass
// streams the cluster's interleaved soup (a_position | a_normal, 24-byte
// stride) and uploads the view-projection through the PUSH-CONSTANT
// block: on NRI/dx12 the pipeline's root-constant range occupies
// register(b0) for BOTH stages, so the cbuffer carries no [[vk::binding]]
// pair (the NRI path is the only consumer of this pair — the bgfx route
// builds the same-shader containers for pipeline uniformity, nothing
// bgfx binds them). Member offsets: u_viewProj = 0 — mirrored in
// cmake/zircon_slang_shaders.cmake (u_viewProj:mat4:0:4).

import zircon_core;

// the push-constant (root-constant) block — 64 bytes, both stages
cbuffer MeshletParams : register(b0, space0)
{
	column_major float4x4 u_viewProj;
};

struct ZirconVsInput_meshlet_cluster
{
	float3 a_position : POSITION;
	float3 a_normal : NORMAL;
};

struct ZirconVsToFs_meshlet_cluster
{
	float4 position : SV_Position;
	float3 v_normal : NORMAL;
};

[shader("vertex")]
ZirconVsToFs_meshlet_cluster vs_main(ZirconVsInput_meshlet_cluster input)
{
	ZirconVsToFs_meshlet_cluster output;
	output.position = mul(u_viewProj, float4(input.a_position, 1.0));
	output.v_normal = input.a_normal;
	return output;
}
