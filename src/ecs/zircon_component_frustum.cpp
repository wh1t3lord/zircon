#include "zircon_component_frustum.h"

zircon_component_frustum::zircon_component_frustum(void) :
	m_is_enabled{true}
{
}

zircon_component_frustum::~zircon_component_frustum(void) {}

/*
void zircon_component_frustum::draw_imgui(
	kotek::Core::ktkMainManager* p_main_manager
) noexcept
{
	if (p_main_manager)
	{
		auto* p_wrapper_imgui =
			p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			if (p_wrapper_imgui->CollapsingHeader("Frustum"))
			{
			}
		}
	}
}*/

kotek::uint8_t zircon_component_frustum::get_component_type(void
) const noexcept
{
	return static_cast<kotek::uint8_t>(
		eZirconComponentType::kzircon_component_frustum
	);
}

bool zircon_component_frustum::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_frustum::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}
