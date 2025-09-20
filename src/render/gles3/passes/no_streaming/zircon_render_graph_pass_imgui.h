#pragma once

#include "../../zircon_render_graph_pass.h"

namespace no_streaming
{
	class zircon_render_graph_pass_imgui_gles3 : public zircon_render_graph_pass
	{
	public:
		zircon_render_graph_pass_imgui_gles3(void);
		~zircon_render_graph_pass_imgui_gles3(void);

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;

		void OnUpdate(
			const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass) override;

		void OnRender(
			const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass) override;

	private:
		kotek::core::ktkMainManager* m_p_main_manager;
		kotek::core::ktkIImguiWrapper* m_p_imgui_wrapper;
	};
}