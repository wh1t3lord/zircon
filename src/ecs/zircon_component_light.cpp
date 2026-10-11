#include "zircon_component_light.h"

zircon_component_light::zircon_component_light() :
	m_is_enabled{true},
	m_light_type{eZirconLightType::kPoint},
	m_color{1.0f, 1.0f, 1.0f},
	m_intensity{1.0f},
	m_range{10.0f},
	m_spot_inner_angle_degrees{25.0f},
	m_spot_outer_angle_degrees{45.0f}
{
}

zircon_component_light::~zircon_component_light() {}

kotek::uint8_t
zircon_component_light::get_component_type(void) const noexcept
{
	return static_cast<kotek::uint8_t>(
		eZirconComponentType::kzircon_component_light);
}

bool zircon_component_light::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_light::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}

eZirconLightType zircon_component_light::get_light_type(
	void) const noexcept
{
	return this->m_light_type;
}

void zircon_component_light::set_light_type(eZirconLightType type) noexcept
{
	this->m_light_type = type;
}

float zircon_component_light::get_color(kotek::uint8_t channel) const noexcept
{
	KOTEK_ASSERT(channel < 3, "the color channel index is out of range");

	if (channel >= 3)
		return 0.0f;

	return this->m_color[channel];
}

void zircon_component_light::set_color(float r, float g, float b) noexcept
{
	this->m_color[0] = r;
	this->m_color[1] = g;
	this->m_color[2] = b;
}

float zircon_component_light::get_intensity(void) const noexcept
{
	return this->m_intensity;
}

void zircon_component_light::set_intensity(float intensity) noexcept
{
	this->m_intensity = intensity;
}

float zircon_component_light::get_range(void) const noexcept
{
	return this->m_range;
}

void zircon_component_light::set_range(float range) noexcept
{
	this->m_range = range;
}

float zircon_component_light::get_spot_inner_angle_degrees(
	void) const noexcept
{
	return this->m_spot_inner_angle_degrees;
}

float zircon_component_light::get_spot_outer_angle_degrees(
	void) const noexcept
{
	return this->m_spot_outer_angle_degrees;
}

void zircon_component_light::set_spot_angles_degrees(
	float inner_degrees, float outer_degrees) noexcept
{
	this->m_spot_inner_angle_degrees = inner_degrees;
	this->m_spot_outer_angle_degrees = outer_degrees;
}
