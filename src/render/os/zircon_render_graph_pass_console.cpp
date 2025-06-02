#include "zircon_render_graph_pass_console.h"

zircon_render_graph_pass_console::zircon_render_graph_pass_console(
	kotek::core::ktkMainManager* p_main_manager, kotek::core::ktkWindowConsole* p_console) :
	m_p_main_manager{p_main_manager}, m_p_console{p_console}
{
}

zircon_render_graph_pass_console::~zircon_render_graph_pass_console()
{
}

void zircon_render_graph_pass_console::OnCreateResources(
	kotek::core::ktkMainManager* p_manager_main,
	kotek::core::ktkIRenderResourceManager* p_manager_resource)
{
}

void zircon_render_graph_pass_console::OnUpdate(
	const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
		p_previous_pass)
{
	if (this->m_p_console)
	{
		this->m_p_console->Update();
	}
}

void zircon_render_graph_pass_console::OnRender(
	const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
		p_previous_pass)
{
}
