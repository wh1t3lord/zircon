// model_static_gpu_driven.fs.slang — the GPU-driven chunked path's draw,
// fragment stage (task Z24 B1). Conventions: see zircon_core.slang. The
// lighting is the model_static forward-Phong verbatim — the SHARED
// zircon_evaluate_phong from zircon_core.slang (never forked), mirrored
// in C++ by zircon_render_graph_pass_model_static_bgfx::evaluate_phong
// (the unit tests pin it — keep all three in sync). The LightParams
// cbuffer contract is model_static's: same layout, same binding
// discipline (b1/vk::binding(1,0), dropping to b0 under
// ZIRCON_SLANG_BGFX_FXC — the 2026-09-03 rule), same four pass-written
// uniforms.

import zircon_core;

// fragment-stage uniform buffer — see the slot comment in
// model_static.fs.slang (identical contract). Member offsets:
// u_lightDir = 0, u_lightColor = 16, u_ambient = 32, u_cameraPos = 48 —
// mirrored in cmake/zircon_slang_shaders.cmake.
#ifdef ZIRCON_SLANG_BGFX_FXC
cbuffer LightParams : register(b0, space0)
#else
[[vk::binding(1, 0)]]
cbuffer LightParams : register(b1, space0)
#endif
{
	float4 u_lightDir;   // xyz = direction the light travels (FROM the light), normalized
	float4 u_lightColor; // xyz = light rgb (w unused, 16-byte slot)
	float4 u_ambient;    // xyz = ambient floor rgb (w unused)
	float4 u_cameraPos;  // xyz = world camera position (the specular view vector)
};

// the same modest gloss as model_static
static const float kShininess = 32.0;

[shader("fragment")]
float4 fs_main(ZirconVsToFs_model_static input) : SV_Target
{
	float3 normal = normalize(input.v_normal);
	float3 light_dir = normalize(u_lightDir.xyz);
	float3 view_dir = normalize(u_cameraPos.xyz - input.v_worldPos);

	float3 lighting = zircon_evaluate_phong(normal, light_dir,
		u_lightColor.xyz, u_ambient.xyz, view_dir, kShininess);

	return float4(input.v_color0.rgb * lighting, input.v_color0.a);
}
