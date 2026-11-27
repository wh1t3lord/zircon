#pragma once

#include "zircon_session.h"
#include "zircon_editor_command_history_interface.h"
#include "zircon_editor_ui_state_interface.h"

class zircon_world;
class zircon_config;

/// the editor session surface shared across independently linked modules.
/// Rich session operations remain on zircon_session_editor; commands and UI
/// consume only this contract, which removes the session <-> commands/UI
/// link cycle without a global locator or hidden ownership.
class zircon_session_editor_interface : public zircon_interface_session
{
public:
	virtual ~zircon_session_editor_interface(void) {}

	virtual zircon_world* get_world(void) const noexcept = 0;

	virtual zircon_editor_ui_state_interface* get_ui_state(void) noexcept = 0;
	virtual const zircon_editor_ui_state_interface* get_ui_state(void
	) const noexcept = 0;

	virtual zircon_editor_command_history_interface*
	get_command_history(void) noexcept = 0;
	virtual const zircon_editor_command_history_interface*
	get_command_history(void) const noexcept = 0;

	virtual zircon_config* get_config(void) noexcept = 0;
	virtual const zircon_config* get_config(void) const noexcept = 0;

	virtual kotek::uint8_t get_imgui_ui_element_count(
		void) const noexcept = 0;
	virtual kotek::core::ktkISDKUIElement* get_imgui_ui_element_at(
		kotek::uint8_t index) const noexcept = 0;
};

class zircon_session_editor_manager_interface
	: public zircon_interface_session_manager
{
public:
	virtual ~zircon_session_editor_manager_interface(void) {}

	virtual zircon_session_editor_interface* get_session(
		kotek::uint8_t id) const noexcept = 0;
	virtual kotek::uint8_t get_current_session_id(void) const noexcept = 0;
	virtual void set_current_session_id(kotek::uint8_t id) noexcept = 0;
};
