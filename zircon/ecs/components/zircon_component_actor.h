#pragma once

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>
#include "zircon_component_interface.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_component_actor : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_actor)

public:
	zircon_component_actor(void) {}
	~zircon_component_actor(void) {}

	void Clear(void) noexcept {}

	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override
	{
	}
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to, const zircon_component_actor& data)
{
	Kotek::ktk::json::object info;

	info["m_is_enabled"] = data.IsEnabled();

	write_to = info;
}

inline zircon_component_actor tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_actor>&,
	const Kotek::ktk::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_actor result;

	result.SetEnabled(data.at("m_is_enabled").as_bool());

	return result;
}
#endif
