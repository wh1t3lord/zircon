#pragma once

// zircon_render_light_buffer — the per-frame light collection of the
// clustered forward+ lighting (task Z24 B5b) and the CPU mirror of the
// binning compute. bgfx-FREE on purpose (the chunk pool's discipline):
// the class owns the AoSoA light table and the pure classification /
// evaluation / serial-binning statics; the passes' GPU rig
// (zircon_render_light_cluster_rig) mirrors the table into bgfx buffers
// and drives the binning dispatch (light_cluster_bin.cs.slang).
//
// THE DATA FLOW (per frame, per game pass):
//   collect_from_world() scans the world's ENABLED zircon_component_light
//   instances (the transform feeds position/direction per the component's
//   header contract) into the AoSoA table below -> the rig uploads the
//   four float4 streams -> the binning compute assigns each light to the
//   screen-space tiles its volume touches (the classification below,
//   mirrored one-to-one by the shader) writing per-tile index lists with
//   the B1 counter+compaction discipline -> the shared fragment path
//   (zircon_core.slang) iterates its tile's list. A scene with ZERO
//   enabled lights never touches any of this on the GPU: the pass leaves
//   the clustered branch off and the fragment runs the pre-B5b fallback
//   (the named-define directional) unchanged.
//
// THE AoSoA TABLE (rule §2.9): four float4 streams (the GPU upload is
// byte-exact against them) + the narrow u8 type/flag records. The lanes:
//   positions[i]  = (world_pos.x, world_pos.y, world_pos.z, range) —
//                   range rides the spare lane (the chunk-pool
//                   discipline); 0/unused for directionals;
//   directions[i] = (dir_from.x, dir_from.y, dir_from.z, 0) — the
//                   normalized direction the light TRAVELS (directional/
//                   spot), zero for points;
//   colors[i]     = (color.r * intensity, color.g * intensity,
//                   color.b * intensity, 0) — the premultiplied rgb so
//                   the fragment loop never re-applies the intensity;
//   params[i]     = (cos(inner), cos(outer), type-bits, 0) — the spot
//                   cone cosines (computed once here, never per fragment)
//                   and the eZirconLightType value carried as the u32 BIT
//                   PATTERN of the z lane (the B1 meta discipline: memcpy
//                   in/out, asuint on the shader side);
//   types[i]/flags[i] = the narrow CPU-side semantic records (the serial
//                   binning mirror reads types[]; flags bit 0 is the
//                   RESERVED shadow-caster pre-wire for B5c, always 0
//                   this phase — same posture as the chunk pool's
//                   material-id lane in B1).

#include "../../../../ecs/zircon_component_light.h"

class zircon_factory;
struct zircon_ecs_context_t;

// the live-light capacity of one frame's table. Sized: 64 lights x 64 B
// of streams = 4 KB of per-frame upload (trivially bounded), and a
// Dark-Messiah-scale scene's visible lights stay far below it (the
// 32 px tiles keep per-tile lists short regardless). Overflow is loud
// (one warning per over-cap collect, never a fault) and the first 64
// collect
#define ZIRCON_DEF_LIGHT_MAX_COUNT 64
// the screen-space tile edge in pixels (the clustered-forward classic
// size); the tile grid derives at runtime from the backbuffer size. This
// define is the SINGLE source of the tile size — the shaders read it
// through the cbuffers (never a shader-side constant to drift)
#define ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS 32
// the tile-grid capacity: 3840x2160 (4K) needs 120x68 = 8160 tiles;
// 8192 covers it with headroom. A backbuffer needing MORE tiles disables
// the clustered path for the frame (loud once, the fallback directional
// answers) — the buffers stay bounded
#define ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES 8192
// the per-tile list capacity. Worst case by construction: every
// directional lands in EVERY tile, so this must hold the plausible
// directional+local mix of a scene — 32 of the 64-light table. A tile
// overflowing drops the remaining assignments into the dropped counter
// (the stats readback reports it — loud, never silent)
#define ZIRCON_DEF_LIGHT_CLUSTER_MAX_LIGHTS_PER_TILE 32
// the entity scan cap of the per-frame collection (the gpu-driven pass's
// chunk collect uses the same bound)
#define ZIRCON_DEF_LIGHT_MAX_ENTITY_SCAN 1024
// the collection's range clamp: a point/spot light's range never goes
// below this so the shaders' 1/range division is finite (documented on
// the component: range <= 0 collects clamped, never a fault)
#define ZIRCON_DEF_LIGHT_MIN_RANGE 0.001f

// one float4 stream record (the lanes' semantics are documented per
// array above); byte-exact with the GPU upload
struct zircon_light_stream_record_t
{
	float m_v[4];
};

// the serial binning's output counters (the GPU's two atomics mirror
// them; the pass's A/B readback compares)
struct zircon_light_bin_stats_t
{
	kotek::uint32_t m_assignment_count; // tile-light pairs written
	kotek::uint32_t m_dropped_count;    // per-tile cap overflows
	kotek::uint32_t m_culled_count;     // lights outside the frustum
};

class zircon_render_light_buffer
{
public:
	zircon_render_light_buffer(void);
	~zircon_render_light_buffer(void) = default;

	// the per-frame world collection: every live entity with an ENABLED
	// zircon_component_light lands in the table (the transform contract is
	// the component header's; a missing/disabled transform is the
	// identity). Returns the collected count (<= ZIRCON_DEF_LIGHT_MAX_COUNT;
	// the overflow is one warning per over-cap call — a member flag, never
	// log spam). 0 = the fallback-directional frame
	kotek::uint32_t collect_from_world(zircon_factory* p_factory,
		zircon_ecs_context_t* p_context,
		kotek::uint32_t entity_count_max_limit) noexcept;

	// appends one light to the table with typed values (the collection's
	// per-component packing, shared so the pass's boot fixture and the
	// tests drive the SAME stream-packing the world collection uses):
	// p_position_xyz feeds point/spot (ignored for directional),
	// p_dir_from_xyz is the normalized travel direction (directional/spot;
	// nullptr = the identity forward (0,0,-1)), color x intensity
	// premultiplies into the color stream, the spot angles are DEGREES
	// clamped per the component's contract (inner <= outer, [0, 179]).
	// false = the table is full (the overflow latch is the collection's)
	bool append_light(eZirconLightType type, const float* p_position_xyz,
		const float* p_dir_from_xyz, const float* p_color_rgb,
		float intensity, float range, float spot_inner_degrees,
		float spot_outer_degrees) noexcept;

	void clear(void) noexcept;

	// ---- read-only views of the table (the rig uploads them byte-exact)
	kotek::uint32_t get_count(void) const noexcept;
	const zircon_light_stream_record_t* get_positions(void) const noexcept;
	const zircon_light_stream_record_t* get_directions(void) const noexcept;
	const zircon_light_stream_record_t* get_colors(void) const noexcept;
	const zircon_light_stream_record_t* get_params(void) const noexcept;
	const kotek::uint8_t* get_types(void) const noexcept;
	const kotek::uint8_t* get_flags(void) const noexcept;

	// ---- the tile grid (the runtime derivation; false = over the tile
	// cap, the caller disables the clustered path for the frame)
	static bool derive_tile_grid(kotek::uint32_t screen_width,
		kotek::uint32_t screen_height, kotek::uint32_t tile_pixels,
		kotek::uint32_t& out_tiles_x, kotek::uint32_t& out_tiles_y) noexcept;

	// ---- the classification statics, mirrored one-to-one by
	// ---- light_cluster_bin.cs.slang (the unit tests pin them — keep the
	// ---- two in sync)

	// sphere-vs-six-planes (the chunk pool's plane convention: dot(n,p)+d
	// >= 0 inside): outside iff fully behind any plane
	static bool sphere_vs_frustum(const float* p_planes_6x4,
		const float* p_center_xyz, float radius) noexcept;

	// the spot cone's conservative bounding sphere (the v1 spot
	// classification): the TIGHTER of the two valid bounds wins — (a) the
	// mid-cone sphere (center = apex + axis*range/2, radius =
	// sqrt((range/2)^2 + (range*tan(outer))^2) — tight for narrow cones,
	// used only below a 90-degree outer angle: past the hemisphere the tan
	// formula is meaningless) and (b) the apex sphere (center = apex,
	// radius = range — universal by construction: every cone point sits
	// within range of the apex, any outer angle). cos_outer is the cone's
	// outer-edge cosine
	static void spot_bounding_sphere(const float* p_apex_xyz,
		const float* p_dir_from_xyz, float range, float cos_outer,
		float* p_out_center_xyz, float& out_radius) noexcept;

	// the sphere's conservative screen-tile range: the sphere's world AABB
	// corners project through the view-projection (clip[j] =
	// dot((x,y,z,1), (vp[j], vp[4+j], vp[8+j], vp[12+j])) — the
	// bx-byte-layout transform the shaders' column_major mul() computes,
	// the B1 plane-extraction convention), the pixel bounds of the
	// projections (ndc.x in [-1,1] -> px = (ndc.x*0.5+0.5)*width, ndc +y up
	// -> pixel rows DOWN: py = (0.5-ndc.y*0.5)*height) clamp to the grid.
	// A corner with w <= 0 (the sphere crosses the camera plane) widens to
	// the FULL grid — conservative, never a miss. Returns false only on a
	// caller error (null args, zero tiles/screen)
	static bool project_sphere_to_tile_range(const float* p_view_projection,
		const float* p_center_xyz, float radius, kotek::uint32_t screen_width,
		kotek::uint32_t screen_height, kotek::uint32_t tile_pixels,
		kotek::uint32_t tiles_x, kotek::uint32_t tiles_y,
		kotek::uint32_t& out_tile_min_x, kotek::uint32_t& out_tile_min_y,
		kotek::uint32_t& out_tile_max_x, kotek::uint32_t& out_tile_max_y) noexcept;

	// the serial mirror of the binning compute (the tests drive it, the
	// pass's CPU-side A/B stats come from it): per light — the frustum
	// reject (sphere tests; directionals never cull), the tile range (the
	// classification above; directionals take the whole grid), then the
	// append: indices[tile * max_per_tile + counts[tile]++] = light while
	// the tile's list has room, else the dropped counter. The output
	// arrays are caller-owned and sized tiles*tiles and
	// tiles*tiles*max_per_tile; the counts are NOT cleared inside (the
	// caller clears — the compute's mode-0 clear mirrors that split).
	// THE COUNT GROWS PAST THE CAP (mirroring the shader's unconditional
	// InterlockedAdd — the only race-free form): the write happens while
	// the pre-increment slot is in range, else the dropped counter; the
	// fragment side clamps min(count, cap) when it reads the list
	static void bin_lights(const zircon_light_stream_record_t* p_positions,
		const zircon_light_stream_record_t* p_directions,
		const zircon_light_stream_record_t* p_params,
		kotek::uint32_t light_count, const float* p_planes_6x4,
		const float* p_view_projection, kotek::uint32_t screen_width,
		kotek::uint32_t screen_height, kotek::uint32_t tile_pixels,
		kotek::uint32_t tiles_x, kotek::uint32_t tiles_y,
		kotek::uint32_t max_lights_per_tile, kotek::uint32_t* p_out_tile_counts,
		kotek::uint32_t* p_out_tile_indices,
		zircon_light_bin_stats_t& out_stats) noexcept;

	// ---- the fragment evaluation mirrors, one-to-one with
	// ---- zircon_core.slang's B5b evaluators (the tests pin them; the
	// ---- clamping discipline: saturate(ndotl/attenuation/cone) per light,
	// ---- the caller saturates ambient + the sum)

	// the directional list light: Lambert only (diffuse per light — the
	// fallback path's specular stays the fallback's alone this phase)
	static void evaluate_directional_diffuse(const float* p_normal_xyz,
		const float* p_light_dir_from_xyz, const float* p_color_rgb,
		float* p_out_rgb) noexcept;

	// the point light: Lambert x the linear-saturated range window
	// (1 - dist/range); a degenerate overlap (dist ~ 0) contributes 0 (the
	// direction is singular there — documented, deterministic)
	static void evaluate_point_diffuse(const float* p_normal_xyz,
		const float* p_world_pos_xyz, const float* p_light_pos_xyz,
		const float* p_color_rgb, float range, float* p_out_rgb) noexcept;

	// the spot light: the point term x the cone window saturate(
	// (cos_angle - cos_outer) / max(cos_inner - cos_outer, 1e-5)) where
	// cos_angle = dot(direction apex->fragment, cone axis)
	static void evaluate_spot_diffuse(const float* p_normal_xyz,
		const float* p_world_pos_xyz, const float* p_light_pos_xyz,
		const float* p_light_dir_from_xyz, const float* p_color_rgb,
		float range, float cos_inner, float cos_outer,
		float* p_out_rgb) noexcept;

private:
	kotek::static_vector_t<zircon_light_stream_record_t,
		ZIRCON_DEF_LIGHT_MAX_COUNT>
		m_positions;
	kotek::static_vector_t<zircon_light_stream_record_t,
		ZIRCON_DEF_LIGHT_MAX_COUNT>
		m_directions;
	kotek::static_vector_t<zircon_light_stream_record_t,
		ZIRCON_DEF_LIGHT_MAX_COUNT>
		m_colors;
	kotek::static_vector_t<zircon_light_stream_record_t,
		ZIRCON_DEF_LIGHT_MAX_COUNT>
		m_params;
	kotek::static_vector_t<kotek::uint8_t, ZIRCON_DEF_LIGHT_MAX_COUNT>
		m_types;
	kotek::static_vector_t<kotek::uint8_t, ZIRCON_DEF_LIGHT_MAX_COUNT>
		m_flags;

	// the collection's entity scan scratch (a member per the no-statics
	// rule; 8 KB at the shipped define)
	kotek::entity_t m_entity_scan[ZIRCON_DEF_LIGHT_MAX_ENTITY_SCAN];

	// the overflow warning's one-shot latch (the collect runs per frame —
	// the warning fires once until a frame fits again)
	bool m_is_warned_about_overflow;
	// the unknown-type degradation's latch (same per-frame discipline)
	bool m_is_warned_about_type;
};

// the GPU/CPU layout contract (the shaders read the streams byte-exact)
static_assert(sizeof(zircon_light_stream_record_t) == 16,
	"the light stream record is one float4");
