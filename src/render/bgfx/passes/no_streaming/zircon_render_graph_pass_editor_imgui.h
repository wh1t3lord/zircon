#pragma once

#include "../../zircon_render_graph_pass_editor.h"

namespace no_streaming
{
	class zircon_render_graph_pass_editor_imgui_bgfx
		: public zircon_render_graph_pass_editor_bgfx
	{
	public:
		zircon_render_graph_pass_editor_imgui_bgfx();
		~zircon_render_graph_pass_editor_imgui_bgfx(void);

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;

		void OnUpdate(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass) override;

		void OnRender(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass) override;

	private:
		kotek::core::ktkMainManager* m_p_main_manager;
		kotek::core::ktkIImguiWrapper* m_p_imgui_wrapper;
	};
}