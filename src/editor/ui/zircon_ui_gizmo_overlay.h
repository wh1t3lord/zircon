#pragma once

class zircon_session_editor_manager;

/// @brief \~english the own-gizmo's live overlay (task Z3 P2e): a pinned,
/// non-interactive imgui overlay that shows the gizmo pass's published
/// state — the current mode (W/E/R), the snap toggle (T), and while a
/// drag runs the per-component delta + absolute result (the "rich
/// output"). The gizmo pass cannot emit imgui calls itself (it updates
/// before the imgui pass's NewFrame), so it publishes
/// zircon_gizmo_overlay_state_t into the editor ui_state and this window
/// draws it inside the imgui frame. Draws nothing while no gizmo-eligible
/// selection exists.
class zircon_editor_ui_window_gizmo_overlay
	: public kotek::core::ktkISDKUIElement
{
public:
	zircon_editor_ui_window_gizmo_overlay(
		zircon_session_editor_manager* p_manager_session_editor);
	~zircon_editor_ui_window_gizmo_overlay(void);

	void Initialize(void) override;
	void Shutdown(void) override;
	void Draw(kotek::core::ktkMainManager* main_manager) override;

	int Get_ID(void) const override;

	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	bool m_is_show_window;
	zircon_session_editor_manager* m_p_manager_session_editor;
};
