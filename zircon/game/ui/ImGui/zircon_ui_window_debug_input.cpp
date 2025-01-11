#include "zircon_ui_window_debug_input.h"
#include "../../../core/zircon_sdk_ui.h"
#include "../../zircon_game_manager.h"

zircon_sdk_ui_window_debug_input::zircon_sdk_ui_window_debug_input() :
	m_is_show_window{}, m_state_keys_buffer{}
{
}

zircon_sdk_ui_window_debug_input::~zircon_sdk_ui_window_debug_input() {}

void zircon_sdk_ui_window_debug_input::initialize(void) {}

void zircon_sdk_ui_window_debug_input::shutdown(void) {}

void zircon_sdk_ui_window_debug_input::Draw(
	kotek::core::ktkMainManager* p_main_manager)
{
	if (!p_main_manager)
		return;

	if (!this->m_is_show_window)
		return;

	auto* p_input = p_main_manager->Get_Input();

	if (!p_input)
	{
		KOTEK_MESSAGE_WARNING(
			"engine: doesn't have initialize input manager (nullptr). Can't "
		    "open window = zircon_sdk_ui_window_debug_input");
		return;
	}

	auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->Begin("Debug - Input"))
		{
			p_input->WriteKeyAsStringToBuffer_IfPressed(
				kotek::core::eInputControllerType::kControllerKeyboard,
				this->m_state_keys_buffer, sizeof(this->m_state_keys_buffer));

			p_wrapper_imgui->Text("Pressed: [%s]", this->m_state_keys_buffer);
			kotek::ktk::memory::memset(this->m_state_keys_buffer, 0,
				sizeof(this->m_state_keys_buffer));

			p_wrapper_imgui->Separator();

			p_wrapper_imgui->Text("Holding: [%s]", this->m_state_keys_buffer);
			kotek::ktk::memory::memset(this->m_state_keys_buffer, 0,
				sizeof(this->m_state_keys_buffer));

			p_wrapper_imgui->Separator();

			p_wrapper_imgui->Text("Release: [%s]", this->m_state_keys_buffer);
			kotek::ktk::memory::memset(this->m_state_keys_buffer, 0,
				sizeof(this->m_state_keys_buffer));

			p_wrapper_imgui->End();
		}
	}
}

int zircon_sdk_ui_window_debug_input::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_Debug_Input);
}

void zircon_sdk_ui_window_debug_input::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_sdk_ui_window_debug_input::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_sdk_ui_window_debug_input::Is_Shown(void) const
{
	return this->m_is_show_window;
}
