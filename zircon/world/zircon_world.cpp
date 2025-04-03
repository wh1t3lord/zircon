#include "zircon_world.h"

zircon_world::zircon_world(void) : m_actor_entity_id{} {}

zircon_world::~zircon_world(void) {}

void zircon_world::initialize(
	const kotek::static_cstring_t<ZIRCON_DEF_WORLD_NAME_MAX_STRING_LENGTH>&
		name,
	zircon_config* p_config, kotek::core::ktkConsole* p_console,
	kotek::core::ktkIInput* p_input) noexcept
{
}

void zircon_world::shutdown(void) noexcept
{
#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("destroying world: {}", this->m_name);
#endif

	this->m_factory.Shutdown();
}

void zircon_world::initialize(
	const kotek::static_cstring_t<ZIRCON_DEF_WORLD_NAME_MAX_STRING_LENGTH>&
		name,
	zircon_config* p_config, kotek::core::ktkConsole* p_console,
	kotek::core::ktkIInput* p_input) noexcept
{
#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("created world: {}", name);
#endif

	this->m_name = name;

	this->m_factory.Initialize(p_config, p_console, p_input);
}

kotek::view_entities_t zircon_world::get_entities(void) const noexcept
{
	return this->m_factory.GetAllEntities();
}

entt::entity zircon_world::get_actor(void) const noexcept
{
	return this->m_actor_entity_id;
}

void zircon_world::set_actor(entt::entity actor_id) noexcept
{
	this->m_actor_entity_id = actor_id;
}

zircon_factory* zircon_world::get_factory(void) noexcept
{
	return &this->m_factory;
}

const zircon_factory* zircon_world::get_factory(void) const noexcept
{
	return static_cast<const zircon_factory*>(&this->m_factory);
}

entt::entity zircon_world::create_entity(void)
{
	auto result = this->m_factory.CreateEntity();
	return result;
}

bool zircon_world::remove_entity(entt::entity id)
{
	auto result = this->m_factory.RemoveEntity(id);
	return result;
}

const char* zircon_world::get_name(void) const noexcept
{
	return this->m_name.c_str();
}
