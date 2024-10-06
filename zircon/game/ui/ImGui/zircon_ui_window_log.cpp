#include "zircon_ui_window_log.h"

zircon_sdk_ui_window_log::zircon_sdk_ui_window_log() {}

zircon_sdk_ui_window_log::~zircon_sdk_ui_window_log() {}

void zircon_sdk_ui_window_log::initialize(void) {}

void zircon_sdk_ui_window_log::shutdown(void) {}

void zircon_sdk_ui_window_log::Draw(Kotek::Core::ktkMainManager* main_manager)
{
	auto* p_wrapper_imgui = main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->Begin("Log"))
		{
		}

		p_wrapper_imgui->End();
	}
}
