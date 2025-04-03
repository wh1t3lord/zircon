#pragma once

#include "../ecs/components/zircon_factory.h"

/// todo: move to generalized config to zircon.core project please (later)
#define ZIRCON_DEF_WORLD_NAME_MAX_STRING_LENGTH 16

// TODO: probably I need to be sure that this class I can use like thread safe
// otherwise I need to implement that because thread safe is only factory...
class zircon_world
{
public:
	zircon_world(void);
	~zircon_world(void);

	void initialize(
		const kotek::static_cstring_t<ZIRCON_DEF_WORLD_NAME_MAX_STRING_LENGTH>&
			name,
		zircon_config* p_config, kotek::core::ktkConsole* p_console,
		kotek::core::ktkIInput* p_input) noexcept;

	void shutdown(void) noexcept;

	entt::entity create_entity(void);

	bool remove_entity(entt::entity id);

	const char* get_name(void) const noexcept;

	kotek::view_entities_t get_entities(void) const noexcept;

	entt::entity get_actor(void) const noexcept;
	void set_actor(entt::entity actor_id) noexcept;

	zircon_factory* get_factory(void) noexcept;
	const zircon_factory* get_factory(void) const noexcept;

private:
	entt::entity m_actor_entity_id;
	kotek::static_cstring_t<ZIRCON_DEF_WORLD_NAME_MAX_STRING_LENGTH> m_name;
	zircon_factory m_factory;
};