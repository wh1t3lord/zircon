#pragma once

#define zircon_DEF_MAX_COMPONENT_NAME_SIZE 128
#define ZIRCON_DEF_COMPONENT_INPUT_DEFAULT_SENSETIVITY 0.1f

class zircon_component_interface
{
public:
	virtual ~zircon_component_interface(void) {}
	virtual kotek::uint8_t get_component_type(void) const noexcept = 0;
};

#include "zircon_ecs_auto_enum_components.h"
#include "zircon_component_fields.h"