#pragma once

class zircon_config;

class zircon_ui_window_settings : public Kotek::Core::ktkISDKUIElement
{
public:
	zircon_ui_window_settings(zircon_config* p_config);
	~zircon_ui_window_settings(void);

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;

	int Get_ID(void) const override;
	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	bool m_is_window_show;
	zircon_config* m_p_config;
};