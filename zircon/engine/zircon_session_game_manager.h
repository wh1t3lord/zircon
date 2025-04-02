#pragma once

#include "../core/zircon_session.h"

class zircon_session_game;

#define ZIRCON_DEF_SESSION_GAME_MANAGER_MAX_SESSION_COUNT 4

class zircon_session_game_manager : public zircon_interface_session_manager
{
public:
	zircon_session_game_manager(void);
	~zircon_session_game_manager(void);

	void initialize(kotek::core::ktkMainManager* p_main_manager);

	bool create_session(
		const kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>&
			session_name) override;

	void destroy_session(kotek::uint8_t id) override;

	eZirconSessionType get_type() const noexcept override;

	void shutdown(void) override;
	void update(void) override;

private:
	kotek::core::ktkMainManager* m_p_main_manager;
	kotek::static_vector_t<zircon_session_game*,
		ZIRCON_DEF_SESSION_GAME_MANAGER_MAX_SESSION_COUNT>
		m_sessions;
};