#include "zircon_render_graph_pass_present.h"

#include <kotek.render.gl/include/kotek_render_graph_simplified_node.h>

zircon_render_graph_pass_present_gles3::zircon_render_graph_pass_present_gles3(
	void)
{
}

zircon_render_graph_pass_present_gles3::~zircon_render_graph_pass_present_gles3(
	void)
{
}

void zircon_render_graph_pass_present_gles3::OnSetupInput(
	Kotek::Render::gl::ktkRenderGraphSimplifiedStorageInput& storage,
	Kotek::Core::ktkIRenderDevice* p_device,
	Kotek::Core::ktkFileSystem* p_file_system)
{
}

void zircon_render_graph_pass_present_gles3::OnSetupOutput(
	Kotek::Render::gl::ktkRenderGraphSimplifiedStorageOutput& storage,
	Kotek::Core::ktkIRenderDevice* p_device)
{
}

void zircon_render_graph_pass_present_gles3::OnCreatedResources(void) {}

void zircon_render_graph_pass_present_gles3::OnUpdate() {}

void zircon_render_graph_pass_present_gles3::OnRender(
	const Kotek::Render::gl::ktkRenderGraphSimplifiedNode& node)
{
	glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}
