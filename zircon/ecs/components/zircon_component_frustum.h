#pragma once

#include "zircon_component_interface.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_component_frustum : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_frustum)

public:
	zircon_component_frustum(void);
	~zircon_component_frustum(void);

	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to, const zircon_component_frustum& data)
{
	Kotek::ktk::json::object frustum;

	frustum[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.IsEnabled();
	ZIRCON_DEF_TAG_INVOKE_REG_COMPONENT_NAME(frustum, data);

	write_to = frustum;
}

inline zircon_component_frustum tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_frustum>&,
	const Kotek::ktk::json::value& read_from)
{
	auto frustum = read_from.as_object();

	zircon_component_frustum result;

	result.SetEnabled(frustum.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());

	return result;
}
#endif