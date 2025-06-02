#include "zircon_ui_top_bar.h"
#include "../session/zircon_session_editor.h"
#include "zircon_editor_ui_state.h"
#include "../session/zircon_session_editor_manager.h"

zircon_editor_ui_state_top_bar::zircon_editor_ui_state_top_bar(
	zircon_session_editor_manager* p_manager_session_editor) :
	m_is_show_window(true), m_p_manager_session_editor{p_manager_session_editor}
{
	KOTEK_ASSERT(p_manager_session_editor,
		"must be valid session editor manager pointer");
}

zircon_editor_ui_state_top_bar::~zircon_editor_ui_state_top_bar(void) {}

void zircon_editor_ui_state_top_bar::initialize(void) {}

void zircon_editor_ui_state_top_bar::shutdown(void) {}

void zircon_editor_ui_state_top_bar::Draw(
	kotek::core::ktkMainManager* p_main_manager)
{
	kotek::core::ktkIImguiWrapper* p_wrapper_imgui =
		p_main_manager->Get_ImguiWrapper();

	this->update_modals(p_main_manager);

	if (p_wrapper_imgui)
	{
		if (this->m_is_show_window)
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
								reinterpret_cast<const char*>(
									utf8_path.u8string().c_str());

							KOTEK_MESSAGE("opening scene: {}", p_utf8_path);

							p_main_manager->GetGameManager()
								->GetConsole()
								->Execute_Command(
									static_cast<kotek::ktk::enum_base_t>(
										kotek::core::eConsoleCommandIndex::
											kConsoleCommand_SDK_LoadScene),
									{utf8_path});
						}
#endif
					}

					if (p_wrapper_imgui->MenuItem("Save"))
					{
						p_main_manager->GetGameManager()
							->GetConsole()
							->Execute_Command(
								static_cast<kotek::ktk::enum_base_t>(
									kotek::core::eConsoleCommandIndex::
										kConsoleCommand_SDK_SaveScene));
					}

					if (p_wrapper_imgui->MenuItem("Close Current Project"))
					{
						p_main_manager->GetGameManager()
							->GetConsole()
							->Push_Command(static_cast<kotek::ktk::enum_base_t>(
								kotek::core::eConsoleCommandIndex::
									kConsoleCommand_SDK_CloseCurrentScene));
					}

					if (p_wrapper_imgui->MenuItem("Exit"))
					{
						p_main_manager->GetGameManager()
							->GetConsole()
							->Push_Command(static_cast<kotek::ktk::enum_base_t>(
								kotek::core::eConsoleCommandIndex::
									kConsoleCommand_SDK_ShowModalWindow_SaveAndCloseOrCloseScene));
					}

					p_wrapper_imgui->EndMenu();
				}

				if (p_wrapper_imgui->BeginMenu("Edit"))
				{
					if (p_wrapper_imgui->MenuItem("Undo"))
					{
						p_main_manager->GetGameManager()
							->GetConsole()
							->Push_Command(static_cast<kotek::ktk::enum_base_t>(
								kotek::core::eConsoleCommandIndex::
									kConsoleCommand_SDK_Undo));
					}

					if (p_wrapper_imgui->MenuItem("Redo"))
					{
						p_main_manager->GetGameManager()
							->GetConsole()
							->Push_Command(static_cast<kotek::ktk::enum_base_t>(
								kotek::core::eConsoleCommandIndex::
									kConsoleCommand_SDK_Redo));
					}

					p_wrapper_imgui->EndMenu();
				}

				if (p_wrapper_imgui->BeginMenu(
						"Tools##ZirconImGuiSDK_MainBar_Tools"))
				{
					if (p_wrapper_imgui->BeginMenu(
							"Debug##ZirconImGuiSDK_MainBar_Tools_Debug"))
					{
						if (p_wrapper_imgui->MenuItem("Input"))
						{
						}

						p_wrapper_imgui->EndMenu();
					}

					p_wrapper_imgui->EndMenu();
				}

				if (p_wrapper_imgui->BeginMenu(
						"View##ZirconImGuiSDK_MainBar_View"))
				{
					if (p_wrapper_imgui->BeginMenu("Show windows"))
					{
						auto* p_renderer =
							p_main_manager->GetGameManager()->GetRenderer();

						if (p_renderer)
						{
							auto* p_game_manager =
								p_main_manager->GetGameManager();

							if (p_game_manager &&
								this->m_p_manager_session_editor)
							{
								zircon_session_editor* p_session =
									this->m_p_manager_session_editor
										->get_session(
											this->m_p_manager_session_editor
												->get_current_session_id());

								KOTEK_ASSERT(p_session,
									"failed to obtain session editor by id: {}",
									this->m_p_manager_session_editor
										->get_current_session_id());

								const auto& imgui_elements =
									p_session->get_imgui_ui_elements();

								for (const auto& p_element : imgui_elements)
								{
									const char* p_window_name =
										Translate_ZirconWindowIDs(
											static_cast<eZirconWindowIDs>(
												p_element->Get_ID()));

									char build_window_name[64];

									kotek::ktk::sprintf(build_window_name,
										sizeof(build_window_name),
										"%s##ViewImGui", p_window_name);

									bool is_shown = p_element->Is_Shown();
									if (p_wrapper_imgui->MenuItem(
											build_window_name, nullptr,
											is_shown))
									{
										if (p_element->Is_Shown() == false)
										{
											p_main_manager->GetGameManager()
												->GetConsole()
												->Push_Command(
													static_cast<kotek::ktk::
															enum_base_t>(
														kotek::core::
															eConsoleCommandIndex::
																kConsoleCommand_SDK_ShowWindow),
													{p_element->Get_ID()});
										}
										else
										{
											p_main_manager->GetGameManager()
												->GetConsole()
												->Push_Command(
													static_cast<kotek::ktk::
															enum_base_t>(
														kotek::core::
															eConsoleCommandIndex::
																kConsoleCommand_SDK_HideWindow),
													{p_element->Get_ID()});
										}
									}
								}
							}
						}

						p_wrapper_imgui->EndMenu();
					}

					p_wrapper_imgui->EndMenu();
				}
			}

			p_wrapper_imgui->EndMainMenuBar();
		}
	}
}

int zircon_editor_ui_state_top_bar::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_Topbar);
}

void zircon_editor_ui_state_top_bar::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_editor_ui_state_top_bar::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_editor_ui_state_top_bar::Is_Shown(void) const
{
	return this->m_is_show_window;
}

void zircon_editor_ui_state_top_bar::update_modal_save_scene(
	kotek::core::ktkMainManager* p_main_manager)
{
	auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		auto* p_game_manager = p_main_manager->GetGameManager();

		if (p_game_manager && this->m_p_manager_session_editor)
		{
			zircon_session_editor* p_session =
				this->m_p_manager_session_editor->get_session(
					this->m_p_manager_session_editor->get_current_session_id());

			KOTEK_ASSERT(p_session, "must be valid!");

			if (!p_session)
			{
				KOTEK_MESSAGE_WARNING("initialize session editor please!");
				return;
			}

			zircon_editor_ui_state* p_state = p_session->get_ui_state();

			KOTEK_ASSERT(p_state, "ui state wasn't initialized");

			if (p_state->is_imgui_show_modal_save_scene())
			{
				p_wrapper_imgui->OpenPopup("Save Scene");
			}

			if (p_wrapper_imgui->BeginPopupModal(
					"Save Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				p_wrapper_imgui->Text(
					"Do you want to save your work before quitting?");

				if (p_wrapper_imgui->Button("Yes"))
				{
					p_main_manager->GetGameManager()
						->GetConsole()
						->Push_Command(static_cast<kotek::ktk::enum_base_t>(
							kotek::core::eConsoleCommandIndex::
								kConsoleCommand_SDK_SaveScene));

					p_main_manager->GetGameManager()
						->GetConsole()
						->Push_Command(static_cast<kotek::ktk::enum_base_t>(
										   kotek::core::eConsoleCommandIndex::
											   kConsoleCommand_App_Close),
							{});

					p_state->set_imgui_show_modal_save_scene(false);
				}

				p_wrapper_imgui->SameLine();

				if (p_wrapper_imgui->Button("No"))
				{
					p_main_manager->GetGameManager()
						->GetConsole()
						->Push_Command(static_cast<kotek::ktk::enum_base_t>(
										   kotek::core::eConsoleCommandIndex::
											   kConsoleCommand_App_Close),
							{});

					p_state->set_imgui_show_modal_save_scene(false);
				}

				p_wrapper_imgui->EndPopup();
			}
		}
	}
}
void zircon_editor_ui_state_top_bar::update_modals(
	kotek::core::ktkMainManager* p_main_manager)
{
	this->update_modal_save_scene(p_main_manager);
}
