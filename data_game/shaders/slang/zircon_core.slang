// zircon_core.slang — shared module for zircon's Slang shaders (imported by
// every *.vs.slang / *.fs.slang). Slang is the ONLY user shading language;
// one source serves both render backends:
//
//   bgfx (vulkan renderer): slangc -target spirv  -> zircon_shaderpack packs
//                           the blob into bgfx's .bin container (cache:
//                           data_user/shader_cache/bgfx/vulkan)
//   NRI  (dx12):            slangc -target dxil   -> raw .dxil (cache:
//                           data_user/shader_cache/nri/dx12)
//
// House conventions (keep every shader on them — the build's per-shader
// metadata in cmake/zircon_slang_shaders.cmake mirrors them):
//
// - names: vertex attributes a_* (bgfx convention: a_position, a_color0,
//   a_texcoord0, ...), varyings v_*, uniforms u_*.
// - IO is bare-parameter structs with HLSL semantics (POSITION/COLOR0/...)
//   — no $input/$output macros, no global `void main`; entry points are
//   named functions tagged [shader("vertex")] / [shader("fragment")]
//   (vs_main / fs_main).
// - declaration order inside the IO structs IS the SPIR-V location order;
//   the vertex attribute list passed to zircon_shaderpack must match it.
// - resources carry an explicit [[vk::binding(b, 0)]] + register(bN, space0)
//   pair so SPIR-V and DXIL agree. bgfx's vulkan binding model (bin v11)
//   fixes the slots: vertex-stage cbuffer at binding 0, fragment-stage
//   cbuffer at binding 1, texture N at binding 2+N with its sampler at
//   binding 18+N (bgfx shaderc_spirv.cpp setShiftBinding /
//   shader_spirv.h kSpirvVertexBinding/kSpirvFragmentBinding/kSpirvBindShift/
//   kSpirvSamplerShift). EXCEPTION — the bgfx d3d11 route (slang -target
//   hlsl + fxc) compiles with -DZIRCON_SLANG_BGFX_FXC and fragment cbuffers
//   MUST drop to register(b0) under that define: bgfx's d3d renderers bind
//   the uniform cbuffer at slot 0 for BOTH stages
//   (renderer_d3d11.cpp PSSetConstantBuffers(0, ...)); a b1 declaration
//   reads zeros there. Vertex cbuffers stay at b0 on every target.
// - cbuffer member byte offsets must match the uniform table metadata
//   (std140/HLSL packing: float4x4 = 64 bytes); u_model at 0, u_viewProj
//   at 64 in ModelParams below.
// - COMPUTE shaders (task Z24 B1, *.cs.slang): [shader("compute")] +
//   [numthreads] + entry cs_main. The single cbuffer sits at register(b0)
//   on every target ([[vk::binding(0, 0)]] — the compute UBO binding is
//   kSpirvVertexBinding = 0). Storage buffers/images bind at bgfx stage
//   N: register(tN/uN) for the d3d11/dx12 routes and
//   [[vk::binding(2+N, 0)]] for vulkan (kSpirvBindShift = 2; the .bin
//   storage table carries them — cmake's CS_UNIFORMS_VULKAN). The bgfx
//   d3d11 route (ZIRCON_SLANG_BGFX_FXC) MUST declare the TYPED buffer
//   forms (Buffer<T>/RWBuffer<T>) — bgfx's d3d11 renderer creates typed
//   UAV/SRV views, never structured (renderer_d3d11.cpp
//   BufferD3D11::create: no D3D11_RESOURCE_MISC_BUFFER_STRUCTURED); the
//   other routes declare StructuredBuffer/RWStructuredBuffer.

// forward-Phong lighting (task Z3 P2g) — ONE implementation shared by
// model_static.fs.slang and model_static_gpu_driven.fs.slang (task Z24
// B1); never fork it per pass. Mirrored one-to-one by the C++ static
// zircon_render_graph_pass_model_static_bgfx::evaluate_phong (the unit
// tests pin them — keep all three in sync): one directional light +
// ambient floor + Phong specular, clamped. All directions are normalized
// by the caller (the shaders normalize the interpolated normal and the
// view vector, the pass uploads a normalized light direction).
// light_dir_from points the way the light travels (FROM the light).
float3 zircon_evaluate_phong(float3 normal, float3 light_dir_from,
	float3 light_color_rgb, float3 ambient_rgb, float3 view_dir,
	float shininess)
{
	// Lambert: the direction TO the light is -light_dir_from
	float ndotl = saturate(dot(normal, -light_dir_from));

	// Phong specular — killed on backfacing normals (reflect() would
	// still peak there); reflect(I, N) mirrors the incident direction
	float specular = 0.0;

	if (ndotl > 0.0)
	{
		float3 reflected = reflect(light_dir_from, normal);
		specular = pow(saturate(dot(reflected, view_dir)), shininess);
	}

	return saturate(ambient_rgb + light_color_rgb * (ndotl + specular));
}

// ---------------------------------------------------------------------------
// task Z24 B5b — the clustered forward+ per-light evaluators. The game
// fragment stages (model_static.fs.slang, model_static_gpu_driven.fs.slang)
// call these while iterating their screen-tile's light list; the CPU mirrors
// live in zircon_render_light_buffer (evaluate_*_diffuse — the unit tests pin
// them, keep all four in sync). The clamping discipline: saturate per factor
// per light (ndotl, the range window, the cone window); the caller saturates
// ambient + the accumulated sum. All directions are normalized by the caller
// (the collection normalizes the light direction; the fragment normalizes the
// interpolated normal). light_dir_from points the way the light travels (FROM
// the light), the zircon_evaluate_phong convention.
//
// the light type values mirror eZirconLightType
// (src/ecs/zircon_component_light.h) — the GPU table carries them as the
// params stream's z lane u32 bit pattern; keep the two in sync
static const uint kZirconLightType_Directional = 0;
static const uint kZirconLightType_Point = 1;
static const uint kZirconLightType_Spot = 2;

// the directional list light: Lambert only — diffuse per light (the
// fallback path's specular stays the fallback's alone this phase)
float3 zircon_evaluate_directional_light_diffuse(float3 normal,
	float3 light_dir_from, float3 light_color_rgb)
{
	return light_color_rgb * saturate(dot(normal, -light_dir_from));
}

// the point light: Lambert x the linear-saturated range window
// (1 - dist/range). The direction is singular at the light's exact
// position — the max() guard makes the degenerate overlap contribute 0
// (deterministic, documented in the mirror)
float3 zircon_evaluate_point_light_diffuse(float3 normal, float3 world_pos,
	float3 light_pos, float3 light_color_rgb, float range)
{
	float3 to_light = light_pos - world_pos;
	float dist = length(to_light);
	float3 dir_to = to_light * (1.0 / max(dist, 1e-6));

	float ndotl = saturate(dot(normal, dir_to));
	float attenuation = saturate(1.0 - dist / range);

	return light_color_rgb * (ndotl * attenuation);
}

// the spot light: the point term x the cone window saturate((cos_angle -
// cos_outer) / (cos_inner - cos_outer)) where cos_angle is the angle between
// the apex->fragment direction and the cone axis
float3 zircon_evaluate_spot_light_diffuse(float3 normal, float3 world_pos,
	float3 light_pos, float3 light_dir_from, float3 light_color_rgb,
	float range, float cos_inner, float cos_outer)
{
	float3 to_light = light_pos - world_pos;
	float dist = length(to_light);
	float3 dir_to = to_light * (1.0 / max(dist, 1e-6));

	float ndotl = saturate(dot(normal, dir_to));
	float attenuation = saturate(1.0 - dist / range);

	float cos_angle = dot(-dir_to, light_dir_from);
	float cone = saturate(
		(cos_angle - cos_outer) / max(cos_inner - cos_outer, 1e-5));

	return light_color_rgb * (ndotl * attenuation * cone);
}

// model_static: forward-Phong static geometry, per-vertex normal + color
// (game pass "model_static", task Z3 P2b unlit -> P2g lit — Slang port of
// data_game/shaders/bgfx/model_static, with explicit u_model/u_viewProj
// instead of bgfx's predefined u_modelViewProj). The varyings carry the
// world position and the world-space normal (the model matrix's linear
// part, normalized in the fragment stage) for the lighting, the color
// modulates the lit result.
struct ZirconVsInput_model_static
{
	float3 a_position : POSITION;
	float3 a_normal : NORMAL;
	float4 a_color0 : COLOR0;
};

struct ZirconVsToFs_model_static
{
	float4 position : SV_Position;
	float3 v_worldPos : TEXCOORD0;
	float3 v_normal : NORMAL;
	float4 v_color0 : COLOR0;
};

// grid: editor procedural infinite grid (editor pass "editor_grid", task
// Z3 P2d). No vertex INPUTS at all: the vertex stage expands a fullscreen
// triangle from SV_VertexID (bgfx::setVertexCount(3)) and forwards the NDC
// corner; the fragment stage unprojects it through u_invViewProj and draws
// the grid analytically. The ray/plane math is mirrored in C++
// (zircon_render_graph_pass_editor_grid.h statics) for the unit tests —
// keep the two in sync.
struct ZirconVsToFs_grid
{
	float4 position : SV_Position;
	float2 v_ndc : TEXCOORD0;
};

// gizmo: editor own-gizmo overlay (editor pass "editor_gizmo_own", task
// Z3 P2e). No shared IO structs: the vertex stage takes a bare
// a_position and returns SV_Position (no varyings), the fragment stage
// takes no inputs and outputs the per-draw u_color.
