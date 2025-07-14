#include "zircon_ui_window_history_command_log.h"
#include "../commands/zircon_command_history.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "zircon_editor_ui_state.h"

zircon_editor_ui_window_history_command_log::zircon_editor_ui_window_history_command_log(
	zircon_editor_command_history* p_manager_history,
	zircon_session_editor_manager* p_manager_session_editor) :
	m_is_show_window(false), m_p_manager_history{p_manager_history},
	m_p_manager_session_editor{p_manager_session_editor}
{
	KOTEK_ASSERT(this->m_p_manager_history,
		"you can't pass an invalid pointer to instance "
		"zircon_editor_command_history");
	KOTEK_ASSERT(this->m_p_manager_session_editor,
		"you can't pass an invalid pointer to instance "
		"zircon_session_editor_manager");
}

zircon_editor_ui_window_history_command_log::~zircon_editor_ui_window_history_command_log() {}

void zircon_editor_ui_window_history_command_log::Initialize(void) {}

void zircon_editor_ui_window_history_command_log::Shutdown(void) {}

void zircon_editor_ui_window_history_command_log::Draw(
	Kotek::Core::ktkMainManager* main_manager)
{
	if (!this->m_is_show_window)
		return;

	auto* p_wrapper_imgui = main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->Begin("History Command Log"))
		{
			auto* p_manager = 
				main_manager->GetGameManager();

			if (p_manager)
			{
				zircon_session_editor* p_session =
					this->m_p_manager_session_editor->get_session(
						this->m_p_manager_session_editor
							->get_current_session_id());

				KOTEK_ASSERT(p_session,
					"failed to obtain session editor by id: {}",
					this->m_p_manager_session_editor->get_current_session_id());

				if (!p_session)
				{
					KOTEK_MESSAGE_WARNING("initialize session editor please!");
					return;
				}

				auto* p_history = p_session->get_command_history();

				KOTEK_ASSERT(p_history,
					"session editor_{}#{} has invalid command history",
					p_session->get_session_name(), p_session->get_id());

				if (p_history)
				{
					const auto& commands = p_history->GetCommands();

					char button_name[64]{};
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

							auto current_index = p_history->get_cursor_index() %
								zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE;
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

int zircon_editor_ui_window_history_command_log::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_HistoryCommandLog);
}

void zircon_editor_ui_window_history_command_log::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_editor_ui_window_history_command_log::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_editor_ui_window_history_command_log::Is_Shown(void) const
{
	return this->m_is_show_window;
}
