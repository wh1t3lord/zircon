#include "zircon_command_create_entity.h"
#include "../../ecs/zircon_factory.h"
#include "../../world/zircon_world.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "zircon_command_history.h"

zircon_command_create_entity::zircon_command_create_entity(
	zircon_session_editor_manager* p_session_manager_editor,
	zircon_factory* p_factory
) :
	m_created_entity{},
	m_entity_previous_id{kotek::ktk::kInvalidECSEntity},
	m_p_editor_session_manager{p_session_manager_editor},
	m_p_factory{p_factory}
{
	KOTEK_ASSERT(
		p_session_manager_editor,
		"must be valid pointer of editor session manager"
	);

	KOTEK_ASSERT(p_factory, "factory must be valid!");
}

zircon_command_create_entity::~zircon_command_create_entity() {}

void zircon_command_create_entity::Execute()
{
	if (!this->m_p_editor_session_manager)
	{
		KOTEK_MESSAGE_ERROR("failed to excute due to invalid "
		                    "editor session manager!");
		return;
	}

	if (!this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid factory"
		);
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_editor_session_manager->get_session(
			this->m_p_editor_session_manager
				->get_current_session_id()
		);

	KOTEK_ASSERT(
		p_session,
		"failed to obtain session editor by id: {}",
		this->m_p_editor_session_manager
			->get_current_session_id()
	);

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session "
			"editor#{}",
			this->m_p_editor_session_manager
				->get_current_session_id()
		);
		return;
	}

	zircon_world* p_world = p_session->get_world();

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world of session "
			"editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	if (p_world->get_ecs_context() == nullptr)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid ecs context that "
			"created for world!"
		);
		return;
	}

	if (p_world)
	{
		// NOTE: the entity id reincarnation on re-execution is
		// reported to the history by
		// zircon_editor_command_history::execute_node itself (it
		// observes GetEntityID before/after), the command must not
		// call update_dependent_commands on its own: a
		// journal-reconstructed command only knows its recorded id
		// and would write a chain link that skips incarnations
		this->m_created_entity =
			this->m_p_factory->create_entity(
				p_world->get_ecs_context()
			);

		KOTEK_MESSAGE(
			"[history]: created entity: {}",
			this->m_created_entity
		);
	}
}

void zircon_command_create_entity::Undo()
{
	if (!this->m_p_editor_session_manager)
	{
		KOTEK_MESSAGE_ERROR(
			"failed to excute due to invalid game manager!"
		);
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_editor_session_manager->get_session(
			this->m_p_editor_session_manager
				->get_current_session_id()
		);

	KOTEK_ASSERT(
		p_session,
		"failed to obtain session editor by id: {}",
		this->m_p_editor_session_manager
			->get_current_session_id()
	);

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session "
			"editor#{}",
			this->m_p_editor_session_manager
				->get_current_session_id()
		);
		return;
	}

	zircon_world* p_world = p_session->get_world();

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world of session "
			"editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	if (p_world)
	{
		KOTEK_MESSAGE(
			"[history][undo]: removed entity {}",
			this->m_created_entity
		);

		this->m_entity_previous_id = this->m_created_entity;

		// the selection must not outlive the entity: a stale selected id
		// reaches per-frame pico consumers (the gizmo pass, the
		// inspector) through is_valid_entity and now reads as invalid —
		// clearing keeps the UI honest instead of leaning on the guard
		zircon_editor_ui_state* p_ui_state = p_session->get_ui_state();

		if (p_ui_state &&
			p_ui_state->get_selected_entity().id ==
				this->m_created_entity.id)
		{
			p_ui_state->set_selected_entity(
				kotek::ktk::kInvalidECSEntity);
		}

		this->m_p_factory->destroy_entity(
			p_world->get_ecs_context(), this->m_created_entity
		);
	}
}
const char* zircon_command_create_entity::GetName()
{
	return "create entity";
}
kotek::entity_t zircon_command_create_entity::GetEntityID(void
) const noexcept
{
	return this->m_created_entity;
}
void zircon_command_create_entity::SetEntityID(
	kotek::entity_t id
) noexcept
{
	this->m_created_entity = id;
}

kotek::ktk::enum_base_t
zircon_command_create_entity::GetCommandType() noexcept
{
	return static_cast<kotek::ktk::enum_base_t>(
		kotek::Core::eConsoleCommandIndex::
			kConsoleCommand_SDK_CreateEntity
	);
}

kotek::ktk::size_t zircon_command_create_entity::Serialize(
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

bool zircon_command_create_entity::Serialize_Delta(
	zircon_command_delta_writer& writer
) noexcept
{
	return writer.write_u32(
		static_cast<kotek::uint32_t>(this->m_created_entity.id)
	);
}

bool zircon_command_create_entity::Deserialize_Delta(
	zircon_command_delta_reader& reader
) noexcept
{
	bool status{};

	const kotek::uint32_t entity_id = reader.read_u32(&status);

	if (status)
	{
		this->m_created_entity.id = entity_id;
	}

	return status;
}
