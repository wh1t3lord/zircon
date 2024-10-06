#include "zircon_render_graph_pass_ui.h"

zircon_render_graph_pass_ui_gles3::zircon_render_graph_pass_ui_gles3(
	Kotek::Core::ktkMainManager* p_main_manager) :
	m_p_main_manager{p_main_manager}
{
	KOTEK_ASSERT(p_main_manager->Get_GameUIEngine(),
		"you must initialize game ui before using this class");

	this->m_p_ui_manager = p_main_manager->Get_GameUIEngine();
}

zircon_render_graph_pass_ui_gles3::~zircon_render_graph_pass_ui_gles3() {}

void zircon_render_graph_pass_ui_gles3::OnSetupInput(
	Kotek::Render::gl::ktkRenderGraphSimplifiedStorageInput& storage,
	Kotek::Core::ktkIRenderDevice* p_device,
	Kotek::Core::ktkFileSystem* p_file_system)
{
}

void zircon_render_graph_pass_ui_gles3::OnSetupOutput(
	Kotek::Render::gl::ktkRenderGraphSimplifiedStorageOutput& storage,
	Kotek::Core::ktkIRenderDevice* p_device)
{
}

void zircon_render_graph_pass_ui_gles3::OnCreatedResources(void) {}

void zircon_render_graph_pass_ui_gles3::OnUpdate()
{
	if (this->m_p_ui_manager)
	{
		this->m_p_ui_manager->Update();
	}
}

void zircon_render_graph_pass_ui_gles3::OnRender(
	const Kotek::Render::gl::ktkRenderGraphSimplifiedNode& node)
{
	if (this->m_p_ui_manager)
	{
		this->m_p_ui_manager->Render();
	}
}
