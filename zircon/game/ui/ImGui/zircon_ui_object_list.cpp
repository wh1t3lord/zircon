#include "zircon_ui_object_list.h"
#include "../../../ecs/components/zircon_factory.h"
#include "../../zircon_game_manager.h"
#include "../../zircon_scene_manager.h"
#include "../../../core/zircon_sdk_ui.h"

zircon_sdk_ui_object_list::zircon_sdk_ui_object_list(void) :
	m_selected_entity_id{}
{
}

zircon_sdk_ui_object_list::~zircon_sdk_ui_object_list(void) {}

void zircon_sdk_ui_object_list::initialize(void) {}

void zircon_sdk_ui_object_list::shutdown(void) {}

void zircon_sdk_ui_object_list::Draw(
	kotek::core::ktkMainManager* p_main_manager)
{
	auto* p_game_manager =
		static_cast<zircon_manager_game*>(p_main_manager->GetGameManager());

	const auto& ids =
		p_game_manager->GetSceneManager()->GetCurrentScene()->GetEntities();
	auto* p_current_scene =
		p_game_manager->GetSceneManager()->GetCurrentScene();

	kotek::core::ktkIImguiWrapper* p_wrapper_imgui =
		p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		p_wrapper_imgui->ShowDemoWindow();

		if (p_wrapper_imgui->Begin("Entity List"))
		{
			if (p_wrapper_imgui->Button("Add"))
			{
				p_game_manager->GetConsole()->Execute(
					static_cast<kotek::ktk::enum_base_t>(
						kotek::core::eConsoleCommandIndex::
							kConsoleCommand_SDK_CreateEntity));
			}

			p_wrapper_imgui->SameLine();

			if (p_wrapper_imgui->Button("Delete"))
			{
				p_game_manager->GetConsole()->Execute(
					static_cast<kotek::ktk::enum_base_t>(
						kotek::core::eConsoleCommandIndex::
							kConsoleCommand_SDK_DeleteEntity),
					{{static_cast<kotek::uint32_t>(this->m_selected_entity_id)}});
			}

			if (p_wrapper_imgui->BeginTable("", 1))
			{
				char table_column_name[32]{};
				kotek::ktk::sprintf(table_column_name,
					sizeof(table_column_name), "Entity ID [%d]", ids.size_hint());

				p_wrapper_imgui->TableSetupColumn(
					table_column_name, ImGuiTableColumnFlags_WidthStretch);
				p_wrapper_imgui->TableHeadersRow();
				
				kotek::size_t i = 0;
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
							"{} [{}]", static_cast<kotek::uint32_t>(id), component.GetName());
					}
					else
					{
						converted_id = Kotek::ktk::format("{} {}", i, static_cast<kotek::uint32_t>(id));
					}

					if (p_wrapper_imgui->Selectable(converted_id.c_str(),
							id == this->m_selected_entity_id))
					{
						this->m_selected_entity_id = id;
						p_game_manager->GetSDKUI()->imgui_SetSelectedEntity(
							&this->m_selected_entity_id);
					}
					++i;
				}

				p_wrapper_imgui->EndTable();
			}
		}

		p_wrapper_imgui->End();
	}
}