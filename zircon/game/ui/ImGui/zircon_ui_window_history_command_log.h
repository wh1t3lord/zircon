#pragma once

class zircon_command_history;

class zircon_ui_window_history_command_log
	: public Kotek::Core::ktkISDKUIElement
{
public:
	zircon_ui_window_history_command_log(
		zircon_command_history* p_manager_history);
	~zircon_ui_window_history_command_log();

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;

private:
	zircon_command_history* m_p_manager_history;
};