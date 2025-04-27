#include "zircon_component_interface.h"
#include "../editor/session/zircon_session_editor_manager.h"
#include "../game/session/zircon_session_game_manager.h"

void zircon_component_interface::register_managers(
	zircon_session_game_manager* p_manager_session_game,
	zircon_session_editor_manager* p_manager_session_editor) noexcept
{
	this->m_p_manager_session_game = p_manager_session_game;

#ifdef KOTEK_USE_SDK_IMGUI
	KOTEK_ASSERT(p_manager_session_editor,
		"you must pass a valid pointer of session manager editor!");
	this->m_p_manager_session_editor = p_manager_session_editor;
#endif
}
