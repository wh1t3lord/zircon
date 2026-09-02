#pragma once

#include "zircon_command_definitions.h"

class zircon_session_editor_manager;
class zircon_factory;

/// @brief \~english bounds-checked byte writer used by commands to
/// serialize their delta payload into a journal entry; never throws,
/// overflow is sticky and queryable
class zircon_command_delta_writer
{
public:
	zircon_command_delta_writer(
		unsigned char* p_buffer, kotek::size_t capacity
	) noexcept;

	bool write_u32(kotek::uint32_t value) noexcept;
	bool write_bytes(
		const void* p_data, kotek::size_t size
	) noexcept;
	/// @brief \~english writes a string as [u32 length][bytes]
	bool write_string(
		const char* p_string, kotek::size_t length
	) noexcept;

	kotek::size_t get_offset(void) const noexcept;
	bool is_valid(void) const noexcept;

private:
	unsigned char* m_p_buffer;
	kotek::size_t m_capacity;
	kotek::size_t m_offset;
	bool m_is_overflow;
};

/// @brief \~english bounds-checked byte reader, mirror of
/// zircon_command_delta_writer
class zircon_command_delta_reader
{
public:
	zircon_command_delta_reader(
		const unsigned char* p_buffer, kotek::size_t size
	) noexcept;

	kotek::uint32_t read_u32(bool* p_is_valid = nullptr) noexcept;
	bool read_bytes(
		void* p_output, kotek::size_t size
	) noexcept;
	/// @brief \~english reads a [u32 length][bytes] string into a
	/// caller buffer and zero terminates it
	bool read_string(
		char* p_output, kotek::size_t output_capacity
	) noexcept;

	kotek::size_t get_offset(void) const noexcept;
	bool is_valid(void) const noexcept;

private:
	const unsigned char* m_p_buffer;
	kotek::size_t m_size;
	kotek::size_t m_offset;
	bool m_is_overflow;
};

/// @brief \~english every journaled command implements this next to
/// ktkISDKRedoUndo: the delta (changed fields before/after, never a
/// whole state) is what gets written into the journal entry payload
class zircon_interface_command_delta
{
public:
	virtual ~zircon_interface_command_delta(void) {}

	virtual bool Serialize_Delta(
		zircon_command_delta_writer& writer
	) noexcept = 0;
	virtual bool Deserialize_Delta(
		zircon_command_delta_reader& reader
	) noexcept = 0;
};

/// @brief \~english creates a command instance into placement memory
/// (the memory is owned by the history pool, at least
/// zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE bytes)
using zircon_command_create_fn_t =
	kotek::core::ktkISDKRedoUndo* (*)(
		void* p_placement_memory,
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	) noexcept;

struct zircon_command_type_info
{
	kotek::uint32_t m_command_type;
	const char* m_p_debug_name;
	zircon_command_create_fn_t m_p_create;
	kotek::size_t m_instance_size;
};

/// @brief \~english static table command_type <-> creation info;
/// future editor commands (terrain ops, prefab ops, batch ops)
/// register here once and join the journal with zero history-manager
/// changes (owner directive, task Z6)
class zircon_command_registry
{
public:
	static constexpr kotek::size_t _k_max_types = 32;

	zircon_command_registry(void);

	bool register_type(
		const zircon_command_type_info& info
	) noexcept;

	const zircon_command_type_info* find_by_type(
		kotek::uint32_t command_type
	) const noexcept;

	/// @brief \~english sizeof of the largest registered command,
	/// must never exceed
	/// zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE
	kotek::size_t get_max_instance_size(void) const noexcept;

	kotek::size_t get_registered_count(void) const noexcept;

private:
	kotek::static_vector_t<
		zircon_command_type_info,
		_k_max_types>
		m_types;
	kotek::size_t m_max_instance_size;
};

/// @brief \~english registers the built-in command set
/// (create/delete entity, add/delete component); called by
/// zircon_editor_command_history::initialize
void zircon_register_builtin_command_types(
	zircon_command_registry& registry
) noexcept;
