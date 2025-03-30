#pragma once

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

#include "zircon_component_interface.h"

// this component defines the fact that entity is about terrain
class zircon_component_terrain_static : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_terrain_static,
		kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>)

public:
	zircon_component_terrain_static();
	~zircon_component_terrain_static();

	void draw_imgui(
		Kotek::Core::ktkMainManager* main_manager) noexcept override;
	kotek::json::value serialize(void) noexcept override;
	void deserialize(const kotek::json::value& data) noexcept override;
	kotek::json::value serialize(
		unsigned char* p_raw_memory, kotek::size_t size) override;
	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

private:
	bool m_is_enabled;
	kotek::uint8_t m_component_type;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to,
	const zircon_component_terrain_static& data)
{
	Kotek::ktk::json::object info;

	info[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_IS_ENABLED] =
		data.is_enabled();

	#ifdef KOTEK_DEBUG
	info[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_COMPONENT_TYPE] =
		data.get_component_type();
	#endif

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

	#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(
		data.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_COMPONENT_TYPE)
				.to_number<kotek::uint8_t>() == result.get_component_type(),
		"component type is not equal, data corruption?"
	);
	#endif

	return result;
}
#endif
