#include "zircon_component_transform.h"
#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

zircon_component_transform::zircon_component_transform(void) :
	m_position(0.0f, 0.0f, 0.0f), m_scale(1.0f, 1.0f, 1.0f),
	m_rotation(0.0f, 0.0f, 0.0f, 1.0f)
{
}

zircon_component_transform::~zircon_component_transform(void) {}

const Kotek::ktk::math::vec3f_t& zircon_component_transform::get_position(
	void) const noexcept
{
	return this->m_position;
}

void zircon_component_transform::set_position(
	const Kotek::ktk::math::vec3f_t& pos) noexcept
{
	this->m_position = pos;
}

const Kotek::ktk::math::vec3f_t& zircon_component_transform::get_scale(
	void) const noexcept
{
	return this->m_scale;
}

void zircon_component_transform::set_scale(
	const Kotek::ktk::math::vec3f_t& scale) noexcept
{
	this->m_scale = scale;
}

const Kotek::ktk::math::quatf_t& zircon_component_transform::get_rotation(
	void) const noexcept
{
	return this->m_rotation;
}

void zircon_component_transform::set_rotation(
	const Kotek::ktk::math::quatf_t& rot) noexcept
{
	this->m_rotation = rot;
}

void zircon_component_transform::DrawImGui(
	Kotek::Core::ktkMainManager* main_manager) noexcept
{
	Kotek::Core::ktkIImguiWrapper* p_wrapper_imgui =
		main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->CollapsingHeader("Component Transform"))
		{
			p_wrapper_imgui->PushID(1);
			p_wrapper_imgui->EditDragVec3f(
				"position##Transform", &this->m_position);
			p_wrapper_imgui->PopID();

			p_wrapper_imgui->PushID(2);
			p_wrapper_imgui->EditDragVec3f("scale##Transform", &this->m_scale);
			p_wrapper_imgui->PopID();

			p_wrapper_imgui->PushID(3);
			p_wrapper_imgui->EditDragQuatf(
				"rotation##Transform", &this->m_rotation);
			p_wrapper_imgui->PopID();
		}
	}
}