#include "zircon_render_graph_pass_editor_present.h"

zircon_render_graph_pass_editor_present_gles3::zircon_render_graph_pass_editor_present_gles3(
	const kotek::static_u8string_view_t& render_pass_name) :
	kotek::render::gl::ktkRenderGraphSimplifiedRenderPass(
		render_pass_name.data())
{
}

zircon_render_graph_pass_editor_present_gles3::zircon_render_graph_pass_editor_present_gles3(
	void)
{
}

zircon_render_graph_pass_editor_present_gles3::~zircon_render_graph_pass_editor_present_gles3(
	void)
{
}

void zircon_render_graph_pass_editor_present_gles3::OnCreateResources(
	kotek::core::ktkMainManager* p_manager_main,
	kotek::core::ktkIRenderResourceManager* p_manager_resource)
{
}

void zircon_render_graph_pass_editor_present_gles3::OnUpdate(
	const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
		p_previous_pass)
{
}

void zircon_render_graph_pass_editor_present_gles3::OnRender(
	const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
		p_previous_pass)
{
	glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}
