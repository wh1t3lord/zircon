#include "zircon_session_game.h"
#include "../world/zircon_world.h"

constexpr kotek::uint8_t _kInvalidSessionID =
	std::numeric_limits<kotek::uint8_t>::max();

zircon_session_game::zircon_session_game(kotek::uint8_t id) :
	m_was_initialized{}, m_id{id}, m_p_world{}, m_name{"not_inited"}
{
}

zircon_session_game::zircon_session_game(void) :
	m_was_initialized{}, m_id{_kInvalidSessionID}, m_p_world{},
	m_name{"not_inited"}
{
}

zircon_session_game::~zircon_session_game(void)
{
	KOTEK_ASSERT(
		!this->m_was_initialized, "you forgot to call shutdown method");
}

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

void zircon_session_game::shutdown(void) 
{
	KOTEK_ASSERT(
		this->m_was_initialized, "call it only for initialized instance!");

	if (this->m_was_initialized)
	{
		if (this->m_p_world)
		{
			this->m_p_world->shutdown();
		}

		this->m_was_initialized = false;
	}
}

void zircon_session_game::update(void) {}

void zircon_session_game::initialize(
	const kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>&
		session_name,
	kotek::uint8_t id, zircon_world* p_current_world)
{
	KOTEK_ASSERT(session_name.empty() == false,
		"you must pass a reasonable name please!");
	KOTEK_ASSERT(p_current_world, "must be a valid world!");
	KOTEK_ASSERT(this->m_id != _kInvalidSessionID ? this->m_id == id : true,
		"you must pass a same id as you passed in ctor. Otherwise it means "
		"that you won't obtain a right session by id. Default ctor was created "
		"for handling rare case where a user needs pure deferred "
		"initialization, but when it comes is unknown... So generally for "
		"validation of your ::initialize calling you have to construct this "
		"instance with ctor that accepts id in order to prevent some "
		"misclicking behaviour");
	KOTEK_ASSERT(p_current_world->get_factory(),
		"you must initialize factory inside of world!");
	KOTEK_ASSERT(this->m_was_initialized == false,
		"you should call shutdown and then initialize or reinitialize method "
	    "for calling twice. "
		"Otherwise why do you need to call this method again?");

	if (!this->m_was_initialized)
	{
		this->m_name = session_name;
		this->m_id = id;

		this->m_p_world = p_current_world;

		this->m_was_initialized = true;
	}
}
