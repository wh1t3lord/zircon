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
	KOTEK_COMPONENT(zircon_component_camera)

public:
	zircon_component_camera(void);
	~zircon_component_camera(void);

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

	float get_mouse_sensetivity(void) const noexcept;
	void set_mouse_sensetivity(float value) noexcept;

	const Kotek::ktk::math::mat4x4f_t& get_view(void) const noexcept;
	void set_view(const Kotek::ktk::math::mat4x4f_t& matrix) noexcept;

	const Kotek::ktk::math::matrix4x4f& get_projection(void) const noexcept;
	void set_projection(const Kotek::ktk::math::mat4x4f_t& matrix) noexcept;

	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override;

private:
	float m_plane_near;
	float m_plane_far;
	float m_fov;
	float m_yaw;
	float m_pitch;
	float m_mouse_sensetivity;
	Kotek::ktk::math::vec3f_t m_front;
	Kotek::ktk::math::vec3f_t m_right;
	Kotek::ktk::math::vec3f_t m_up;
	Kotek::ktk::math::mat4x4f_t m_view;
	Kotek::ktk::math::mat4x4f_t m_projection;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to,
	const zircon_component_camera& data)
{
	Kotek::ktk::json::object camera;

	camera["m_is_enabled"] = data.IsEnabled();
	camera["m_plane_near"] = data.get_plane_near();
	camera["m_plane_far"] = data.get_plane_far();
	camera["m_fov"] = data.get_field_of_view();
	camera["m_yaw"] = data.get_yaw();
	camera["m_pitch"] = data.get_pitch();
	camera["m_mouse_sensetivity"] = data.get_mouse_sensetivity();
	camera["m_front"] = 
		Kotek::ktk::json::value_from(data.get_front());
	camera["m_up"] =
		Kotek::ktk::json::value_from(data.get_up());
	zircon_DEF_TAG_INVOKE_REG_COMPONENT_NAME(camera, data);

	write_to = camera;
}

inline zircon_component_camera tag_invoke(
	const Kotek::ktk::json::value_to_tag<
		zircon_component_camera>&,
	const Kotek::ktk::json::value& read_from)
{
	auto camera = read_from.as_object();

	zircon_component_camera result;

	result.SetEnabled(camera.at("m_is_enabled").as_bool());
	result.set_plane_far(camera.at("m_plane_far").to_number<float>());
	result.set_plane_near(camera.at("m_plane_near").to_number<float>());
	result.set_field_of_view(camera.at("m_fov").to_number<float>());
	result.set_yaw(camera.at("m_yaw").to_number<float>());
	result.set_pitch(camera.at("m_pitch").to_number<float>());
	result.set_mouse_sensetivity(
		camera.at("m_mouse_sensetivity").to_number<float>());
	result.set_front(Kotek::ktk::json::value_to<Kotek::ktk::math::vec3f_t>(
		camera.at("m_front")));
	result.set_up(Kotek::ktk::json::value_to<Kotek::ktk::math::vec3f_t>(
		camera.at("m_up")));

	return result;
}
#endif
