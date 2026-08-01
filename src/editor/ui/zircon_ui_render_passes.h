#pragma once

class zircon_config;
class zircon_renderer_bgfx;

/// @brief \~english Render Passes window (task Z3 P2a — the "wizard"
/// delivered as a normal dockable editor window whose first-run
/// presentation is the wizard): lists the live pass set of the editor
/// and the game session in execution order — enable checkbox (instant
/// per-pass skip flag in the executor, no destroy/create), up/down
/// reorder and add/remove (structural edits queue a frame-boundary
/// rebuild of that session's graph), a status marker per row
/// (active / missing-from-library), a "modified" dirty marker against
/// the saved config sets and a Save button that persists both lists
/// plus the "don't show on start again" choice through
/// zircon_config::serialize into game_config.json.
///
/// First run: zircon_game_manager auto-opens the window while the
/// show_pass_manager_on_start config flag is true (default); the
/// window is always reopenable from View > Show windows All like every
/// other editor window.
class zircon_editor_ui_window_render_passes
	: public kotek::Core::ktkISDKUIElement
{
public:
	/// the registry tables enumerate the passes registered in the pass
	/// library per session kind without instantiating them (the
	/// generated zircon_render_passes_registry entries — plain string
	/// literals with static storage); the window never includes the
	/// generated factory header itself, so zircon.editor.ui stays free
	/// of concrete pass types and of a build-order dependency on the
	/// passes project's codegen step
	zircon_editor_ui_window_render_passes(zircon_config* p_config,
		zircon_renderer_bgfx* p_renderer_bgfx,
		const char* const* p_registry_editor_pass_names,
		kotek::uint8_t registry_editor_pass_count,
		const char* const* p_registry_game_pass_names,
		kotek::uint8_t registry_game_pass_count);
	~zircon_editor_ui_window_render_passes(void);

	void Initialize(void) override;
	void Shutdown(void) override;
	void Draw(kotek::Core::ktkMainManager* main_manager) override;

	int Get_ID(void) const override;
	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	/// re-reads the registered-pass name tables; called from Show() so
	/// an open always reflects the current library. P3 (hot-reload)
	/// calls this after a pass-DLL reload — that's when the tables can
	/// actually change under the window
	void refresh_registry(void) noexcept;

	void draw_session_section(
		kotek::Core::ktkIImguiWrapper* p_wrapper_imgui,
		bool is_game_session) noexcept;

	bool is_registered(
		bool is_game_session, const char* p_pass_name) const noexcept;
	bool is_session_dirty(bool is_game_session) noexcept;

	/// writes both live sets into the config members + the
	/// don't-show-on-start choice into its feature flag, then persists
	/// everything through the config's own serialize
	void save(kotek::Core::ktkMainManager* p_main_manager) noexcept;

private:
	bool m_is_window_show;
	/// mirrors the inverted show_pass_manager_on_start flag between
	/// Show()/construction and Save (the checkbox edits only this
	/// member; the config flag changes on Save)
	bool m_dont_show_on_start;
	/// one-shot SetNextWindowFocus after a Show() (first-run focus)
	bool m_need_initial_focus;
	zircon_config* m_p_config;
	/// nullptr when the active renderer is not bgfx (NRI) — the window
	/// then only shows an explanatory line
	zircon_renderer_bgfx* m_p_renderer_bgfx;

	const char* const* m_p_registry_editor_pass_names;
	kotek::uint8_t m_registry_editor_pass_count;
	const char* const* m_p_registry_game_pass_names;
	kotek::uint8_t m_registry_game_pass_count;
};
