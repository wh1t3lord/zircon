#include "zircon_component_terrain_cbt.h"

zircon_component_terrain_cbt::zircon_component_terrain_cbt() :
	m_is_enabled{true}
{
}

zircon_component_terrain_cbt::~zircon_component_terrain_cbt() {}

/*
void zircon_component_terrain_cbt::draw_imgui(
    kotek::Core::ktkMainManager* p_main_manager
) noexcept
{
    if (p_main_manager)
    {
        auto* p_wrapper_imgui =
            p_main_manager->Get_ImguiWrapper();

        if (p_wrapper_imgui)
        {
            if (p_wrapper_imgui->CollapsingHeader(
                    "Component Terrain Implementation - CBT"
                ))
            {
            }
        }
    }
}*/

kotek::uint8_t
zircon_component_terrain_cbt::get_component_type(void
) const noexcept
{
	return static_cast<kotek::uint8_t>(
		eZirconComponentType::kzircon_component_terrain_cbt
	);
}

bool zircon_component_terrain_cbt::is_enabled(void
) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_terrain_cbt::set_enabled(bool status
) noexcept
{
	this->m_is_enabled = status;
}
