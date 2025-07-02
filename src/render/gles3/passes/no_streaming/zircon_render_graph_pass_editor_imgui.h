#pragma once

#include "../../zircon_render_graph_pass_editor.h"

class zircon_render_graph_pass_editor_imgui_gles3
	: public zircon_render_graph_pass_editor
{
public:
	zircon_render_graph_pass_editor_imgui_gles3();
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
};
