#pragma once

#include "../core/zircon_session.h"

class zircon_world;
class zircon_manager_game;
class zircon_editor_command_history;
class zircon_factory_game;

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE

class ktkMainManager;

KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_session_editor : public zircon_interface_session
{
public:
	zircon_session_editor(void);
	~zircon_session_editor(void);

	void initialize(zircon_world* p_current_scene,
		kotek::core::ktkMainManager* p_main_manager,
		zircon_editor_command_history* p_command_history,
		zircon_factory_game* p_factory_game,
		kotek::core::ktkConsole* p_console);
	void shutdown(void) override;
	void update(void) override;

	void Serialize(const ktk_filesystem_path& full_path_to_file) noexcept;

	void Serialize_Settings(Kotek::Core::ktkFileText& output,
		const Kotek::ktk::cstring& scenename) noexcept;
	void Deserialize_Settings(Kotek::Core::ktkFileText& input) noexcept;

	void Serialize_Entities(Kotek::Core::ktkFileText& output) noexcept;
	void Deserialize_Entities(Kotek::Core::ktkFileText& input) noexcept;

	void Deserialize(const ktk_filesystem_path& full_path_to_file) noexcept;

private:
	void update_component_input_sdk(void) noexcept;
	void update_editing_status(void) noexcept;
	void update_component_camera(void) noexcept;
	void update_component_camera_sdk(void) noexcept;

private:
	bool m_is_change_title_once_for_editing_status;
	zircon_world* m_p_scene;
	zircon_editor_command_history* m_p_command_history;
	zircon_factory_game* m_p_factory_game;
	kotek::core::ktkConsole* m_p_console;
	kotek::core::ktkMainManager* m_p_main_manager;
};