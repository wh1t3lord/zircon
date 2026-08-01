// gizmo.vs.slang — editor own-gizmo overlay, vertex stage. Conventions:
// see zircon_core.slang. Position-only: the handle meshes are static unit
// shapes, the pass's per-draw model matrix (handle orientation + the
// constant-screen-size scale at the selection's position) places them;
// u_modelViewProj is a bgfx PREDEFINED uniform — the renderer fills it
// from setTransform * the view transform, the pass never writes it.

import zircon_core;

// vertex-stage uniform buffer. bgfx-vulkan expects the VS cbuffer at
// set 0 / binding 0 (kSpirvVertexBinding); the NRI-dx12 path uses
// register(b0, space0). Member offsets (cbuffer packing): u_modelViewProj
// = 0 — mirrored in cmake/zircon_slang_shaders.cmake
// (u_modelViewProj:mat4:0:4).
[[vk::binding(0, 0)]]
cbuffer GizmoParams : register(b0, space0)
{
	column_major float4x4 u_modelViewProj;
};

[shader("vertex")]
float4 vs_main(float3 a_position : POSITION) : SV_Position
{
	return mul(u_modelViewProj, float4(a_position, 1.0));
}
