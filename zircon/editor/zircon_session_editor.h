#pragma once

#include "../core/zircon_session.h"
#include "ui/zircon_editor_ui_state.h"

class zircon_world;

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE

class ktkMainManager;
class ktkConsole;

KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_session_editor : public zircon_interface_session
{
	friend class zircon_session_editor_manager;

public:
	zircon_session_editor(kotek::uint8_t id);
	zircon_session_editor(void);
	~zircon_session_editor(void);

	void initialize(
		const kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>&
			session_name,
		kotek::uint8_t id, zircon_world* p_current_scene,
		kotek::core::ktkMainManager* p_main_manager,
		kotek::core::ktkConsole* p_console,
		kotek::core::ktkIFileSystem* p_filesystem,
		kotek::core::ktkIResourceManager* p_resource_manager);
	void shutdown(void) override;
	void update(void) override;

	kotek::uint8_t get_id(void) const noexcept override;
	eZirconSessionType get_type(void) const noexcept override;
	const char* get_session_name(void) const noexcept override;

	void Serialize(const ktk_filesystem_path& full_path_to_file) noexcept;

	void Serialize_Settings(Kotek::Core::ktkFileText& output,
		const Kotek::ktk::cstring& scenename) noexcept;
	void Deserialize_Settings(Kotek::Core::ktkFileText& input) noexcept;

	void Serialize_Entities(Kotek::Core::ktkFileText& output) noexcept;
	void Deserialize_Entities(Kotek::Core::ktkFileText& input) noexcept;

	void Deserialize(const ktk_filesystem_path& full_path_to_file) noexcept;
	kotek::uint8_t get_render_graph_id(void) const noexcept;
	void set_render_graph_id(kotek::uint8_t id) noexcept;

	zircon_editor_ui_state* get_ui_state(void) noexcept;
	zircon_editor_command_history* get_command_history(void) noexcept;

	const zircon_editor_ui_state* get_ui_state(void) const noexcept;
	const zircon_editor_command_history* get_command_history(
		void) const noexcept;

	zircon_world* get_world(void) const noexcept;

private:
	void update_component_input_sdk(void) noexcept;
	void update_editing_status(void) noexcept;
	void update_component_camera(void) noexcept;
	void update_component_camera_sdk(void) noexcept;

private:
#ifdef KOTEK_DEBUG
	bool m_was_allocated_by_manager;
	bool m_was_destroyed_by_manager;
#endif
	bool m_was_initialized;
	bool m_is_change_title_once_for_editing_status;
	kotek::uint8_t m_id;
	kotek::uint8_t m_render_graph_id;
	zircon_world* m_p_world;
	kotek::core::ktkConsole* m_p_console;
	kotek::core::ktkMainManager* m_p_main_manager;
	kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH> m_name;
	zircon_editor_ui_state m_state;
	zircon_editor_command_history m_command_history_manager;
};