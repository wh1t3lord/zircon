#pragma once

// zircon_render_shadow_cascade — the cascade math of the CSM sun shadows
// (task Z24 B5c, the "shadow CSM for the sun" of the approved renderer
// plan; spot/point shadow atlases are the explicitly deferred B5c+ phase).
// bgfx-FREE on purpose (the chunk pool / light buffer discipline): the
// class owns ONLY pure statics — the split scheme, the light-space ortho
// fit with texel snapping, the coordinate transforms, the cascade
// selection and the PCF kernel's CPU mirror — plus the POD the pass fills
// per frame. The GPU rig (zircon_render_shadow_rig) consumes the POD: it
// renders the chunk pool depth-only into the cascade layers and feeds the
// FS cbuffer. The unit tests pin every step headlessly
// (zircon_unit_tests_shadow.cpp); the FS mirrors the selection + the PCF
// kernel (zircon_core.slang / the two model_static* FS shaders) — keep
// all three in sync.
//
// THE MATRIX CONVENTION (the house one — the light buffer's clip comment,
// the B1 plane extraction, bx::mtxMul): the 16 floats are ROW-MAJOR with
// row-vector-on-the-left math — transform(v)_c = sum_i v_i * m[i*4+c],
// translation at [12..14], multiply(out, a, b) applies a first
// (out[r*4+c] = sum_j a[r*4+j] * b[j*4+c], the bx::mtxMul formula). This
// is exactly what the shaders' column_major mul(m, v) computes over the
// same bytes and what bgfx's predefined u_viewProj receives, so the depth
// pass's rasterization and the FS's sampling agree bit-for-bit.
//
// THE PIPELINE (per frame, per shadowed frame):
//   compute_cascades(camera view+proj, the caster's direction):
//     1. near/far extract from the perspective projection (a
//        non-perspective/unsupported projection answers false — shadows
//        off, loud once at the caller);
//     2. the practical split scheme (log/uniform blend, the lambda
//        define) -> the N+1 split distances in view-space z;
//     3. the camera frustum corners via the inverse view-projection, the
//        slice corners by the corner-ray interpolation at each split;
//     4. the light view — ROTATION-ONLY (zero translation): light-space
//        coordinates of a world point are then camera-independent, which
//        is what makes the texel snap below a genuine stabilization (the
//        quantized grid is world-anchored across camera motion);
//     5. per cascade: the 8 slice corners -> light space -> min/max ->
//        the tight ortho (the standard CSM fit), the near side extended
//        by the caster margin (casters between the sun and the slice),
//        x/y SNAPPED to whole texels (origin quantized to the texel grid
//        so the map doesn't shimmer) -> the cascade's view/proj/VP.
//   the rig renders the per-cascade depth (the chunk pool culled against
//     the cascade VP's frustum — the SAME B1 statics);
//   the FS: view-z -> select_cascade -> the cascade's VP -> uv/depth ->
//     the 3x3 PCF mirror -> the factor scales the directional's diffuse.
//     No caster / shadow rig inert / fragment past the last cascade /
//     outside the map => factor 1.0 (the pre-B5c output byte-identical).

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>

// the cascade count. Sized: the open-world standard is 4 (Unity/UE ship 4
// by default); 3 leaves the far cascade visibly coarse, >4 multiplies the
// depth draws for no measured need at the v1 shadow distance. The FS
// cbuffer's cascade array size mirrors this (kZirconShadowCascadeCount in
// zircon_core.slang — keep the two in sync)
#define ZIRCON_DEF_SHADOW_CASCADE_COUNT 4
// the per-cascade map edge in texels (square maps). Sized: 4 layers x
// 2048x2048 x 4 B (D32F) = 64 MiB of GPU depth; 2048 is the CSM quality
// floor (the coarsest v1 cascade spans ~150 m of world diagonal -> ~7
// cm/texel). Halve to 1024 for the shrinkable configuration (16 MiB) —
// the math is size-parametric
#define ZIRCON_DEF_SHADOW_MAP_SIZE 2048
// the practical split scheme's log/uniform blend (0 = uniform, 1 = log;
// 0.5 is the literature default — log alone over-resolves the nearest
// cascade, uniform alone starves it)
#define ZIRCON_DEF_SHADOW_CASCADE_SPLIT_LAMBDA 0.5f
// the ortho's near-side extension in meters (light-space z before the
// slice's nearest point): casters sit between the sun and the slice, so
// the depth range must reach BACK from the slice. Sized: covers
// architecture-scale occluders above the visible slice (Dark-Messiah
// buildings are ~20-40 m); a caster beyond the margin is clipped at the
// ortho's near plane and does not cast (the v1 bound — the proper
// receiver-driven z-range tightening is the noted refinement)
#define ZIRCON_DEF_SHADOW_CASTER_DEPTH_MARGIN 50.0f
// the constant receiver-side depth bias in [0,1] depth units (lit iff
// receiver - bias <= stored). Sized: the coarsest cascade's z-range is
// ~150 m over a 2048 map -> a 45-degree slope shifts depth ~7 cm/texel ~
// 5e-4; 0.0015 covers ~3 texels of slope. The slope-scaled bias is the
// recorded refinement (acne on grazing angles)
#define ZIRCON_DEF_SHADOW_DEPTH_BIAS 0.0015f
// the bgfx view ids of the cascade depth views (one per cascade,
// ascending). Sized: BGFX_CONFIG_MAX_VIEWS is 256 and the render graphs
// use dense low queue ids (editor ~8, game ~3), the editor imgui backend
// sits at 255 — 240..243 are provably free. The rig orders them BEFORE
// the graph's views via bgfx::setViewOrder with a full-permutation order
// array (the remap must stay bijective — a partial order array corrupts
// the inverse remap; see the rig's cpp)
#define ZIRCON_DEF_SHADOW_FIRST_VIEW_ID 240

// the fragment-stage texture stage of the shadow map array (stages 0..5
// are the B5b light table + tile lists — the shaders' register(t6)/s6
// declarations and the cmake storage entry mirror this). Shared by the
// shadow rig (the real array) and the CPU model_static pass (the inert
// dummy)
#define ZIRCON_DEF_SHADOW_FS_TEXTURE_STAGE 6

// the per-frame cascade set (the POD the pass fills and the rig consumes):
// per cascade the light view, the snapped ortho and their product (the FS
// contract); the selection thresholds are the splits' far boundaries in
// camera view-space z
struct zircon_shadow_cascade_set_t
{
	float m_cascade_view[ZIRCON_DEF_SHADOW_CASCADE_COUNT][16];
	float m_cascade_proj[ZIRCON_DEF_SHADOW_CASCADE_COUNT][16];
	float m_cascade_view_proj[ZIRCON_DEF_SHADOW_CASCADE_COUNT][16];
	// selection thresholds: cascade i covers (split_far[i-1], split_far[i]]
	// of the camera's view-space z (cascade 0 starts at the camera near)
	float m_split_far[ZIRCON_DEF_SHADOW_CASCADE_COUNT];
	kotek::uint8_t m_cascade_count;
};

class zircon_render_shadow_cascade
{
public:
	// ---- the small matrix kit (the convention above; mirrored nowhere
	// ---- else — the chunk pool / light buffer never needed builders)

	// out = a x b (a applies first — the bx::mtxMul formula, re-derived so
	// this module stays bgfx-free; pinned against known products in the
	// tests)
	static void multiply_4x4(const float* p_a_16, const float* p_b_16,
		float* p_out_16) noexcept;

	// clip = v x m (out receives 4 floats, NO perspective divide)
	static void transform_point_raw(const float* p_m_16,
		const float* p_point_xyzw, float* p_out_xyzw) noexcept;

	// the general 4x4 inverse (adjugate over the plain row-major matrix —
	// convention-free algebra); false + zeroed output on a singular matrix
	// (the camera path validates before calling, this is the belt)
	static bool invert_4x4(const float* p_m_16, float* p_out_16) noexcept;

	// ---- the split scheme (the practical log/uniform blend):
	// split[0] = near, split[count] = far, interior i = lambda * (n *
	// (f/n)^(i/count)) + (1 - lambda) * (n + (f - n) * i/count)
	static void compute_split_distances(float near_z, float far_z,
		kotek::uint8_t cascade_count, float lambda,
		float* p_out_splits /* count + 1 */) noexcept;

	// extracts near/far from a standard perspective projection with [0,1]
	// depth (d = m[10] = f/(f-n) > 1, e = m[14] = -n*f/(f-n) < 0, the w
	// producer m[11] = 1, m[15] = 0). false = not that projection (ortho,
	// infinite/reverse-z, degenerate) — the caller disables shadows for
	// the frame (loud once), never a fault
	static bool extract_perspective_near_far(const float* p_projection_16,
		float& out_near_z, float& out_far_z) noexcept;

	// the camera frame from a view matrix: the basis vectors are the
	// strided columns (right = (m[0], m[4], m[8]) etc.) and the last row
	// holds -dot(eye, basis) (the pass's proven camera_position
	// extraction); eye = -(m12*right + m13*up + m14*forward)
	static void extract_camera_frame(const float* p_view_16,
		float* p_out_eye_xyz, float* p_out_forward_xyz) noexcept;

	// the light's ROTATION-ONLY view matrix (see the banner): forward =
	// the normalized light travel direction, right = normalize(cross(
	// up_hint, forward)), up = cross(forward, right); the up hint is
	// (0,1,0), falling back to (0,0,1) for a near-vertical light. false =
	// a degenerate (zero) direction
	static bool build_light_view(const float* p_light_dir_from_xyz,
		float* p_out_view_16) noexcept;

	// the ortho projection, row-vector convention, [0,1] depth: x/y map
	// [min,max] -> [-1,1], z maps [min,max] -> [0,1] (bgfx's
	// homogeneousDepth=true shape, re-derived bgfx-free)
	static void build_ortho_projection(float min_x, float max_x,
		float min_y, float max_y, float min_z, float max_z,
		float* p_out_projection_16) noexcept;

	// the texel-grid snap (the shimmer stabilization): the x/y window is
	// expanded OUTWARD to whole-texel multiples of the current texel size
	// (min floors, max ceils) — the light view is rotation-only, so the
	// quantized grid is world-anchored and the map stops crawling. A
	// degenerate (empty) axis is widened to one texel first
	static void snap_ortho_to_texels(float& io_min_x, float& io_max_x,
		float& io_min_y, float& io_max_y,
		kotek::uint32_t shadow_map_size) noexcept;

	// the camera frustum's 8 world corners via the inverse
	// view-projection: entries 0..3 are the near-plane corners at ndc
	// (x,y) = (-1,-1), (1,-1), (1,1), (-1,1) (ndc z = 0), entries 4..7
	// the same at ndc z = 1 (the [0,1] depth convention). false = the
	// view-projection is singular
	static bool compute_frustum_corners(const float* p_camera_view_16,
		const float* p_camera_proj_16,
		float* p_out_corners_8x3) noexcept;

	// the one-call driver (the banner's pipeline): fills the cascade set
	// for the frame. false on a degenerate input (null args, zero light
	// direction, non-perspective projection, singular view-projection,
	// cascade_count over the define) — the caller keeps shadows off for
	// the frame. The map size / lambda / margin are parameters so the
	// tests drive the SAME code with their own constants
	static bool compute_cascades(const float* p_camera_view_16,
		const float* p_camera_proj_16, const float* p_light_dir_from_xyz,
		kotek::uint32_t shadow_map_size, kotek::uint8_t cascade_count,
		float split_lambda, float caster_depth_margin,
		zircon_shadow_cascade_set_t& out_set) noexcept;

	// ---- the FS-side mirrors (zircon_core.slang's B5c helpers — keep
	// ---- the two in sync; the tests pin these)

	// the cascade of a fragment by its camera view-space z: the first
	// cascade whose far boundary is >= view_z; cascade_count = the
	// fragment is past the last cascade (the FS answers factor 1.0)
	static kotek::uint8_t select_cascade(float view_z,
		const float* p_split_far_per_cascade,
		kotek::uint8_t cascade_count) noexcept;

	// world -> the cascade's light-space texel + depth: clip through the
	// cascade VP, the perspective divide, ndc -> uv (x: ndc*0.5+0.5; y:
	// the pixel-rows-down flip 0.5-ndc*0.5 — the light buffer's
	// projection mapping, consistent with the RT's row 0 at ndc y=+1),
	// uv * map_size = texel space, depth = ndc z. false = the point is on
	// or behind the light's eye plane (w <= 0 — the caller lights it)
	static bool world_to_light_texel(const float* p_cascade_view_proj_16,
		const float* p_world_xyz, kotek::uint32_t shadow_map_size,
		float* p_out_texel_xy, float& out_depth) noexcept;

	// one tap's compare: lit iff receiver - bias <= stored (the [0,1]
	// depth convention, LESS test, clear value 1.0)
	static float compare_shadow_depth(float stored_depth,
		float receiver_depth, float bias) noexcept;

	// the 3x3 PCF kernel's CPU mirror over a caller-owned depth layer
	// (map_size x map_size, row-major — texel (x,y) at
	// p_depth_layer[y * map_size + x]): the 9 taps sit at
	// floor(texel) + {-1,0,1}^2 clamped to the map edge, each compared
	// through compare_shadow_depth, averaged (the FS's exact loop)
	static float sample_shadow_pcf_3x3(const float* p_depth_layer,
		kotek::uint32_t shadow_map_size, float texel_x, float texel_y,
		float receiver_depth, float bias) noexcept;
};
