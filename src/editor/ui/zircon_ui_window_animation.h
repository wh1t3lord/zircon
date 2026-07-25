#pragma once

class zircon_editor_ui_window_animation : public kotek::Core::ktkISDKUIElement
{
public:
	zircon_editor_ui_window_animation(void);
	~zircon_editor_ui_window_animation(void);

	void Initialize(void) override;
	void Shutdown(void) override;
	void Draw(kotek::Core::ktkMainManager* main_manager) override;
	int Get_ID(void) const override;

	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
};