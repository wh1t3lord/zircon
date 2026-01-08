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

class zircon_factory;

#define ZIRCON_DEF_INPUT_KEYBOARD_HOLDING_FRAMES 2
#define ZIRCON_DEF_INPUT_MOUSE_HOLDING_FRAMES 16

class zircon_component_input : public zircon_component_interface
{
	friend class zircon_factory;

public:
	zircon_component_input(void);
	zircon_component_input(const kotek::core::ktkIInput* p_manager);
	~zircon_component_input(void);

	void draw_imgui(
		Kotek::Core::ktkMainManager* main_manager) noexcept override;
	kotek::json::value serialize(void) noexcept override;
	void deserialize(const kotek::json::value& data) noexcept override;
	kotek::json::value serialize(
		unsigned char* p_raw_memory, kotek::size_t size) override;
	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	void set_input_type(kotek::enum_base_t type) noexcept;
	kotek::enum_base_t get_input_type(void) const noexcept;

	kotek::enum_base_t get_input_type_previous(void) const noexcept;

	bool is_key_holding(
		kotek::core::eInputAllKeys key, unsigned char frames = 16) const;
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
	bool m_is_enabled;
	kotek::uint8_t m_component_type;
	bool m_is_invert_mouse_axis_x;
	bool m_is_invert_mouse_axis_y;
	float m_sensetivity;
	kotek::enum_base_t m_input_type;
	kotek::enum_base_t m_input_type_previous;
	const kotek::core::ktkIInput* m_p_input_manager;
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_input& data)
{
	#ifdef KOTEK_DEBUG
	unsigned char p_storage_memory[1024];
	#else
	KOTEK_ASSERT(false, "provide optimized buffer for release");
	#endif
	kotek::json::static_resource storage(p_storage_memory);
	kotek::json::object input(&storage);

	input[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_INPUT_FIELD_M_IS_ENABLED] =
		data.is_enabled();

	#ifdef KOTEK_DEBUG
	input[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_INPUT_FIELD_M_COMPONENT_TYPE] =
		data.get_component_type();
	#endif

	write_to = input;
}

inline zircon_component_input tag_invoke(
	const kotek::json::value_to_tag<zircon_component_input>&,
	const kotek::json::value& read_from)
{
	auto input = read_from.as_object();

	zircon_component_input result;

	result.set_enabled(
		input.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_INPUT_FIELD_M_IS_ENABLED)
			.as_bool());

	#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(
		input.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_INPUT_FIELD_M_COMPONENT_TYPE)
				.to_number<kotek::uint8_t>() == result.get_component_type(),
		"component type is not equal, data corruption?");
	#endif

	return result;
}
#endif