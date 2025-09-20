#include "zircon_render_graph_pass_present.h"

namespace no_streaming
{
	zircon_render_graph_pass_present_bgfx::
		zircon_render_graph_pass_present_bgfx(void) :
		zircon_render_graph_pass_bgfx()
	{
	}

	zircon_render_graph_pass_present_bgfx::
		~zircon_render_graph_pass_present_bgfx(void)
	{
	}

	void zircon_render_graph_pass_present_bgfx::OnCreateResources(
		kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
	}

	void zircon_render_graph_pass_present_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
	}

	void zircon_render_graph_pass_present_bgfx::OnRender(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t)
	{
		//	::bgfx::touch(0);
		//	::bgfx::setViewClear(0, BGFX_CLEAR_COLOR);
		//	::bgfx::setViewRect(0, 0, 0, ::bgfx::BackbufferRatio::Equal);
		//	glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
		//	glClear(GL_COLOR_BUFFER_BIT);
	}
} // namespace no_streaming