#include "zircon_world_manager.h"
#include "../ecs/components/zircon_factory.h"

zircon_world_manager::zircon_world_manager(
	zircon_factory_game* p_game_factory) :
	m_p_game_factory{p_game_factory}, m_p_current_scene{}
{
}

zircon_world_manager::~zircon_world_manager(void) {}

void zircon_world_manager::Initialize()
{
	this->m_scenes.emplace_back();
	this->m_p_current_scene = &this->m_scenes.back();

	this->m_p_current_scene->Initialize(
		this->m_p_game_factory);
}

void zircon_world_manager::Shutdown(void)
{
	for (auto& scene : this->m_scenes)
	{
		scene.Shutdown();
	}

	this->m_scenes.clear();
}

zircon_world* zircon_world_manager::GetCurrentScene(void) const noexcept
{
	return this->m_p_current_scene;
}
