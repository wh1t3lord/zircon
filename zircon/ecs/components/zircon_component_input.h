#pragma once
#pragma once

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

#include "zircon_component_interface.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_component_input : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_input)

public:
	zircon_component_input(void);
	~zircon_component_input(void);

	float get_position_mouse_y(void) const noexcept;
	float get_position_mouse_x(void) const noexcept;

	void set_position_mouse_y(float value) noexcept;
	void set_position_mouse_x(float value) noexcept;

	float get_offset_mouse_position_y(void) const noexcept;
	float get_offset_mouse_position_x(void) const noexcept;

	void set_offset_mouse_position_y(float value) noexcept;
	void set_offset_mouse_position_x(float value) noexcept;

	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override;

	bool is_first_iteration(void) const noexcept;
	void set_first_iteration(bool status) noexcept;

	bool is_mouse_right_hold(void) const noexcept;
	int get_mouse_right_hold_tick_count(void) const noexcept;
	void set_mouse_right_hold_tick_count(int value) noexcept;


	bool is_mouse_left_hold(void) const noexcept;
	int get_mouse_left_hold_tick_count(void) const noexcept;
	void set_mouse_left_hold_tick_count(int value) noexcept;

	void set_input_type(Kotek::ktk::enum_base_t type) noexcept;
	Kotek::ktk::enum_base_t get_input_type(void) const noexcept;

	Kotek::ktk::enum_base_t get_input_type_previous(void) const noexcept;

private:
	bool m_is_first_iteration;
	int m_mouse_right_tick_count;
	int m_mouse_left_tick_count;
	Kotek::ktk::enum_base_t m_input_type;
	Kotek::ktk::enum_base_t m_input_type_previous;
	float m_position_mouse_x;
	float m_position_mouse_y;
	float m_offset_mouse_position_x;
	float m_offset_mouse_position_y;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to, const zircon_component_input& data)
{
	KOTEK_ASSERT(false, "not implemented, continue");
}

inline zircon_component_input tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_input>&,
	const Kotek::ktk::json::value& read_from)
{
	KOTEK_ASSERT(false, "not implemented, continue");

	zircon_component_input result;

	return result;
}
#endif