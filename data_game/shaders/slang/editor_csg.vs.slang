// editor_csg.vs.slang — the editor CSG compound draw, vertex stage
// (task Z25 A2). Conventions: see zircon_core.slang. All editor
// compounds live in the pass's ONE shared dynamic pool with
// world-space positions/normals baked at rebuild-merge time (the
// evaluation outputs world-space geometry — the primitive transforms
// are part of the brush math), so this stage only projects: no model
// matrix, and the cbuffer carries ONLY u_viewProj — a bgfx
// predefined uniform (auto-filled from setViewTransform, name-matched
// at shader create — never createUniform it, the 2026-09-03 rule):
// the model_static_gpu_driven vertex contract verbatim. The IO
// structs are model_static's (shared from zircon_core.slang — the
// varying set hashes identically in the .bin pair and the geometry
// layout is position+normal+color, the shared forward-Phong
// contract).

import zircon_core;

// vertex-stage uniform buffer (bgfx-vulkan set 0 / binding 0, NRI-dx12
// / fxc register(b0, space0) — the every-target vertex slot). Member
// offsets: u_viewProj = 0 — mirrored in cmake/zircon_slang_shaders.cmake
// (u_viewProj:mat4:0:4).
[[vk::binding(0, 0)]]
cbuffer ModelParams : register(b0, space0)
{
	column_major float4x4 u_viewProj;
};

[shader("vertex")]
ZirconVsToFs_model_static vs_main(ZirconVsInput_model_static input)
{
	ZirconVsToFs_model_static output;
	float4 world = float4(input.a_position, 1.0);
	output.position = mul(u_viewProj, world);
	output.v_worldPos = world.xyz;
	output.v_normal = input.a_normal;
	output.v_color0 = input.a_color0;
	return output;
}
