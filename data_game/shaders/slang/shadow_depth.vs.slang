// shadow_depth.vs.slang — the CSM cascade depth pass's vertex stage (task
// Z24 B5c). Conventions: see zircon_core.slang. The chunk pool's vertices
// are baked in world space (the B1 registration), so the stage only
// projects through the cascade's view-projection — u_viewProj is a bgfx
// PREDEFINED uniform (name-matched, filled per draw from the cascade
// view's transform; NEVER createUniform'd — the documented trap). The
// depth pass outputs no varyings: the fragment stage is empty (the
// cascade frame buffers carry no color attachment), so the VS returns a
// bare SV_Position (the .bin's hashOut = 0 matches the FS's hashIn = 0).

import zircon_core;

// vertex-stage uniform buffer (bgfx-vulkan set 0 / binding 0, fxc
// register(b0, space0) — the every-target vertex slot). Mirrored in
// cmake/zircon_slang_shaders.cmake (u_viewProj:mat4:0:4)
[[vk::binding(0, 0)]]
cbuffer ModelParams : register(b0, space0)
{
	column_major float4x4 u_viewProj;
};

struct ZirconVsInput_shadow_depth
{
	float3 a_position : POSITION;
};

[shader("vertex")]
float4 vs_main(ZirconVsInput_shadow_depth input) : SV_Position
{
	return mul(u_viewProj, float4(input.a_position, 1.0));
}
