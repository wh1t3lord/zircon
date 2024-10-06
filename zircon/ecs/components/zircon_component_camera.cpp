#include "zircon_component_camera.h"
#include <kotek.core.main_manager/include/kotek_core_main_manager.h>
#include "zircon_component_geometry.h"

zircon_component_camera::zircon_component_camera(void) :
	m_plane_near(0.1f), m_plane_far(1000.0f), m_fov(60.0f), m_yaw(-90.0f),
	m_pitch(0.0f), m_mouse_sensetivity(0.5f), m_front(0.0f, 0.0f, -1.0f),
	m_up(0.0f, 1.0f, 0.0f)
{
}

zircon_component_camera::~zircon_component_camera(void) {}

float zircon_component_camera::get_yaw(void) const noexcept
{
	return this->m_yaw;
}

void zircon_component_camera::set_yaw(float value) noexcept
{
	this->m_yaw = value;
}

float zircon_component_camera::get_pitch(void) const noexcept
{
	return this->m_pitch;
}

void zircon_component_camera::set_pitch(float value) noexcept
{
	this->m_pitch = value;
}

float zircon_component_camera::get_plane_near(void) const noexcept
{
	return this->m_plane_near;
}

void zircon_component_camera::set_plane_near(float value) noexcept
{
	this->m_plane_near = value;
}

float zircon_component_camera::get_plane_far(void) const noexcept
{
	return this->m_plane_far;
}

void zircon_component_camera::set_plane_far(float value) noexcept
{
	this->m_plane_far = value;
}

float zircon_component_camera::get_field_of_view(void) const noexcept
{
	return this->m_fov;
}

void zircon_component_camera::set_field_of_view(float value) noexcept
{
	this->m_fov = value;
}

const Kotek::ktk::math::vec3f_t& zircon_component_camera::get_front(
	void) const noexcept
{
	return this->m_front;
}

void zircon_component_camera::set_front(
	const Kotek::ktk::math::vec3f_t& vector) noexcept
{
	this->m_front = vector;
}

const Kotek::ktk::math::vec3f_t& zircon_component_camera::get_up(
	void) const noexcept
{
	return this->m_up;
}

void zircon_component_camera::set_up(
	const Kotek::ktk::math::vec3f_t& vector) noexcept
{
	this->m_up = vector;
}

const Kotek::ktk::math::vec3f_t& zircon_component_camera::get_right(
	void) const noexcept
{
	return this->m_right;
}

void zircon_component_camera::set_right(
	const Kotek::ktk::math::vec3f_t& data) noexcept
{
	this->m_right = data;
}

float zircon_component_camera::get_mouse_sensetivity(void) const noexcept
{
	return this->m_mouse_sensetivity;
}

void zircon_component_camera::set_mouse_sensetivity(float value) noexcept
{
	this->m_mouse_sensetivity = value;
}

const Kotek::ktk::math::mat4x4f_t& zircon_component_camera::get_view(
	void) const noexcept
{
	return this->m_view;
}

void zircon_component_camera::set_view(
	const Kotek::ktk::math::mat4x4f_t& matrix) noexcept
{
	this->m_view = matrix;
}

const Kotek::ktk::math::matrix4x4f& zircon_component_camera::get_projection(
	void) const noexcept
{
	return this->m_projection;
}

void zircon_component_camera::set_projection(
	const Kotek::ktk::math::mat4x4f_t& matrix) noexcept
{
	this->m_projection = matrix;
}

void zircon_component_camera::DrawImGui(
	Kotek::Core::ktkMainManager* main_manager) noexcept
{
}