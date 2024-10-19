#pragma once

class zircon_sdk_ui_object_list : public kotek::core::ktkISDKUIElement
{
public:
	zircon_sdk_ui_object_list(void);
	~zircon_sdk_ui_object_list(void);

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(kotek::core::ktkMainManager* main_manager) override;

private:
	kotek::size_t m_amount_of_entites;
	entt::entity m_selected_entity_id;
};