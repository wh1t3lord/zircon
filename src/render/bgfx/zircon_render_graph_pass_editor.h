#pragma once

#include <kotek.render.bgfx/include/kotek_render_graph_simplified_render_pass.h>

class zircon_session_editor_manager;

class zircon_render_graph_pass_editor
	: public kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass
{
public:
	zircon_render_graph_pass_editor(
		const kotek::static_u8string_view_t& render_pass_name);
	zircon_render_graph_pass_editor();
	~zircon_render_graph_pass_editor();
	
	virtual void OnRegisterManagers(
		zircon_session_editor_manager* p_manager_session_editor) noexcept;


protected:
	zircon_session_editor_manager* m_p_manager_session_editor;
};