// grid.fs.slang — editor infinite grid, fragment stage. Conventions: see
// zircon_core.slang. Reconstructs the world ray through the pixel from the
// inverse view-projection and draws the world XZ-plane grid analytically:
// 1 m cells + 10 m major lines, colored X/Z axis lines, distance fade to
// the horizon. The pass renders the grid alpha-blended, depth-tested with
// no depth write (it sits between the present clear and imgui, UNDER the
// scene geometry the later passes draw).
//
// The ray/plane math below is mirrored one-to-one by the C++ statics in
// zircon_render_graph_pass_editor_grid.h (the unit tests pin them) — keep
// the two in sync.

import zircon_core;

// fragment-stage uniform buffer. bgfx-vulkan expects the FS cbuffer at
// set 0 / binding 1 (kSpirvFragmentBinding); the NRI-dx12 path uses
// register(b1, space0). Member offsets (cbuffer packing): u_invViewProj =
// 0, u_cameraPos = 64 — mirrored in cmake/zircon_slang_shaders.cmake
// (u_invViewProj:mat4:0:4, u_cameraPos:vec4:64:1). u_invViewProj is a bgfx
// PREDEFINED uniform: the renderer fills it from the view's
// setViewTransform state (inverse(P*V)), the pass never writes it.
[[vk::binding(1, 0)]]
cbuffer GridParams : register(b1, space0)
{
	column_major float4x4 u_invViewProj;
	float4 u_cameraPos; // xyz = world camera position (w unused, 16-byte slot)
};

// cell/line parameters are shader constants (task Z3 P2d); the fade is in
// meters of ray length
static const float kMajorLineEvery = 10.0;   // one major line per N cells
static const float kFadeDistance = 75.0;     // ray length of full fade-out
static const float kCellAlpha = 0.35;        // peak alpha of the 1 m cells
static const float kMajorAlpha = 0.70;       // peak alpha of the 10 m lines
static const float kAxisAlpha = 0.90;        // peak alpha of the axis lines
static const float3 kCellColor = float3(0.42, 0.42, 0.45);
static const float3 kMajorColor = float3(0.60, 0.60, 0.64);
static const float3 kAxisXColor = float3(0.86, 0.28, 0.28); // X axis, red
static const float3 kAxisZColor = float3(0.28, 0.46, 0.86); // Z axis, blue

// antialiased coverage of the unit lattice through `coord` (filtered grid:
// the distance to the nearest lattice line measured in pixel units via
// fwidth, so a line is ~1 pixel wide at any zoom)
float grid_coverage(float2 coord)
{
	float2 derivative = fwidth(coord);
	float2 distance_to_line =
		abs(frac(coord - 0.5) - 0.5) / max(derivative, float2(1e-6, 1e-6));

	return 1.0 - min(min(distance_to_line.x, distance_to_line.y), 1.0);
}

// antialiased coverage of the single world line coordinate == 0 (the axis
// lines), same pixel-unit filtering as grid_coverage
float axis_coverage(float coordinate)
{
	float derivative = fwidth(coordinate);

	return 1.0 - min(abs(coordinate) / max(derivative, 1e-6), 1.0);
}

[shader("fragment")]
float4 fs_main(ZirconVsToFs_grid input) : SV_Target
{
	// world ray through the pixel: unproject the NDC point at two depth
	// values — any two distinct z stay on the same view ray whatever the
	// projection's depth convention (OpenGL -1..1 vs D3D/Vulkan 0..1)
	float4 unprojected_near =
		mul(u_invViewProj, float4(input.v_ndc, -1.0, 1.0));
	float4 unprojected_far =
		mul(u_invViewProj, float4(input.v_ndc, 1.0, 1.0));

	float3 point_near = unprojected_near.xyz / unprojected_near.w;
	float3 point_far = unprojected_far.xyz / unprojected_far.w;

	float3 ray_origin = u_cameraPos.xyz;
	float3 ray_direction = normalize(point_far - point_near);

	// the y=0 plane: parallel or behind the camera means no grid pixel
	if (abs(ray_direction.y) < 1e-6)
		discard;

	float ray_length = -ray_origin.y / ray_direction.y;

	if (ray_length <= 0.0)
		discard;

	float3 world = ray_origin + ray_direction * ray_length;

	// quadratic distance fade to the horizon (grazing rays report huge
	// lengths, so the grid dissolves instead of moiré-ing at the horizon)
	float fade = 1.0 - saturate(ray_length / kFadeDistance);
	fade *= fade;

	float cell = grid_coverage(world.xz);
	float major = grid_coverage(world.xz / kMajorLineEvery);
	float axis_x = axis_coverage(world.z); // the X axis is the line z == 0
	float axis_z = axis_coverage(world.x); // the Z axis is the line x == 0

	// layering: cells, then majors over them, then the axes on top
	float3 color = kCellColor;
	float alpha = cell * kCellAlpha;

	color = lerp(color, kMajorColor, major);
	alpha = max(alpha, major * kMajorAlpha);

	color = lerp(color, kAxisXColor, axis_x);
	alpha = max(alpha, axis_x * kAxisAlpha);

	color = lerp(color, kAxisZColor, axis_z);
	alpha = max(alpha, axis_z * kAxisAlpha);

	alpha *= fade;

	if (alpha <= 0.0)
		discard;

	return float4(color, alpha);
}
