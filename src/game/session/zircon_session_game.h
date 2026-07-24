#pragma once

#include "../../core/zircon_session.h"

class zircon_world;

class zircon_session_game : public zircon_interface_session
{
	friend class zircon_session_game_manager;

public:
	zircon_session_game(kotek::uint8_t id);
	zircon_session_game(void);
	~zircon_session_game(void);

	kotek::uint8_t get_id(void) const noexcept override;
	const char* get_session_name(void) const noexcept override;
	eZirconSessionType get_type(void) const noexcept override;
	void shutdown(void) override;
	void update(void) override;

	void initialize(
		const kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>&
			session_name,
		kotek::uint8_t id, kotek::core::ktkMainManager* p_main_manager,
		kotek::core::ktkConsole* p_console, zircon_world* p_current_world,
		bool is_need_to_initialize_render_graph);

	kotek::uint8_t get_render_graph_id(void) const noexcept;
	void set_render_graph_id(kotek::uint8_t id) noexcept;

	zircon_world* get_world(void) const noexcept;

	void set_imgui_ui_elements(
		const zircon_imgui_elements_t& imgui_elements) noexcept;
	const zircon_imgui_elements_t&
	get_imgui_ui_elements(void) const noexcept;

private:
	void try_to_initialize_render_graph(void) noexcept;

private:
#ifdef KOTEK_DEBUG
	bool m_was_allocated_by_manager;
	bool m_was_destroyed_by_manager;
#endif

	bool m_was_initialized;
	bool m_was_render_graph_initialized;
	kotek::uint8_t m_id;
	kotek::uint8_t m_render_graph_id;
	kotek::core::ktkMainManager* m_p_main_manager;
	kotek::core::ktkConsole* m_p_console;
	zircon_world* m_p_world;
	kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH> m_name;
	zircon_imgui_elements_t m_imgui_ui_elements;
};
