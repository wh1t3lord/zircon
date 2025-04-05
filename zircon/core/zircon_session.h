#pragma once

// todo: move to one global header in core that will accumulate all defines in
// one and provide CMake changing feature for compile-time changes
#define ZIRCON_DEF_MAX_SESSION_NAME_LENGTH 16

enum class eZirconSessionType : kotek::enum_base_t
{
	kEditor,
	kGame,
	kUnknown = -1
};

class zircon_interface_session
{
public:
	virtual ~zircon_interface_session(void) {}

	virtual kotek::uint8_t get_id(void) const noexcept = 0;
	virtual const char* get_session_name(void) const noexcept = 0;
	virtual eZirconSessionType get_type() const noexcept = 0;
	virtual void shutdown(void) = 0;
	virtual void update(void) = 0;
};

class zircon_interface_session_manager
{
public:
	virtual ~zircon_interface_session_manager(void) {}

	virtual kotek::uint8_t create_session(void) = 0;
	virtual void destroy_session(kotek::uint8_t id) = 0;
	virtual eZirconSessionType get_type() const noexcept = 0;
	virtual void shutdown(void) = 0;
	virtual void update(void) = 0;
};