#include "zircon_ui_object_list.h"
#include "../../ecs/components/zircon_factory.h"
#include "../../engine/zircon_game_manager.h"
#include "../../world/zircon_world.h"
#include "../../world/zircon_world_manager.h"
#include "../zircon_session_editor.h"
#include "zircon_editor_ui_state.h"

zircon_editor_ui_state_object_list::zircon_editor_ui_state_object_list(void) :
	m_is_show_window{}, m_amount_of_entites{}, m_selected_entity_id{}
{
}

zircon_editor_ui_state_object_list::~zircon_editor_ui_state_object_list(void) {}

void zircon_editor_ui_state_object_list::initialize(void) {}

void zircon_editor_ui_state_object_list::shutdown(void) {}

void zircon_editor_ui_state_object_list::Draw(
	kotek::core::ktkMainManager* p_main_manager)
{
	auto* p_game_manager =
		static_cast<zircon_game_manager*>(p_main_manager->GetGameManager());

	KOTEK_ASSERT(p_game_manager,
		"you didn't initialize game manager pointer in main manager!");

	if (!p_game_manager)
	{
		KOTEK_MESSAGE_WARNING("set game manager to main manager!");
		return;
	}

	zircon_session_editor* p_session = p_game_manager->get_session_editor(
		p_game_manager->get_session_editor_id());

	KOTEK_ASSERT(p_session, "failed to obtain session editor by id: {}",
		p_game_manager->get_session_editor_id());

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING("initialize session editor!");
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(p_world,
		"session editor_{}#{} must contain a valid pointer of zircon_world",
		p_session->get_session_name(), p_session->get_id());

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING("set world to session editor_{}#{}",
			p_session->get_session_name(), p_session->get_id());
		return;
	}

	const auto& ids = p_world->get_entities();

	kotek::core::ktkIImguiWrapper* p_wrapper_imgui =
		p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		p_wrapper_imgui->ShowDemoWindow();

		if (!this->m_is_show_window)
			return;

		if (p_wrapper_imgui->Begin("Entity List"))
		{
			if (p_wrapper_imgui->Button("Add"))
			{
				p_game_manager->GetConsole()->Execute_Command(
					static_cast<kotek::ktk::enum_base_t>(
						kotek::core::eConsoleCommandIndex::
							kConsoleCommand_SDK_CreateEntity));
			}

			p_wrapper_imgui->SameLine();

			if (p_wrapper_imgui->Button("Delete"))
			{
				if (p_game_manager)
				{
					zircon_factory* p_factory = p_world->get_factory();

					if (p_factory)
					{
						if (p_factory->IsValidEntity(
								this->m_selected_entity_id))
						{
							p_game_manager->GetConsole()->Execute_Command(
								static_cast<kotek::ktk::enum_base_t>(
									kotek::core::eConsoleCommandIndex::
										kConsoleCommand_SDK_DeleteEntity),
								{{static_cast<kotek::uint32_t>(
									this->m_selected_entity_id)}});
						}
					}
				}
			}

			if (p_wrapper_imgui->BeginTable("", 1))
			{
				char table_column_name[32]{};
				kotek::ktk::sprintf(table_column_name,
					sizeof(table_column_name), "Entity ID [%zu]",
					this->m_amount_of_entites);
				p_wrapper_imgui->TableSetupColumn(
					table_column_name, ImGuiTableColumnFlags_WidthStretch);
				p_wrapper_imgui->TableHeadersRow();

				kotek::size_t i = 0;
				for (auto id : ids)
				{
					p_wrapper_imgui->TableNextRow();
					p_wrapper_imgui->TableSetColumnIndex(0);

					kotek::static_cstring_t<128> converted_id;

					bool has_sdk_name =
						p_world->get_factory()
							->HasComponent<zircon_component_sdk_scene_name>(id);
					if (has_sdk_name)
					{
						auto component =
							p_world->get_factory()
								->GetComponent<

									zircon_component_sdk_scene_name>(id);

						converted_id = kotek::ktk::static_format<128>(
							"[{}]", component.get_name());
					}
					else
					{
						converted_id = Kotek::ktk::static_format<128>(
							"{}", static_cast<kotek::uint32_t>(id));
					}

					if (p_wrapper_imgui->Selectable(converted_id.c_str(),
							id == this->m_selected_entity_id))
					{
						this->m_selected_entity_id = id;
						KOTEK_ASSERT(
							p_session->get_ui_state(), "must be valid!");
						p_session->get_ui_state()->set_selected_entity(
							this->m_selected_entity_id);
					}

					if (has_sdk_name)
					{
						p_wrapper_imgui->SameLine();
						p_wrapper_imgui->TextDisabled(
							"(%d)", static_cast<kotek::uint32_t>(id));
					}

					++i;
				}

				this->m_amount_of_entites = i;
				p_wrapper_imgui->EndTable();
			}
		}

		p_wrapper_imgui->End();
	}
}

int zircon_editor_ui_state_object_list::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_ObjectList);
}

void zircon_editor_ui_state_object_list::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_editor_ui_state_object_list::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_editor_ui_state_object_list::Is_Shown(void) const
{
	return this->m_is_show_window;
}
