#pragma once

class zircon_sdk_ui_object_list : public Kotek::Core::kotek_sdk_ui_element
{
public:
	zircon_sdk_ui_object_list(void);
	~zircon_sdk_ui_object_list(void);

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* main_manager) override;

private:
	Kotek::ktk::entity_t m_selected_entity_id;
};