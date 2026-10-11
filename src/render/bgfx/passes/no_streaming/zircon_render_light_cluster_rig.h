#pragma once

// zircon_render_light_cluster_rig — the bgfx-side GPU rig of the clustered
// forward+ lighting (task Z24 B5b): ONE per game pass (member), owning the
// frame's light-table buffers, the binning output buffers, the stats
// readback hop and the binning program (light_cluster_bin.cs.slang — the
// pass hands the created program in; the rig owns it from there). The CPU
// side of the lighting lives in zircon_render_light_buffer (bgfx-free; the
// collection + the classification + the serial binning mirror) — this rig
// is the mirror image of that table on the GPU plus the dispatch driver.
//
// THE PER-FRAME DRIVE (the pass's OnRender, before its draws):
//   drive(pass_id, lights, planes, view_proj, screen w/h):
//     1. the tile grid derives from the backbuffer size (over the tile cap
//        = loud once + the frame falls back);
//     2. a non-empty light table uploads the four float4 streams (bounded:
//        <= 64 lights x 16 B per stream) and the serial mirror runs on the
//        CPU (the A/B stats' cpu side);
//     3. three dispatches (clear -> bin -> stats, the B1 three-mode
//        discipline) + the stats blit into the readback twin;
//     4. the cluster-info cbuffer vec4 (the FS contract: tiles_x / active /
//        tile pixels / per-tile cap as u32 bit patterns) is filled and set;
//   bind_for_draw(pass_id): the six fragment-stage SRVs (stages 0..5) bind
//     EVERY frame the pass draws — the fallback branch never reads them,
//     but the always-bound rule keeps the d3d11 binding validation quiet;
//   the A/B readback: the GPU's assignment/drop counters come back through
//     the 2x1 R32U texture hop (the B1 discipline: compute-write texture +
//     blit + readback twin, one readTexture in flight, the first completed
//     read dropped as pipeline warmup) and are logged against the CPU
//     mirror's — first valid readback + on changes.

#include "zircon_render_light_buffer.h"

// the bgfx handles/state this rig owns (the passes-project texture_bcn
// precedent: the header pulls bgfx itself, never relies on include order)
#include <bgfx/bgfx.h>

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_render_light_cluster_rig
{
public:
	zircon_render_light_cluster_rig(void);
	~zircon_render_light_cluster_rig(void) = default;

	// the one-time creation (the pass's OnCreateResources): the buffers
	// (the streams are 16-byte-element dynamic vertex buffers with the
	// compute formats — the B1 table discipline; the counts/indices/
	// counters are 32-bit index-buffer-backed so d3d11 types the UAV/SRV
	// views R32_UINT) + the cbuffer uniforms. Buffer creation failure is
	// a programmer-level assert exactly like the B1 pass's
	void create(const char* p_log_tag) noexcept;

	// the pass hands the created binning program (loaded through its own
	// shader-blob path); the rig owns + destroys it from here. An invalid
	// handle keeps the rig binning-inert (the frame falls back — the
	// shader cache simply predates the pipeline)
	void set_binning_program(bgfx::ProgramHandle program_bin) noexcept;

	void destroy(void) noexcept;

	// the binning program exists (the buffers always do after create)
	bool is_binning_ready(void) const noexcept;

	// the per-frame drive (see the banner): uploads + dispatches + the
	// cluster-info uniform. p_planes_6x4 / p_view_projection_16 are the
	// frame's frustum planes + view-projection (the pass computes them for
	// its cull/draw already)
	void drive(bgfx::ViewId pass_id, const zircon_render_light_buffer& lights,
		const float* p_planes_6x4, const float* p_view_projection_16,
		kotek::uint32_t screen_width, kotek::uint32_t screen_height) noexcept;

	// binds the six fragment-stage SRVs (stages 0..5, Access::Read) — call
	// before the pass's draw submits EVERY frame it draws
	void bind_for_draw(bgfx::ViewId pass_id) noexcept;

	// ---- test/A-B seams (headless-readable state)
	const float* get_cluster_info(void) const noexcept;
	kotek::uint32_t get_gpu_assignment_count(void) const noexcept;
	kotek::uint32_t get_gpu_dropped_count(void) const noexcept;
	kotek::uint32_t get_cpu_assignment_count(void) const noexcept;
	kotek::uint32_t get_cpu_dropped_count(void) const noexcept;
	kotek::uint32_t get_tiles_x(void) const noexcept;
	kotek::uint32_t get_tiles_y(void) const noexcept;
	bool is_clustered_active(void) const noexcept;

private:
	// the stats readback pump (one readTexture in flight, the first
	// completed read dropped — the B1 hop discipline)
	void pump_stats_readback(void) noexcept;

private:
	bgfx::VertexLayout m_layout_stream;
	bgfx::DynamicVertexBufferHandle m_streams[4]; // positions/directions/colors/params
	bgfx::IndexBufferHandle m_tile_counts;
	bgfx::IndexBufferHandle m_tile_indices;
	bgfx::IndexBufferHandle m_bin_counters; // [0]=assignments [1]=drops
	bgfx::TextureHandle m_stats_texture;
	bgfx::TextureHandle m_stats_readback_texture;
	bgfx::ProgramHandle m_program_bin;

	// the cbuffer members (the binning params + the FS cluster info)
	bgfx::UniformHandle m_uniform_cull_planes;
	bgfx::UniformHandle m_uniform_view_proj;
	bgfx::UniformHandle m_uniform_screen_meta;
	bgfx::UniformHandle m_uniform_bin_meta;
	bgfx::UniformHandle m_uniform_cluster_info;

	// the CPU mirror's output scratch (the A/B's cpu side — heap per the
	// fixture rule: 32 KB of counts + 1 MB of indices at the shipped
	// defines, allocated once at create, freed at destroy)
	kotek::uint32_t* m_mirror_tile_counts;
	kotek::uint32_t* m_mirror_tile_indices;

	// the frame's cluster-info vec4 (the FS contract — u32 bit patterns
	// memcpy'd into the float lanes)
	float m_cluster_info[4];

	// the stats readback state (the B1 discipline)
	kotek::uint32_t m_stats_readback_values[2];
	kotek::uint32_t m_stats_ready_frame;
	bool m_stats_readback_pending;
	bool m_stats_warmup_done;
	kotek::uint32_t m_gpu_assignment_count;
	kotek::uint32_t m_gpu_dropped_count;
	kotek::uint32_t m_cpu_assignment_count;
	kotek::uint32_t m_cpu_dropped_count;
	kotek::uint32_t m_tiles_x;
	kotek::uint32_t m_tiles_y;
	bool m_clustered_active;
	bool m_first_readback_logged;
	// the last logged pair for the rate-limited trace
	kotek::uint32_t m_last_logged_assignments;
	kotek::uint32_t m_last_logged_dropped;
	// the over-tile-cap degradation's one-shot latch
	bool m_is_warned_about_tile_cap;
	// the log prefix distinguishing the two passes' rigs (a string literal
	// owned by the pass — never freed)
	const char* m_p_log_tag;
};
