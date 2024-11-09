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

	const char* GetName(void) const noexcept;
	void SetName(const kotek::static_cstring_view_t& name) noexcept;

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

	info["m_is_enabled"] = data.IsEnabled();
	info["m_name"] = data.GetName();

	write_to = info;
}

inline zircon_component_sdk_scene_name tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_sdk_scene_name>&,
	const Kotek::ktk::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_sdk_scene_name result;

	result.SetEnabled(data.at("m_is_enabled").as_bool());
	result.SetName(data.at("m_name").as_string().c_str());

	return result;
}
#endif
