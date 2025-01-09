#pragma once

class zircon_sdk_ui_window_debug_input : public kotek::core::ktkISDKUIElement
{
public:
	zircon_sdk_ui_window_debug_input();
	~zircon_sdk_ui_window_debug_input();

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;
	int Get_ID(void) const override;

	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	bool m_is_show_window;
};
