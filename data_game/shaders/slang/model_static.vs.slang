// model_static.vs.slang — unlit static geometry, vertex stage.
// Conventions: see zircon_core.slang.

import zircon_core;

// vertex-stage uniform buffer. bgfx-vulkan expects the VS cbuffer at
// set 0 / binding 0 (kSpirvVertexBinding); the NRI-dx12 path uses
// register(b0, space0). Member offsets (cbuffer packing): u_model = 0,
// u_viewProj = 64 — mirrored in cmake/zircon_slang_shaders.cmake
// (u_model:mat4:0:4, u_viewProj:mat4:64:4).
[[vk::binding(0, 0)]]
cbuffer ModelParams : register(b0, space0)
{
	column_major float4x4 u_model;
	column_major float4x4 u_viewProj;
};

[shader("vertex")]
ZirconVsToFs_model_static vs_main(ZirconVsInput_model_static input)
{
	ZirconVsToFs_model_static output;
	float4 world = mul(u_model, float4(input.a_position, 1.0));
	output.position = mul(u_viewProj, world);
	output.v_color0 = input.a_color0;
	return output;
}
