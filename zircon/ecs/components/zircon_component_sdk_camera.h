#pragma once

#include "zircon_component_camera.h"

class zircon_component_sdk_camera : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_sdk_camera)

public:
	zircon_component_sdk_camera();
	~zircon_component_sdk_camera();

	const zircon_component_camera& get_camera(void) const noexcept;
	zircon_component_camera& get_camera(void) noexcept;

	void set_camera(const zircon_component_camera& data) noexcept;

	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override;

private:
	zircon_component_camera m_camera;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to, const zircon_component_sdk_camera& data)
{
	Kotek::ktk::json::object info;

	info[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.IsEnabled();
	info["m_camera"] = Kotek::ktk::json::value_from(data.get_camera());

	write_to = info;
}

inline zircon_component_sdk_camera tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_sdk_camera>&,
	const Kotek::ktk::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_sdk_camera result;

	result.SetEnabled(data.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());
	result.set_camera(Kotek::ktk::json::value_to<zircon_component_camera>(
		data.at("m_camera")));

	return result;
}
#endif