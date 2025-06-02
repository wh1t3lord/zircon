#pragma once

#include "../../core/zircon_session.h"

class zircon_config;
class zircon_session_game;

#define ZIRCON_DEF_SESSION_GAME_MANAGER_MAX_SESSION_COUNT 1

class zircon_session_game_manager : public zircon_interface_session_manager
{
public:
	zircon_session_game_manager(void);
	~zircon_session_game_manager(void);

	void initialize(zircon_config* p_config, kotek::core::ktkMainManager* p_main_manager);

	kotek::uint8_t create_session(void) override;
	zircon_session_game* get_session(kotek::uint8_t id);
	void destroy_session(kotek::uint8_t id) override;

	eZirconSessionType get_type() const noexcept override;

	void shutdown(void) override;
	void update(void) override;

	kotek::uint8_t get_current_session_id(void) const noexcept;
	void set_current_session_id(kotek::uint8_t session_id) noexcept;

private:
	kotek::uint8_t m_current_session_id;
	kotek::core::ktkMainManager* m_p_main_manager;
	zircon_config* m_p_config;
	kotek::static_vector_t<zircon_session_game*,
		ZIRCON_DEF_SESSION_GAME_MANAGER_MAX_SESSION_COUNT>
		m_sessions;
};