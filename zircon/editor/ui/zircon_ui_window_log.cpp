#include "zircon_ui_window_log.h"
#include "../../../core/zircon_sdk_ui.h"

zircon_sdk_ui_window_log::zircon_sdk_ui_window_log() : m_is_show_window(false)
{
}

zircon_sdk_ui_window_log::~zircon_sdk_ui_window_log() {}

void zircon_sdk_ui_window_log::initialize(void) {}

void zircon_sdk_ui_window_log::shutdown(void) {}

void zircon_sdk_ui_window_log::Draw(Kotek::Core::ktkMainManager* main_manager)
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

int zircon_sdk_ui_window_log::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_Log);
}

void zircon_sdk_ui_window_log::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_sdk_ui_window_log::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_sdk_ui_window_log::Is_Shown(void) const
{
	return this->m_is_show_window;
}
