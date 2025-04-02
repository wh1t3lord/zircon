#include "zircon_session_editor_manager.h"
#include "zircon_session_editor.h"

static_assert(std::numeric_limits<kotek::uint8_t>::max() >
		ZIRCON_DEF_EDITOR_SESSION_MANAGER_MAX_SESSION_COUNT,
	"overflow, too much sessions, are you even sure you need it ? report to "
	"developers https://github.com/wh1t3lord/zircon/issues");

zircon_session_editor_manager::zircon_session_editor_manager() :
	m_p_main_manager{}
{
}

zircon_session_editor_manager::~zircon_session_editor_manager()
{
	KOTEK_ASSERT(this->m_sessions.size(),
		"sessions weren't deallocated as well as shutdown wasn't called for "
		"manager!");
}

void zircon_session_editor_manager::initialize(
	kotek::core::ktkMainManager* p_main_manager)
{
	KOTEK_ASSERT(p_main_manager, "you have to pass a valid main manager");

	this->m_p_main_manager = p_main_manager;
}

bool zircon_session_editor_manager::create_session(
	const kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>&
		session_name)
{
	kotek::uint8_t generated_session_id{};

	// todo: probably you have to make complex heuristic for generation but for
	// keep architecture simplier let's define as like this
	generated_session_id = this->m_sessions.size();

	zircon_session_editor* p_session =
		new zircon_session_editor(session_name, generated_session_id);

	KOTEK_ASSERT(p_session, "failed to allocate memory for session: {} {}",
		generated_session_id, session_name);

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("created session: {} {}", generated_session_id, session_name);
#endif

	return bool(p_session);
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
			p_session->shutdown();
			delete p_session;
		}
	}

	this->m_sessions.clear();
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
