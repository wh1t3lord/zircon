#pragma once

#include "../core/zircon_session.h"

class zircon_session_editor;

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE

class ktkMainManager;

KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

#define ZIRCON_DEF_EDITOR_SESSION_MANAGER_MAX_SESSION_COUNT 1

class zircon_session_editor_manager : public zircon_interface_session_manager
{
public:
	zircon_session_editor_manager();
	~zircon_session_editor_manager();

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
	kotek::static_vector_t<zircon_session_editor*,
		ZIRCON_DEF_EDITOR_SESSION_MANAGER_MAX_SESSION_COUNT>
		m_sessions;
};