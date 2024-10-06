#pragma once

class zircon_factory_game;
class zircon_manager_sdk_ui;

class zircon_sdk_ui_component_inspector
	: public Kotek::Core::kotek_sdk_ui_element
{
public:
	zircon_sdk_ui_component_inspector(
		zircon_manager_sdk_ui* p_sdk_ui, zircon_factory_game* p_factory);
	~zircon_sdk_ui_component_inspector();

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* p_main_manager) override;

private:
	bool HasComponentByName(
		const Kotek::ktk::cstring& component_name_from_preprocessor,
		Kotek::ktk::entity_t id) noexcept;
	void CreateComponentByName(
		const Kotek::ktk::cstring& component_name_from_preprocessor,
		Kotek::ktk::entity_t id) noexcept;
	void RemoveComponentByName(
		const Kotek::ktk::cstring& component_name_from_preprocessor,
		Kotek::ktk::entity_t id) noexcept;

private:
	zircon_manager_sdk_ui* m_p_manager_sdk_ui;
	zircon_factory_game* m_p_factory;
	Kotek::ktk::cstring m_combobox_current_item;
	Kotek::ktk::cstring m_list_selected_item_allocator;
};