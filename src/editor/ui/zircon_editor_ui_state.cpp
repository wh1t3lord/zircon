#include "zircon_editor_ui_state.h"

#include "../../ecs/zircon_factory.h"

zircon_editor_ui_state::zircon_editor_ui_state() :
	m_is_imgui_show_modal_save_scene{},
	m_selected_entity{kotek::ktk::kInvalidECSEntity},
	m_gizmo_overlay_state{}
{
}

zircon_editor_ui_state::~zircon_editor_ui_state() {}

void zircon_editor_ui_state::initialize(
	zircon_factory* p_factory_game
)
{
	if (p_factory_game)
	{
#ifdef KOTEK_USE_ECS_BACKEND_ENTT
		const auto& map_component_name_and_hash =
			p_factory_game->GetRegisteredComponents();

		for (const auto& [component_name, hash] :
		     map_component_name_and_hash)
		{
			this->m_components_to_show[component_name.data()] =
				true;
		}

		this->m_components_to_show
			[zircon_component_bounding_sphere::GetComponentName(
			)
		         .c_str()] = false;
#elif defined(KOTEK_USE_ECS_BACKEND_PICO)
			// PICO has no runtime registry to iterate — enumerate the
			// codegen'd component enum instead. The map's keys are
			// const char* with POINTER identity (etl's hash has no
			// const char* content specialization), so the inserted keys
			// must be these exact codegen literals — the inspector's
			// is_need_to_show_component_in_widget queries with the same
			// pointers
			for (int component_index = 0;
			     component_index <
			     static_cast<int>(eZirconComponentType::kunknown);
			     ++component_index)
			{
				const eZirconComponentType component_type =
					static_cast<eZirconComponentType>(component_index);

				this->m_components_to_show
					[p_factory_game->get_component_name_by_enum(
						component_type)] =
						(component_type !=
							eZirconComponentType::
								kzircon_component_bounding_sphere);
			}
#endif
	}
}

void zircon_editor_ui_state::destroy() {}

kotek::entity_t
zircon_editor_ui_state::get_selected_entity() const
{
	return this->m_selected_entity;
}

void zircon_editor_ui_state::set_selected_entity(
	kotek::entity_t id
)
{
	this->m_selected_entity = id;
}

bool zircon_editor_ui_state::
	is_need_to_show_component_in_widget(
		const char* p_component_name
	)
{
	bool result{};

	if (p_component_name)
	{
		if (this->m_components_to_show.find(p_component_name) !=
		    this->m_components_to_show.end())
		{
			result =
				this->m_components_to_show[p_component_name];
		}
	}

	return result;
}

void zircon_editor_ui_state::set_imgui_show_modal_save_scene(
	bool show
)
{
	this->m_is_imgui_show_modal_save_scene = show;
}

bool zircon_editor_ui_state::is_imgui_show_modal_save_scene(void
) const
{
	return this->m_is_imgui_show_modal_save_scene;
}

zircon_gizmo_overlay_state_t&
zircon_editor_ui_state::get_gizmo_overlay_state(void) noexcept
{
	return this->m_gizmo_overlay_state;
}

const zircon_gizmo_overlay_state_t&
zircon_editor_ui_state::get_gizmo_overlay_state(void) const noexcept
{
	return this->m_gizmo_overlay_state;
}
