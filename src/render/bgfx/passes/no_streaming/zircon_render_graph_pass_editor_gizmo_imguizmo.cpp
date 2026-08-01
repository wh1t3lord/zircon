#include "zircon_render_graph_pass_editor_gizmo_imguizmo.h"

namespace no_streaming
{
	zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx::
		zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx(void)
	{
	}

	zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx::
		~zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx(void)
	{
	}

	void zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx::
		OnCreateResources(
			kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
		KOTEK_ASSERT(p_manager_main, "must be valid!");

		// manager pointers only — the pass deliberately creates no GPU
		// resources (the header documents why the hooks stay inert)
		this->m_p_manager_main = p_manager_main;
		this->m_p_manager_resource = p_manager_resource;
	}

	void zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx::
		OnDestroyResources()
	{
	}

	void zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass, kotek::ktk::uint32_t my_id_in_queue)
	{
		// inert by design: the ImGuizmo Manipulate runs inside the imgui
		// pass's frame, hosted by zircon_editor_ui_window_gizmo_imguizmo
	}

	void zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx::OnRender(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass, kotek::ktk::uint32_t my_id_in_queue)
	{
		// inert by design (see OnUpdate)
	}
} // namespace no_streaming
