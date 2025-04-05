#include "zircon_session_game_manager.h"
#include "zircon_session_game.h"

static_assert(std::numeric_limits<kotek::uint8_t>::max() >
		ZIRCON_DEF_SESSION_GAME_MANAGER_MAX_SESSION_COUNT,
	"overflow, too much sessions, are you even sure you need it ? report to "
	"developers https://github.com/wh1t3lord/zircon/issues");

constexpr kotek::uint8_t _kInvalidSessionID =
	std::numeric_limits<kotek::uint8_t>::max();

zircon_session_game_manager::zircon_session_game_manager(void) :
	m_p_main_manager{}
{
}

zircon_session_game_manager::~zircon_session_game_manager(void)
{
	KOTEK_ASSERT(this->m_sessions.size(),
		"sessions weren't deallocated as well as shutdown wasn't called for "
		"manager!");
}

void zircon_session_game_manager::initialize(
	kotek::core::ktkMainManager* p_main_manager)
{
	KOTEK_ASSERT(
		p_main_manager, "you have to pass a valid instance of main manager!");

	this->m_p_main_manager = p_main_manager;
}

kotek::uint8_t zircon_session_game_manager::create_session(void)
{
	kotek::uint8_t generated_session_id{};

	// todo: probably you have to make complex heuristic for generation but for
	// keep architecture simplier let's define as like this
	generated_session_id = this->m_sessions.size();

	zircon_session_game* p_session =
		new zircon_session_game(generated_session_id);

	KOTEK_ASSERT(p_session, "failed to allocate memory for session: {}",
		generated_session_id);

#ifdef KOTEK_DEBUG
	if (p_session)
	{
		KOTEK_MESSAGE("created session: {}", generated_session_id);
	}
#endif
	if (!p_session)
	{
		KOTEK_MESSAGE_ERROR(
			"failed to allocate session: {}", generated_session_id);
		generated_session_id = _kInvalidSessionID;
	}

	if (p_session)
	{
		this->m_sessions.push_back(p_session);
#ifdef KOTEK_DEBUG
		KOTEK_MESSAGE("added session#{} to pool", generated_session_id);
#endif
	}

	return generated_session_id;
}

zircon_session_game* zircon_session_game_manager::get_session(kotek::uint8_t id)
{
#ifdef KOTEK_DEBUG
	bool was_found{};
	kotek::uint8_t duplicate{};
#endif

	zircon_session_game* p_result{};

	for (zircon_session_game* p_session : this->m_sessions)
	{
		if (p_session)
		{
			if (p_session->get_id() == id)
			{
#ifdef KOTEK_DEBUG
				if (!p_result)
#endif
					p_result = p_session;

#ifdef KOTEK_DEBUG
				was_found = true;
				++duplicate;
				KOTEK_ASSERT(
					duplicate == 1, "found a duplicate with same id={}!", id);
#endif

#ifndef KOTEK_DEBUG
				break;
#endif
			}
		}
	}

#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(was_found, "failed to obtain session#{}!", id);
#endif

	return p_result;
}

void zircon_session_game_manager::destroy_session(kotek::uint8_t id)
{
#ifdef KOTEK_DEBUG
	bool was_found{};
	kotek::uint8_t has_duplicate{};
#endif

	kotek::uint8_t index{};
	for (zircon_session_game* p_session : this->m_sessions)
	{
		if (p_session)
		{
			if (id == p_session->get_id())
			{
#ifdef KOTEK_DEBUG
				if (has_duplicate > 0)
				{
					KOTEK_ASSERT(has_duplicate == 1,
						"found duplicate of session can't be, did you "
						"destroyed the object right or memory corruption?");
				}

				KOTEK_MESSAGE("session is deleted {} {}", p_session->get_id(),
					p_session->get_session_name());

				was_found = true;
				++has_duplicate;
#endif

				delete p_session;
				this->m_sessions[index] = nullptr;
			}
		}

		++index;
	}

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE_WARNING("session by id[{}] wasn't found, are you sure that "
						  "you passed right data?");
#endif
}

eZirconSessionType zircon_session_game_manager::get_type() const noexcept
{
	return eZirconSessionType::kGame;
}

void zircon_session_game_manager::shutdown(void)
{
	for (zircon_session_game* p_session : this->m_sessions)
	{
		if (p_session)
		{
			p_session->shutdown();
			delete p_session;
		}
	}

	this->m_sessions.clear();
}

void zircon_session_game_manager::update(void)
{
	for (zircon_session_game* p_session : this->m_sessions)
	{
		if (p_session)
		{
			p_session->update();
		}
	}
}
