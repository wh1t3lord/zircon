#include "zircon_session_game.h"

constexpr kotek::uint8_t _kInvalidSessionID =
	std::numeric_limits<kotek::uint8_t>::max();

zircon_session_game::zircon_session_game(
	const kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>&
		session_name,
	kotek::uint8_t id) : m_id{_kInvalidSessionID}, m_name{"not_inited"}
{
}

zircon_session_game::zircon_session_game(void) {}

zircon_session_game::~zircon_session_game(void) {}

kotek::uint8_t zircon_session_game::get_id(void) const noexcept
{
	return this->m_id;
}

const char* zircon_session_game::get_session_name(void) const noexcept
{
	return this->m_name.c_str();
}

eZirconSessionType zircon_session_game::get_type(void) const noexcept
{
	return eZirconSessionType::kGame;
}

void zircon_session_game::initialize(const Kotek::ktk::ustring& scene_name) {}

void zircon_session_game::shutdown(void) {}

void zircon_session_game::update(void) {}
