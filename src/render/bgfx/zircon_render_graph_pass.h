#pragma once

#include <kotek.render.bgfx/include/kotek_render_graph_simplified_render_pass.h>

class zircon_session_game_manager;

class zircon_render_graph_pass_bgfx
	: public kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass
{
public:
	zircon_render_graph_pass_bgfx(
		const kotek::static_u8string_view_t& render_pass_name);
	zircon_render_graph_pass_bgfx();
	~zircon_render_graph_pass_bgfx();

	virtual void OnRegisterManagers(
		zircon_session_game_manager* p_manager_session_game) noexcept;

protected:
	zircon_session_game_manager* m_p_manager_session_game;
};