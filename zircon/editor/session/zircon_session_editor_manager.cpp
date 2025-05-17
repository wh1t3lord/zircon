#include "zircon_session_editor_manager.h"
#include "zircon_session_editor.h"
#include "../../core/zircon_config.h"

static_assert(std::numeric_limits<kotek::uint8_t>::max() >
		ZIRCON_DEF_EDITOR_SESSION_MANAGER_MAX_SESSION_COUNT,
	"overflow, too much sessions, are you even sure you need it ? report to "
	"developers https://github.com/wh1t3lord/zircon/issues");

constexpr kotek::uint8_t _kInvalidSessionID =
	std::numeric_limits<kotek::uint8_t>::max();

zircon_session_editor_manager::zircon_session_editor_manager() :
	m_current_session_id{_kInvalidSessionID}, m_p_main_manager{}
{
}

zircon_session_editor_manager::~zircon_session_editor_manager()
{
	KOTEK_ASSERT(this->m_sessions.empty(),
		"sessions weren't deallocated as well as shutdown wasn't called for "
		"manager!");
}

void zircon_session_editor_manager::initialize(
	zircon_config* p_config, kotek::core::ktkMainManager* p_main_manager)
{
	KOTEK_ASSERT(p_main_manager, "you have to pass a valid main manager");

	this->m_p_main_manager = p_main_manager;
	this->m_p_config = p_config;
}

kotek::uint8_t zircon_session_editor_manager::create_session(void)
{
	kotek::uint8_t generated_session_id{};

	// todo: probably you have to make complex heuristic for generation but for
	// keep architecture simplier let's define as like this
	generated_session_id = this->m_sessions.size();

	zircon_session_editor* p_session =
		new zircon_session_editor(generated_session_id);

	KOTEK_ASSERT(p_session, "failed to allocate memory for session: {}",
		generated_session_id);

	if (p_session)
	{
#ifdef KOTEK_DEBUG
		KOTEK_MESSAGE("created session: {}", generated_session_id);
#endif
		p_session->m_was_allocated_by_manager = true;
		this->m_sessions.push_back(p_session);
	}
	else
	{
		KOTEK_MESSAGE_ERROR(
			"failed to allocate session: {}", generated_session_id);
		generated_session_id = _kInvalidSessionID;
	}

	return generated_session_id;
}

zircon_session_editor* zircon_session_editor_manager::get_session(
	kotek::uint8_t id) const noexcept
{
#ifdef KOTEK_DEBUG
	bool was_found{};
	kotek::uint8_t duplicate{};
#endif

	zircon_session_editor* p_result{};

	for (zircon_session_editor* p_session : this->m_sessions)
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

void zircon_session_editor_manager::destroy_session(kotek::uint8_t id)
{
#ifdef KOTEK_DEBUG
	bool was_found{};
	kotek::uint8_t has_duplicate{};
#endif

	kotek::uint8_t index{};
	for (zircon_session_editor* p_session : this->m_sessions)
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

				p_session->m_was_destroyed_by_manager = true;

				KOTEK_ASSERT(p_session->m_was_initialized == false,
					"you have to call ::shutdown in session instance before "
					"destruction!");
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

eZirconSessionType zircon_session_editor_manager::get_type() const noexcept
{
	return eZirconSessionType::kEditor;
}

void zircon_session_editor_manager::shutdown(void)
{
	for (zircon_session_editor* p_session : this->m_sessions)
	{
		if (p_session)
		{
#ifdef KOTEK_DEBUG
			p_session->m_was_destroyed_by_manager = true;
#endif

			p_session->shutdown();
			delete p_session;
		}
	}

	this->m_sessions.clear();

	this->m_p_main_manager = nullptr;
	this->m_p_config = nullptr;
}

void zircon_session_editor_manager::update(void)
{
	for (zircon_session_editor* p_session : this->m_sessions)
	{
		if (p_session)
		{
			p_session->update();
		}
	}
}

kotek::uint8_t zircon_session_editor_manager::get_current_session_id(
	void) const noexcept
{
	return this->m_current_session_id;
}

void zircon_session_editor_manager::set_current_session_id(
	kotek::uint8_t id) noexcept
{
	this->m_current_session_id = id;
}
