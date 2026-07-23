#include "zircon_command_edit_component_state.h"
#include "../../ecs/zircon_factory.h"
#include "../../world/zircon_world.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"

zircon_command_edit_component_state::
	zircon_command_edit_component_state(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory,
		kotek::entity_t id,
		const char* p_component_name,
		const kotek::ktk::json::value& state_after
	) :
	m_id{id},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_factory{p_factory},
	m_component_name{p_component_name},
	m_state_before{},
	m_state_after{state_after}
{
	KOTEK_ASSERT(
		p_manager_session_editor,
		"you must pass a valid pointer editor session manager!"
	);

	KOTEK_ASSERT(p_factory, "passed invalid factory!");

	KOTEK_ASSERT(
		p_component_name,
		"you can't pass an invalid component name!"
	);

	KOTEK_ASSERT(
		strlen(p_component_name),
		"you can't pass an empty component name!"
	);
}

zircon_command_edit_component_state::
	zircon_command_edit_component_state(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	) :
	m_id{kotek::ktk::kInvalidECSEntity},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_factory{p_factory},
	m_component_name{},
	m_state_before{},
	m_state_after{}
{
	KOTEK_ASSERT(
		p_manager_session_editor,
		"you must pass a valid pointer of editor session manager!"
	);

	KOTEK_ASSERT(p_factory, "passed invalid factory!");
}

zircon_command_edit_component_state::
	~zircon_command_edit_component_state()
{
}

void zircon_command_edit_component_state::Execute(void)
{
	KOTEK_ASSERT(
		this->m_component_name.empty() == false,
		"you must initialize this field from constructor"
	);

	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid editor session "
			"manager pointer!"
		);
		return;
	}

	if (!this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING("failed to execute due to invalid factory");
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
		"failed to obtain world from session_{}#{}",
		p_session->get_session_name(),
		p_session->get_id()
	);

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world "
			"in session editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	if (p_world->get_ecs_context() == nullptr)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid ecs context in "
			"editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	if (this->m_p_factory->is_valid_entity(
			p_world->get_ecs_context(), this->m_id
		) == false)
	{
		KOTEK_MESSAGE_WARNING(
			"[history] entity {} is invalid, nothing to edit",
			static_cast<kotek::uint32_t>(this->m_id.id)
		);
		return;
	}

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
			"[{}], nothing to edit",
			static_cast<kotek::uint32_t>(this->m_id.id),
			this->m_component_name.c_str()
		);
		return;
	}

	// the previous state is captured eagerly so the journal entry
	// is complete even if this object gets evicted from the live
	// pool later
	this->m_state_before =
		zircon_serialize_component(p_component);

	this->m_p_factory->DeserializeComponent(
		p_component, this->m_state_after
	);

	KOTEK_MESSAGE(
		"[history] edited component by name: {} in entity {}",
		this->m_component_name.c_str(),
		static_cast<kotek::uint32_t>(this->m_id.id)
	);
}

void zircon_command_edit_component_state::Undo(void)
{
	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING("failed to execute command due "
		                      "to invalid game manager!");
		return;
	}

	if (!this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING("failed to execute due to invalid factory");
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
		"failed to obtain world from session_{}#{}",
		p_session->get_session_name(),
		p_session->get_id()
	);

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world "
			"in session editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	if (this->m_p_factory->is_valid_entity(
			p_world->get_ecs_context(), this->m_id
		) == false)
	{
		KOTEK_MESSAGE_WARNING(
			"[history][undo] entity {} is invalid, nothing to "
			"restore",
			static_cast<kotek::uint32_t>(this->m_id.id)
		);
		return;
	}

	zircon_component_interface* p_component =
		this->m_p_factory->get_component_by_name(
			p_world->get_ecs_context(),
			this->m_id,
			this->m_component_name.c_str()
		);

	if (p_component)
	{
		this->m_p_factory->DeserializeComponent(
			p_component, this->m_state_before
		);

		KOTEK_MESSAGE(
			"[history][undo] restored component state by "
			"name: {} for entity {}",
			this->m_component_name.c_str(),
			static_cast<kotek::uint32_t>(this->m_id.id)
		);
	}
}

const char* zircon_command_edit_component_state::GetName()
{
	return "edit component state";
}

kotek::entity_t
zircon_command_edit_component_state::GetEntityID(void) const noexcept
{
	return this->m_id;
}

void zircon_command_edit_component_state::SetEntityID(
	kotek::entity_t id
) noexcept
{
	this->m_id = id;
}

kotek::enum_base_t
zircon_command_edit_component_state::GetCommandType() noexcept
{
	return static_cast<kotek::enum_base_t>(
		zircon_DEF_COMMAND_TYPE_EDIT_COMPONENT_STATE
	);
}

kotek::size_t zircon_command_edit_component_state::Serialize(
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

bool zircon_command_edit_component_state::Serialize_Delta(
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

	auto before_string =
		kotek::ktk::json::serialize(this->m_state_before);

	auto after_string =
		kotek::ktk::json::serialize(this->m_state_after);

	status = status &&
		writer.write_string(
			before_string.data(), before_string.size()
		);

	status = status &&
		writer.write_string(
			after_string.data(), after_string.size()
		);

	return status && writer.is_valid();
}

bool zircon_command_edit_component_state::Deserialize_Delta(
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

	char state_buffer
		[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON];

	status = reader.read_string(
		state_buffer, sizeof(state_buffer)
	);

	if (status == false)
		return false;

	{
		kotek::ktk::json::error_code parse_error;

		this->m_state_before = kotek::ktk::json::parse(
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
	}

	status = reader.read_string(
		state_buffer, sizeof(state_buffer)
	);

	if (status == false)
		return false;

	{
		kotek::ktk::json::error_code parse_error;

		this->m_state_after = kotek::ktk::json::parse(
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
	}

	return reader.is_valid();
}
