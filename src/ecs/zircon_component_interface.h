#pragma once

#define zircon_DEF_MAX_COMPONENT_NAME_SIZE 128
#define ZIRCON_DEF_COMPONENT_INPUT_DEFAULT_SENSETIVITY 0.1f

class zircon_session_game_manager;
class zircon_session_editor_manager;

class zircon_component_interface
{
public:
	virtual ~zircon_component_interface(void) {}

	virtual void draw_imgui(
		Kotek::Core::ktkMainManager* main_manager) noexcept = 0;
	virtual void deserialize(
		const Kotek::ktk::json::value& serialized_data) noexcept = 0;
	virtual Kotek::ktk::json::value serialize() noexcept = 0;
	virtual Kotek::ktk::json::value serialize(
		unsigned char* p_raw_memory, Kotek::ktk::size_t size) = 0;
	virtual kotek::uint8_t get_component_type(void) const noexcept = 0;

	virtual void register_managers(
		zircon_session_game_manager* p_manager_session_game,
		zircon_session_editor_manager* p_manager_session_editor) noexcept;

protected:
#ifdef KOTEK_USE_SDK_IMGUI
	// todo: provide preprocessor guards in case of shipping configuration
	// please
	zircon_session_editor_manager* m_p_manager_session_editor;
#endif

	zircon_session_game_manager* m_p_manager_session_game;
};

#include "zircon_ecs_auto_enum_components.h"
#include "zircon_component_fields.h"