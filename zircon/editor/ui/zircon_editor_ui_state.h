#pragma once

#include "../../core/zircon_editor_ui_state_interface.h"

class zircon_factory;

/// @brief class for managining global state of UI
class zircon_editor_ui_state : public zircon_editor_ui_state_interface
{
public:
	zircon_editor_ui_state();
	~zircon_editor_ui_state();

	void initialize(zircon_factory* p_factory_game);
	void destroy();

	entt::entity get_selected_entity() const override;
	void set_selected_entity(entt::entity id) override;

	bool is_need_to_show_component_in_widget(
		const char* p_component_name) override;

	void set_imgui_show_modal_save_scene(bool show) override;
	bool is_imgui_show_modal_save_scene(void) const override;

private:
	bool m_is_imgui_show_modal_save_scene;
	entt::entity m_selected_entity;
	kotek::static_unordered_map_t<const char*, bool, 64> m_components_to_show;
};