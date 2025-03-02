#pragma once

#define zircon_DEF_MAX_COMPONENT_NAME_SIZE 128
#define zircon_DEF_JSON_SERIALIZE_COMPONENT_NAME_FIELD "m_component_name"
#define zircon_DEF_TAG_INVOKE_REG_COMPONENT_NAME(json_object, entity_class) \
	json_object[zircon_DEF_JSON_SERIALIZE_COMPONENT_NAME_FIELD] =           \
		entity_class.GetComponentName().c_str();
#define ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD "m_is_enabled"
#define ZIRCON_DEF_COMPONENT_INPUT_DEFAULT_SENSETIVITY 0.1f

class zircon_component_interface
{
public:
	virtual ~zircon_component_interface(void) {}

	virtual void DrawImGui(
		Kotek::Core::ktkMainManager* main_manager) noexcept = 0;
	virtual void Deserialize(
		const Kotek::ktk::json::value& serialized_data) noexcept = 0;
	virtual Kotek::ktk::json::value Serialize() noexcept = 0;
	virtual Kotek::ktk::json::value Serialize(
		unsigned char* p_raw_memory, Kotek::ktk::size_t size) = 0;
	virtual kotek::uint8_t get_component_type(void) const noexcept = 0;
};

#include "zircon_ecs_auto_enum_components.h"