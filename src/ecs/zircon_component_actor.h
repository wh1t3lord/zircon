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
public:
	zircon_component_actor(void);
	~zircon_component_actor(void);

	kotek::uint8_t get_component_type(void) const noexcept override;

	void set_enabled(bool status) noexcept;
	bool is_enabled(void) const noexcept;

private:
	bool m_is_enabled;
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_actor& data)
{
	kotek::json::object info;

	info[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_ACTOR_FIELD_M_IS_ENABLED] =
		data.is_enabled();

	write_to = info;
}

inline zircon_component_actor tag_invoke(
	const kotek::json::value_to_tag<zircon_component_actor>&,
	const kotek::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_actor result;

	result.set_enabled(
		data.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_ACTOR_FIELD_M_IS_ENABLED)
			.as_bool());

	#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(
		data.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_ACTOR_FIELD_M_COMPONENT_TYPE)
				.to_number<kotek::uint8_t>() == result.get_component_type(),
		"component type is not equal, data corruption?");
	#endif

	return result;
}
#endif
