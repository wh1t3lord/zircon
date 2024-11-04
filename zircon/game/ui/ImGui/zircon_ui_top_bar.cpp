#include "zircon_ui_top_bar.h"

#include "../../zircon_game_manager.h"
#include "../../../core/zircon_sdk_ui.h"

zircon_sdk_ui_top_bar::zircon_sdk_ui_top_bar(void) {}

zircon_sdk_ui_top_bar::~zircon_sdk_ui_top_bar(void) {}

void zircon_sdk_ui_top_bar::initialize(void) {}

void zircon_sdk_ui_top_bar::shutdown(void) {}

void zircon_sdk_ui_top_bar::Draw(kotek::core::ktkMainManager* p_main_manager)
{
	kotek::core::ktkIImguiWrapper* p_wrapper_imgui =
		p_main_manager->Get_ImguiWrapper();

	this->update_modals(p_main_manager);

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->BeginMainMenuBar())
		{
			if (p_wrapper_imgui->BeginMenu("File"))
			{
				if (p_wrapper_imgui->MenuItem("Open"))
				{
#ifdef KOTEK_PLATFORM_WINDOWS
					OPENFILENAME ofn;
					TCHAR szFile[MAX_PATH] = {0};

					ZeroMemory(&ofn, sizeof(ofn));

					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = nullptr;
					ofn.lpstrFile = szFile;
					ofn.nMaxFile = sizeof(szFile);
					ofn.lpstrFilter = TEXT("JSON files (*.json)\0*.json\0");
					ofn.nFilterIndex = 1;
					ofn.lpstrFileTitle = NULL;
					ofn.nMaxFileTitle = 0;
					ofn.lpstrInitialDir = NULL;
					ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

					if (GetOpenFileName(&ofn) == TRUE)
					{
						const auto& utf8_path =
							kotek::static_path_t(ofn.lpstrFile);

						const char* p_utf8_path =
							reinterpret_cast<const char*>(utf8_path.u8string().c_str());

						KOTEK_MESSAGE("opening scene: {}", p_utf8_path);

						p_main_manager->GetGameManager()->GetConsole()->Execute(
							static_cast<Kotek::ktk::enum_base_t>(
								Kotek::Core::eConsoleCommandIndex::
									kConsoleCommand_SDK_LoadScene),
							{utf8_path});
					}
#endif
				}

				if (p_wrapper_imgui->MenuItem("Save"))
				{
					p_main_manager->GetGameManager()->GetConsole()->Execute(
						static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eConsoleCommandIndex::
								kConsoleCommand_SDK_SaveScene));
				}

				if (p_wrapper_imgui->MenuItem("Close Current Project"))
				{
					p_main_manager->GetGameManager()->GetConsole()->PushCommand(
						static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eConsoleCommandIndex::
								kConsoleCommand_SDK_CloseCurrentScene));
				}

				if (p_wrapper_imgui->MenuItem("Exit"))
				{
					p_main_manager->GetGameManager()->GetConsole()->PushCommand(
						static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eConsoleCommandIndex::
								kConsoleCommand_SDK_ShowModalWindow_SaveAndCloseOrCloseScene));
				}

				p_wrapper_imgui->EndMenu();
			}

			if (p_wrapper_imgui->BeginMenu("Edit"))
			{
				if (p_wrapper_imgui->MenuItem("Undo"))
				{
					p_main_manager->GetGameManager()->GetConsole()->PushCommand(
						static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eConsoleCommandIndex::
								kConsoleCommand_SDK_Undo));
				}

				if (p_wrapper_imgui->MenuItem("Redo"))
				{
					p_main_manager->GetGameManager()->GetConsole()->PushCommand(
						static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eConsoleCommandIndex::
								kConsoleCommand_SDK_Redo));
				}

				p_wrapper_imgui->EndMenu();
			}
		}
		p_wrapper_imgui->EndMainMenuBar();
	}
}

void zircon_sdk_ui_top_bar::update_modal_save_scene(
	Kotek::Core::ktkMainManager* p_main_manager)
{
	auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		auto* p_game_manager =
			static_cast<zircon_manager_game*>(p_main_manager->GetGameManager());

		if (p_game_manager->get_sdk_ui()
				->is_imgui_show_modal_save_scene())
		{
			p_wrapper_imgui->OpenPopup("Modal - Save Scene");
		}

		if (p_wrapper_imgui->BeginPopupModal("Modal - Save Scene",
				nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			p_wrapper_imgui->Text("Do you want to save?");

			if (p_wrapper_imgui->Button("Yes"))
			{
				p_main_manager->GetGameManager()->GetConsole()->PushCommand(
					static_cast<Kotek::ktk::enum_base_t>(
						Kotek::Core::eConsoleCommandIndex::
							kConsoleCommand_SDK_SaveScene));

				p_main_manager->GetGameManager()->GetConsole()->PushCommand(
					static_cast<Kotek::ktk::enum_base_t>(Kotek::Core::
							eConsoleCommandIndex::kConsoleCommand_App_Close),
					{kotek::static_cstring_t<8>("false")});

				p_game_manager->get_sdk_ui()
					->set_imgui_show_modal_save_scene(false);
			}

			p_wrapper_imgui->SameLine();

			if (p_wrapper_imgui->Button("No"))
			{
				p_main_manager->GetGameManager()->GetConsole()->PushCommand(
					static_cast<Kotek::ktk::enum_base_t>(Kotek::Core::
							eConsoleCommandIndex::kConsoleCommand_App_Close),
					{kotek::static_cstring_t<8>("false")});

				p_game_manager->get_sdk_ui()
					->set_imgui_show_modal_save_scene(false);
			}

			p_wrapper_imgui->EndPopup();
		}
	}
}
void zircon_sdk_ui_top_bar::update_modals(
	Kotek::Core::ktkMainManager* p_main_manager)
{
	this->update_modal_save_scene(p_main_manager);
}
