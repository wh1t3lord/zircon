#pragma once

#include "zircon_scene.h"

class zircon_manager_game;
class zircon_factory_game;

class zircon_scene_manager
{
public:
	zircon_scene_manager(zircon_factory_game* p_game_factory,
		zircon_manager_game* p_game_manager);
	~zircon_scene_manager(void);

	void Initialize(void);
	void Shutdown(void);

	zircon_scene* GetCurrentScene(void) const noexcept;

private:
	zircon_factory_game* m_p_game_factory;
	zircon_manager_game* m_p_game_manager;
	zircon_scene* m_p_current_scene;

	// TODO: maybe better to use list instead of vector
	Kotek::ktk::vector<zircon_scene> m_scenes;
};