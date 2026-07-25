#include "zircon_component_sdk_input.h"

zircon_component_sdk_input::zircon_component_sdk_input() :
	m_is_enabled{true}
{
}

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

/*
void zircon_component_sdk_input::draw_imgui(
	kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		this->m_input.draw_imgui(p_main_manager);
	}
}

kotek::json::value zircon_component_sdk_input::serialize(void) noexcept
{
	return kotek::json::value_from(*this);
}

void zircon_component_sdk_input::deserialize(
	const kotek::json::value& data) noexcept
{
	*this = kotek::json::value_to<zircon_component_sdk_input>(data);
}

kotek::json::value zircon_component_sdk_input::serialize(
	unsigned char* p_raw_memory, kotek::size_t size)
{
	KOTEK_ASSERT(p_raw_memory, "you passed an invalid part of memory!");
	kotek::json::static_resource res(p_raw_memory, size);
	kotek::json::storage_ptr ptr(&res);
	return kotek::json::value_from(*this, ptr);
}
*/

kotek::uint8_t zircon_component_sdk_input::get_component_type(
	void) const noexcept
{
	return static_cast<kotek::uint8_t>(eZirconComponentType::kzircon_component_sdk_input);
}

bool zircon_component_sdk_input::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_sdk_input::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}
