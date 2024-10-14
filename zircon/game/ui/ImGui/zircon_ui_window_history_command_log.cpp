#include "zircon_ui_window_history_command_log.h"
#include "../../zircon_game_manager.h"
#include "../../zircon_command_history.h"

zircon_ui_window_history_command_log::zircon_ui_window_history_command_log(
	zircon_command_history* p_manager_history) :
	m_p_manager_history{p_manager_history}
{
	KOTEK_ASSERT(this->m_p_manager_history,
		"you can't pass an invalid pointer to instance "
		"zircon_command_history");
}

zircon_ui_window_history_command_log::~zircon_ui_window_history_command_log() {}

void zircon_ui_window_history_command_log::initialize(void) {}

void zircon_ui_window_history_command_log::shutdown(void) {}

void zircon_ui_window_history_command_log::Draw(
	Kotek::Core::ktkMainManager* main_manager)
{
	auto* p_wrapper_imgui = main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->Begin("History Command Log"))
		{
			auto* p_manager =
				static_cast<zircon_manager_game*>(
					main_manager->GetGameManager());

			if (p_manager)
			{
				auto* p_history = p_manager->GetCommandHistoryManager();

				if (p_history)
				{
					const auto& commands = p_history->GetCommands();

					/*
					for (auto p_command : commands)
					{
						if (p_command)
						{
							p_wrapper_imgui->Selectable(p_command->GetName());
						}
					}
					*/

					char button_name[32]{};
					for (int i = 0;
						 i < zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE; ++i)
					{
						auto* p_command = commands[i];

						if (p_command)
						{
							kotek::ktk::sprintf(button_name,
								sizeof(button_name), "[%d] %s", i,
								p_command->GetName());

							bool selected{};

							auto current_index = p_history->get_current_index();
							selected = i == current_index;

							if (i > current_index)
								p_wrapper_imgui->BeginDisabled();

							p_wrapper_imgui->Selectable(button_name, &selected);

							if (i > current_index)
								p_wrapper_imgui->EndDisabled();
						}
					}
				}
			}
		}

		p_wrapper_imgui->End();
	}
}
