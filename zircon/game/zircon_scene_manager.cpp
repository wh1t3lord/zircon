#include "zircon_scene_manager.h"
#include "../ecs/components/zircon_factory.h"
#include "zircon_game_manager.h"

zircon_scene_manager::zircon_scene_manager(
	zircon_factory_game* p_game_factory, zircon_manager_game* p_game_manager) :
	m_p_game_factory{p_game_factory},
	m_p_game_manager{p_game_manager}
{
}

zircon_scene_manager::~zircon_scene_manager(void) {}

void zircon_scene_manager::Initialize()
{
	this->m_scenes.emplace_back();
	this->m_p_current_scene = &this->m_scenes.back();

	this->m_p_current_scene->Initialize(
		this->m_p_game_factory, this->m_p_game_manager);
}

void zircon_scene_manager::Shutdown(void)
{
	for (auto& scene : this->m_scenes)
	{
		scene.Shutdown();
	}

	this->m_scenes.clear();
}

zircon_scene* zircon_scene_manager::GetCurrentScene(void) const noexcept
{
	return this->m_p_current_scene;
}
