// meshlet_cluster.vs.slang — the shared visibility/forward vertex stage.
// The 28-byte interleaved soup is position | normal | nonzero cluster id.
// The id is flat across each triangle: the visibility FS writes it and the
// forward FS compares it with the winning id in the visibility texture.

import zircon_core;

// NRI maps this 64-byte block to root constants at b0 for BOTH stages.
// The explicit Vulkan binding keeps the portability-only bgfx route clean.
[[vk::binding(0, 0)]]
cbuffer MeshletParams : register(b0, space0)
{
	column_major float4x4 u_viewProj;
};

struct ZirconVsInput_meshlet_cluster
{
	float3 a_position : POSITION;
	float3 a_normal : NORMAL;
	uint a_texcoord0 : TEXCOORD0; // the nonzero visibility cluster id
};

struct ZirconVsToFs_meshlet_cluster
{
	float4 position : SV_Position;
	float3 v_normal : NORMAL;
	nointerpolation uint v_cluster_id : TEXCOORD1;
};

[shader("vertex")]
ZirconVsToFs_meshlet_cluster vs_main(ZirconVsInput_meshlet_cluster input)
{
	ZirconVsToFs_meshlet_cluster output;
	output.position = mul(u_viewProj, float4(input.a_position, 1.0));
	output.v_normal = input.a_normal;
	output.v_cluster_id = input.a_texcoord0;
	return output;
}
