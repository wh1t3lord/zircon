#include "zircon_sdk_ui.h"

#include "../../../ecs/components/zircon_factory.h"

zircon_sdk_ui::zircon_sdk_ui() :
	m_is_imgui_show_modal_save_scene{}, m_selected_entity{entt::null}
{
}

zircon_sdk_ui::~zircon_sdk_ui() {}

void zircon_sdk_ui::initialize(zircon_factory_game* p_factory_game)
{
	if (p_factory_game)
	{
		const auto& map_component_name_and_hash =
			p_factory_game->GetRegisteredComponents();

		for (const auto& [component_name, hash] : map_component_name_and_hash)
		{
			this->m_components_to_show[component_name.data()] = true;
		}

		this->m_components_to_show
			[zircon_component_bounding_sphere::GetComponentName().c_str()] =
			false;
	}
}

void zircon_sdk_ui::destroy() {}

entt::entity zircon_sdk_ui::get_selected_entity() const
{
	return this->m_selected_entity;
}

void zircon_sdk_ui::set_selected_entity(entt::entity id)
{
	this->m_selected_entity = id;
}

bool zircon_sdk_ui::is_need_to_show_component_in_widget(
	const char* p_component_name)
{
	bool result{};

	if (p_component_name)
	{
		if (this->m_components_to_show.find(p_component_name) !=
			this->m_components_to_show.end())
		{
			result = this->m_components_to_show[p_component_name];
		}
	}

	return result;
}

void zircon_sdk_ui::set_imgui_show_modal_save_scene(bool show)
{
	this->m_is_imgui_show_modal_save_scene = show;
}

bool zircon_sdk_ui::is_imgui_show_modal_save_scene(void) const
{
	return this->m_is_imgui_show_modal_save_scene;
}
