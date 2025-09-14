#pragma once

#include "../../zircon_render_graph_pass_editor.h"

namespace no_streaming
{
	class zircon_render_graph_pass_editor_present_bgfx
		: public zircon_render_graph_pass_editor_bgfx
	{
	public:
		zircon_render_graph_pass_editor_present_bgfx(
			const kotek::static_u8string_view_t& render_pass_name);
		zircon_render_graph_pass_editor_present_bgfx(void);
		~zircon_render_graph_pass_editor_present_bgfx(void);

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;
		void OnUpdate(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass) override;
		void OnRender(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass) override;
	};
}