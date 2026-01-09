#pragma once

#include <kotek.core.math/include/kotek_core_math.h>
#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

#include "zircon_component_interface.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_component_camera : public zircon_component_interface
{
public:
	zircon_component_camera(void);
	~zircon_component_camera(void);

	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	float get_yaw(void) const noexcept;
	void set_yaw(float value) noexcept;

	float get_pitch(void) const noexcept;
	void set_pitch(float value) noexcept;

	float get_plane_near(void) const noexcept;
	void set_plane_near(float value) noexcept;

	float get_plane_far(void) const noexcept;
	void set_plane_far(float value) noexcept;

	float get_field_of_view(void) const noexcept;
	void set_field_of_view(float value) noexcept;

	const Kotek::ktk::math::vec3f_t& get_front(void) const noexcept;
	void set_front(const Kotek::ktk::math::vec3f_t& vector) noexcept;

	const Kotek::ktk::math::vec3f_t& get_up(void) const noexcept;
	void set_up(const Kotek::ktk::math::vec3f_t& vector) noexcept;

	const Kotek::ktk::math::vec3f_t& get_right(void) const noexcept;
	void set_right(const Kotek::ktk::math::vec3f_t& data) noexcept;

	const Kotek::ktk::math::mat4x4f_t& get_view(void) const noexcept;
	void set_view(const Kotek::ktk::math::mat4x4f_t& matrix) noexcept;

	const Kotek::ktk::math::matrix4x4f& get_projection(void) const noexcept;
	void set_projection(const Kotek::ktk::math::mat4x4f_t& matrix) noexcept;

private:
	bool m_is_enabled;
	float m_plane_near;
	float m_plane_far;
	float m_fov;
	float m_yaw;
	float m_pitch;
	Kotek::ktk::math::vec3f_t m_front;
	Kotek::ktk::math::vec3f_t m_right;
	Kotek::ktk::math::vec3f_t m_up;
	Kotek::ktk::math::mat4x4f_t m_view;
	Kotek::ktk::math::mat4x4f_t m_projection;
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to, const zircon_component_camera& data)
{
	#ifdef KOTEK_DEBUG
	unsigned char p_storage_memory[2048];
	#else
	KOTEK_ASSERT(false, "defined buffer for release");
	#endif

	kotek::ktk::json::static_resource storage(p_storage_memory);
	Kotek::ktk::json::object camera(&storage);

	camera[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_IS_ENABLED] =
		data.is_enabled();
	camera[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_PLANE_NEAR] =
		data.get_plane_near();
	camera[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_PLANE_FAR] =
		data.get_plane_far();
	camera[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_FOV] =
		data.get_field_of_view();
	camera[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_YAW] = data.get_yaw();
	camera[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_PITCH] = data.get_pitch();
	camera[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_FRONT] =
		Kotek::ktk::json::value_from(data.get_front());
	camera[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_UP] =
		Kotek::ktk::json::value_from(data.get_up());

	#ifdef KOTEK_DEBUG
	camera[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_COMPONENT_TYPE] =
		data.get_component_type();
	#endif

	write_to = camera;
}

inline zircon_component_camera tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_camera>&,
	const Kotek::ktk::json::value& read_from)
{
	auto camera = read_from.as_object();

	zircon_component_camera result;

	result.set_enabled(
		camera.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_IS_ENABLED)
			.as_bool());
	result.set_plane_far(
		camera.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_PLANE_FAR)
			.to_number<float>());
	result.set_plane_near(
		camera.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_PLANE_NEAR)
			.to_number<float>());
	result.set_field_of_view(
		camera.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_FOV)
			.to_number<float>());
	result.set_yaw(camera.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_YAW)
			.to_number<float>());
	result.set_pitch(camera.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_PITCH)
			.to_number<float>());
	result.set_front(Kotek::ktk::json::value_to<Kotek::ktk::math::vec3f_t>(
		camera.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_FRONT)));
	result.set_up(Kotek::ktk::json::value_to<Kotek::ktk::math::vec3f_t>(
		camera.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CAMERA_FIELD_M_UP)));

	return result;
}
#endif
