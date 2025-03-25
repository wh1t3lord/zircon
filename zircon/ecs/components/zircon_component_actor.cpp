#include "zircon_component_actor.h"

zircon_component_actor::zircon_component_actor(void) :
	m_is_enabled{true}, m_component_type{kComponentTypezircon_component_actor}
{
}

zircon_component_actor::~zircon_component_actor(void) {}

void zircon_component_actor::draw_imgui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			if (p_wrapper_imgui->CollapsingHeader("actor"))
			{
			}
		}
	}
}

kotek::json::value zircon_component_actor::serialize(void) noexcept
{
	return kotek::json::value_from(*this);
}

void zircon_component_actor::deserialize(
	const kotek::json::value& data) noexcept
{
	*this = kotek::json::value_to<zircon_component_actor>(data);
}

kotek::json::value zircon_component_actor::serialize(
	unsigned char* p_raw_memory, kotek::size_t size)
{
	KOTEK_ASSERT(p_raw_memory, "you passed an invalid part of memory!");
	kotek::json::static_resource res(p_raw_memory, size);
	kotek::json::storage_ptr ptr(&res);
	return kotek::json::value_from(*this, ptr);
}

kotek::uint8_t zircon_component_actor::get_component_type(void) const noexcept
{
	return m_component_type;
}

bool zircon_component_actor::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_actor::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}
