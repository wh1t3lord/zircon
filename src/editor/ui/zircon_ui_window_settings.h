#pragma once

class zircon_config;
class zircon_localization_manager;

class zircon_editor_ui_window_settings : public kotek::Core::ktkISDKUIElement
{
public:
	zircon_editor_ui_window_settings(zircon_config* p_config,
		zircon_localization_manager* p_localization);
	~zircon_editor_ui_window_settings(void);

	void Initialize(void) override;
	void Shutdown(void) override;
	void Draw(kotek::Core::ktkMainManager* main_manager) override;

	int Get_ID(void) const override;
	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	/// @brief \~english one lookup against the EDITOR localization
	/// instance (task Z22); a null manager (asserted, a wiring bug)
	/// degrades the label to the key itself — the manager's own
	/// missing-key echo is the same fallback, so the release build never
	/// crashes on it
	const char* translate(const char* p_key) const noexcept;

private:
	bool m_is_window_show;
	zircon_config* m_p_config;
	zircon_localization_manager* m_p_localization;
};
