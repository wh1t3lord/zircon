#pragma once

class zircon_sdk_ui_top_bar : public kotek::core::ktkISDKUIElement
{
public:
	zircon_sdk_ui_top_bar(void);
	~zircon_sdk_ui_top_bar(void);

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(kotek::core::ktkMainManager* main_manager) override;

private:
	void update_modal_save_scene(kotek::core::ktkMainManager* p_main_manager);
	void update_modals(kotek::core::ktkMainManager* p_main_manager);
};