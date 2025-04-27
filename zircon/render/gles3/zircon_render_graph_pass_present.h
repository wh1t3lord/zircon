#pragma once

#include "zircon_render_graph_pass.h"

class zircon_render_graph_pass_present_gles3
	: public zircon_render_graph_pass
{
public:
	zircon_render_graph_pass_present_gles3(
		const kotek::static_u8string_view_t& render_pass_name);
	zircon_render_graph_pass_present_gles3(void);
	~zircon_render_graph_pass_present_gles3(void);

	void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource) override;
	void OnUpdate(const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass) override;
	void OnRender(const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass) override;
};
