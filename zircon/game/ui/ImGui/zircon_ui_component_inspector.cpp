#include "zircon_ui_component_inspector.h"
#include "../../../ecs/components/zircon_factory.h"
#include "../../zircon_game_manager.h"
#include "../../zircon_scene_manager.h"
#include "../../../core/zircon_sdk_ui.h"

zircon_sdk_ui_component_inspector::zircon_sdk_ui_component_inspector(
	zircon_sdk_ui_interface* p_sdk_ui, zircon_factory_game* p_factory) :
	m_is_show_window{}, m_p_manager_sdk_ui{p_sdk_ui}, m_p_factory{p_factory},
	m_p_combobox_current_item{}, m_p_list_selected_item_allocator{}
{
	KOTEK_ASSERT(
		p_factory, "you can't pass an invalid pointer to zircon_GameFactory");
	KOTEK_ASSERT(p_sdk_ui, "must be initialized!");

	this->m_p_combobox_current_item =
		p_factory->GetRegisteredComponents().cbegin()->first.data();
}

zircon_sdk_ui_component_inspector::~zircon_sdk_ui_component_inspector() {}

void zircon_sdk_ui_component_inspector::initialize(void) {}

void zircon_sdk_ui_component_inspector::shutdown(void) {}

void zircon_sdk_ui_component_inspector::Draw(
	Kotek::Core::ktkMainManager* p_main_manager)
{
	if (!this->m_is_show_window)
		return;


	zircon_manager_game* p_game_manager =
		static_cast<zircon_manager_game*>(p_main_manager->GetGameManager());

	auto* p_current_scene =
		p_game_manager->GetSceneManager()->GetCurrentScene();

	auto selected_entity = p_game_manager->get_sdk_ui()->get_selected_entity();

	Kotek::Core::ktkIImguiWrapper* p_wrapper_imgui =
		p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->Begin(
				"Component Inspector"))
		{
			if (p_wrapper_imgui->BeginCombo(
					"Add Component", this->m_p_combobox_current_item))
			{
				for (const auto& [component_name, id_type] :
					this->m_p_factory->GetRegisteredComponents())

				{
					bool is_need_to_show =
						this->m_p_manager_sdk_ui
							->is_need_to_show_component_in_widget(
								component_name.data());

					if (is_need_to_show)
					{
						if (p_wrapper_imgui->Selectable(component_name.data(),
								this->m_p_combobox_current_item ==
									component_name))
						{
							this->m_p_combobox_current_item =
								component_name.data();
						}
					}
				}

				p_wrapper_imgui->EndCombo();
			}

			if (selected_entity != entt::null)
			{
				auto real_entity_id = selected_entity;
				if (p_wrapper_imgui->Button("Add component"))
				{
					p_game_manager->GetConsole()->Execute_Command(
						static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eConsoleCommandIndex::
								kConsoleCommand_SDK_CreateComponentForEntity),
						{{this->m_p_combobox_current_item},
							{static_cast<kotek::uint32_t>(real_entity_id)}});
				}

				if (p_wrapper_imgui->Button("Delete component from list box"))
				{
					if (this->m_p_list_selected_item_allocator)
					{
						p_game_manager->GetConsole()->Execute_Command(
							static_cast<Kotek::ktk::enum_base_t>(
								Kotek::Core::eConsoleCommandIndex::
									kConsoleCommand_SDK_DeleteComponentFromEntity),
							{{this->m_p_list_selected_item_allocator},
								{static_cast<kotek::uint32_t>(
									real_entity_id)}});
					}
				}

				p_wrapper_imgui->Text(Kotek::ktk::format("Selected entity: {}",
					static_cast<kotek::uint32_t>(selected_entity))
										  .c_str());

				if (p_wrapper_imgui->BeginListBox("list"))
				{
					bool is_presented{};
					for (const auto& [component_name, id_type] :
						this->m_p_factory->GetRegisteredComponents())
					{
						bool is_selected =
							(this->m_p_list_selected_item_allocator ==
								component_name);

						is_presented = this->HasComponentByName(
							component_name.data(), real_entity_id);

						if (is_presented)
						{
							if (p_wrapper_imgui->Selectable(
									component_name.data(), &is_selected))
							{
								this->m_p_list_selected_item_allocator =
									component_name.data();
							}
						}

						if (is_selected)
						{
							p_wrapper_imgui->SetItemDefaultFocus();
						}
					}

					p_wrapper_imgui->EndListBox();
				}

				if (this->m_p_list_selected_item_allocator)
				{
					void* p_raw_data =
						p_game_manager->get_factory_game()->GetComponentByName(
							selected_entity,
							this->m_p_list_selected_item_allocator);

					if (p_raw_data)
					{
						zircon_component_interface* p_interface =
							static_cast<zircon_component_interface*>(
								p_raw_data);

						p_interface->DrawImGui(p_main_manager);
					}
				}
			}
		}
		else
		{
			p_wrapper_imgui->Text(
				"you didn't select any element on scene or from "
				"object list");
		}

		if (p_wrapper_imgui)
		{
			p_wrapper_imgui->End();
		}
	}
}

int zircon_sdk_ui_component_inspector::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_ComponentInspector);
}

void zircon_sdk_ui_component_inspector::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_sdk_ui_component_inspector::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_sdk_ui_component_inspector::Is_Shown(void) const
{
	return this->m_is_show_window;
}

bool zircon_sdk_ui_component_inspector::HasComponentByName(
	const kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>&
		component_name_from_preprocessor,
	entt::entity id) noexcept
{
	bool result{};

	result =
		this->m_p_factory->HasComponent(id, component_name_from_preprocessor);

	return result;
}

void zircon_sdk_ui_component_inspector::CreateComponentByName(
	const kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>&
		component_name_from_preprocessor,
	entt::entity id) noexcept
{
	if (component_name_from_preprocessor ==
		zircon_component_actor::GetComponentName())
	{
		if (this->m_p_factory->HasComponent<zircon_component_actor>(id) ==
			false)
		{
			this->m_p_factory->CreateComponent<zircon_component_actor>(id);
		}
	}
	else if (component_name_from_preprocessor ==
		zircon_component_camera::GetComponentName())
	{
		if (this->m_p_factory->HasComponent<zircon_component_camera>(id) ==
			false)
		{
			this->m_p_factory->CreateComponent<zircon_component_camera>(id);
		}
	}
	else if (component_name_from_preprocessor ==
		zircon_component_geometry::GetComponentName())
	{
		if (this->m_p_factory->HasComponent<zircon_component_geometry>(id) ==
			false)
		{
			this->m_p_factory->CreateComponent<zircon_component_geometry>(id);
		}
	}
	else if (component_name_from_preprocessor ==
		zircon_component_input::GetComponentName())
	{
		if (this->m_p_factory->HasComponent<zircon_component_input>(id) ==
			false)
		{
			this->m_p_factory->CreateComponent<zircon_component_input>(id);
		}
	}
	else if (component_name_from_preprocessor ==
		zircon_component_transform::GetComponentName())
	{
		if (this->m_p_factory->HasComponent<zircon_component_transform>(id) ==
			false)
		{
			this->m_p_factory->CreateComponent<zircon_component_transform>(id);
		}
	}
	else if (component_name_from_preprocessor ==
		zircon_component_visibility::GetComponentName())
	{
		if (this->m_p_factory->HasComponent<zircon_component_visibility>(id) ==
			false)
		{
			this->m_p_factory->CreateComponent<zircon_component_visibility>(id);
		}
	}
	else
	{
		KOTEK_ASSERT(false,
			"you didn't provide a code for your "
			"component it is like your registered in "
			"factory but forgot to add code here or "
			"something similar "
			"to that, you forgot to specify your "
			"component somewhere. "
			"Factory or anything else, we can't find "
			"your component by "
			"name: {}",
			component_name_from_preprocessor);
	}
}

void zircon_sdk_ui_component_inspector::RemoveComponentByName(
	const kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>&
		component_name_from_preprocessor,
	entt::entity id) noexcept
{
	if (component_name_from_preprocessor ==
		zircon_component_actor::GetComponentName())
	{
		this->m_p_factory->RemoveComponent<zircon_component_actor>(id);
	}
	else if (component_name_from_preprocessor ==
		zircon_component_camera::GetComponentName())
	{
		this->m_p_factory->RemoveComponent<zircon_component_camera>(id);
	}
	else if (component_name_from_preprocessor ==
		zircon_component_geometry::GetComponentName())
	{
		this->m_p_factory->RemoveComponent<zircon_component_geometry>(id);
	}
	else if (component_name_from_preprocessor ==
		zircon_component_input::GetComponentName())
	{
		this->m_p_factory->RemoveComponent<zircon_component_input>(id);
	}
	else if (component_name_from_preprocessor ==
		zircon_component_transform::GetComponentName())
	{
		this->m_p_factory->RemoveComponent<zircon_component_transform>(id);
	}
	else if (component_name_from_preprocessor ==
		zircon_component_visibility::GetComponentName())
	{
		this->m_p_factory->RemoveComponent<zircon_component_visibility>(id);
	}
	else
	{
		KOTEK_ASSERT(false,
			"you didn't provide a code for your "
			"component it is like your registered in "
			"factory but forgot to add code here or "
			"something similar "
			"to that, you forgot to specify your "
			"component somewhere. "
			"Factory or anything else, we can't find "
			"your component by "
			"name: {}",
			component_name_from_preprocessor);
	}
}
