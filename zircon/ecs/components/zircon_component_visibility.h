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

// TODO: delete this because it is pointless to have...
// because if we hide geometry it means we hide the existence of object in terms
// of visibility...
class zircon_component_visibility : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_visibility)

public:
	zircon_component_visibility(void);
	~zircon_component_visibility(void);

	void SetVisible(bool is_visible) noexcept;

	bool GetVisible(void) const noexcept;

	void Clear(void) noexcept;
	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override;

private:
	bool m_is_visible;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to, const zircon_component_visibility& data)
{
	Kotek::ktk::json::object info;

	info[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.IsEnabled();
	info["m_is_visible"] = data.GetVisible();
	ZIRCON_DEF_TAG_INVOKE_REG_COMPONENT_NAME(info, data);

	write_to = info;
}

inline zircon_component_visibility tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_visibility>&,
	const Kotek::ktk::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_visibility result;

	result.SetEnabled(data.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());
	result.SetVisible(data.at("m_is_visible").as_bool());

	return result;
}
#endif
