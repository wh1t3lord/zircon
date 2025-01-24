#pragma once

#include <kotek.render.gl/include/kotek_render_graph_simplified_render_pass.h>

class zircon_render_graph_pass_console
{
public:
	zircon_render_graph_pass_console(
		kotek::core::ktkMainManager* p_main_manager);
	~zircon_render_graph_pass_console();

	void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource) override;

	void OnUpdate(const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass) override;

	void OnRender(const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass) override;

private:
	kotek::core::ktkMainManager* m_p_main_manager;
	kotek::core::ktkWindowConsole* m_p_console;
};