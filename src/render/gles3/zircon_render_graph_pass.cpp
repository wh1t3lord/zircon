#include "zircon_render_graph_pass.h"

zircon_render_graph_pass::zircon_render_graph_pass() :
	kotek::render::gl::ktkRenderGraphSimplifiedRenderPass(),
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
