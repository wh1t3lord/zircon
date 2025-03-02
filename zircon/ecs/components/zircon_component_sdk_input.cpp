#include "zircon_component_sdk_input.h"

zircon_component_sdk_input::zircon_component_sdk_input() {}

zircon_component_sdk_input::zircon_component_sdk_input(
	const kotek::core::ktkIInput* p_input) : m_input(p_input)
{
}

zircon_component_sdk_input::~zircon_component_sdk_input() {}

const zircon_component_input& zircon_component_sdk_input::get_input(
	void) const noexcept
{
	return this->m_input;
}

zircon_component_input& zircon_component_sdk_input::get_input(void) noexcept
{
	return this->m_input;
}

void zircon_component_sdk_input::DrawImGui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		this->m_input.DrawImGui(p_main_manager);
	}
}
