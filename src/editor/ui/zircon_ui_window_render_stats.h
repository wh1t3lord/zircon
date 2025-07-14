#pragma once

class zircon_editor_ui_window_render_stats : public Kotek::Core::ktkISDKUIElement
{
public:
	zircon_editor_ui_window_render_stats();
	~zircon_editor_ui_window_render_stats();

	void Initialize(void) override;
	void Shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;

	int Get_ID(void) const override;

	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	bool m_is_show_window;
};
