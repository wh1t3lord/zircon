#pragma once

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

#include "zircon_component_interface.h"

// this component defines the fact that entity is about terrain
class zircon_component_terrain : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_terrain)

public:
	zircon_component_terrain();
	~zircon_component_terrain();

	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override;

private:
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to, const zircon_component_terrain& data)
{
	Kotek::ktk::json::object info;

	info["m_is_enabled"] = data.IsEnabled();
	zircon_DEF_TAG_INVOKE_REG_COMPONENT_NAME(info, data);

	write_to = info;
}

inline zircon_component_terrain tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_terrain>&,
	const Kotek::ktk::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_terrain result;

	result.SetEnabled(data.at("m_is_enabled").as_bool());

	return result;
}
#endif
