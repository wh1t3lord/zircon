#include "zircon_command_csg_edit_primitive.h"

#include "../../ecs/zircon_factory.h"
#include "../../ecs/zircon_component_csg.h"
#include "../../ecs/zircon_component_csg_primitive.h"
#include "zircon_command_csg_utils.h"

zircon_command_csg_edit_primitive::zircon_command_csg_edit_primitive(
	zircon_session_editor_manager* p_manager_session_editor,
	zircon_factory* p_factory,
	kotek::entity_t compound_id,
	kotek::entity_t primitive_id,
	const kotek::ktk::json::value& state_after
) :
	m_compound_id{compound_id},
	m_primitive_id{primitive_id},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_factory{p_factory},
	m_state_before{},
	m_state_after{state_after}
{
	KOTEK_ASSERT(p_manager_session_editor,
		"must pass a valid editor session manager");

	KOTEK_ASSERT(p_factory, "factory must be valid!");
}

zircon_command_csg_edit_primitive::zircon_command_csg_edit_primitive(
	zircon_session_editor_manager* p_manager_session_editor,
	zircon_factory* p_factory
) :
	m_compound_id{kotek::ktk::kInvalidECSEntity},
	m_primitive_id{kotek::ktk::kInvalidECSEntity},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_factory{p_factory},
	m_state_before{},
	m_state_after{}
{
	KOTEK_ASSERT(p_manager_session_editor,
		"must pass a valid editor session manager");

	KOTEK_ASSERT(p_factory, "factory must be valid!");
}

zircon_command_csg_edit_primitive::~zircon_command_csg_edit_primitive()
{
}

void zircon_command_csg_edit_primitive::Execute(void)
{
	if (!this->m_p_manager_session_editor || !this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] edit primitive: invalid manager/factory");
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
			"[csg] edit primitive: entity {} is not live, "
			"nothing to edit",
			static_cast<kotek::uint32_t>(this->m_primitive_id.id));
		return;
	}

	zircon_component_interface* p_component =
		p_factory->get_component_by_enum(
			p_context,
			this->m_primitive_id,
			eZirconComponentType::kzircon_component_csg_primitive);

	if (p_component == nullptr)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] edit primitive: entity {} has no csg primitive "
			"component",
			static_cast<kotek::uint32_t>(this->m_primitive_id.id));
		return;
	}

	// the previous state is captured eagerly so the journal entry
	// is complete even if this object gets evicted from the live
	// pool later (the edit_component_state contract)
	this->m_state_before =
		zircon_serialize_component(p_component);

	p_factory->DeserializeComponent(
		p_component, this->m_state_after);

	zircon_component_csg* p_compound =
		zircon_command_csg_resolve_compound(p_session, p_factory,
			p_context, this->m_compound_id);

	if (p_compound)
	{
		p_compound->mark_dirty();
	}

	KOTEK_MESSAGE(
		"[csg] edited primitive entity {}",
		static_cast<kotek::uint32_t>(this->m_primitive_id.id));
}

void zircon_command_csg_edit_primitive::Undo(void)
{
	if (!this->m_p_manager_session_editor || !this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] edit primitive undo: invalid manager/factory");
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
			"[csg] edit primitive undo: entity {} is not live, "
			"nothing to restore",
			static_cast<kotek::uint32_t>(this->m_primitive_id.id));
		return;
	}

	zircon_component_interface* p_component =
		p_factory->get_component_by_enum(
			p_context,
			this->m_primitive_id,
			eZirconComponentType::kzircon_component_csg_primitive);

	if (p_component == nullptr)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] edit primitive undo: entity {} has no csg "
			"primitive component",
			static_cast<kotek::uint32_t>(this->m_primitive_id.id));
		return;
	}

	p_factory->DeserializeComponent(
		p_component, this->m_state_before);

	zircon_component_csg* p_compound =
		zircon_command_csg_resolve_compound(p_session, p_factory,
			p_context, this->m_compound_id);

	if (p_compound)
	{
		p_compound->mark_dirty();
	}

	KOTEK_MESSAGE(
		"[csg][undo] restored primitive entity {}",
		static_cast<kotek::uint32_t>(this->m_primitive_id.id));
}

const char* zircon_command_csg_edit_primitive::GetName(void)
{
	return "csg edit primitive";
}

kotek::entity_t
zircon_command_csg_edit_primitive::GetEntityID(void) const noexcept
{
	return this->m_primitive_id;
}

void zircon_command_csg_edit_primitive::SetEntityID(
	kotek::entity_t id
) noexcept
{
	this->m_primitive_id = id;
}

kotek::enum_base_t
zircon_command_csg_edit_primitive::GetCommandType(void) noexcept
{
	return static_cast<kotek::enum_base_t>(
		zircon_DEF_COMMAND_TYPE_CSG_EDIT_PRIMITIVE);
}

kotek::size_t zircon_command_csg_edit_primitive::Serialize(
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

bool zircon_command_csg_edit_primitive::Serialize_Delta(
	zircon_command_delta_writer& writer
) noexcept
{
	bool status = writer.write_u32(
		static_cast<kotek::uint32_t>(this->m_compound_id.id));

	status = status &&
		writer.write_u32(
			static_cast<kotek::uint32_t>(this->m_primitive_id.id));

	auto before_string =
		kotek::ktk::json::serialize(this->m_state_before);

	auto after_string =
		kotek::ktk::json::serialize(this->m_state_after);

	status = status &&
		writer.write_string(
			before_string.data(), before_string.size());

	status = status &&
		writer.write_string(
			after_string.data(), after_string.size());

	return status && writer.is_valid();
}

bool zircon_command_csg_edit_primitive::Deserialize_Delta(
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

	{
		kotek::ktk::json::error_code parse_error;

		this->m_state_before = kotek::ktk::json::parse(
			kotek::cstring_view_t(
				state_buffer, strlen(state_buffer)),
			parse_error);

		if (parse_error)
		{
			KOTEK_MESSAGE_ERROR(
				"failed to parse a serialized csg primitive "
				"state: {}",
				parse_error.message());
			return false;
		}
	}

	status = reader.read_string(state_buffer, sizeof(state_buffer));

	if (status == false)
		return false;

	{
		kotek::ktk::json::error_code parse_error;

		this->m_state_after = kotek::ktk::json::parse(
			kotek::cstring_view_t(
				state_buffer, strlen(state_buffer)),
			parse_error);

		if (parse_error)
		{
			KOTEK_MESSAGE_ERROR(
				"failed to parse a serialized csg primitive "
				"state: {}",
				parse_error.message());
			return false;
		}
	}

	return reader.is_valid();
}
