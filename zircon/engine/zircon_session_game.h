#pragma once

#include "../core/zircon_session.h"

class zircon_world;
class zircon_manager_game;

class zircon_session_game : public zircon_interface_session
{
public:
	zircon_session_game(
		const kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>&
			session_name,
		kotek::uint8_t id);
	zircon_session_game(void);
	~zircon_session_game(void);

	kotek::uint8_t get_id(void) const noexcept override;
	const char* get_session_name(void) const noexcept override;
	eZirconSessionType get_type(void) const noexcept override;
	void shutdown(void) override;
	void update(void) override;

	void initialize(const Kotek::ktk::ustring& scene_name);

private:
	kotek::uint8_t m_id;
	zircon_manager_game* m_p_game_manager;
	kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH> m_name;
};
