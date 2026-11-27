#pragma once

#include "../../core/zircon_editor_ui_state_interface.h"

class zircon_factory;

/// @brief class for managining global state of UI
class zircon_editor_ui_state
	: public zircon_editor_ui_state_interface
{
public:
	zircon_editor_ui_state();
	~zircon_editor_ui_state();

	void initialize(zircon_factory* p_factory_game);
	void destroy();

	kotek::entity_t get_selected_entity() const override;
	void set_selected_entity(kotek::entity_t id) override;

	bool is_need_to_show_component_in_widget(
		const char* p_component_name
	) override;

	void set_imgui_show_modal_save_scene(bool show) override;
	bool is_imgui_show_modal_save_scene(void) const override;

	zircon_gizmo_overlay_state_t& get_gizmo_overlay_state(
		void) noexcept override;
	const zircon_gizmo_overlay_state_t& get_gizmo_overlay_state(
		void) const noexcept override;

	// Play-in-editor diagnostics shared with the UI-press harness without a
	// UI -> game-manager dependency (the session owns this POD state)
	bool is_simulation_active(void) const noexcept override
	{
		return this->m_is_simulation_active;
	}
	kotek::uint32_t get_simulation_start_count(void) const noexcept override
	{
		return this->m_simulation_start_count;
	}
	void set_simulation_active(bool status) noexcept override
	{
		this->m_is_simulation_active = status;
	}
	void increment_simulation_start_count(void) noexcept override
	{
		++this->m_simulation_start_count;
	}

private:
	bool m_is_imgui_show_modal_save_scene;
	bool m_is_simulation_active{};
	kotek::uint32_t m_simulation_start_count{};
	kotek::entity_t m_selected_entity;
	zircon_gizmo_overlay_state_t m_gizmo_overlay_state;
	kotek::static_unordered_map_t<const char*, bool, 64>
		m_components_to_show;
};
