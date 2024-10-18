#pragma once

class zircon_sdk_ui_window_prefab : public Kotek::Core::ktkISDKUIElement
{
public:
	zircon_sdk_ui_window_prefab(void);
	~zircon_sdk_ui_window_prefab(void);

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;

private:
};
