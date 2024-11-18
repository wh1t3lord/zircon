#pragma once

#include <kotek.render.gl/include/kotek_render_graph_simplified_render_pass.h>

class zircon_render_graph_pass_editor_imgui_gles3
	: public kotek::render::gl::ktkRenderGraphSimplifiedRenderPass
{
public:
	zircon_render_graph_pass_editor_imgui_gles3(
		const kotek::static_u8string_view_t& render_pass_name,
		kotek::core::ktkMainManager* p_main_manager,
		const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>& elements);
	~zircon_render_graph_pass_editor_imgui_gles3(void);

	void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource) override;

	void OnUpdate(const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass) override;

	void OnRender(const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass) override;

private:
	kotek::core::ktkMainManager* m_p_main_manager;
	kotek::core::ktkIImguiWrapper* m_p_imgui_wrapper;
	const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>*
		m_p_imgui_ui_elements;
};
