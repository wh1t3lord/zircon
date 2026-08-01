#pragma once

#include "../../zircon_render_graph_pass.h"

class zircon_factory;
struct zircon_ecs_context_t;

// game pass "model_static" (task Z3 P2b) capacities — named per the
// memory-budget rule; a level's static draws stay far below this
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT 128
// upper bound of one compiled shader blob read through the kotek
// filesystem (the unlit pair is <1 KB today; headroom for the Phong
// follow-up)
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_SHADER_BIN_MAX_SIZE 65536

// interleaved vertex of the pass's fallback mesh (position + packed ABGR
// color, matches the vertex layout created in OnCreateResources and the
// varying.def.sc of the cooked shaders)
struct zircon_model_static_vertex_t
{
	float m_position[3];
	kotek::uint32_t m_color_abgr;
};

// one static-model submission: the entity's model matrix; the mesh itself
// is the pass's fallback cube until the glTF-lite loader lands (P2c)
struct zircon_render_pass_model_static_draw_item_t
{
	float m_model_matrix[16];
};

namespace no_streaming
{
	class zircon_render_graph_pass_model_static_bgfx
		: public zircon_render_graph_pass_bgfx
	{
	public:
		// the fallback cube fixture: 24 vertices (6 faces x 4, per-face
		// ABGR color) and 36 indices (12 triangles); doubles as the
		// unit-test fixture and the editor's future "add primitive" source
		static constexpr kotek::uint8_t kCubeVertexCount = 24;
		static constexpr kotek::uint8_t kCubeIndexCount = 36;

		zircon_render_graph_pass_model_static_bgfx(void);
		~zircon_render_graph_pass_model_static_bgfx(void);

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;
		void OnDestroyResources() override;
		void OnUpdate(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass,
			kotek::ktk::uint32_t my_id_in_queue) override;
		void OnRender(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass,
			kotek::ktk::uint32_t my_id_in_queue) override;

		// emits the fallback cube into caller storage (capacities are the
		// kCube* constants above)
		static void build_cube_mesh(
			zircon_model_static_vertex_t* p_vertices,
			kotek::uint16_t* p_indices) noexcept;

		// gathers the frame's static-model draws: every live entity with
		// an enabled+visible zircon_component_geometry of type kBox and an
		// enabled zircon_component_transform; writes at most
		// out_items_capacity items and returns the amount written (0 = the
		// empty-world no-op)
		static kotek::uint32_t collect_draw_items(
			zircon_factory* p_factory,
			zircon_ecs_context_t* p_context,
			kotek::uint32_t entity_count_max_limit,
			zircon_render_pass_model_static_draw_item_t* p_out_items,
			kotek::uint32_t out_items_capacity) noexcept;

	private:
		// reads one compiled shader blob from
		// data_user/shader_cache/<backend>/<name> through the kotek
		// filesystem (the blob production pipeline — Slang — is spiked
		// separately; this pass only consumes the results by name). An
		// absent file is not an error: the handle stays invalid and the
		// pass no-ops
		bgfx::ShaderHandle load_shader_blob(
			const char* p_shader_file_name) noexcept;

	private:
		bgfx::VertexLayout m_layout;
		bgfx::VertexBufferHandle m_vertex_buffer;
		bgfx::IndexBufferHandle m_index_buffer;
		bgfx::ProgramHandle m_program;
		// the pass is inert (renders nothing) until its compiled program
		// exists (e.g. the shader pipeline has not produced the blobs
		// yet) — tracks the one-time warning so the log is not spammed
		bool m_is_warned_about_missing_program;
		// last submitted draw count for the rate-limited trace (the boot
		// log shows submission counts without per-frame spam)
		kotek::uint32_t m_last_submitted_draw_count;
		zircon_render_pass_model_static_draw_item_t m_draw_items
			[zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT];
	};
} // namespace no_streaming
