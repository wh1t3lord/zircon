#include "zircon_command_add_component_to_entity.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "../../world/zircon_world.h"
#include "../../ecs/zircon_factory.h"

zircon_command_add_component_to_entity::
	zircon_command_add_component_to_entity(
		zircon_session_editor_manager* p_manager_session_editor,
		kotek::entity_t id,
		const char* component_string
	) :
	m_is_serialized{}, m_id{id},
	m_component_name{component_string},
	m_p_manager_session_editor{p_manager_session_editor},
	m_serialized_state_of_deleted_component{}
{
	KOTEK_ASSERT(
		this->m_p_manager_session_editor,
		"you must pass a valid pointer editor session manager!"
	);
	KOTEK_ASSERT(
		component_string,
		"you can't pass an invalid component name!"
	);
	KOTEK_ASSERT(
		strlen(component_string),
		"you can't pass an empty string!"
	);
}

zircon_command_add_component_to_entity::
	zircon_command_add_component_to_entity(
		zircon_session_editor_manager* p_manager_session_editor
	) :
	m_is_serialized{}, m_id{kotek::ktk::kInvalidECSEntity},
	m_component_name{},
	m_p_manager_session_editor{p_manager_session_editor},
	m_serialized_state_of_deleted_component{}
{
	KOTEK_ASSERT(
		this->m_p_manager_session_editor,
		"you must pass a valid pointer of editor session "
		"manager!"
	);
}

zircon_command_add_component_to_entity::
	~zircon_command_add_component_to_entity()
{
}

void zircon_command_add_component_to_entity::Execute(void)
{
	KOTEK_ASSERT(
		this->m_component_name.empty() == false,
		"you must initialize this field from constructor"
	);
	KOTEK_ASSERT(
		this->m_p_manager_session_editor,
		"should be initialzed editor session manager here"
	);

	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid editor session "
			"manager pointer!"
		);
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

	zircon_factory* p_factory = p_world->get_factory();

	KOTEK_ASSERT(p_factory, "world must have valid factory!");

	if (p_factory)
	{
		if (p_factory->is_valid_entity(
				p_world->get_ecs_context(), this->m_id
			))
		{
			p_factory->create_component(
				p_world->get_ecs_context(),
				this->m_id,
				p_factory->get_component_enum_by_name(
					this->m_component_name.c_str()
				)
			);

			auto* p_raw_data_of_component =
				p_factory->get_component_by_name(
					p_world->get_ecs_context(),
					this->m_id,
					this->m_component_name.c_str()
				);

			if (this->m_is_serialized)
			{
				p_factory->DeserializeComponent(
					p_raw_data_of_component,
					this->m_serialized_state_of_deleted_component
				);
			}

			KOTEK_ASSERT(
				p_raw_data_of_component,
				"failed to create component to entity: {}",
				static_cast<kotek::uint32_t>(this->m_id.id)
			);

			if (p_raw_data_of_component)
			{
				// capture the state eagerly so the journal entry
				// is complete even if this command object gets
				// evicted from the live pool later
				this->m_serialized_state_of_deleted_component =
					zircon_serialize_component(
						p_raw_data_of_component
					);

				this->m_is_serialized = true;
			}

			KOTEK_MESSAGE(
				"[history][{}] [{}] for entity[{}]",
				this->GetName(),
				this->m_component_name.c_str(),
				static_cast<kotek::uint32_t>(this->m_id.id)
			);
		}
	}
}

void zircon_command_add_component_to_entity::Undo(void)
{
	KOTEK_ASSERT(
		this->m_component_name.empty() == false,
		"you must initialize this field from constructor"
	);
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

	zircon_factory* p_factory = p_world->get_factory();

	KOTEK_ASSERT(p_factory, "world must have valid factory!");

	if (p_factory)
	{
		if (p_factory->is_valid_entity(
				p_world->get_ecs_context(), this->m_id
			))
		{
			auto* p_raw_data_of_component =
				p_factory->get_component_by_name(
					p_world->get_ecs_context(),
					this->m_id,
					this->m_component_name.c_str()
				);

			if (p_raw_data_of_component)
			{
				this->m_serialized_state_of_deleted_component =
					zircon_serialize_component(
						p_raw_data_of_component
					);

				this->m_is_serialized = true;
			}

			p_factory->remove_component(
				p_world->get_ecs_context(),
				this->m_id,
				p_factory->get_component_enum_by_name(
					this->m_component_name.c_str()
				)
			);

			KOTEK_MESSAGE(
				"[history] removed component[{}] from "
				"entity[{}]",
				this->m_component_name.c_str(),
				static_cast<kotek::uint32_t>(this->m_id.id)
			);
		}
	}
}

const char* zircon_command_add_component_to_entity::GetName()
{
	return "add component to entity";
}

kotek::entity_t
zircon_command_add_component_to_entity::GetEntityID(void
) const noexcept
{
	return static_cast<kotek::entity_t>(this->m_id);
}

void zircon_command_add_component_to_entity::SetEntityID(
	kotek::entity_t id
) noexcept
{
	this->m_id = id;
}

kotek::enum_base_t
zircon_command_add_component_to_entity::GetCommandType() noexcept
{
	return static_cast<kotek::enum_base_t>(
		kotek::core::eConsoleCommandIndex::
			kConsoleCommand_SDK_CreateComponentForEntityByName
	);
}

kotek::size_t zircon_command_add_component_to_entity::Serialize(
	kotek::core::ktkFileHandleType file
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

bool zircon_command_add_component_to_entity::Serialize_Delta(
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

	if (this->m_is_serialized)
	{
		auto state_string = kotek::ktk::json::serialize(
			this->m_serialized_state_of_deleted_component
		);

		status = status && writer.write_u32(1) &&
			writer.write_string(
				state_string.data(), state_string.size()
			);
	}
	else
	{
		status = status && writer.write_u32(0) &&
			writer.write_string(nullptr, 0);
	}

	return status && writer.is_valid();
}

bool zircon_command_add_component_to_entity::Deserialize_Delta(
	zircon_command_delta_reader& reader
) noexcept
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

	const kotek::uint32_t has_state = reader.read_u32(&status);

	if (status == false)
		return false;

	char state_buffer[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON];

	status = reader.read_string(state_buffer, sizeof(state_buffer));

	if (status == false)
		return false;

	if (has_state)
	{
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
				"failed to parse a serialized component state: "
				"{}",
				parse_error.message()
			);
			return false;
		}

		this->m_is_serialized = true;
	}
	else
	{
		this->m_is_serialized = false;
	}

	return reader.is_valid();
}

void zircon_command_add_component_to_entity::serialize_state()
{
	if (this->m_is_serialized == false)
	{
		KOTEK_ASSERT(
			this->m_p_manager_session_editor,
			"should be initialzed game manager here"
		);

		if (!this->m_p_manager_session_editor)
		{
			KOTEK_MESSAGE_WARNING(
				"failed to execute due to invalid game manager "
				"pointer!"
			);
			return;
		}

		zircon_session_editor* p_session =
			this->m_p_manager_session_editor->get_session(
				this->m_p_manager_session_editor
					->get_current_session_id()
			);

		if (!p_session)
		{
			KOTEK_MESSAGE_WARNING(
				"failed to execute due to invalid session "
				"editor#{}",
				this->m_p_manager_session_editor
					->get_current_session_id()
			);
			return;
		}

		zircon_world* p_world = p_session->get_world();

		if (!p_world)
		{
			KOTEK_MESSAGE_WARNING(
				"failed to execute due to invalid world in "
				"session editor_{}#{}",
				p_session->get_session_name(),
				p_session->get_id()
			);
			return;
		}

		zircon_factory* p_factory = p_world->get_factory();

		KOTEK_ASSERT(
			p_factory, "world must have valid factory!"
		);

		if (p_factory)
		{
			if (p_factory->is_valid_entity(
					p_world->get_ecs_context(), this->m_id
				))
			{
				auto* p_raw_data_of_component =
					p_factory->get_component_by_name(
						p_world->get_ecs_context(),
						this->m_id,
						this->m_component_name.c_str()
					);

				if (p_raw_data_of_component)
				{
					this->m_serialized_state_of_deleted_component =
						zircon_serialize_component(
							p_raw_data_of_component
						);

					this->m_is_serialized = true;
				}
			}
		}
	}
}

bool zircon_command_add_component_to_entity::
	is_state_serialized() const noexcept
{
	return this->m_is_serialized;
}

eZirconComponentType
zircon_command_add_component_to_entity::get_component_type()
{
	KOTEK_ASSERT(
		this->m_p_manager_session_editor,
		"should be initialzed editor session manager here"
	);

	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING("failed to execute due to "
		                      "invalid game manager pointer!");
		return eZirconComponentType::kunknown;
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor
				->get_current_session_id()
		);

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session "
			"editor#{}",
			this->m_p_manager_session_editor->get_current_session_id(
			)
		);
		return eZirconComponentType::kunknown;
	}

	zircon_world* p_world = p_session->get_world();

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world in session "
			"editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return eZirconComponentType::kunknown;
	}

	zircon_factory* p_factory = p_world->get_factory();

	KOTEK_ASSERT(p_factory, "world must have valid factory!");

	eZirconComponentType result = eZirconComponentType::kunknown;

	if (p_factory)
	{
		if (this->m_component_name.empty() == false)
		{
			result = p_factory->get_component_enum_by_name(
				this->m_component_name.c_str()
			);
		}
	}

	return result;
}
