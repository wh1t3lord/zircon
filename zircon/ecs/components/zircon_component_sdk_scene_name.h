#pragma once

#include "zircon_component_interface.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

#define ZIRCON_DEF_COMPONENT_SDK_SCENE_NAME_MAX_LENGTH 64

class zircon_component_sdk_scene_name : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_sdk_scene_name)

public:
	zircon_component_sdk_scene_name();
	~zircon_component_sdk_scene_name();

	void DrawImGui(kotek::core::ktkMainManager* main_manager) noexcept override;

	const char* get_name(void) const noexcept;
	void set_name(const kotek::static_cstring_view_t& name) noexcept;

private:
	kotek::static_cstring_t<ZIRCON_DEF_COMPONENT_SDK_SCENE_NAME_MAX_LENGTH>
		m_name;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to,
	const zircon_component_sdk_scene_name& data)
{
	Kotek::ktk::json::object info;

	info[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.IsEnabled();
	info["m_name"] = data.get_name();

	write_to = info;
}

inline zircon_component_sdk_scene_name tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_sdk_scene_name>&,
	const Kotek::ktk::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_sdk_scene_name result;

	result.SetEnabled(
		data.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());
	result.set_name(data.at("m_name").as_string().c_str());

	return result;
}
#endif
