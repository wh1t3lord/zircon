#include "zircon_render_graph_pass_editor.h"

#include "../../editor/session/zircon_session_editor_manager.h"

zircon_render_graph_pass_editor::zircon_render_graph_pass_editor(
	const kotek::static_u8string_view_t& render_pass_name) :
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass(
		render_pass_name.data()),
	m_p_manager_session_editor{}
{
}

zircon_render_graph_pass_editor::zircon_render_graph_pass_editor() :
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass(),
	m_p_manager_session_editor{}
{
}

zircon_render_graph_pass_editor::~zircon_render_graph_pass_editor() {}

void zircon_render_graph_pass_editor::OnRegisterManagers(
	zircon_session_editor_manager* p_manager_session_editor) noexcept
{
	KOTEK_ASSERT(p_manager_session_editor,
		"you must pass a valid editor session manager!");

	this->m_p_manager_session_editor = p_manager_session_editor;
}
