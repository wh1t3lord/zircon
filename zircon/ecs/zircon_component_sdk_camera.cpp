#include "zircon_component_sdk_camera.h"

zircon_component_sdk_camera::zircon_component_sdk_camera() :
	m_is_enabled{true}, m_is_initialized{},
	m_component_type{kComponentTypezircon_component_sdk_camera}
{
}

zircon_component_sdk_camera::~zircon_component_sdk_camera() {}

const zircon_component_camera& zircon_component_sdk_camera::get_camera(
	void) const noexcept
{
	return this->m_camera;
}

zircon_component_camera& zircon_component_sdk_camera::get_camera(void) noexcept
{
	return this->m_camera;
}

void zircon_component_sdk_camera::set_camera(
	const zircon_component_camera& data) noexcept
{
	this->m_camera = data;
}

void zircon_component_sdk_camera::draw_imgui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		this->m_camera.draw_imgui(p_main_manager);
	}
}

kotek::json::value zircon_component_sdk_camera::serialize(void) noexcept
{
	return kotek::json::value_from(*this);
}

void zircon_component_sdk_camera::deserialize(
	const kotek::json::value& data) noexcept
{
	*this = kotek::json::value_to<zircon_component_sdk_camera>(data);
}

kotek::json::value zircon_component_sdk_camera::serialize(
	unsigned char* p_raw_memory, kotek::size_t size)
{
	KOTEK_ASSERT(p_raw_memory, "you passed an invalid part of memory!");
	kotek::json::static_resource res(p_raw_memory, size);
	kotek::json::storage_ptr ptr(&res);
	return kotek::json::value_from(*this, ptr);
}

kotek::uint8_t zircon_component_sdk_camera::get_component_type(
	void) const noexcept
{
	return this->m_component_type;
}

bool zircon_component_sdk_camera::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_sdk_camera::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}

bool zircon_component_sdk_camera::is_initialized(void) const
{
	return m_is_initialized;
}

void zircon_component_sdk_camera::set_initialized(bool status)
{
	this->m_is_initialized = status;
}
