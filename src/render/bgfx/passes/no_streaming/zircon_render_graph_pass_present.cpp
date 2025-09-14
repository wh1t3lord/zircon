#include "zircon_render_graph_pass_present.h"

namespace no_streaming
{
	zircon_render_graph_pass_present_bgfx::
		zircon_render_graph_pass_present_bgfx(
			const kotek::static_u8string_view_t& render_pass_name) :
		zircon_render_graph_pass(render_pass_name.data())
	{
	}

	zircon_render_graph_pass_present_bgfx::
		zircon_render_graph_pass_present_bgfx(void) :
		zircon_render_graph_pass()
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
		const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass)
	{
	}

	void zircon_render_graph_pass_present_bgfx::OnRender(
		const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass)
	{
		glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
} // namespace no_streaming