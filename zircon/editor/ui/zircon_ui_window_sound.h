#pragma once

class zircon_sdk_ui_window_sound : public Kotek::Core::ktkISDKUIElement
{
public:
	zircon_sdk_ui_window_sound(void);
	~zircon_sdk_ui_window_sound(void);

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;
	int Get_ID(void) const override;
	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
};
