// model_static.fs.slang — forward-Phong static geometry, fragment stage
// (task Z3 P2g). Conventions: see zircon_core.slang. One directional light
// + ambient floor + a modest Phong specular, clamped; the lit result
// modulates the interpolated vertex color so the unlit path's coloring
// still reads. The lighting math is mirrored one-to-one by the C++ static
// zircon_render_graph_pass_model_static_bgfx::evaluate_phong (the unit
// tests pin it) — keep the two in sync.

import zircon_core;

// fragment-stage uniform buffer. bgfx-vulkan expects the FS cbuffer at
// set 0 / binding 1 (kSpirvFragmentBinding); the NRI-dx12 path uses
// register(b1, space0). Member offsets (cbuffer packing): u_lightDir = 0,
// u_lightColor = 16, u_ambient = 32, u_cameraPos = 48 — mirrored in
// cmake/zircon_slang_shaders.cmake (u_lightDir:vec4:0:1,
// u_lightColor:vec4:16:1, u_ambient:vec4:32:1, u_cameraPos:vec4:48:1).
// Every member is pass-written (no bgfx predefined uniforms here): the
// pass fills them per frame from the zircon_DEF_RENDER_PASS_MODEL_STATIC_*
// defines until light components land (a later task).
[[vk::binding(1, 0)]]
cbuffer LightParams : register(b1, space0)
{
	float4 u_lightDir;   // xyz = direction the light travels (FROM the light), normalized
	float4 u_lightColor; // xyz = light rgb (w unused, 16-byte slot)
	float4 u_ambient;    // xyz = ambient floor rgb (w unused)
	float4 u_cameraPos;  // xyz = world camera position (the specular view vector)
};

// modest gloss (PBR is explicitly deferred — no material system yet)
static const float kShininess = 32.0;

[shader("fragment")]
float4 fs_main(ZirconVsToFs_model_static input) : SV_Target
{
	float3 normal = normalize(input.v_normal);
	float3 light_dir = normalize(u_lightDir.xyz);
	float3 view_dir = normalize(u_cameraPos.xyz - input.v_worldPos);

	// Lambert: the direction TO the light is -light_dir
	float ndotl = saturate(dot(normal, -light_dir));

	// Phong specular — killed on backfacing normals (reflect() would
	// still peak there); reflect(I, N) mirrors the incident direction
	float specular = 0.0;

	if (ndotl > 0.0)
	{
		float3 reflected = reflect(light_dir, normal);
		specular = pow(saturate(dot(reflected, view_dir)), kShininess);
	}

	float3 lighting =
		saturate(u_ambient.xyz + u_lightColor.xyz * (ndotl + specular));

	return float4(input.v_color0.rgb * lighting, input.v_color0.a);
}
