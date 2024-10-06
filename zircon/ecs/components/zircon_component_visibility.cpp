#include "zircon_component_visibility.h"
#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

zircon_component_visibility::zircon_component_visibility(void) :
	m_is_visible(true)
{
}

zircon_component_visibility::~zircon_component_visibility(void) {}

void zircon_component_visibility::SetVisible(bool is_visible) noexcept
{
	this->m_is_visible = is_visible;
}

bool zircon_component_visibility::GetVisible(void) const noexcept
{
	return this->m_is_visible;
}

void zircon_component_visibility::Clear(void) noexcept
{
	this->m_is_visible = true;
}

void zircon_component_visibility::DrawImGui(
	Kotek::Core::ktkMainManager* main_manager) noexcept
{
	Kotek::Core::ktkIImguiWrapper* p_wrapper_imgui =
		main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui->CollapsingHeader("Component Visibility"))
	{
		p_wrapper_imgui->Checkbox("visibility", &this->m_is_visible);
	}
}
