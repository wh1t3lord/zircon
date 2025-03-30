#pragma once

class zircon_sdk_ui_object_list : public kotek::core::ktkISDKUIElement
{
public:
	zircon_sdk_ui_object_list(void);
	~zircon_sdk_ui_object_list(void);

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
};