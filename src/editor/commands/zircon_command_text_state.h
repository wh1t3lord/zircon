#pragma once

#include "zircon_command_definitions.h"

class zircon_component_interface;
class zircon_factory;
struct zircon_ecs_context_t;
enum eZirconComponentType;

/// One component state as bounded JSON text. Serialization and destruction
/// of backend JSON objects happen inside their owning module; only bytes
/// cross the ECS <-> commands DLL boundary (task Z9).
class zircon_command_text_state
{
public:
	bool capture(zircon_component_interface* p_component) noexcept;
	bool assign_text(const kotek::cstring_view_t& text,
		kotek::ktk::json::error_code& parse_error) noexcept;
	bool assign_json(const kotek::ktk::json::value& state) noexcept;
	bool apply(zircon_factory* p_factory,
		zircon_component_interface* p_component) const noexcept;
	bool create_component(zircon_factory* p_factory,
		zircon_ecs_context_t* p_context, kotek::entity_t id,
		eZirconComponentType type) const noexcept;

	const char* c_str(void) const noexcept;
	kotek::cstring_view_t text(void) const noexcept;
	kotek::size_t size(void) const noexcept;
	bool empty(void) const noexcept;

private:
	kotek::static_cstring_t<zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON> m_text;
};
