#include "zircon_command_registry.h"

#include "zircon_command_create_entity.h"
#include "zircon_command_delete_entity.h"
#include "zircon_command_add_component_to_entity.h"
#include "zircon_command_delete_component_from_entity.h"
#include "zircon_command_edit_component_state.h"

zircon_command_delta_writer::zircon_command_delta_writer(
	unsigned char* p_buffer, kotek::size_t capacity
) noexcept :
	m_p_buffer{p_buffer},
	m_capacity{capacity}, m_offset{}, m_is_overflow{}
{
	KOTEK_ASSERT(p_buffer, "you must pass a valid buffer");
	KOTEK_ASSERT(capacity, "you must pass a non-zero capacity");
}

bool zircon_command_delta_writer::write_u32(
	kotek::uint32_t value
) noexcept
{
	return this->write_bytes(&value, sizeof(value));
}

bool zircon_command_delta_writer::write_bytes(
	const void* p_data, kotek::size_t size
) noexcept
{
	KOTEK_ASSERT(p_data, "you must pass a valid data pointer");

	if (this->m_is_overflow)
		return false;

	if (p_data == nullptr || size == 0)
		return true;

	if (this->m_offset + size > this->m_capacity)
	{
		this->m_is_overflow = true;

		KOTEK_MESSAGE_ERROR(
			"delta writer overflow: want {} bytes with only {} "
			"left",
			size,
			this->m_capacity - this->m_offset
		);

		return false;
	}

	kotek::ktk::memory::memcpy(
		this->m_p_buffer + this->m_offset, p_data, size
	);

	this->m_offset += size;

	return true;
}

bool zircon_command_delta_writer::write_string(
	const char* p_string, kotek::size_t length
) noexcept
{
	if (this->write_u32(
			static_cast<kotek::uint32_t>(length)
		) == false)
	{
		return false;
	}

	if (length == 0)
		return true;

	return this->write_bytes(p_string, length);
}

kotek::size_t
zircon_command_delta_writer::get_offset(void) const noexcept
{
	return this->m_offset;
}

bool zircon_command_delta_writer::is_valid(void) const noexcept
{
	return this->m_is_overflow == false;
}

zircon_command_delta_reader::zircon_command_delta_reader(
	const unsigned char* p_buffer, kotek::size_t size
) noexcept :
	m_p_buffer{p_buffer},
	m_size{size}, m_offset{}, m_is_overflow{}
{
	KOTEK_ASSERT(p_buffer, "you must pass a valid buffer");
}

kotek::uint32_t zircon_command_delta_reader::read_u32(
	bool* p_is_valid
) noexcept
{
	kotek::uint32_t result{};

	const bool status = this->read_bytes(&result, sizeof(result));

	if (p_is_valid)
	{
		*p_is_valid = status;
	}

	return result;
}

bool zircon_command_delta_reader::read_bytes(
	void* p_output, kotek::size_t size
) noexcept
{
	KOTEK_ASSERT(p_output, "you must pass a valid output pointer");

	if (this->m_is_overflow)
		return false;

	if (p_output == nullptr || size == 0)
		return true;

	if (this->m_offset + size > this->m_size)
	{
		this->m_is_overflow = true;

		KOTEK_MESSAGE_ERROR(
			"delta reader overflow: want {} bytes with only {} "
			"left",
			size,
			this->m_size - this->m_offset
		);

		return false;
	}

	kotek::ktk::memory::memcpy(
		p_output, this->m_p_buffer + this->m_offset, size
	);

	this->m_offset += size;

	return true;
}

bool zircon_command_delta_reader::read_string(
	char* p_output, kotek::size_t output_capacity
) noexcept
{
	KOTEK_ASSERT(p_output, "you must pass a valid output buffer");
	KOTEK_ASSERT(output_capacity, "must be a non-zero capacity");

	bool status{};

	const kotek::uint32_t length = this->read_u32(&status);

	if (status == false)
		return false;

	if (length + 1 > output_capacity)
	{
		this->m_is_overflow = true;

		KOTEK_MESSAGE_ERROR(
			"delta reader string does not fit: string is {} "
			"bytes, buffer is {} bytes",
			length,
			output_capacity
		);

		return false;
	}

	if (this->read_bytes(p_output, length) == false)
		return false;

	p_output[length] = '\0';

	return true;
}

kotek::size_t
zircon_command_delta_reader::get_offset(void) const noexcept
{
	return this->m_offset;
}

bool zircon_command_delta_reader::is_valid(void) const noexcept
{
	return this->m_is_overflow == false;
}

zircon_command_registry::zircon_command_registry(void) :
	m_types{}, m_max_instance_size{}
{
}

bool zircon_command_registry::register_type(
	const zircon_command_type_info& info
) noexcept
{
	KOTEK_ASSERT(info.m_p_create, "creation fn must be valid");
	KOTEK_ASSERT(
		info.m_p_debug_name, "debug name must be valid"
	);
	KOTEK_ASSERT(
		info.m_instance_size <=
			zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE,
		"command [{}] is too big ({} bytes), increase "
		"zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE (currently "
		"{})",
		info.m_p_debug_name,
		info.m_instance_size,
		zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE
	);

	if (this->find_by_type(info.m_command_type))
	{
		// idempotent registration (initialize may be called more
		// than once across sessions)
		return true;
	}

	if (this->m_types.full())
	{
		KOTEK_MESSAGE_ERROR(
			"command registry is full ({} types), increase "
			"zircon_command_registry::_k_max_types",
			this->m_types.size()
		);
		return false;
	}

	this->m_types.push_back(info);

	if (info.m_instance_size > this->m_max_instance_size)
	{
		this->m_max_instance_size = info.m_instance_size;
	}

	return true;
}

const zircon_command_type_info*
zircon_command_registry::find_by_type(
	kotek::uint32_t command_type
) const noexcept
{
	const zircon_command_type_info* p_result = nullptr;

	for (const auto& info : this->m_types)
	{
		if (info.m_command_type == command_type)
		{
			p_result = &info;
			break;
		}
	}

	return p_result;
}

kotek::size_t zircon_command_registry::get_max_instance_size(
	void
) const noexcept
{
	return this->m_max_instance_size;
}

kotek::size_t zircon_command_registry::get_registered_count(
	void
) const noexcept
{
	return this->m_types.size();
}

namespace
{
	kotek::core::ktkISDKRedoUndo* zircon_create_command_create_entity(
		void* p_placement_memory,
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	) noexcept
	{
		return new (p_placement_memory) zircon_command_create_entity(
			p_manager_session_editor, p_factory
		);
	}

	kotek::core::ktkISDKRedoUndo* zircon_create_command_delete_entity(
		void* p_placement_memory,
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	) noexcept
	{
		return new (p_placement_memory) zircon_command_delete_entity(
			p_manager_session_editor,
			p_factory,
			kotek::ktk::kInvalidECSEntity
		);
	}

	kotek::core::ktkISDKRedoUndo*
	zircon_create_command_add_component(
		void* p_placement_memory,
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	) noexcept
	{
		(void)p_factory;

		return new (p_placement_memory)
			zircon_command_add_component_to_entity(
				p_manager_session_editor
			);
	}

	kotek::core::ktkISDKRedoUndo*
	zircon_create_command_delete_component(
		void* p_placement_memory,
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	) noexcept
	{
		return new (p_placement_memory)
			zircon_command_delete_component_from_entity(
				p_manager_session_editor, p_factory
			);
	}

	kotek::core::ktkISDKRedoUndo*
	zircon_create_command_edit_component_state(
		void* p_placement_memory,
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	) noexcept
	{
		return new (p_placement_memory)
			zircon_command_edit_component_state(
				p_manager_session_editor, p_factory
			);
	}
} // namespace

void zircon_register_builtin_command_types(
	zircon_command_registry& registry
) noexcept
{
	registry.register_type(
		{static_cast<kotek::uint32_t>(
			 kotek::core::eConsoleCommandIndex::
				 kConsoleCommand_SDK_CreateEntity
		 ),
		 "zircon_command_create_entity",
		 &zircon_create_command_create_entity,
		 sizeof(zircon_command_create_entity)}
	);

	registry.register_type(
		{static_cast<kotek::uint32_t>(
			 kotek::core::eConsoleCommandIndex::
				 kConsoleCommand_SDK_DeleteEntity
		 ),
		 "zircon_command_delete_entity",
		 &zircon_create_command_delete_entity,
		 sizeof(zircon_command_delete_entity)}
	);

	registry.register_type(
		{static_cast<kotek::uint32_t>(
			 kotek::core::eConsoleCommandIndex::
				 kConsoleCommand_SDK_CreateComponentForEntityByName
		 ),
		 "zircon_command_add_component_to_entity",
		 &zircon_create_command_add_component,
		 sizeof(zircon_command_add_component_to_entity)}
	);

	registry.register_type(
		{static_cast<kotek::uint32_t>(
			 kotek::core::eConsoleCommandIndex::
				 kConsoleCommand_SDK_DeleteComponentFromEntityByName
		 ),
		 "zircon_command_delete_component_from_entity",
		 &zircon_create_command_delete_component,
		 sizeof(zircon_command_delete_component_from_entity)}
	);

	registry.register_type(
		{zircon_DEF_COMMAND_TYPE_EDIT_COMPONENT_STATE,
		 "zircon_command_edit_component_state",
		 &zircon_create_command_edit_component_state,
		 sizeof(zircon_command_edit_component_state)}
	);
}
