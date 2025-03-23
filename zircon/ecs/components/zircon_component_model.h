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

// TODO: delete this component
class zircon_component_model : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_model)

public:
	zircon_component_model(void) {}
	~zircon_component_model(void) {}

	void Clear(void) noexcept {}

	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override
	{
	}
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_model& data)
{
	#ifdef KOTEK_DEBUG
	unsigned char p_storage_memory[1024];
	#else
	KOTEK_ASSERT(false, "provide optimized buffer for release");
	#endif
	kotek::json::static_resource storage(p_storage_memory);
	kotek::json::object model(&storage);

	model[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.IsEnabled();

	write_to = model;
}

inline zircon_component_model tag_invoke(
	const kotek::json::value_to_tag<zircon_component_model>&,
	const kotek::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_model result;

	result.SetEnabled(
		data.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());

	return result;
}
#endif
