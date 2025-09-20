#pragma once

#include "../../zircon_render_graph_pass.h"

namespace no_streaming
{
	class zircon_render_graph_pass_imgui_bgfx : public zircon_render_graph_pass_bgfx
	{
	public:
		zircon_render_graph_pass_imgui_bgfx(void);
		~zircon_render_graph_pass_imgui_bgfx(void);

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;

		void OnUpdate(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;

		void OnRender(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;

	private:
		kotek::core::ktkIImguiWrapper* m_p_imgui_wrapper;
	};
}