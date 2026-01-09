#include "zircon_component_terrain_gcm.h"

zircon_component_terrain_gcm::zircon_component_terrain_gcm() :
	m_is_enabled{true}
{
}

zircon_component_terrain_gcm::~zircon_component_terrain_gcm() {}

/*
void zircon_component_terrain_gcm::draw_imgui(
    Kotek::Core::ktkMainManager* p_main_manager
) noexcept
{
    if (p_main_manager)
    {
        auto* p_wrapper_imgui =
            p_main_manager->Get_ImguiWrapper();

        if (p_wrapper_imgui)
        {
            if (p_wrapper_imgui->CollapsingHeader(""))
            {
            }
        }
    }
}*/

kotek::uint8_t
zircon_component_terrain_gcm::get_component_type(void
) const noexcept
{
	return static_cast<kotek::uint8_t>(
		eZirconComponentType::kzircon_component_terrain_gcm
	);
}

bool zircon_component_terrain_gcm::is_enabled(void
) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_terrain_gcm::set_enabled(bool status
) noexcept
{
	this->m_is_enabled = status;
}
