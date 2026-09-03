// gizmo.fs.slang — editor own-gizmo overlay, fragment stage. Conventions:
// see zircon_core.slang. Unlit solid color: the pass sets u_color per
// draw (the per-axis base color with the hover/active highlight boost
// folded in); the pass's render state does the depth-test-off overlay
// blending, the shader itself is trivial.

import zircon_core;

// fragment-stage uniform buffer. The slot depends on the compile target:
// bgfx-vulkan expects the FS cbuffer at set 0 / binding 1
// (kSpirvFragmentBinding) and the NRI-dx12 path at register(b1, space0),
// but the bgfx d3d11 route (slang -target hlsl + fxc, compiled with
// -DZIRCON_SLANG_BGFX_FXC from cmake/zircon_slang_shaders.cmake) MUST sit
// at b0 — bgfx's d3d renderers bind the uniform cbuffer at slot 0 for BOTH
// stages (renderer_d3d11.cpp PSSetConstantBuffers(0, ...)), so a b1
// declaration there reads zeros (the 2026-09-03 "black screen" bug).
// Member offsets (cbuffer packing): u_color = 0 — mirrored in
// cmake/zircon_slang_shaders.cmake (u_color:vec4:0:1).
#ifdef ZIRCON_SLANG_BGFX_FXC
cbuffer GizmoFragmentParams : register(b0, space0)
#else
[[vk::binding(1, 0)]]
cbuffer GizmoFragmentParams : register(b1, space0)
#endif
{
	float4 u_color;
};

[shader("fragment")]
float4 fs_main() : SV_Target
{
	return u_color;
}
