#pragma once

#include "../core/zircon_defs.h"

class zircon_factory;
class zircon_config;
struct zircon_ecs_context_t;

// TODO: probably I need to be sure that this class I can use
// like thread safe otherwise I need to implement that because
// thread safe is only factory...
class zircon_world
{
public:
	zircon_world(kotek::uint8_t id);
	zircon_world(void);
	~zircon_world(void);

	void initialize(
		const kotek::static_cstring_t<
			ZIRCON_DEF_WORLD_NAME_MAX_STRING_LENGTH>& name,
		zircon_config* p_config,
		kotek::core::ktkConsole* p_console,
		kotek::core::ktkIInput* p_input,
		zircon_factory* p_factory,
		kotek::uint32_t max_limit_entity_count =
			ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT
	) noexcept;

	void shutdown(zircon_factory* p_factory) noexcept;

	const char* get_name(void) const noexcept;

	zircon_ecs_context_t* get_ecs_context(void) const noexcept;

	kotek::uint8_t get_id(void) const noexcept;

	bool is_initialized(void) const noexcept;

private:
	bool m_is_initialized;
	kotek::uint8_t m_id;
	kotek::uint32_t m_entity_count_max_limit;
	/// @brief for pico is ecs_new instance and for entt it is
	/// entt::registry
	zircon_ecs_context_t* m_p_ecs_factory;
	kotek::static_cstring_t<
		ZIRCON_DEF_WORLD_NAME_MAX_STRING_LENGTH>
		m_name;
};