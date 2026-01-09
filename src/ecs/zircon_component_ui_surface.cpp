#include "zircon_component_ui_surface.h"

zircon_component_ui_surface::zircon_component_ui_surface() :
	m_is_enabled{true}
{
}

zircon_component_ui_surface::~zircon_component_ui_surface() {}

/*
void zircon_component_ui_surface::draw_imgui(
    Kotek::Core::ktkMainManager* main_manager
) noexcept
{
}*/

kotek::uint8_t
zircon_component_ui_surface::get_component_type(void
) const noexcept
{
	return static_cast<Kotek::uint8_t>(
		eZirconComponentType::kzircon_component_ui_surface
	);
}

bool zircon_component_ui_surface::is_enabled(void
) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_ui_surface::set_enabled(bool status
) noexcept
{
	this->m_is_enabled = status;
}
