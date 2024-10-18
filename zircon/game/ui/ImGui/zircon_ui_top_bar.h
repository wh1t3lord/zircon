#pragma once

class zircon_sdk_ui_top_bar : public Kotek::Core::ktkISDKUIElement
{
public:
	zircon_sdk_ui_top_bar(void);
	~zircon_sdk_ui_top_bar(void);

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;

private:
	void update_modal_save_scene(Kotek::Core::ktkMainManager* p_main_manager);
	void update_modals(Kotek::Core::ktkMainManager* p_main_manager);
};