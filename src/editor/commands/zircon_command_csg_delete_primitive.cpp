#include "zircon_command_csg_delete_primitive.h"

#include "../../ecs/zircon_factory.h"
#include "../../ecs/zircon_component_csg.h"
#include "../../ecs/zircon_component_csg_primitive.h"
#include "zircon_command_csg_utils.h"

zircon_command_csg_delete_primitive::
	zircon_command_csg_delete_primitive(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory,
		kotek::entity_t compound_id,
		kotek::entity_t primitive_id
	) :
	m_compound_id{compound_id},
	m_primitive_id{primitive_id},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_factory{p_factory},
	m_state_buffer{}
{
	KOTEK_ASSERT(p_manager_session_editor,
		"must pass a valid editor session manager");

	KOTEK_ASSERT(p_factory, "factory must be valid!");
}

zircon_command_csg_delete_primitive::
	zircon_command_csg_delete_primitive(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	) :
	m_compound_id{kotek::ktk::kInvalidECSEntity},
	m_primitive_id{kotek::ktk::kInvalidECSEntity},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_factory{p_factory},
	m_state_buffer{}
{
	KOTEK_ASSERT(p_manager_session_editor,
		"must pass a valid editor session manager");

	KOTEK_ASSERT(p_factory, "factory must be valid!");
}

zircon_command_csg_delete_primitive::
	~zircon_command_csg_delete_primitive()
{
}

void zircon_command_csg_delete_primitive::Execute(void)
{
	if (!this->m_p_manager_session_editor || !this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] delete primitive: invalid manager/factory");
		return;
	}

	zircon_session_editor* p_session = nullptr;
	zircon_world* p_world = nullptr;
	zircon_factory* p_factory = nullptr;

	if (zircon_command_csg_resolve_world(
			this->m_p_manager_session_editor, &p_session, &p_world,
			&p_factory) == false)
	{
		return;
	}

	zircon_ecs_context_t* p_context = p_world->get_ecs_context();

	if (p_factory->is_valid_entity(p_context, this->m_primitive_id) ==
		false)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] delete primitive: entity {} is not live, "
			"nothing to delete",
			static_cast<kotek::uint32_t>(this->m_primitive_id.id));
		return;
	}

	// the undo delta must be captured before the entity dies
	this->capture_primitive_state(p_context);

	zircon_component_csg* p_compound =
		zircon_command_csg_resolve_compound(p_session, p_factory,
			p_context, this->m_compound_id);

	if (p_compound)
	{
		p_compound->remove_primitive_entity(this->m_primitive_id);
		p_compound->mark_dirty();
	}

	zircon_editor_ui_state* p_ui_state = p_session->get_ui_state();

	if (p_ui_state &&
		p_ui_state->get_selected_entity().id ==
			this->m_primitive_id.id)
	{
		p_ui_state->set_selected_entity(kotek::ktk::kInvalidECSEntity);
	}

	p_factory->destroy_entity(p_context, this->m_primitive_id);

	KOTEK_MESSAGE(
		"[csg] deleted primitive entity {}",
		static_cast<kotek::uint32_t>(this->m_primitive_id.id));
}

void zircon_command_csg_delete_primitive::Undo(void)
{
	if (!this->m_p_manager_session_editor || !this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] delete primitive undo: invalid manager/factory");
		return;
	}

	zircon_session_editor* p_session = nullptr;
	zircon_world* p_world = nullptr;
	zircon_factory* p_factory = nullptr;

	if (zircon_command_csg_resolve_world(
			this->m_p_manager_session_editor, &p_session, &p_world,
			&p_factory) == false)
	{
		return;
	}

	zircon_ecs_context_t* p_context = p_world->get_ecs_context();

	// the recreate pattern of zircon_command_delete_entity::Undo: a
	// fresh id (reported to the history through the GetEntityID
	// observation), then the captured state is reinstalled
	this->m_primitive_id = p_factory->create_entity(p_context);

	kotek::ktk::json::error_code parse_error;

	kotek::ktk::json::value primitive_state =
		kotek::ktk::json::parse(
			kotek::cstring_view_t(this->m_state_buffer.c_str(),
				this->m_state_buffer.size()),
			parse_error);

	if (parse_error)
	{
		KOTEK_MESSAGE_ERROR(
			"[csg][undo] failed to parse the captured primitive "
			"state: {}",
			parse_error.message());
		return;
	}

	if (p_factory->create_component(p_context, this->m_primitive_id,
			eZirconComponentType::kzircon_component_csg_primitive,
			primitive_state) == false)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg][undo] failed to reinstall the primitive on "
			"entity {}",
			static_cast<kotek::uint32_t>(this->m_primitive_id.id));
		return;
	}

	zircon_component_csg* p_compound =
		zircon_command_csg_resolve_compound(p_session, p_factory,
			p_context, this->m_compound_id);

	if (p_compound)
	{
		p_compound->add_primitive_entity(this->m_primitive_id);
		p_compound->mark_dirty();
	}

	KOTEK_MESSAGE(
		"[csg][undo] restored primitive entity {}",
		static_cast<kotek::uint32_t>(this->m_primitive_id.id));
}

const char* zircon_command_csg_delete_primitive::GetName(void)
{
	return "csg delete primitive";
}

kotek::entity_t
zircon_command_csg_delete_primitive::GetEntityID(void) const noexcept
{
	return this->m_primitive_id;
}

void zircon_command_csg_delete_primitive::SetEntityID(
	kotek::entity_t id
) noexcept
{
	this->m_primitive_id = id;
}

kotek::enum_base_t
zircon_command_csg_delete_primitive::GetCommandType(void) noexcept
{
	return static_cast<kotek::enum_base_t>(
		zircon_DEF_COMMAND_TYPE_CSG_DELETE_PRIMITIVE);
}

kotek::size_t zircon_command_csg_delete_primitive::Serialize(
	kotek::core::ktkFileHandleType file
) noexcept
{
	KOTEK_MESSAGE_WARNING(
		"[history][{}]: Serialize(file) is deprecated, the "
		"journal uses Serialize_Delta instead",
		this->GetName());

	(void)file;

	return 0;
}

bool zircon_command_csg_delete_primitive::Serialize_Delta(
	zircon_command_delta_writer& writer
) noexcept
{
	bool status = writer.write_u32(
		static_cast<kotek::uint32_t>(this->m_compound_id.id));

	status = status &&
		writer.write_u32(
			static_cast<kotek::uint32_t>(this->m_primitive_id.id));

	status = status &&
		writer.write_string(
			this->m_state_buffer.c_str(),
			this->m_state_buffer.size());

	return status && writer.is_valid();
}

bool zircon_command_csg_delete_primitive::Deserialize_Delta(
	zircon_command_delta_reader& reader
) noexcept
{
	bool status{};

	const kotek::uint32_t compound_id = reader.read_u32(&status);

	if (status == false)
		return false;

	this->m_compound_id.id = compound_id;

	const kotek::uint32_t primitive_id = reader.read_u32(&status);

	if (status == false)
		return false;

	this->m_primitive_id.id = primitive_id;

	char state_buffer[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON];

	status = reader.read_string(state_buffer, sizeof(state_buffer));

	if (status == false)
		return false;

	this->m_state_buffer = state_buffer;

	return reader.is_valid();
}

void zircon_command_csg_delete_primitive::capture_primitive_state(
	zircon_ecs_context_t* p_context
) noexcept
{
	KOTEK_ASSERT(p_context, "must be a valid ecs context");
	KOTEK_ASSERT(this->m_p_factory, "must be a valid factory");

	if (p_context == nullptr || this->m_p_factory == nullptr)
		return;

	zircon_component_interface* p_component =
		this->m_p_factory->get_component_by_enum(
			p_context,
			this->m_primitive_id,
			eZirconComponentType::kzircon_component_csg_primitive);

	if (p_component == nullptr)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] entity {} has no csg primitive component, "
			"the undo will have nothing to restore",
			static_cast<kotek::uint32_t>(this->m_primitive_id.id));
		return;
	}

	kotek::ktk::json::value serialized_state =
		zircon_serialize_component(p_component);

	auto serialized_string =
		kotek::ktk::json::serialize(serialized_state);

	if (serialized_string.size() >= this->m_state_buffer.capacity())
	{
		KOTEK_MESSAGE_ERROR(
			"[csg] the primitive state ({} bytes) exceeds the "
			"command buffer ({} bytes) — undo will be partial",
			serialized_string.size(),
			this->m_state_buffer.capacity());
		return;
	}

	this->m_state_buffer.clear();
	this->m_state_buffer.append(
		serialized_string.data(), serialized_string.size());
}
