#pragma once

/// the gizmo pass's published drag state. POD only; the render pass writes
/// it and editor windows read it without a render-module link edge.
struct zircon_gizmo_overlay_state_t
{
	kotek::uint8_t m_mode;
	bool m_is_gizmo_active;
	bool m_is_dragging;
	bool m_is_snap_enabled;
	bool m_is_drag_active;
	bool m_cancel_drag_requested;
	float m_drag_delta[3];
	float m_result[4];
};

class zircon_editor_ui_state_interface
{
public:
	virtual ~zircon_editor_ui_state_interface() {}

	virtual kotek::entity_t get_selected_entity() const = 0;
	virtual void set_selected_entity(kotek::entity_t id) = 0;

	virtual bool is_need_to_show_component_in_widget(
		const char* p_component_name) = 0;

	virtual void set_imgui_show_modal_save_scene(bool show) = 0;
	virtual bool is_imgui_show_modal_save_scene(void) const = 0;

	virtual zircon_gizmo_overlay_state_t&
	get_gizmo_overlay_state(void) noexcept = 0;
	virtual const zircon_gizmo_overlay_state_t&
	get_gizmo_overlay_state(void) const noexcept = 0;

	virtual bool is_simulation_active(void) const noexcept = 0;
	virtual kotek::uint32_t get_simulation_start_count(
		void) const noexcept = 0;
	virtual void set_simulation_active(bool status) noexcept = 0;
	virtual void increment_simulation_start_count(void) noexcept = 0;
};

#include "zircon_editor_enums.h"
