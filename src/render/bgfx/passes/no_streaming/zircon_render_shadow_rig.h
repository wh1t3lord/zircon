#pragma once

// zircon_render_shadow_rig — the bgfx-side GPU rig of the CSM sun shadows
// (task Z24 B5c): ONE member per game pass that shadows (today only the
// GPU-driven model_static pass — the CPU A/B path keeps the factor-1.0
// contract, see its pass). Owns the cascade depth texture ARRAY (one
// D32F texture of ZIRCON_DEF_SHADOW_CASCADE_COUNT layers — the RT shape
// decision below), one frame buffer per cascade (each binds its layer as
// the depth attachment), the depth-only program (the pass hands the
// created shadow_depth.vs/fs pair in) and the FS cbuffer members the two
// model_static shaders sample (u_shadowCascadeViewProj[4] /
// u_shadowSplitFar / u_shadowMeta / the u_shadowMap sampler). The cascade
// math itself lives in zircon_render_shadow_cascade (bgfx-free, the unit
// tests pin it); this rig is the POD's GPU consumer.
//
// THE RT SHAPE (the documented choice): ONE depth texture ARRAY + N
// frame buffers with per-cascade attachment layers, NOT N separate
// textures. Reasons: (a) the FS samples ONE Texture2DArray with the
// cascade as the array slice — separate textures would need four sampler
// declarations and a per-cascade branch (sm_5_0 has no dynamic sampler
// indexing); (b) the vendored bgfx's d3d11 renderer supports array-slice
// depth attachments natively (FrameBufferD3D11::create fills a
// Texture2DArray DSV from Attachment::layer, and a D32F RT texture is
// created R32_TYPELESS with an R32_FLOAT SRV — renderer_d3d11.cpp's
// format table), verified on the boot's active backend.
//
// THE VIEW ORDERING: the cascade depth views
// (ZIRCON_DEF_SHADOW_FIRST_VIEW_ID + k) must execute BEFORE the game
// graph's views so the map is ready when the FS samples it. The graph
// assigns dense queue-index view ids (0..N), so the rig calls
// bgfx::setViewOrder with a FULL-PERMUTATION order array over [0, 244)
// (the shadow views first, then 0..239 natural) — a partial order array
// breaks the bijection bgfx's inverse remap assumes (positions left
// identity still name the high view ids, double-mapping them). On
// deactivation/destroy the identity order over the touched range is
// restored (the remap is persistent Context state, unlike the per-frame
// view state).
//
// THE FS UNIFORM CONTRACT (the b0/b1 discipline — the members extend the
// EXISTING LightParams cbuffer; offsets mirror
// cmake/zircon_slang_shaders.cmake):
//   u_shadowCascadeViewProj : mat4 x4 at byte  80 (regCount 16)
//   u_shadowSplitFar        : vec4   at byte 336 (the per-cascade far
//                             view-z selection thresholds; lanes past the
//                             active count are FLT_MAX so the fixed 4-lane
//                             FS loop answers "out of range")
//   u_shadowMeta            : vec4   at byte 352 — x = active cascade
//                             count as u32 bits (0 = shadows OFF: the
//                             factor-1.0 path, the pre-B5c output
//                             byte-identical), y = 1/map_size, z = the
//                             depth bias, w = the caster's light-table
//                             index as u32 bits (0xffffffff = none)
//   u_view                  : mat4 predefined at byte 368 (NEVER
//                             createUniform'd — bgfx fills predefined
//                             slots from the view state; the FS computes
//                             the fragment's camera view-z with it)
// The setUniform discipline is the B5b one: the meta is set EVERY frame
// (active or not — bgfx uniforms are per-frame state, the branch key is
// never stale), the cascade VPs/splits only when active (unread then).

#include "zircon_render_shadow_cascade.h"
#include "zircon_render_chunk_pool.h"

// the bgfx handles/state this rig owns (the passes-project
// texture_bcn/light_cluster_rig precedent: the header pulls bgfx itself)
#include <bgfx/bgfx.h>

class zircon_render_shadow_rig
{
public:
	zircon_render_shadow_rig(void);
	~zircon_render_shadow_rig(void) = default;

	// the one-time creation (the pass's OnCreateResources): the depth
	// array + the cascade frame buffers + the FS uniforms. Creation
	// failure is a programmer-level assert exactly like the B1 pass's
	void create(const char* p_log_tag) noexcept;

	// the pass hands the created depth program (shadow_depth.vs/fs,
	// loaded through its own shader-blob path); the rig owns + destroys
	// it from here. An invalid handle keeps the rig shadow-inert (meta
	// stays 0 — the shader cache simply predates the pipeline)
	void set_depth_program(bgfx::ProgramHandle program_depth) noexcept;

	void destroy(void) noexcept;

	// the depth program exists (the textures/frame buffers always do
	// after create)
	bool is_shadow_ready(void) const noexcept;

	// the ACTIVE per-frame drive: renders every cascade's depth (the
	// chunk pool's chunks culled against the cascade VP's frustum through
	// the SAME B1 statics — scope item 3 — one depth-only submit per
	// visible chunk, the v1 shape; a GPU-culled indirect shadow draw is
	// the noted refinement), orders the cascade views before the graph's
	// views, and fills the FS uniforms (cascade VPs, split thresholds,
	// the meta with the caster's light-table index)
	void render_cascades(const zircon_shadow_cascade_set_t& cascades,
		const zircon_render_chunk_pool& pool,
		bgfx::DynamicVertexBufferHandle vertex_pool,
		bgfx::DynamicIndexBufferHandle index_pool,
		kotek::uint32_t caster_light_index) noexcept;

	// the INACTIVE per-frame drive (no caster / not ready / degenerate
	// camera): the meta is zeroed (the factor-1.0 path) and a previously
	// installed view order is restored to identity once
	void set_inactive(void) noexcept;

	// binds the depth array at the fragment stage — call before the
	// pass's draw submits EVERY frame it draws (the always-bound rule:
	// the factor-1.0 branch never samples, but an unbound-but-declared
	// Texture2DArray trips the d3d11 binding validation)
	void bind_for_draw(bgfx::ViewId pass_id) noexcept;

	// ---- test/evidence seams (headless-readable state)
	bool is_active(void) const noexcept;
	const float* get_shadow_meta(void) const noexcept;
	kotek::uint32_t get_caster_light_index(void) const noexcept;

private:
	// installs/restores the view order (the banner's full-permutation
	// contract)
	void install_view_order(void) noexcept;
	void restore_view_order(void) noexcept;

private:
	bgfx::TextureHandle m_depth_array;
	bgfx::FrameBufferHandle
		m_cascade_frame_buffers[ZIRCON_DEF_SHADOW_CASCADE_COUNT];
	bgfx::ProgramHandle m_program_depth;

	// the FS cbuffer members (see the banner; u_view is predefined and
	// NEVER created — the documented trap)
	bgfx::UniformHandle m_uniform_cascade_view_proj;
	bgfx::UniformHandle m_uniform_split_far;
	bgfx::UniformHandle m_uniform_meta;
	bgfx::UniformHandle m_uniform_shadow_map;

	// the full-permutation order array ([shadow views first, then 0..239]
	// over [0, 244)) — a member per the no-statics rule
	bgfx::ViewId m_view_order
		[ZIRCON_DEF_SHADOW_FIRST_VIEW_ID + ZIRCON_DEF_SHADOW_CASCADE_COUNT];

	// the per-cascade caster-cull scratch (the planes of the cascade VP +
	// the pool's visible-id list; used immediately per cascade)
	float m_cascade_planes[24];
	kotek::uint32_t m_visible_ids[zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS];

	// the frame's meta vec4 (the FS contract — u32 bit patterns memcpy'd
	// into x/w)
	float m_shadow_meta[4];

	kotek::uint32_t m_caster_light_index;
	bool m_active;
	bool m_view_order_installed;
	bool m_first_render_logged;
	// the log prefix distinguishing passes (a string literal owned by the
	// pass — never freed; the light rig's discipline)
	const char* m_p_log_tag;
};
