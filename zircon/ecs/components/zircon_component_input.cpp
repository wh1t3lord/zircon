#include "zircon_component_input.h"
#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

zircon_component_input::zircon_component_input(void) :
	m_is_first_iteration{}, m_mouse_right_tick_count{},
	m_mouse_left_tick_count{},
	m_input_type{static_cast<Kotek::ktk::enum_base_t>(
		Kotek::Core::eInputType::kInputType_DisabledCursor)},
	m_input_type_previous{m_input_type}, m_position_mouse_x(0.0f),
	m_position_mouse_y(0.0f), m_offset_mouse_position_x(0.0f),
	m_offset_mouse_position_y(0.0f)
{
}

zircon_component_input::~zircon_component_input(void) {}

float zircon_component_input::get_position_mouse_y(void) const noexcept
{
	return this->m_position_mouse_y;
}

float zircon_component_input::get_position_mouse_x(void) const noexcept
{
	return this->m_position_mouse_x;
}

void zircon_component_input::set_position_mouse_y(float value) noexcept
{
	this->m_position_mouse_y = value;
}

void zircon_component_input::set_position_mouse_x(float value) noexcept
{
	this->m_position_mouse_x = value;
}

float zircon_component_input::get_offset_mouse_position_y(void) const noexcept
{
	return this->m_offset_mouse_position_y;
}

float zircon_component_input::get_offset_mouse_position_x(void) const noexcept
{
	return this->m_offset_mouse_position_x;
}

void zircon_component_input::set_offset_mouse_position_y(float value) noexcept
{
	this->m_offset_mouse_position_y = value;
}

void zircon_component_input::set_offset_mouse_position_x(float value) noexcept
{
	this->m_offset_mouse_position_x = value;
}

void zircon_component_input::DrawImGui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			p_wrapper_imgui->Text("mouse x: %.2f", this->m_position_mouse_x);
			p_wrapper_imgui->Text("mouse y: %.2f", this->m_position_mouse_y);

			p_wrapper_imgui->Text("dx (curr - prev): %.2f", this->m_offset_mouse_position_x);
			p_wrapper_imgui->Text("dy (curr - prev): %.2f", this->m_offset_mouse_position_y);

			p_wrapper_imgui->Text("Mouse left hold: %d", this->is_mouse_left_hold());
			p_wrapper_imgui->Text("Mouse right hold: %d", this->is_mouse_right_hold());

			p_wrapper_imgui->Text("Input type: %s",
				Kotek::Core::helper::Translate_InputType(static_cast<Kotek::Core::eInputType>(this->m_input_type)).c_str());
		}
	}
}

bool zircon_component_input::is_first_iteration(void) const noexcept
{
	return this->m_is_first_iteration;
}

void zircon_component_input::set_first_iteration(bool status) noexcept
{
	this->m_is_first_iteration = status;
}

bool zircon_component_input::is_mouse_right_hold(void) const noexcept
{
	return this->m_mouse_right_tick_count >= 1;
}

int zircon_component_input::get_mouse_right_hold_tick_count(void) const noexcept
{
	return this->m_mouse_right_tick_count;
}

void zircon_component_input::set_mouse_right_hold_tick_count(int value) noexcept
{
	this->m_mouse_right_tick_count = value;
}

bool zircon_component_input::is_mouse_left_hold(void) const noexcept
{
	return this->m_mouse_left_tick_count >= 1;
}

int zircon_component_input::get_mouse_left_hold_tick_count(void) const noexcept
{
	return this->m_mouse_left_tick_count;
}

void zircon_component_input::set_mouse_left_hold_tick_count(int value) noexcept
{
	this->m_mouse_left_tick_count = value;
}

void zircon_component_input::set_input_type(
	Kotek::ktk::enum_base_t type) noexcept
{
	if (type != this->m_input_type)
		this->m_input_type_previous = this->m_input_type;

	this->m_input_type = type;
}

Kotek::ktk::enum_base_t zircon_component_input::get_input_type(
	void) const noexcept
{
	return this->m_input_type;
}

Kotek::ktk::enum_base_t zircon_component_input::get_input_type_previous(
	void) const noexcept
{
	return this->m_input_type_previous;
}
