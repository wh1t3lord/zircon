#include "zircon_render_graph_pass.h"

zircon_render_graph_pass::zircon_render_graph_pass(
	const kotek::static_u8string_view_t& render_pass_name) :
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass(
		render_pass_name.data()),
	m_p_manager_session_game{}
{
}

zircon_render_graph_pass::zircon_render_graph_pass() :
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass(),
	m_p_manager_session_game{}
{
}

zircon_render_graph_pass::~zircon_render_graph_pass() {}

void zircon_render_graph_pass::OnRegisterManagers(
	zircon_session_game_manager* p_manager_session_game) noexcept
{
	KOTEK_ASSERT(p_manager_session_game,
		"must be a valid pointer of session game manager!");

	this->m_p_manager_session_game = p_manager_session_game;
}
