#pragma once

class zircon_ui_window_render_system : public Kotek::Core::kotek_sdk_ui_element
{
public:
	zircon_ui_window_render_system();
	~zircon_ui_window_render_system();

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;
};
