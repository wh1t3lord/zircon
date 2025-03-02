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

class zircon_factory_game;

#define ZIRCON_DEF_INPUT_KEYBOARD_HOLDING_FRAMES 2
#define ZIRCON_DEF_INPUT_MOUSE_HOLDING_FRAMES 16

class zircon_component_input : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_input)

	friend class zircon_factory_game;

public:
	zircon_component_input(void);
	zircon_component_input(const kotek::core::ktkIInput* p_manager);
	~zircon_component_input(void);

	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override;

	void set_input_type(Kotek::ktk::enum_base_t type) noexcept;
	Kotek::ktk::enum_base_t get_input_type(void) const noexcept;

	Kotek::ktk::enum_base_t get_input_type_previous(void) const noexcept;

	bool is_key_holding(kotek::core::eInputAllKeys key, unsigned char frames=16) const;
	bool is_key_pressed(kotek::core::eInputAllKeys key) const;
	bool is_key_released(kotek::core::eInputAllKeys key) const;

	float get_sensetivity(void) const;
	void set_sensetivity(float sensentivity);

	float get_delta_x(kotek::core::eInputControllerType type) const;
	float get_delta_y(kotek::core::eInputControllerType type) const;
	
	bool is_invert_mouse_axis_x(void) const;
	bool is_invert_mouse_axis_y(void) const;

	void set_invert_mouse_axis_x(bool status);
	void set_invert_mouse_axis_y(bool status);

private:
	// for deffered initialization of component
	void register_input(const kotek::core::ktkIInput* p_input_manager);

private:
	bool m_is_invert_mouse_axis_x;
	bool m_is_invert_mouse_axis_y;
	float m_sensetivity;
	Kotek::ktk::enum_base_t m_input_type;
	Kotek::ktk::enum_base_t m_input_type_previous;
	const kotek::core::ktkIInput* m_p_input_manager;
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