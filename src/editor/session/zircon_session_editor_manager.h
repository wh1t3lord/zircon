#pragma once

#include "../../core/zircon_session.h"

class zircon_session_editor;
class zircon_config;

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

	void initialize(zircon_config* p_config, kotek::core::ktkMainManager* p_main_manager);

	kotek::uint8_t create_session(void) override;
	zircon_session_editor* get_session(kotek::uint8_t id) const noexcept;
	void destroy_session(kotek::uint8_t id) override;

	eZirconSessionType get_type() const noexcept override;

	void shutdown(void) override;
	void update(void) override;

	kotek::uint8_t get_current_session_id(void) const noexcept;
	void set_current_session_id(kotek::uint8_t id) noexcept;

private:
	kotek::uint8_t m_current_session_id;
	kotek::core::ktkMainManager* m_p_main_manager;
	zircon_config* m_p_config;
	kotek::static_vector_t<zircon_session_editor*,
		ZIRCON_DEF_EDITOR_SESSION_MANAGER_MAX_SESSION_COUNT>
		m_sessions;
};