#pragma once

class zircon_editor_ui_window_debug_input : public kotek::core::ktkISDKUIElement
{
public:
	zircon_editor_ui_window_debug_input();
	~zircon_editor_ui_window_debug_input();

	void Initialize(void) override;
	void Shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;
	int Get_ID(void) const override;

	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	bool m_is_show_window;
	char m_state_keys_buffer[512];
};
