#pragma once

#include "zircon_session.h"

class zircon_world;
class zircon_manager_game;

class zircon_session_game : public zircon_interface_session
{
public:
	zircon_session_game(void);
	~zircon_session_game(void);

	void initialize(const Kotek::ktk::ustring& scene_name);
	void shutdown(void) override;
	void update(void) override;

private:
	zircon_manager_game* m_p_game_manager;
};
