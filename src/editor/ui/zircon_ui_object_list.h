#pragma once

class zircon_session_editor_manager;

class zircon_editor_ui_state_object_list : public kotek::core::ktkISDKUIElement
{
public:
	zircon_editor_ui_state_object_list(
		zircon_session_editor_manager* p_manager_session_editor, kotek::core::ktkConsole* p_console);
	~zircon_editor_ui_state_object_list(void);

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(kotek::core::ktkMainManager* main_manager) override;
	int Get_ID(void) const override;

	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	bool m_is_show_window;
	kotek::size_t m_amount_of_entites;
	entt::entity m_selected_entity_id;
	zircon_session_editor_manager* m_p_manager_session_editor;
	kotek::core::ktkConsole* m_p_console;
};