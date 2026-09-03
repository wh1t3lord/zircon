#pragma once

#include "../../core/zircon_defs.h"
#include "../../core/zircon_session.h"
#include "../../core/zircon_cancel_arbiter.h"
#include "../ui/zircon_editor_ui_state.h"
#include "../commands\zircon_command_history.h"

class zircon_world;
class zircon_session_editor_manager;

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE

class ktkMainManager;
class ktkConsole;

KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

// todo: probably you have to move session to separated project
// because by design we don't have to have a circular
// (recursive) dependency from linking and at the same time we
// shouldn't share projects without direct connections
class zircon_session_editor : public zircon_interface_session
{
	friend class zircon_session_editor_manager;

public:
	using text_t =
		kotek::core::ktkResourceText<1024, 2048, false>;

public:
	zircon_session_editor(kotek::uint8_t id);
	zircon_session_editor(void);
	~zircon_session_editor(void);

	void initialize(
		const kotek::static_cstring_t<
			ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>& session_name,
		kotek::uint8_t id,
		zircon_world* p_current_scene,
		zircon_session_editor_manager* p_manager_session_editor,
		kotek::core::ktkMainManager* p_main_manager,
		kotek::core::ktkConsole* p_console,
		kotek::core::ktkIFileSystem* p_filesystem,
		const char* p_history_streaming_folder_name = "current"
	);
	void shutdown(void) override;
	void update(void) override;

	kotek::uint8_t get_id(void) const noexcept override;
	eZirconSessionType get_type(void) const noexcept override;
	const char* get_session_name(void) const noexcept override;

	void Serialize(const ktk_filesystem_path& full_path_to_file
	) noexcept;

	void Serialize_Settings(
		text_t& output, const kotek::ktk::cstring& scenename
	) noexcept;
	void Deserialize_Settings(text_t& input) noexcept;

	void Serialize_Entities(text_t& output) noexcept;
	void Deserialize_Entities(text_t& input) noexcept;

	void
	Deserialize(const ktk_filesystem_path& full_path_to_file
	) noexcept;
	kotek::uint8_t get_render_graph_id(void) const noexcept;
	void set_render_graph_id(kotek::uint8_t id) noexcept;

	zircon_editor_ui_state* get_ui_state(void) noexcept;
	zircon_editor_command_history* get_command_history(void
	) noexcept;

	const zircon_editor_ui_state* get_ui_state(void
	) const noexcept;
	const zircon_editor_command_history*
	get_command_history(void) const noexcept;

	/// the session's ESC/cancel arbiter (task Z19) — the editor imgui
	/// pass's OnUpdate adapter feeds it the semantic cancel event
	zircon_cancel_arbiter* get_cancel_arbiter(void) noexcept;
	const zircon_cancel_arbiter* get_cancel_arbiter(void
	) const noexcept;

	zircon_world* get_world(void) const noexcept;

	void set_imgui_ui_elements(
		const zircon_imgui_elements_t& imgui_elements
	) noexcept;
	const zircon_imgui_elements_t&
	get_imgui_ui_elements(void) const noexcept;

private:
	void update_editing_status(void) noexcept;
	void update_component_input_sdk(void) noexcept;
	void update_component_camera(void) noexcept;
	void update_component_camera_sdk(void) noexcept;
	void try_to_initialize_render_graph(void) noexcept;

	/// the arbiter's default consumers (task Z19), registered ONCE at
	/// initialize — poll-on-event, so nothing re-registers per frame
	void register_cancel_arbiter_consumers(void) noexcept;

private:
#ifdef KOTEK_DEBUG
	bool m_was_allocated_by_manager;
	bool m_was_destroyed_by_manager;
#endif
	bool m_was_render_graph_initialized;
	bool m_was_initialized;
	bool m_is_change_title_once_for_editing_status;
	kotek::uint8_t m_id;
	kotek::uint8_t m_render_graph_id;
	zircon_world* m_p_world;
	kotek::core::ktkConsole* m_p_console;
	kotek::core::ktkMainManager* m_p_main_manager;
	zircon_imgui_elements_t m_imgui_ui_elements;
	kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>
		m_name;
	zircon_editor_ui_state m_state;
	zircon_editor_command_history m_command_history_manager;
	/// one arbiter per session (task Z19) — like its command history;
	/// the game layer instantiates the same class for its own UI later
	zircon_cancel_arbiter m_cancel_arbiter;
};