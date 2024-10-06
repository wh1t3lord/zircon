#include "zircon_scene.h"
#include "../ecs/components/zircon_factory.h"
#include "zircon_game_manager.h"
#include "../core/zircon_sdk_ui.h"

zircon_scene::zircon_scene(void) :
	m_actor_entity_id{}, m_p_game_factory{}, m_p_game_manager{}
{
}

zircon_scene::~zircon_scene(void) {}

void zircon_scene::Initialize(
	zircon_factory_game* p_factory, zircon_manager_game* p_game_manager) noexcept
{
	KOTEK_ASSERT(
		p_factory, "you can't pass an empty pointer of zircon_GameFactory!");
	KOTEK_ASSERT(
		p_game_manager, "you must pass a valid pointer of zircon_GameManager!");

	this->m_p_game_factory = p_factory;
	this->m_p_game_manager = p_game_manager;
}

void zircon_scene::Shutdown(void) noexcept
{
	// TODO: think about render resource manager and handling
	// model,texture deallocation
	for (auto id : this->m_entities)
	{
		this->m_p_game_factory->RemoveEntity(id);
	}

	this->m_entities.clear();
}

const Kotek::ktk::ustring& zircon_scene::GetSceneName(void) const noexcept
{
	return this->m_scene_name;
}

void zircon_scene::SetSceneName(const Kotek::ktk::ustring& scene_name) noexcept
{
	this->m_scene_name = scene_name;
}

const Kotek::ktk::unordered_set<Kotek::ktk::entity_t>&
zircon_scene::GetEntities(void) const noexcept
{
	return this->m_entities;
}

Kotek::ktk::entity_t zircon_scene::GetActor(void) const noexcept
{
	return this->m_actor_entity_id;
}

void zircon_scene::SetActor(Kotek::ktk::entity_t actor_id) noexcept
{
	this->m_actor_entity_id = actor_id;
}

Kotek::ktk::entity_t zircon_scene::CreateEntity(void)
{
	auto result = this->m_p_game_factory->CreateEntity();

	this->m_entities.insert(result);

	this->m_p_game_manager->GetSDKUI()->AddObjectToSceneList(result);

	return result;
}

bool zircon_scene::RemoveEntity(Kotek::ktk::entity_t id)
{
	auto result = this->m_p_game_factory->RemoveEntity(id);

	auto iter = this->m_entities.find(id);
	if (iter != this->m_entities.end())
		this->m_entities.erase(iter);

	this->m_p_game_manager->GetSDKUI()->DeleteObjectFromSceneList(id);

	return result;
}
