#pragma once

class zircon_ui_window_settings : public Kotek::Core::kotek_sdk_ui_element
{
public:
	zircon_ui_window_settings(void);
	~zircon_ui_window_settings(void);

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;
};