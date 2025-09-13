#pragma once

#include <kotek.render.gl/include/kotek_render_graph_simplified_render_pass.h>

class zircon_session_game_manager;

class zircon_render_graph_pass
	: public kotek::render::gl::ktkRenderGraphSimplifiedRenderPass
{
public:
	zircon_render_graph_pass(
		const kotek::static_u8string_view_t& render_pass_name);
	zircon_render_graph_pass();
	~zircon_render_graph_pass();

	virtual void OnRegisterManagers(
		zircon_session_game_manager* p_manager_session_game) noexcept;

protected:
	zircon_session_game_manager* m_p_manager_session_game;
};