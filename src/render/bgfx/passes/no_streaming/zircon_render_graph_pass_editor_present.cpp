#include "zircon_render_graph_pass_editor_present.h"

namespace no_streaming
{
	zircon_render_graph_pass_editor_present_bgfx::
		zircon_render_graph_pass_editor_present_bgfx(void)
	{
	}

	zircon_render_graph_pass_editor_present_bgfx::
		~zircon_render_graph_pass_editor_present_bgfx(void)
	{
	}

	void zircon_render_graph_pass_editor_present_bgfx::OnCreateResources(
		kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
	}

	void zircon_render_graph_pass_editor_present_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
	}

	void zircon_render_graph_pass_editor_present_bgfx::OnRender(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		bgfx::ViewId pass_id = static_cast<bgfx::ViewId>(my_id_in_queue);

		bgfx::touch(pass_id);
		bgfx::setViewClear(pass_id, BGFX_CLEAR_COLOR);
		bgfx::setViewRect(pass_id, 0, 0, bgfx::BackbufferRatio::Equal);
	}
} // namespace no_streaming
