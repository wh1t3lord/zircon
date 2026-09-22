#pragma once

#include "../../zircon_render_graph_pass_editor.h"

#include "zircon_render_csg_editor_pool.h"

// editor pass "editor_csg" (task Z25 A2) capacities — named per the
// memory-budget rule
// upper bound of one compiled shader blob read through the kotek
// filesystem (the pair is a few KB today; generous headroom)
#define zircon_DEF_RENDER_PASS_EDITOR_CSG_SHADER_BIN_MAX_SIZE 16384
// the entity scan of the per-frame pool reconcile (a stale pool
// record whose compound died through a NON-csg command — e.g. the
// generic delete-entity — is freed here)
#define zircon_DEF_RENDER_PASS_EDITOR_CSG_MAX_ENTITY_SCAN_COUNT 256

namespace no_streaming
{
	// the editor CSG compound draw: ONE dynamic vertex pool + ONE
	// dynamic index pool shared by every editor compound (the plan's
	// one draw call), mirrored from zircon_render_csg_editor_pool's
	// CPU shadows through bgfx::update of the CHANGED spans only.
	// The per-frame flow: drain the session's rebuild scheduler
	// (completed evaluations -> the pool), reconcile the pool against
	// the world (dead compounds release their ranges), upload the
	// pending dirty spans, then ONE submit with the shared
	// forward-Phong lighting (the model_static contract — never
	// forked). An empty pool is a no-op. POD/handles only; every bgfx
	// handle is released in OnDestroyResources.
	class zircon_render_graph_pass_editor_csg_bgfx
		: public zircon_render_graph_pass_editor_bgfx
	{
	public:
		zircon_render_graph_pass_editor_csg_bgfx(void);
		~zircon_render_graph_pass_editor_csg_bgfx(void);

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;
		void OnDestroyResources() override;
		void OnUpdate(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;
		void OnRender(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;

	private:
		// reads one compiled shader blob from
		// data_user/shader_cache/bgfx/<active renderer>/ through the
		// kotek filesystem (same route as the grid pass). An absent
		// file is not an error: the handle stays invalid and the pass
		// no-ops
		bgfx::ShaderHandle load_shader_blob(
			const char* p_shader_file_name) noexcept;

		// the editor camera (the sdk_camera component of the editor
		// session's world); false -> the caller falls back to the
		// default orbit (the gizmo pass's resolve pattern)
		bool resolve_editor_camera(
			float* p_out_view, float* p_out_projection) noexcept;

		// drains the session scheduler's completed rebuilds into the
		// pool (the render-thread consumer half of the checkout
		// contract)
		void merge_completed_rebuilds(void) noexcept;

		// frees the pool records whose compound left the world
		void reconcile_pool_with_world(void) noexcept;

		// uploads the pool's pending dirty spans to the dynamic bgfx
		// buffers
		void upload_dirty_spans(void) noexcept;

	private:
		zircon_render_csg_editor_pool m_pool;
		bgfx::VertexLayout m_layout;
		bgfx::DynamicVertexBufferHandle m_vertex_buffer;
		bgfx::DynamicIndexBufferHandle m_index_buffer;
		bgfx::ProgramHandle m_program;
		// the LightParams cbuffer members of the fragment stage (the
		// model_static contract — the fixed scene light until light
		// components land)
		bgfx::UniformHandle m_uniform_light_dir;
		bgfx::UniformHandle m_uniform_light_color;
		bgfx::UniformHandle m_uniform_ambient;
		bgfx::UniformHandle m_uniform_camera_pos;
		// the scalar -> float conversion scratch of a merge (the
		// scheduler's view aliases ITS storage — the converted copy
		// must not)
		kotek::static_vector_t<
			float,
			ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION * 3>
			m_position_scratch;
		kotek::static_vector_t<
			float,
			ZIRCON_DEF_CSG_MAX_TRIANGLES_PER_EVALUATION * 3>
			m_normal_scratch;
		// the pass is inert (renders nothing) until its compiled
		// program exists — tracks the one-time warning so the log is
		// not spammed
		bool m_is_warned_about_missing_program;
		// the one-time first-draw evidence (compound/index counts)
		bool m_is_first_draw_logged;
		// the last submitted index count for the rate-limited trace
		kotek::uint32_t m_last_submitted_index_count;
	};
} // namespace no_streaming
