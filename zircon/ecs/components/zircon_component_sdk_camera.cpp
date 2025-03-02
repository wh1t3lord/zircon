#include "zircon_component_sdk_camera.h"

zircon_component_sdk_camera::zircon_component_sdk_camera() : m_is_initialized{}
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

void zircon_component_sdk_camera::DrawImGui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		this->m_camera.DrawImGui(p_main_manager);
	}
}

bool zircon_component_sdk_camera::is_initialized(void) const
{
	return m_is_initialized;
}

void zircon_component_sdk_camera::set_initialized(bool status)
{
	this->m_is_initialized = status;
}
