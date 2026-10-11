#include "zircon_component_terrain_static.h"

zircon_component_terrain_static::zircon_component_terrain_static() :
	m_is_enabled{true},
	m_heightmap_format{eZirconTerrainHeightmapFormat::kGray8},
	m_resolution_x{0},
	m_resolution_y{0},
	m_world_size_x{0.0f},
	m_world_size_z{0.0f},
	m_height_scale{1.0f},
	m_heightmap_path{}
{
}

zircon_component_terrain_static::~zircon_component_terrain_static() {}

kotek::uint8_t
zircon_component_terrain_static::get_component_type(void) const noexcept
{
	return static_cast<kotek::uint8_t>(
		eZirconComponentType::kzircon_component_terrain_static);
}

bool zircon_component_terrain_static::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_terrain_static::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}

const char* zircon_component_terrain_static::get_heightmap_path(
	void) const noexcept
{
	return this->m_heightmap_path.c_str();
}

void zircon_component_terrain_static::set_heightmap_path(
	const char* p_heightmap_path) noexcept
{
	this->m_heightmap_path = p_heightmap_path ? p_heightmap_path : "";
}

kotek::uint16_t zircon_component_terrain_static::get_resolution_x(
	void) const noexcept
{
	return this->m_resolution_x;
}

kotek::uint16_t zircon_component_terrain_static::get_resolution_y(
	void) const noexcept
{
	return this->m_resolution_y;
}

void zircon_component_terrain_static::set_resolution(
	kotek::uint16_t resolution_x, kotek::uint16_t resolution_y) noexcept
{
	this->m_resolution_x = resolution_x;
	this->m_resolution_y = resolution_y;
}

float zircon_component_terrain_static::get_world_size_x(
	void) const noexcept
{
	return this->m_world_size_x;
}

float zircon_component_terrain_static::get_world_size_z(
	void) const noexcept
{
	return this->m_world_size_z;
}

void zircon_component_terrain_static::set_world_size(
	float world_size_x, float world_size_z) noexcept
{
	this->m_world_size_x = world_size_x;
	this->m_world_size_z = world_size_z;
}

float zircon_component_terrain_static::get_height_scale(
	void) const noexcept
{
	return this->m_height_scale;
}

void zircon_component_terrain_static::set_height_scale(
	float height_scale) noexcept
{
	this->m_height_scale = height_scale;
}

eZirconTerrainHeightmapFormat
zircon_component_terrain_static::get_heightmap_format(void) const noexcept
{
	return this->m_heightmap_format;
}

void zircon_component_terrain_static::set_heightmap_format(
	eZirconTerrainHeightmapFormat format) noexcept
{
	this->m_heightmap_format = format;
}
