#pragma once

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

#include "zircon_component_interface.h"

// this component defines the fact that entity is about terrain
class zircon_component_terrain_static : public zircon_component_interface
{
public:
	zircon_component_terrain_static();
	~zircon_component_terrain_static();

	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

private:
	bool m_is_enabled;
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to,
	const zircon_component_terrain_static& data)
{
	Kotek::ktk::json::object info;

	info[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_IS_ENABLED] =
		data.is_enabled();

	write_to = info;
}

inline zircon_component_terrain_static tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_terrain_static>&,
	const Kotek::ktk::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_terrain_static result;

	result.set_enabled(
		data.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_IS_ENABLED)
			.as_bool());

	return result;
}
#endif
