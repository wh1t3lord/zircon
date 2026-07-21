#include "zircon_ui_component_inspector.h"
#include "../../ecs/zircon_factory.h"
#include "../../world/zircon_world.h"
#include "../../world/zircon_world_manager.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "zircon_editor_ui_state.h"

constexpr const char* _kSDKModalWindowFailedToAddComponent =
	"Warning##ComponentInspectorFailedToAddComponent";

zircon_editor_ui_window_component_inspector::
	zircon_editor_ui_window_component_inspector(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_editor_ui_state_interface* p_sdk_ui, zircon_factory* p_factory) :
	m_is_show_window{}, m_current_item_name{},
	m_p_manager_sdk_ui{p_sdk_ui}, m_p_factory{p_factory},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_list_selected_item_allocator{}
{
	KOTEK_ASSERT(
		p_factory, "you can't pass an invalid pointer to zircon_GameFactory");
	KOTEK_ASSERT(p_sdk_ui, "must be initialized!");
	KOTEK_ASSERT(p_manager_session_editor,
		"you must pass a valid pointer of session editor manager!");

	this->m_p_list_selected_item_allocator =
		p_factory->get_component_name_by_enum(
			static_cast<eZirconComponentType>(0));
}

zircon_editor_ui_window_component_inspector::
	~zircon_editor_ui_window_component_inspector()
{
}

void zircon_editor_ui_window_component_inspector::Initialize(void) {}

void zircon_editor_ui_window_component_inspector::Shutdown(void) {}

void zircon_editor_ui_window_component_inspector::Draw(
	Kotek::Core::ktkMainManager* p_main_manager)
{
	if (!this->m_is_show_window)
		return;

	auto* p_game_manager = p_main_manager->GetGameManager();

	KOTEK_ASSERT(p_game_manager, "game manager must be initialized!");

	if (!p_game_manager)
	{
		KOTEK_MESSAGE_WARNING("set game manager pointer to main manager!");
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor->get_current_session_id());

	KOTEK_ASSERT(p_session, "failed to obtain session editor by id: {}",
		this->m_p_manager_session_editor->get_current_session_id());

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING("initialize session editor !");
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(p_world, "failed to get world!");

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"session editor_{}#{} must contain a valid pointer to zircon_world",
			p_session->get_session_name(), p_session->get_id());
		return;
	}

	auto selected_entity = p_session->get_ui_state()->get_selected_entity();

	Kotek::Core::ktkIImguiWrapper* p_wrapper_imgui =
		p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->Begin("Component Inspector"))
		{
			if (p_wrapper_imgui->BeginCombo(
					"Add Component", this->m_p_list_selected_item_allocator))
			{
				for (int component_index = 0;
					 component_index <
					 static_cast<int>(eZirconComponentType::kunknown);
					 ++component_index)
				{
					eZirconComponentType component_type =
						static_cast<eZirconComponentType>(component_index);
					const char* component_name =
						this->m_p_factory->get_component_name_by_enum(
							component_type);

					bool is_need_to_show =
						this->m_p_manager_sdk_ui
							->is_need_to_show_component_in_widget(
								component_name);

					if (is_need_to_show)
					{
						if (p_wrapper_imgui->Selectable(component_name,
								this->m_p_list_selected_item_allocator ==
									component_name))
						{
							this->m_p_list_selected_item_allocator =
								component_name;
							this->m_current_item_name =
								static_cast<kotek::uint32_t>(component_type);
						}
					}
				}

				p_wrapper_imgui->EndCombo();
			}

			if (!ecs_is_invalid_entity(selected_entity))
			{
				if (p_wrapper_imgui->Button("Add component"))
				{
					bool is_valid_for_adding = true;

					if (this->m_current_item_name ==
						static_cast<kotek::uint32_t>(
							eZirconComponentType::
								kzircon_component_transform))
					{
						bool required_components =
							!this->m_p_factory->has_component(
								p_world->get_ecs_context(), selected_entity,
								eZirconComponentType::
									kzircon_component_geometry) &&
							!this->m_p_factory->has_component(
								p_world->get_ecs_context(), selected_entity,
								eZirconComponentType::
									kzircon_component_camera) &&
							!this->m_p_factory->has_component(
								p_world->get_ecs_context(), selected_entity,
								eZirconComponentType::
									kzircon_component_sdk_camera);

						if (required_components)
						{
							p_wrapper_imgui->OpenPopup(
								_kSDKModalWindowFailedToAddComponent);

							is_valid_for_adding = false;
						}
					}

					if (this->m_current_item_name ==
						static_cast<kotek::uint32_t>(
							eZirconComponentType::
								kzircon_component_sdk_camera))
					{
						// TODO(zircon): pico has no view<>, scanning all
						// entities; owner may want a cheaper check
						bool is_already_presented = false;
						kotek::entity_t
							ids[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};
						kotek::uint32_t ids_count =
							this->m_p_factory->get_all_entities(
								p_world->get_ecs_context(),
								p_world->get_entity_count_max_limit(), ids,
								ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT);
						for (kotek::uint32_t entity_index = 0;
							 entity_index < ids_count; ++entity_index)
						{
							if (this->m_p_factory->has_component(
									p_world->get_ecs_context(),
									ids[entity_index],
									eZirconComponentType::
										kzircon_component_sdk_camera))
							{
								is_already_presented = true;
								break;
							}
						}

						if (is_already_presented)
						{
							p_wrapper_imgui->OpenPopup(
								_kSDKModalWindowFailedToAddComponent);

							is_valid_for_adding = false;
						}
					}

					if (this->m_current_item_name ==
						static_cast<kotek::uint32_t>(
							eZirconComponentType::
								kzircon_component_sdk_input))
					{
						// TODO(zircon): pico has no view<>, scanning all
						// entities; owner may want a cheaper check
						bool is_already_presented = false;
						kotek::entity_t
							ids[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};
						kotek::uint32_t ids_count =
							this->m_p_factory->get_all_entities(
								p_world->get_ecs_context(),
								p_world->get_entity_count_max_limit(), ids,
								ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT);
						for (kotek::uint32_t entity_index = 0;
							 entity_index < ids_count; ++entity_index)
						{
							if (this->m_p_factory->has_component(
									p_world->get_ecs_context(),
									ids[entity_index],
									eZirconComponentType::
										kzircon_component_sdk_input))
							{
								is_already_presented = true;
								break;
							}
						}

						if (is_already_presented)
						{
							p_wrapper_imgui->OpenPopup(
								_kSDKModalWindowFailedToAddComponent);
							is_valid_for_adding = false;
						}
					}

					if (this->m_current_item_name ==
						static_cast<kotek::uint32_t>(
							eZirconComponentType::
								kzircon_component_animation))
					{
						bool required_components =
							!this->m_p_factory->has_component(
								p_world->get_ecs_context(), selected_entity,
								eZirconComponentType::
									kzircon_component_geometry);

						if (required_components)
						{
							p_wrapper_imgui->OpenPopup(
								_kSDKModalWindowFailedToAddComponent);
							is_valid_for_adding = false;
						}
					}

					if (is_valid_for_adding)
					{
						p_game_manager->GetConsole()->Execute_Command(
							static_cast<Kotek::ktk::enum_base_t>(
								Kotek::Core::eConsoleCommandIndex::
									kConsoleCommand_SDK_CreateComponentForEntityByName),
							{{this->m_p_list_selected_item_allocator},
								{static_cast<kotek::uint32_t>(
									selected_entity.id)}});
					}
				}

				if (p_wrapper_imgui->Button("Delete component from list box"))
				{
					if (this->m_p_list_selected_item_allocator)
					{
						p_game_manager->GetConsole()->Execute_Command(
							static_cast<Kotek::ktk::enum_base_t>(
								Kotek::Core::eConsoleCommandIndex::
									kConsoleCommand_SDK_DeleteComponentFromEntityByName),
							{{this->m_p_list_selected_item_allocator},
								{static_cast<kotek::uint32_t>(
									selected_entity.id)}});
					}
				}

				p_wrapper_imgui->Text(Kotek::ktk::format("Selected entity: {}",
					static_cast<kotek::uint32_t>(selected_entity.id))
						.c_str());

				if (p_wrapper_imgui->BeginListBox("list"))
				{
					bool is_presented{};
					for (int component_index = 0;
						 component_index <
						 static_cast<int>(eZirconComponentType::kunknown);
						 ++component_index)
					{
						eZirconComponentType component_type =
							static_cast<eZirconComponentType>(component_index);
						const char* component_name =
							this->m_p_factory->get_component_name_by_enum(
								component_type);

						bool is_selected =
							(this->m_p_list_selected_item_allocator ==
								component_name);

						is_presented = this->HasComponent(
							component_type, selected_entity);

						if (is_presented)
						{
							if (p_wrapper_imgui->Selectable(
									component_name, &is_selected))
							{
								this->m_p_list_selected_item_allocator =
									component_name;
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
					zircon_component_interface* p_interface =
						p_world->get_factory()->get_component_by_name(
							p_world->get_ecs_context(), selected_entity,
							this->m_p_list_selected_item_allocator);

					if (p_interface)
					{
						// TODO(zircon): components lost draw_imgui in the
						// ecs refactor; per-component imgui drawing needs
						// owner decision
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

		this->update_modal_windows(p_wrapper_imgui);

		if (p_wrapper_imgui)
		{
			p_wrapper_imgui->End();
		}
	}
}

int zircon_editor_ui_window_component_inspector::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_ComponentInspector);
}

void zircon_editor_ui_window_component_inspector::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_editor_ui_window_component_inspector::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_editor_ui_window_component_inspector::Is_Shown(void) const
{
	return this->m_is_show_window;
}

bool zircon_editor_ui_window_component_inspector::HasComponent(
	eZirconComponentType component_type, kotek::entity_t id) noexcept
{
	bool result{};

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor->get_current_session_id());

	if (p_session)
	{
		zircon_world* p_world = p_session->get_world();

		if (p_world)
		{
			result = this->m_p_factory->has_component(
				p_world->get_ecs_context(), id, component_type);
		}
	}

	return result;
}

void zircon_editor_ui_window_component_inspector::update_modal_windows(
	kotek::core::ktkIImguiWrapper* p_wrapper_imgui)
{
	if (p_wrapper_imgui)
	{
		bool enable_cross = true;
		if (p_wrapper_imgui->BeginPopupModal(
				_kSDKModalWindowFailedToAddComponent, &enable_cross,
				ImGuiWindowFlags_AlwaysAutoResize))
		{
			auto
				pReasonNoRequiredComponentsPresentedInEntityForAddingComponent =
					[p_wrapper_imgui,
						this]<eZirconComponentType component_for_adding,
						eZirconComponentType... required_component_types>()
			{
				if (this->m_current_item_name ==
					static_cast<kotek::uint32_t>(component_for_adding))
				{
					p_wrapper_imgui->Text("Failed to add component [%s] "
										  "because you need to add some "
										  "of these components:",
						this->m_p_factory->get_component_name_by_enum(
							component_for_adding));

					// Print each component on a separate line
					(p_wrapper_imgui->Text("- [%s]",
						 this->m_p_factory->get_component_name_by_enum(
							 required_component_types)),
						...);
				}
			};

			auto pReasonNoMoreThanOne =
				[p_wrapper_imgui,
					this]<eZirconComponentType component_id>()
			{
				if (this->m_current_item_name ==
					static_cast<kotek::uint32_t>(component_id))
				{
					p_wrapper_imgui->Text(
						"You can't add more than one of [%s] on scene!",
						this->m_p_factory->get_component_name_by_enum(
							component_id));
				}
			};

			pReasonNoRequiredComponentsPresentedInEntityForAddingComponent
				.template
				operator()<eZirconComponentType::kzircon_component_transform,
					eZirconComponentType::kzircon_component_geometry,
					eZirconComponentType::kzircon_component_sdk_camera,
					eZirconComponentType::kzircon_component_camera>();

			pReasonNoRequiredComponentsPresentedInEntityForAddingComponent
				.template
				operator()<eZirconComponentType::kzircon_component_animation,
					eZirconComponentType::kzircon_component_geometry>();

			pReasonNoMoreThanOne.template
			operator()<eZirconComponentType::kzircon_component_sdk_camera>();
			pReasonNoMoreThanOne.template
			operator()<eZirconComponentType::kzircon_component_sdk_input>();

			p_wrapper_imgui->EndPopup();
		}
	}
}
