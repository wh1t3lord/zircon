// model_static_gpu_driven.vs.slang — the GPU-driven chunked path's draw,
// vertex stage (task Z24 B1). Conventions: see zircon_core.slang. The
// vertex streams from the pass's ONE shared pool; the chunk's transform
// was baked into the pool copy at registration (chunks are static —
// identity model matrix here by construction, so no u_model), and the
// GPU cull already decided visibility: this stage only projects.
// The IO structs are model_static's (shared from zircon_core.slang — the
// varying set hashes identically in the .bin pair).

import zircon_core;

// vertex-stage uniform buffer (bgfx-vulkan set 0 / binding 0, NRI-dx12 /
// fxc register(b0, space0) — the every-target vertex slot). Member
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
	// the pool position IS the world position (registration-time bake,
	// see the pass header): no model transform. The normal was baked with
	// the same 3x3 — the fragment stage normalizes, so the model_static
	// no-inverse-transpose approximation carries over unchanged
	float4 world = float4(input.a_position, 1.0);
	output.position = mul(u_viewProj, world);
	output.v_worldPos = world.xyz;
	output.v_normal = input.a_normal;
	output.v_color0 = input.a_color0;
	return output;
}
