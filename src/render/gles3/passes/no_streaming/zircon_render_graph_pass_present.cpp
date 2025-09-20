#include "zircon_render_graph_pass_present.h"

namespace no_streaming
{
	zircon_render_graph_pass_present_gles3::
		zircon_render_graph_pass_present_gles3(void) :
		zircon_render_graph_pass()
	{
	}

	zircon_render_graph_pass_present_gles3::
		~zircon_render_graph_pass_present_gles3(void)
	{
	}

	void zircon_render_graph_pass_present_gles3::OnCreateResources(
		kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
	}

	void zircon_render_graph_pass_present_gles3::OnUpdate(
		const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass)
	{
	}

	void zircon_render_graph_pass_present_gles3::OnRender(
		const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass)
	{
		glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
} // namespace no_streaming