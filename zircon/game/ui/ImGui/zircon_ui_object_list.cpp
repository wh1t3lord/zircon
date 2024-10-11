#include "zircon_ui_object_list.h"
#include "../../../ecs/components/zircon_factory.h"
#include "../../zircon_game_manager.h"
#include "../../zircon_scene_manager.h"
#include "../../../core/zircon_sdk_ui.h"

namespace kotek = Kotek;

zircon_sdk_ui_object_list::zircon_sdk_ui_object_list(void) :
	m_selected_entity_id{}
{
}

zircon_sdk_ui_object_list::~zircon_sdk_ui_object_list(void) {}

void zircon_sdk_ui_object_list::initialize(void) {}

void zircon_sdk_ui_object_list::shutdown(void) {}

void zircon_sdk_ui_object_list::Draw(
	Kotek::Core::ktkMainManager* p_main_manager)
{
	auto* p_game_manager =
		static_cast<zircon_manager_game*>(p_main_manager->GetGameManager());

	const auto& ids =
		p_game_manager->GetSceneManager()->GetCurrentScene()->GetEntities();
	auto* p_current_scene =
		p_game_manager->GetSceneManager()->GetCurrentScene();

	Kotek::Core::ktkIImguiWrapper* p_wrapper_imgui =
		p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		p_wrapper_imgui->ShowDemoWindow();

		if (p_wrapper_imgui->Begin("Entity List"))
		{
			if (p_wrapper_imgui->Button("Add"))
			{
				p_game_manager->GetConsole()->Execute(
					static_cast<Kotek::ktk::enum_base_t>(
						Kotek::Core::eConsoleCommandIndex::
							kConsoleCommand_SDK_CreateEntity));
			}

			p_wrapper_imgui->SameLine();

			if (p_wrapper_imgui->Button("Delete"))
			{
				p_game_manager->GetConsole()->Execute(
					static_cast<Kotek::ktk::enum_base_t>(
						Kotek::Core::eConsoleCommandIndex::
							kConsoleCommand_SDK_DeleteEntity),
					{{this->m_selected_entity_id}});
			}

			if (p_wrapper_imgui->BeginTable("", 1))
			{
				char table_column_name[32]{};
				kotek::ktk::sprintf(table_column_name,
					sizeof(table_column_name), "Entity ID [%d]", ids.size());

				p_wrapper_imgui->TableSetupColumn(
					table_column_name, ImGuiTableColumnFlags_WidthStretch);
				p_wrapper_imgui->TableHeadersRow();
				
				for (auto id : ids)
				{
					p_wrapper_imgui->TableNextRow();
					p_wrapper_imgui->TableSetColumnIndex(0);

					Kotek::ktk::cstring converted_id;

					if (p_game_manager->get_factory_game()
							->HasComponent<zircon_component_sdk_scene_name>(id))
					{
						auto component =
							p_game_manager->get_factory_game()
								->GetComponent<

									zircon_component_sdk_scene_name>(id);

						converted_id = Kotek::ktk::format(
							"{} [{}]", id, component.GetName());
					}
					else
					{
						converted_id = Kotek::ktk::format("{}", id);
					}

					if (p_wrapper_imgui->Selectable(converted_id.c_str(),
							id == this->m_selected_entity_id))
					{
						this->m_selected_entity_id = id;
						p_game_manager->GetSDKUI()->imgui_SetSelectedEntity(
							&this->m_selected_entity_id);
					}
				}

				p_wrapper_imgui->EndTable();
			}
		}

		p_wrapper_imgui->End();
	}
}