#pragma once

#include "zircon_component_camera.h"

class zircon_component_sdk_camera : public zircon_component_interface
{
public:
	zircon_component_sdk_camera();
	~zircon_component_sdk_camera();

	const zircon_component_camera& get_camera(void) const noexcept;
	zircon_component_camera& get_camera(void) noexcept;

	void set_camera(const zircon_component_camera& data) noexcept;
	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	bool is_initialized(void) const;
	void set_initialized(bool status);

private:
	bool m_is_enabled;
	bool m_is_initialized;
	zircon_component_camera m_camera;
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_sdk_camera& data)
{
	kotek::json::object info;

	info[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_SDK_CAMERA_FIELD_M_IS_ENABLED] =
		data.is_enabled();
	info[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_SDK_CAMERA_FIELD_M_CAMERA] =
		kotek::json::value_from(data.get_camera());

	write_to = info;
}

inline zircon_component_sdk_camera tag_invoke(
	const kotek::json::value_to_tag<zircon_component_sdk_camera>&,
	const kotek::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_sdk_camera result;

	result.set_enabled(
		data.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_SDK_CAMERA_FIELD_M_IS_ENABLED)
			.as_bool());
	result.set_camera(kotek::json::value_to<zircon_component_camera>(
		data.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_SDK_CAMERA_FIELD_M_CAMERA)));

	return result;
}
#endif