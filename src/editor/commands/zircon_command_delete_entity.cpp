#include "zircon_command_delete_entity.h"
#include "../../ecs/zircon_factory.h"
#include "../../world/zircon_world.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "../commands/zircon_command_history.h"

zircon_command_delete_entity::zircon_command_delete_entity(
	zircon_session_editor_manager* p_manager_session_editor,
	zircon_factory* p_factory,
	kotek::entity_t entity_to_delete
) :
	m_entity_created{entity_to_delete},
	m_entity_previous_id{kotek::ktk::kInvalidECSEntity},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_factory{p_factory}, m_components{}, m_states_index{},
	m_states_used{}, m_states_buffer{}
{
	KOTEK_ASSERT(
		p_manager_session_editor, "passed invalid game manager!"
	);

	KOTEK_ASSERT(p_factory, "passed invalid factory!");
}

zircon_command_delete_entity::~zircon_command_delete_entity() {}

void zircon_command_delete_entity::Execute(void)
{
	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING("failed to execute command due "
		                      "to invalid game manager!");
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
			"failed to execute command due to invalid session "
			"editor#{}",
			this->m_p_manager_session_editor->get_current_session_id(
			)
		);
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(
		p_world,
		"failed to obtain world from session_{}#{}",
		p_session->get_session_name(),
		p_session->get_id()
	);

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute command due to invalid world "
			"in session editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	KOTEK_ASSERT(
		p_world->get_ecs_context(),
		"world must contain ecs context!"
	);

	if (p_world->get_ecs_context() == nullptr)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to "
			"invalid ecs context in world [{}][{}]!",
			p_world->get_id(),
			p_world->get_name()
		);

		return;
	}

	KOTEK_ASSERT(
		this->m_p_factory, "factory must be initialized"
	);

	if (!this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid factory that you "
			"passed for command construction!"
		);
		return;
	}

	if (p_world)
	{
		zircon_ecs_context_t* p_ecs_context =
			p_world->get_ecs_context();

		// the delta (component types + states) must be captured
		// before the entity is destroyed
		this->capture_component_states(p_ecs_context);

		this->m_p_factory->destroy_entity(
			p_ecs_context, this->m_entity_created
		);

		if (this->m_entity_created.id !=
		    kotek::ktk::kInvalidECSEntity.id)
		{
			this->m_entity_previous_id = this->m_entity_created;

			KOTEK_MESSAGE(
				"[history] removed entity: {}",
				this->m_entity_created
			);
		}
	}
}

void zircon_command_delete_entity::Undo(void)
{
	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING("failed to execute command due "
		                      "to invalid game manager!");
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
			"failed to execute command due to invalid session "
			"editor#{}",
			this->m_p_manager_session_editor->get_current_session_id(
			)
		);
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(
		p_world,
		"failed to obtain world from session_{}#{}",
		p_session->get_session_name(),
		p_session->get_id()
	);

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute command due to invalid world "
			"in session editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	KOTEK_ASSERT(
		p_world->get_factory(), "world must contain factory!"
	);

	if (p_world)
	{
		zircon_factory* p_factory = p_world->get_factory();

		// NOTE: the entity id reincarnation is reported to the
		// history by zircon_editor_command_history::Undo itself
		// (it observes GetEntityID before/after), the command must
		// not call update_dependent_commands on its own: a
		// journal-reconstructed command only knows its recorded id
		// and would write a chain link that skips incarnations
		this->m_entity_created = p_factory->create_entity(
			p_world->get_ecs_context()
		);

		// restore the components with the states that were
		// captured by Execute (self-contained delta, no history
		// lookups)
		for (const zircon_delete_entity_component_state_entry&
		         entry : this->m_states_index)
		{
			const eZirconComponentType component_type =
				static_cast<eZirconComponentType>(
					entry.m_component_type
				);

			p_factory->create_component(
				p_world->get_ecs_context(),
				this->m_entity_created,
				component_type
			);

			zircon_component_interface* p_component =
				p_factory->get_component_by_enum(
					p_world->get_ecs_context(),
					this->m_entity_created,
					component_type
				);

			if (p_component == nullptr)
			{
				KOTEK_MESSAGE_WARNING(
					"[history][undo] failed to recreate "
					"component {} for entity {}",
					entry.m_component_type,
					static_cast<kotek::uint32_t>(
						this->m_entity_created.id
					)
				);
				continue;
			}

			kotek::ktk::json::error_code parse_error;
			kotek::ktk::json::value component_state =
				kotek::ktk::json::parse(
					kotek::cstring_view_t(
						reinterpret_cast<const char*>(
							this->m_states_buffer +
							entry.m_state_offset
						),
						entry.m_state_size
					),
					parse_error
				);

			if (parse_error)
			{
				KOTEK_MESSAGE_ERROR(
					"[history][undo] failed to parse a "
					"component state: {}",
					parse_error.message()
				);
				continue;
			}

			p_factory->DeserializeComponent(
				p_component, component_state
			);
		}

		KOTEK_MESSAGE(
			"[history][undo] created entity: {}",
			static_cast<kotek::uint32_t>(this->m_entity_created.id)
		);
	}
}

const char* zircon_command_delete_entity::GetName()
{
	return "delete entity";
}

kotek::entity_t zircon_command_delete_entity::GetEntityID(void
) const noexcept
{
	return this->m_entity_created;
}

void zircon_command_delete_entity::SetEntityID(
	kotek::entity_t id
) noexcept
{
	this->m_entity_created = id;
}

kotek::enum_base_t
zircon_command_delete_entity::GetCommandType() noexcept
{
	return static_cast<kotek::ktk::enum_base_t>(
		kotek::core::eConsoleCommandIndex::
			kConsoleCommand_SDK_DeleteEntity
	);
}

kotek::size_t zircon_command_delete_entity::Serialize(
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

bool zircon_command_delete_entity::Serialize_Delta(
	zircon_command_delta_writer& writer
) noexcept
{
	bool status = writer.write_u32(
		static_cast<kotek::uint32_t>(this->m_entity_created.id)
	);

	status = status &&
		writer.write_u32(
			static_cast<kotek::uint32_t>(
				this->m_states_index.size()
			)
		);

	for (const zircon_delete_entity_component_state_entry&
	         entry : this->m_states_index)
	{
		status = status &&
			writer.write_u32(entry.m_component_type) &&
			writer.write_string(
				reinterpret_cast<const char*>(
					this->m_states_buffer + entry.m_state_offset
				),
				entry.m_state_size
			);
	}

	return status && writer.is_valid();
}

bool zircon_command_delete_entity::Deserialize_Delta(
	zircon_command_delta_reader& reader
) noexcept
{
	bool status{};

	const kotek::uint32_t entity_id = reader.read_u32(&status);

	if (status == false)
		return false;

	this->m_entity_created.id = entity_id;
	this->m_entity_previous_id = this->m_entity_created;

	const kotek::uint32_t component_count =
		reader.read_u32(&status);

	if (status == false)
		return false;

	this->m_components.clear();
	this->m_states_index.clear();
	this->m_states_used = 0;

	for (kotek::uint32_t i = 0; i < component_count; ++i)
	{
		const kotek::uint32_t component_type =
			reader.read_u32(&status);

		if (status == false)
			return false;

		char state_buffer
			[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON];

		status = reader.read_string(
			state_buffer, sizeof(state_buffer)
		);

		if (status == false)
			return false;

		const kotek::size_t state_size =
			strlen(state_buffer);

		if (this->m_states_used + state_size + 1 >
		        sizeof(this->m_states_buffer) ||
		    this->m_states_index.full())
		{
			KOTEK_MESSAGE_ERROR(
				"delete entity states buffer overflow during "
				"deserialization"
			);
			return false;
		}

		zircon_delete_entity_component_state_entry entry{};
		entry.m_component_type =
			static_cast<kotek::uint16_t>(component_type);
		entry.m_state_offset = static_cast<kotek::uint16_t>(
			this->m_states_used
		);
		entry.m_state_size =
			static_cast<kotek::uint16_t>(state_size);

		kotek::ktk::memory::memcpy(
			this->m_states_buffer + this->m_states_used,
			state_buffer,
			state_size + 1
		);

		this->m_states_used +=
			static_cast<kotek::uint32_t>(state_size + 1);

		this->m_states_index.push_back(entry);
		this->m_components.push_back(
			static_cast<eZirconComponentType>(component_type)
		);
	}

	return reader.is_valid();
}

void zircon_command_delete_entity::capture_component_states(
	zircon_ecs_context_t* p_ecs_context
) noexcept
{
	this->m_components.clear();
	this->m_states_index.clear();
	this->m_states_used = 0;

	KOTEK_ASSERT(p_ecs_context, "must be a valid ecs context");
	KOTEK_ASSERT(this->m_p_factory, "must be a valid factory");

	if (p_ecs_context == nullptr || this->m_p_factory == nullptr)
		return;

	// NOTE: zircon_factory::get_all_components_of_entity has no
	// pico implementation, so enumeration is done by probing every
	// registered component type
	for (int i = 0;
	     i < static_cast<int>(eZirconComponentType::kunknown);
	     ++i)
	{
		const eZirconComponentType component_type =
			static_cast<eZirconComponentType>(i);

		if (this->m_p_factory->has_component(
				p_ecs_context,
				this->m_entity_created,
				component_type
			) == false)
		{
			continue;
		}

		zircon_component_interface* p_component =
			this->m_p_factory->get_component_by_enum(
				p_ecs_context,
				this->m_entity_created,
				component_type
			);

		if (p_component == nullptr)
			continue;

		this->m_components.push_back(component_type);

		kotek::ktk::json::value serialized_state =
			zircon_serialize_component(p_component);

		auto serialized_string =
			kotek::ktk::json::serialize(serialized_state);

		if (this->m_states_used + serialized_string.size() + 1 >
		        sizeof(this->m_states_buffer) ||
		    this->m_states_index.full())
		{
			KOTEK_ASSERT(
				false,
				"delete entity states buffer is too small for "
				"entity {}, increase "
				"zircon_DEF_COMMAND_DELETE_ENTITY_STATE_BUFFER_"
				"SIZE",
				static_cast<kotek::uint32_t>(
					this->m_entity_created.id
				)
			);

			KOTEK_MESSAGE_ERROR(
				"[history]: not all component states of entity "
				"{} were captured, its undo will be partial",
				static_cast<kotek::uint32_t>(
					this->m_entity_created.id
				)
			);

			return;
		}

		zircon_delete_entity_component_state_entry entry{};
		entry.m_component_type =
			static_cast<kotek::uint16_t>(component_type);
		entry.m_state_offset = static_cast<kotek::uint16_t>(
			this->m_states_used
		);
		entry.m_state_size = static_cast<kotek::uint16_t>(
			serialized_string.size()
		);

		kotek::ktk::memory::memcpy(
			this->m_states_buffer + this->m_states_used,
			serialized_string.data(),
			serialized_string.size()
		);

		this->m_states_buffer
			[this->m_states_used + serialized_string.size()] = 0;

		this->m_states_used += static_cast<kotek::uint32_t>(
			serialized_string.size() + 1
		);

		this->m_states_index.push_back(entry);
	}
}
