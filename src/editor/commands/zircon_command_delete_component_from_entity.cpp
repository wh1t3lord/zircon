#include "zircon_command_delete_component_from_entity.h"
#include "../../world/zircon_world.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "../../ecs/zircon_factory.h"

zircon_command_delete_component_from_entity::
	zircon_command_delete_component_from_entity(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory,
		kotek::entity_t id,
		const char* p_component_string
	) :
	m_id{id},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_factory{p_factory},
	m_component_name{p_component_string},
	m_serialized_state_of_deleted_component{}
{
	KOTEK_ASSERT(
		this->m_p_manager_session_editor,
		"you can't pass an invalid session editor here"
	);

	KOTEK_ASSERT(
		this->m_p_factory,
		"you can't pass an invalid factory here"
	);

	KOTEK_ASSERT(
		p_component_string,
		"you can't pass an empty string here"
	);
	KOTEK_ASSERT(
		strlen(p_component_string),
		"string must not be an empty!"
	);
}

zircon_command_delete_component_from_entity::
	zircon_command_delete_component_from_entity(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	) :
	m_id{kotek::ktk::kInvalidECSEntity},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_factory{p_factory}, m_component_name{},
	m_serialized_state_of_deleted_component{}
{
	KOTEK_ASSERT(
		this->m_p_manager_session_editor,
		"you can't pass invalid pointer of game manager!"
	);

	KOTEK_ASSERT(
		this->m_p_factory, "you must pass a valid factory"
	);
}

zircon_command_delete_component_from_entity::
	~zircon_command_delete_component_from_entity()
{
}

void zircon_command_delete_component_from_entity::Execute(void)
{
	KOTEK_ASSERT(
		this->m_p_manager_session_editor,
		"should be initialzed game manager here"
	);

	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING("failed to execute due to "
		                      "invalid game manager pointer!");
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor
				->get_current_session_id()
		);

	KOTEK_ASSERT(
		p_session,
		"failed to obtain session editor by id: {}",
		this->m_p_manager_session_editor->get_current_session_id()
	);

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session "
			"editor#{}",
			this->m_p_manager_session_editor->get_current_session_id(
			)
		);
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(
		p_world,
		"failed to obtain world in session editor_{}#{}",
		p_session->get_session_name(),
		p_session->get_id()
	);

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world in session "
			"editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	if (!this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING("failed to execute due to invalid factory");
		return;
	}

	if (p_world->get_ecs_context() == nullptr)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid ecs context in editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	if (this->m_p_factory && p_world && p_world->get_ecs_context())
	{
		if (this->m_component_name.empty() == false)
		{
			if (this->m_p_factory->is_valid_entity(
					p_world->get_ecs_context(), this->m_id
				))
			{
				zircon_component_interface* p_component =
					this->m_p_factory->get_component_by_name(
						p_world->get_ecs_context(),
						this->m_id,
						this->m_component_name.c_str()
					);

				if (p_component == nullptr)
				{
					KOTEK_MESSAGE_WARNING(
						"[history] entity {} has no component "
						"[{}], nothing to delete",
						static_cast<kotek::uint32_t>(this->m_id.id),
						this->m_component_name.c_str()
					);
					return;
				}

				// the delta (state before deletion) is captured
				// eagerly so the journal entry is complete even
				// if this object gets evicted from the live pool
				this->m_serialized_state_of_deleted_component =
					zircon_serialize_component(p_component);

				this->m_p_factory->remove_component(
					p_world->get_ecs_context(),
					this->m_id,
					this->m_p_factory->get_component_enum_by_name(
						this->m_component_name.c_str()
					)
				);

				KOTEK_MESSAGE(
					"[history] removed component by name: {} "
					"in entity {}",
					this->m_component_name.c_str(),
					this->m_id
				);
			}
		}
	}
}

void zircon_command_delete_component_from_entity::Undo(void)
{
	KOTEK_ASSERT(
		this->m_p_manager_session_editor,
		"should be initialzed game manager here"
	);

	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING("failed to execute due to "
		                      "invalid game manager pointer!");
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor
				->get_current_session_id()
		);

	KOTEK_ASSERT(
		p_session,
		"failed to obtain session editor by id: {}",
		this->m_p_manager_session_editor->get_current_session_id()
	);

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session "
			"editor#{}",
			this->m_p_manager_session_editor->get_current_session_id(
			)
		);
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(
		p_world,
		"failed to obtain world in session editor_{}#{}",
		p_session->get_session_name(),
		p_session->get_id()
	);

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world in session "
			"editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	if (!this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING("failed to execute due to invalid factory");
		return;
	}

	if (this->m_p_factory)
	{
		if (this->m_component_name.empty() == false)
		{
			if (this->m_p_factory->is_valid_entity(
					p_world->get_ecs_context(), this->m_id
				))
			{
				this->m_p_factory->create_component(
					p_world->get_ecs_context(),
					this->m_id,
					this->m_p_factory->get_component_enum_by_name(
						this->m_component_name.c_str()
					)
				);

				auto* p_component =
					this->m_p_factory->get_component_by_name(
						p_world->get_ecs_context(),
						this->m_id,
						this->m_component_name.c_str()
					);

				if (p_component)
				{
					this->m_p_factory->DeserializeComponent(
						p_component,
						this->m_serialized_state_of_deleted_component
					);
				}

				KOTEK_MESSAGE(
					"[history][undo] restored component by "
					"name: {} for entity {}",
					this->m_component_name.c_str(),
					this->m_id
				);
			}
		}
	}
}

const char*
zircon_command_delete_component_from_entity::GetName()
{
	return "delete component from entity";
}

kotek::entity_t
zircon_command_delete_component_from_entity::GetEntityID(void
) const noexcept
{
	return this->m_id;
}

void zircon_command_delete_component_from_entity::SetEntityID(
	kotek::entity_t id
) noexcept
{
	this->m_id = id;
}

kotek::enum_base_t
zircon_command_delete_component_from_entity::GetCommandType(
) noexcept
{
	return static_cast<kotek::enum_base_t>(
		kotek::core::eConsoleCommandIndex::
			kConsoleCommand_SDK_DeleteComponentFromEntityByName
	);
}

kotek::size_t
zircon_command_delete_component_from_entity::Serialize(
	Kotek::core::ktkFileHandleType file
) noexcept
{
	KOTEK_MESSAGE_WARNING(
		"[history][{}]: Serialize(file) is deprecated, the "
		"journal uses Serialize_Delta instead",
		this->GetName()
	);

	(void)file;

	return 0;
}

bool zircon_command_delete_component_from_entity::Serialize_Delta(
	zircon_command_delta_writer& writer
) noexcept
{
	bool status = writer.write_u32(
		static_cast<kotek::uint32_t>(this->m_id.id)
	);

	status = status &&
		writer.write_string(
			this->m_component_name.c_str(),
			this->m_component_name.size()
		);

	auto state_string = kotek::ktk::json::serialize(
		this->m_serialized_state_of_deleted_component
	);

	status = status &&
		writer.write_string(
			state_string.data(), state_string.size()
		);

	return status && writer.is_valid();
}

bool zircon_command_delete_component_from_entity::
	Deserialize_Delta(zircon_command_delta_reader& reader) noexcept
{
	bool status{};

	const kotek::uint32_t entity_id = reader.read_u32(&status);

	if (status == false)
		return false;

	this->m_id.id = entity_id;

	char component_name_buffer
		[zircon_DEF_MAX_COMPONENT_NAME_SIZE];

	status = reader.read_string(
		component_name_buffer, sizeof(component_name_buffer)
	);

	if (status == false)
		return false;

	this->m_component_name = component_name_buffer;

	char state_buffer[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON];

	status = reader.read_string(state_buffer, sizeof(state_buffer));

	if (status == false)
		return false;

	kotek::ktk::json::error_code parse_error;

	this->m_serialized_state_of_deleted_component =
		kotek::ktk::json::parse(
			kotek::cstring_view_t(
				state_buffer, strlen(state_buffer)
			),
			parse_error
		);

	if (parse_error)
	{
		KOTEK_MESSAGE_ERROR(
			"failed to parse a serialized component state: {}",
			parse_error.message()
		);
		return false;
	}

	return reader.is_valid();
}

eZirconComponentType
zircon_command_delete_component_from_entity::get_component_type()
{
	eZirconComponentType result = eZirconComponentType::kunknown;

	if (this->m_p_factory &&
	    this->m_component_name.empty() == false)
	{
		result = this->m_p_factory->get_component_enum_by_name(
			this->m_component_name.c_str()
		);
	}

	return result;
}
