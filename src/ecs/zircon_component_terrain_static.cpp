#include "zircon_component_terrain_static.h"

zircon_component_terrain_static::
	zircon_component_terrain_static() : m_is_enabled{true}
{
}

zircon_component_terrain_static::
	~zircon_component_terrain_static()
{
}

/*
void zircon_component_terrain_static::draw_imgui(
    kotek::core::ktkMainManager* p_main_manager) noexcept
{
    if (p_main_manager)
    {
        auto* p_wrapper_imgui =
p_main_manager->Get_ImguiWrapper();

        if (p_wrapper_imgui)
        {
            if (p_wrapper_imgui->CollapsingHeader(
                    "Component Terrain Implementation - CBT"))
            {
            }
        }
    }
}*/

kotek::uint8_t
zircon_component_terrain_static::get_component_type(void
) const noexcept
{
	return static_cast<kotek::uint8_t>(
		eZirconComponentType::kzircon_component_terrain_static
	);
}

bool zircon_component_terrain_static::is_enabled(void
) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_terrain_static::set_enabled(bool status
) noexcept
{
	this->m_is_enabled = status;
}
