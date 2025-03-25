#pragma once

#define zircon_DEF_MAX_COMPONENT_NAME_SIZE 128
#define ZIRCON_DEF_COMPONENT_INPUT_DEFAULT_SENSETIVITY 0.1f

class zircon_component_interface
{
public:
	virtual ~zircon_component_interface(void) {}

	virtual void draw_imgui(
		Kotek::Core::ktkMainManager* main_manager) noexcept = 0;
	virtual void deserialize(
		const Kotek::ktk::json::value& serialized_data) noexcept = 0;
	virtual Kotek::ktk::json::value serialize() noexcept = 0;
	virtual Kotek::ktk::json::value serialize(
		unsigned char* p_raw_memory, Kotek::ktk::size_t size) = 0;
	virtual kotek::uint8_t get_component_type(void) const noexcept = 0;
};

#include "zircon_ecs_auto_enum_components.h"
#include "zircon_component_fields.h"