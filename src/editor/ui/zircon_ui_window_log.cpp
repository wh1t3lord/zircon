#include "zircon_ui_window_log.h"
#include "zircon_editor_ui_state.h"

zircon_editor_ui_window_log::zircon_editor_ui_window_log() : m_is_show_window(false)
{
}

zircon_editor_ui_window_log::~zircon_editor_ui_window_log() {}

void zircon_editor_ui_window_log::Initialize(void) {}

void zircon_editor_ui_window_log::Shutdown(void) {}

void zircon_editor_ui_window_log::Draw(kotek::Core::ktkMainManager* main_manager)
{
	if (!this->m_is_show_window)
		return;

	auto* p_wrapper_imgui = main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->Begin("Log"))
		{
		}

		p_wrapper_imgui->End();
	}
}

int zircon_editor_ui_window_log::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_Log);
}

void zircon_editor_ui_window_log::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_editor_ui_window_log::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_editor_ui_window_log::Is_Shown(void) const
{
	return this->m_is_show_window;
}
