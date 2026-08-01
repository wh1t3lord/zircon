#pragma once

#include "../../core/zircon_editor_ui_state_interface.h"

class zircon_factory;

/// @brief \~english the gizmo pass's published drag state (task Z3 P2e):
/// the pass writes it every frame, the gizmo overlay window
/// (zircon_editor_ui_window_gizmo_overlay) reads and draws it inside the
/// imgui frame — the pass itself runs before the imgui pass's NewFrame
/// and cannot emit imgui calls. POD only. m_mode carries the pass's
/// eZirconRenderPassGizmoMode values: 0 = translate, 1 = rotate,
/// 2 = scale (stored as a plain uint8_t so this header stays free of
/// render-pass headers)
struct zircon_gizmo_overlay_state_t
{
	kotek::uint8_t m_mode;
	bool m_is_gizmo_active;
	bool m_is_dragging;
	bool m_is_snap_enabled;
	// per-mode delta: translate -> world xyz delta, rotate -> the
	// dragged axis's angle in degrees (one component), scale -> the
	// per-component scale delta
	float m_drag_delta[3];
	// the absolute result: translate -> position xyz, rotate -> the
	// quaternion (x,y,z,w), scale -> scale xyz
	float m_result[4];
};

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
		void) noexcept;
	const zircon_gizmo_overlay_state_t& get_gizmo_overlay_state(
		void) const noexcept;

private:
	bool m_is_imgui_show_modal_save_scene;
	kotek::entity_t m_selected_entity;
	zircon_gizmo_overlay_state_t m_gizmo_overlay_state;
	kotek::static_unordered_map_t<const char*, bool, 64>
		m_components_to_show;
};
