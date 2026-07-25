#pragma once

class zircon_editor_command_history;
class zircon_session_editor_manager;

class zircon_editor_ui_window_history_command_log
	: public kotek::Core::ktkISDKUIElement
{
public:
	zircon_editor_ui_window_history_command_log(
		zircon_editor_command_history* p_manager_history, zircon_session_editor_manager* p_manager_session_editor);
	~zircon_editor_ui_window_history_command_log();

	void Initialize(void) override;
	void Shutdown(void) override;
	void Draw(kotek::Core::ktkMainManager* main_manager) override;
	int Get_ID(void) const override;

		void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	bool m_is_show_window;
	zircon_editor_command_history* m_p_manager_history;
	zircon_session_editor_manager* m_p_manager_session_editor;
};