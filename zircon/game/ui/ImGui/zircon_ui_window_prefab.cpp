#include "zircon_ui_window_prefab.h"
#include "../../../core/zircon_sdk_ui.h"

zircon_sdk_ui_window_prefab::zircon_sdk_ui_window_prefab(void) :
	m_is_show_window(false)
{
}

zircon_sdk_ui_window_prefab::~zircon_sdk_ui_window_prefab(void) {}

void zircon_sdk_ui_window_prefab::initialize(void) {}

void zircon_sdk_ui_window_prefab::shutdown(void) {}

void zircon_sdk_ui_window_prefab::Draw(
	kotek::core::ktkMainManager* p_main_manager)
{
}

int zircon_sdk_ui_window_prefab::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_Prefab);
}

void zircon_sdk_ui_window_prefab::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_sdk_ui_window_prefab::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_sdk_ui_window_prefab::Is_Shown(void) const
{
	return this->m_is_show_window;
}
