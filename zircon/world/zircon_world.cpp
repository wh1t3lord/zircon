#include "zircon_world.h"
#include "../ecs/components/zircon_factory.h"
#include "../editor/ui/zircon_editor_ui_state.h"

zircon_world::zircon_world(void) :
	m_actor_entity_id{}, m_p_game_factory{}
{
}

zircon_world::~zircon_world(void) {}

void zircon_world::Initialize(
	zircon_factory_game* p_factory) noexcept
{
	KOTEK_ASSERT(
		p_factory, "you can't pass an empty pointer of zircon_GameFactory!");

	this->m_p_game_factory = p_factory;
}

void zircon_world::Shutdown(void) noexcept
{
	// TODO: think about render resource manager and handling
	// model,texture deallocation
}

const kotek::ktk::ustring& zircon_world::GetSceneName(void) const noexcept
{
	return this->m_scene_name;
}

void zircon_world::SetSceneName(const kotek::ktk::ustring& scene_name) noexcept
{
	this->m_scene_name = scene_name;
}

kotek::view_entities_t zircon_world::GetEntities(void) const noexcept
{
	return this->m_p_game_factory->GetAllEntities();
}

entt::entity zircon_world::GetActor(void) const noexcept
{
	return this->m_actor_entity_id;
}

void zircon_world::SetActor(entt::entity actor_id) noexcept
{
	this->m_actor_entity_id = actor_id;
}

entt::entity zircon_world::CreateEntity(void)
{
	auto result = this->m_p_game_factory->CreateEntity();
	return result;
}

bool zircon_world::RemoveEntity(entt::entity id)
{
	auto result = this->m_p_game_factory->RemoveEntity(id);
	return result;
}
