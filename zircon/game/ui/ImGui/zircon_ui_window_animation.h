#pragma once

class zircon_sdk_ui_window_animation : public Kotek::Core::kotek_sdk_ui_element
{
public:
	zircon_sdk_ui_window_animation(void);
	~zircon_sdk_ui_window_animation(void);

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;

private:
};