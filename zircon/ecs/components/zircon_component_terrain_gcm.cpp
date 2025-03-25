#include "zircon_component_terrain_gcm.h"

zircon_component_terrain_gcm::zircon_component_terrain_gcm() :
	m_is_enabled{true},
	m_component_type{kComponentTypezircon_component_terrain_gcm}
{
}

zircon_component_terrain_gcm::~zircon_component_terrain_gcm() {}

void zircon_component_terrain_gcm::draw_imgui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			if (p_wrapper_imgui->CollapsingHeader(""))
			{
			}
		}
	}
}

kotek::json::value zircon_component_terrain_gcm::serialize(void) noexcept
{
	return kotek::json::value_from(*this);
}

void zircon_component_terrain_gcm::deserialize(
	const kotek::json::value& data) noexcept
{
	*this = kotek::json::value_to<zircon_component_terrain_gcm>(data);
}

kotek::json::value zircon_component_terrain_gcm::serialize(
	unsigned char* p_raw_memory, kotek::size_t size)
{
	KOTEK_ASSERT(p_raw_memory, "you passed an invalid part of memory!");
	kotek::json::static_resource res(p_raw_memory, size);
	kotek::json::storage_ptr ptr(&res);
	return kotek::json::value_from(*this, ptr);
}

kotek::uint8_t zircon_component_terrain_gcm::get_component_type(
	void) const noexcept
{
	return this->m_component_type;
}

bool zircon_component_terrain_gcm::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_terrain_gcm::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}
