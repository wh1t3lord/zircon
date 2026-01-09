#include "zircon_component_camera.h"
#include <kotek.core.main_manager/include/kotek_core_main_manager.h>
#include "zircon_component_geometry.h"

zircon_component_camera::zircon_component_camera(void) :
	m_is_enabled{true}, m_plane_near(0.1f),
	m_plane_far(1000.0f), m_fov(60.0f), m_yaw(-90.0f),
	m_pitch(0.0f), m_front(0.0f, 0.0f, -1.0f),
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

float zircon_component_camera::get_plane_near(void
) const noexcept
{
	return this->m_plane_near;
}

void zircon_component_camera::set_plane_near(float value
) noexcept
{
	this->m_plane_near = value;
}

float zircon_component_camera::get_plane_far(void
) const noexcept
{
	return this->m_plane_far;
}

void zircon_component_camera::set_plane_far(float value
) noexcept
{
	this->m_plane_far = value;
}

float zircon_component_camera::get_field_of_view(void
) const noexcept
{
	return this->m_fov;
}

void zircon_component_camera::set_field_of_view(float value
) noexcept
{
	this->m_fov = value;
}

const Kotek::ktk::math::vec3f_t&
zircon_component_camera::get_front(void) const noexcept
{
	return this->m_front;
}

void zircon_component_camera::set_front(
	const Kotek::ktk::math::vec3f_t& vector
) noexcept
{
	this->m_front = vector;
}

const Kotek::ktk::math::vec3f_t&
zircon_component_camera::get_up(void) const noexcept
{
	return this->m_up;
}

void zircon_component_camera::set_up(
	const Kotek::ktk::math::vec3f_t& vector
) noexcept
{
	this->m_up = vector;
}

const Kotek::ktk::math::vec3f_t&
zircon_component_camera::get_right(void) const noexcept
{
	return this->m_right;
}

void zircon_component_camera::set_right(
	const Kotek::ktk::math::vec3f_t& data
) noexcept
{
	this->m_right = data;
}

const Kotek::ktk::math::mat4x4f_t&
zircon_component_camera::get_view(void) const noexcept
{
	return this->m_view;
}

void zircon_component_camera::set_view(
	const Kotek::ktk::math::mat4x4f_t& matrix
) noexcept
{
	this->m_view = matrix;
}

const Kotek::ktk::math::matrix4x4f&
zircon_component_camera::get_projection(void) const noexcept
{
	return this->m_projection;
}

void zircon_component_camera::set_projection(
	const Kotek::ktk::math::mat4x4f_t& matrix
) noexcept
{
	this->m_projection = matrix;
}

/*
void zircon_component_camera::draw_imgui(
	Kotek::Core::ktkMainManager* p_main_manager
) noexcept
{
	if (p_main_manager)
	{
		kotek::core::ktkIImguiWrapper* p_wrapper_imgui =
			p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			if (p_wrapper_imgui->BeginTabBar(
					"ZirconComponentCameraTab"
				))
			{
				if (p_wrapper_imgui->BeginTabItem("info"))
				{
					p_wrapper_imgui->Text(
						"plane: \n\tnear: %.3f \n\tfar: %.3f",
						this->m_plane_near,
						this->m_plane_far
					);
					p_wrapper_imgui->Text(
						"pitch: \n\t%.3f (deg) \n\t%.3f (rad)",
						this->m_pitch,
						kotek::ktk::math::convert_to_radians(
							this->m_pitch
						)
					);
					p_wrapper_imgui->Text(
						"yaw: \n\t%.3f (deg) \n\t%.3f (rad)",
						this->m_yaw,
						kotek::ktk::math::convert_to_radians(
							this->m_yaw
						)
					);

					p_wrapper_imgui->Text(
						"view: \n\t[0]: %.3f %.3f %.3f %.3f "
						"\n\t[1]: "
						"%.3f %.3f %.3f %.3f \n\t[2]: %.3f "
						"%.3f %.3f "
						"%.3f \n\t[3]: %.3f %.3f %.3f %.3f",
						this->m_view[0][0],
						this->m_view[0][1],
						this->m_view[0][2],
						this->m_view[0][3],
						this->m_view[1][0],
						this->m_view[1][1],
						this->m_view[1][2],
						this->m_view[1][3],
						this->m_view[2][0],
						this->m_view[2][1],
						this->m_view[2][2],
						this->m_view[2][3],
						this->m_view[3][0],
						this->m_view[3][1],
						this->m_view[3][2],
						this->m_view[3][3]
					);

					p_wrapper_imgui->Text(
						"projection: \n\t[0]: %.3f %.3f %.3f "
						"%.3f \n\t[1]: %.3f "
						"%.3f "
						"%.3f %.3f \n\t[2]: %.3f %.3f %.3f "
						"%.3f \n\t[3]: %.3f %.3f "
						"%.3f %.3f",
						this->m_projection[0][0],
						this->m_projection[0][1],
						this->m_projection[0][2],
						this->m_projection[0][3],
						this->m_projection[1][0],
						this->m_projection[1][1],
						this->m_projection[1][2],
						this->m_projection[1][3],
						this->m_projection[2][0],
						this->m_projection[2][1],
						this->m_projection[2][2],
						this->m_projection[2][3],
						this->m_projection[3][0],
						this->m_projection[3][1],
						this->m_projection[3][2],
						this->m_projection[3][3]
					);

					p_wrapper_imgui->EndTabItem();
				}

				if (p_wrapper_imgui->BeginTabItem("edit"))
				{
					p_wrapper_imgui->DragFloat(
						"plane far",
						&this->m_plane_far,
						1.0f,
						1.0f,
						10000.0f
					);
					p_wrapper_imgui->DragFloat(
						"plane near",
						&this->m_plane_near,
						1.0f,
						0.00001f,
						0.5f
					);
					p_wrapper_imgui->DragFloat(
						"pitch",
						&this->m_pitch,
						1.0f,
						-89.0f,
						89.0f
					);
					p_wrapper_imgui->DragFloat(
						"yaw",
						&this->m_yaw,
						1.0f,
						-360.0f,
						360.0f
					);

					p_wrapper_imgui->DragFloat(
						"fov", &this->m_fov, 1.0f, 0.1f, 120.0f
					);

					p_wrapper_imgui->SeparatorText("view");
					p_wrapper_imgui->DragFloat4(
						"[0]##view",
						this->m_view[0].data(),
						1.0f,
						-10.0f,
						10.0f
					);
					p_wrapper_imgui->DragFloat4(
						"[1]##view",
						this->m_view[1].data(),
						1.0f,
						-10.0f,
						10.0f
					);
					p_wrapper_imgui->DragFloat4(
						"[2]##view",
						this->m_view[2].data(),
						1.0f,
						-10.0f,
						10.0f
					);
					p_wrapper_imgui->DragFloat4(
						"[3]##view",
						this->m_view[3].data(),
						1.0f,
						-10.0f,
						10.0f
					);

					p_wrapper_imgui->SeparatorText("projection"
					);

					if (p_wrapper_imgui->Button(
							"reset##projection"
						))
					{
						this->m_yaw = -90.0f;
						this->m_pitch = 0.0f;
						this->m_fov = 60.0f;

						auto width =
							p_main_manager->Get_WindowManager()
								->ActiveWindow_GetWidth();
						auto height =
							p_main_manager->Get_WindowManager()
								->ActiveWindow_GetHeight();
						this->m_plane_near = 0.1f;
						this->m_plane_far = 1000.0f;
						this->m_projection =
							kotek::ktk::math::perspective(
								kotek::ktk::math::
									convert_to_radians(
										this->m_fov
									),
								width / height,
								this->m_plane_near,
								this->m_plane_far
							);
					}

					p_wrapper_imgui->EndTabItem();
				}

				p_wrapper_imgui->EndTabBar();
			}
		}
	}
}*/

kotek::uint8_t zircon_component_camera::get_component_type(void
) const noexcept
{
	return static_cast<kotek::uint8_t>(
		eZirconComponentType::kzircon_component_camera
	);
}

bool zircon_component_camera::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_camera::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}
