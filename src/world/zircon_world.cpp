#include "zircon_world.h"
#include "../ecs/zircon_factory.h"

constexpr kotek::uint8_t _kInvalidWorldID =
	std::numeric_limits<kotek::uint8_t>::max();

zircon_world::zircon_world(kotek::uint8_t id) :
	m_is_initialized{}, m_id{id}, m_entity_count_max_limit{},
	m_p_ecs_factory{}, m_name{"not_inited"}
{
}

zircon_world::zircon_world(void) :
	m_is_initialized{}, m_id{_kInvalidWorldID},
	m_entity_count_max_limit{}, m_p_ecs_factory{},
	m_name{"not_inited"}
{
}

zircon_world::~zircon_world(void) {}

void zircon_world::shutdown(zircon_factory* p_factory) noexcept
{
#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("destroying world: {}", this->m_name);
#endif

	KOTEK_ASSERT(p_factory, "must be valid");

	if (p_factory)
	{
		KOTEK_ASSERT(
			this->m_p_ecs_factory,
			"memory corruption or logic is broken?"
		);

		p_factory->destroy_context(this->m_p_ecs_factory);
		this->m_p_ecs_factory = nullptr;
	}

	this->m_is_initialized = false;
}

void zircon_world::initialize(
	const kotek::static_cstring_t<
		ZIRCON_DEF_WORLD_NAME_MAX_STRING_LENGTH>& name,
	zircon_config* p_config,
	kotek::core::ktkConsole* p_console,
	kotek::core::ktkIInput* p_input,
	zircon_factory* p_factory,
	kotek::uint32_t max_limit_entity_count
) noexcept
{
	KOTEK_ASSERT(
		max_limit_entity_count > 0,
		"should be a valid arg (non-zero)"
	);

	KOTEK_ASSERT(p_factory, "must be valid");

	KOTEK_ASSERT(
		this->m_p_ecs_factory == nullptr,
		"you must have a not initialized field otherwise it "
		"means that might be a memory corruption otherwise "
		"some logic is broken..."
	);

	KOTEK_ASSERT(
		this->m_is_initialized == false,
		"you need to call this only when is not initialized!"
	);

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("created world: {}", name);
#endif

	this->m_name = name;
	this->m_entity_count_max_limit = max_limit_entity_count;

	if (p_factory)
	{
		this->m_p_ecs_factory = p_factory->create_context(
			this->m_entity_count_max_limit
		);
	}

	this->m_is_initialized = true;
}

kotek::uint8_t zircon_world::get_id(void) const noexcept
{
	return this->m_id;
}

bool zircon_world::is_initialized(void) const noexcept
{
	return this->m_is_initialized;
}

const char* zircon_world::get_name(void) const noexcept
{
	return this->m_name.c_str();
}

zircon_ecs_context_t* zircon_world::get_ecs_context(void
) const noexcept
{
	return this->m_p_ecs_factory;
}
