// gizmo.fs.slang — editor own-gizmo overlay, fragment stage. Conventions:
// see zircon_core.slang. Unlit solid color: the pass sets u_color per
// draw (the per-axis base color with the hover/active highlight boost
// folded in); the pass's render state does the depth-test-off overlay
// blending, the shader itself is trivial.

import zircon_core;

// fragment-stage uniform buffer. bgfx-vulkan expects the FS cbuffer at
// set 0 / binding 1 (kSpirvFragmentBinding); the NRI-dx12 path uses
// register(b1, space0). Member offsets (cbuffer packing): u_color = 0 —
// mirrored in cmake/zircon_slang_shaders.cmake (u_color:vec4:0:1).
[[vk::binding(1, 0)]]
cbuffer GizmoFragmentParams : register(b1, space0)
{
	float4 u_color;
};

[shader("fragment")]
float4 fs_main() : SV_Target
{
	return u_color;
}
