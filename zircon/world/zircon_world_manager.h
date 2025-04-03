#pragma once

#include "zircon_world.h"

class zircon_manager_game;
class zircon_factory;

class zircon_world_manager
{
public:
	zircon_world_manager(zircon_factory* p_game_factory);
	~zircon_world_manager(void);

	void Initialize(void);
	void Shutdown(void);

	zircon_world* GetCurrentScene(void) const noexcept;

private:
	zircon_factory* m_p_game_factory;
	zircon_world* m_p_current_scene;

	// TODO: maybe better to use list instead of vector
	Kotek::ktk::vector<zircon_world> m_scenes;
};