#include "zircon_sdk_ui.h"
#include "../ecs/components/zircon_factory.h"

zircon_manager_sdk_ui::zircon_manager_sdk_ui(
	zircon_factory_game* p_factory, bool is_use_sdk) :
	m_is_use_sdk_imgui{is_use_sdk},
	m_is_show_modal_scene_save{}, m_p_view_selected_entity{}
{
	KOTEK_ASSERT(p_factory, "you must initialize factory");

	const auto& map_component_name_and_component_hash =
		p_factory->GetRegisteredComponents();

	for (const auto& [component_name, component_hash] :
		map_component_name_and_component_hash)
	{
		this->m_components_to_show[component_name] = true;
	}

	this->m_components_to_show
		[zircon_component_bounding_sphere::GetComponentName()] = false;
}

zircon_manager_sdk_ui::~zircon_manager_sdk_ui(void) {}

void zircon_manager_sdk_ui::AddObjectToSceneList(
	Kotek::ktk::entity_t id) noexcept
{
	if (this->m_is_use_sdk_imgui)
	{
	}
}

void zircon_manager_sdk_ui::DeleteObjectFromSceneList(
	Kotek::ktk::entity_t id) noexcept
{
#ifdef KOTEK_USE_SDK
	if (this->m_is_use_sdk)
	{
		this->m_p_main_window->DeleteObjectFromSceneList(id);
	}
#endif

	if (this->m_is_use_sdk_imgui)
	{
	}
}

void zircon_manager_sdk_ui::imgui_SetSelectedEntity(
	Kotek::ktk::entity_t* id) noexcept
{
	this->m_p_view_selected_entity = id;
}

Kotek::ktk::entity_t* zircon_manager_sdk_ui::imgui_GetSelectedEntity(
	void) noexcept
{
	return this->m_p_view_selected_entity;
}

void zircon_manager_sdk_ui::imgui_SetShowModalSceneSaveAndCloseOrClose(
	bool status) noexcept
{
	this->m_is_show_modal_scene_save = status;
}

bool zircon_manager_sdk_ui::imgui_GetShowModalSceneSaveAndCloseOrClose(
	void) noexcept
{
	return this->m_is_show_modal_scene_save;
}

bool zircon_manager_sdk_ui::is_need_to_show_component_in_widget(
	const kn_kotek::kn_ktk::cstring& component_name) const noexcept
{
	bool result{};

	if (this->m_components_to_show.find(component_name) !=
		this->m_components_to_show.end())
	{
		result = this->m_components_to_show.at(component_name);
	}

	return result;
}
