#pragma once

#include <kotek.render.shared.bgfx/include/kotek_render_graph_simplified_render_pass.h>

class zircon_render_graph_pass_console : public kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass
{
public:
	zircon_render_graph_pass_console(
		kotek::core::ktkMainManager* p_main_manager, kotek::core::ktkWindowConsole* p_console);
	~zircon_render_graph_pass_console();

	void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource) override;

	void OnUpdate(const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue) override;

	void OnRender(const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue) override;

private:
	kotek::core::ktkMainManager* m_p_main_manager;
	kotek::core::ktkWindowConsole* m_p_console;
};