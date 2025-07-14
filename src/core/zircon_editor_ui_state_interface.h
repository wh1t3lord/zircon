#pragma once

class zircon_editor_ui_state_interface
{
public:
	virtual ~zircon_editor_ui_state_interface() {}

	virtual entt::entity get_selected_entity() const = 0;
	virtual void set_selected_entity(entt::entity id) = 0;

	virtual bool is_need_to_show_component_in_widget(
		const char* p_component_name) = 0;

	virtual void set_imgui_show_modal_save_scene(bool show) = 0;
	virtual bool is_imgui_show_modal_save_scene(void) const = 0;
};

#include "zircon_editor_enums.h"