#include "zircon_component_ui_surface.h"

zircon_component_ui_surface::zircon_component_ui_surface() :
	m_is_enabled{true},
	m_component_type{kComponentTypezircon_component_ui_surface}
{
}

zircon_component_ui_surface::~zircon_component_ui_surface() {}

void zircon_component_ui_surface::draw_imgui(
	Kotek::Core::ktkMainManager* main_manager) noexcept
{
}

kotek::json::value zircon_component_ui_surface::serialize(void) noexcept
{
	return kotek::json::value_from(*this);
}

void zircon_component_ui_surface::deserialize(
	const kotek::json::value& data) noexcept
{
	*this = kotek::json::value_to<zircon_component_animation>(data);
}

kotek::json::value zircon_component_ui_surface::serialize(
	unsigned char* p_raw_memory, kotek::size_t size)
{
	KOTEK_ASSERT(p_raw_memory, "you passed an invalid part of memory!");
	kotek::json::static_resource res(p_raw_memory, size);
	kotek::json::storage_ptr ptr(&res);
	return kotek::json::value_from(*this, ptr);
}

kotek::uint8_t zircon_component_ui_surface::get_component_type(
	void) const noexcept
{
	return this->m_component_type;
}

bool zircon_component_ui_surface::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_ui_surface::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}
