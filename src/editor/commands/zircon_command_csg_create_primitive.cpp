#include "zircon_command_csg_create_primitive.h"

#include "../../ecs/zircon_factory.h"
#include "../../ecs/zircon_component_csg.h"
#include "../../ecs/zircon_component_csg_primitive.h"
#include "zircon_command_csg_utils.h"

zircon_command_csg_create_primitive::
	zircon_command_csg_create_primitive(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory,
		kotek::entity_t compound_id,
		eZirconCsgPrimitiveType primitive_type,
		const double* p_dimensions_xyz,
		const double* p_position_xyz,
		const double* p_rotation_xyzw,
		kotek::uint8_t operation_flags,
		kotek::uint16_t material_id
	) :
	m_compound_id{compound_id},
	m_created_entity{kotek::ktk::kInvalidECSEntity},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_factory{p_factory},
	m_primitive_state{}
{
	KOTEK_ASSERT(p_manager_session_editor,
		"must pass a valid editor session manager");

	KOTEK_ASSERT(p_factory, "factory must be valid!");

	KOTEK_ASSERT(p_dimensions_xyz && p_position_xyz &&
			p_rotation_xyzw,
		"the primitive parameter arrays must be valid");

	// build the serialized component state the execute path
	// installs (the factory's json-create does the whole
	// installation; the journal roundtrips the same state)
	zircon_component_csg_primitive primitive{};

	primitive.set_primitive_type(primitive_type);
	primitive.set_operation_flags(operation_flags);
	primitive.set_material_id(material_id);

	using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_t>;

	for (kotek::uint8_t axis = 0; axis < 3; ++axis)
	{
		primitive.set_dimension(axis,
			traits_t::from_double(p_dimensions_xyz[axis]));
		primitive.set_position_axis(axis,
			traits_t::from_double(p_position_xyz[axis]));
	}

	for (kotek::uint8_t component = 0; component < 4; ++component)
	{
		primitive.set_rotation_component(component,
			traits_t::from_double(p_rotation_xyzw[component]));
	}

	this->m_primitive_state =
		zircon_serialize_component(&primitive);
}

zircon_command_csg_create_primitive::
	zircon_command_csg_create_primitive(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	) :
	m_compound_id{kotek::ktk::kInvalidECSEntity},
	m_created_entity{kotek::ktk::kInvalidECSEntity},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_factory{p_factory},
	m_primitive_state{}
{
	KOTEK_ASSERT(p_manager_session_editor,
		"must pass a valid editor session manager");

	KOTEK_ASSERT(p_factory, "factory must be valid!");
}

zircon_command_csg_create_primitive::
	~zircon_command_csg_create_primitive()
{
}

void zircon_command_csg_create_primitive::Execute(void)
{
	if (!this->m_p_manager_session_editor || !this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] create primitive: invalid manager/factory");
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

	// NOTE: the entity id reincarnation on re-execution is reported
	// to the history by zircon_editor_command_history::execute_node
	// itself (it observes GetEntityID before/after) — the command
	// must not call update_dependent_commands on its own (a
	// journal-reconstructed command only knows its recorded id)
	this->m_created_entity = p_factory->create_entity(p_context);

	if (p_factory->create_component(p_context, this->m_created_entity,
			eZirconComponentType::kzircon_component_csg_primitive,
			this->m_primitive_state) == false)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] create primitive: failed to install the "
			"component on entity {}",
			static_cast<kotek::uint32_t>(this->m_created_entity.id));
		return;
	}

	zircon_component_csg* p_compound =
		zircon_command_csg_resolve_compound(p_session, p_factory,
			p_context, this->m_compound_id);

	if (p_compound)
	{
		p_compound->add_primitive_entity(this->m_created_entity);
		// the dirty flag is the rebuild trigger: set on execute
		// AND undo/redo (the rebuild keys on dirtiness, not the
		// command kind)
		p_compound->mark_dirty();
	}

	KOTEK_MESSAGE(
		"[csg] created primitive entity {} in compound {}",
		static_cast<kotek::uint32_t>(this->m_created_entity.id),
		static_cast<kotek::uint32_t>(this->m_compound_id.id));
}

void zircon_command_csg_create_primitive::Undo(void)
{
	if (!this->m_p_manager_session_editor || !this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] create primitive undo: invalid manager/factory");
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

	zircon_component_csg* p_compound =
		zircon_command_csg_resolve_compound(p_session, p_factory,
			p_context, this->m_compound_id);

	if (p_compound)
	{
		p_compound->remove_primitive_entity(this->m_created_entity);
		p_compound->mark_dirty();
	}

	// the selection must not outlive the entity (the same rule as
	// zircon_command_create_entity::Undo)
	zircon_editor_ui_state* p_ui_state = p_session->get_ui_state();

	if (p_ui_state &&
		p_ui_state->get_selected_entity().id ==
			this->m_created_entity.id)
	{
		p_ui_state->set_selected_entity(kotek::ktk::kInvalidECSEntity);
	}

	p_factory->destroy_entity(p_context, this->m_created_entity);

	KOTEK_MESSAGE(
		"[csg][undo] removed primitive entity {}",
		static_cast<kotek::uint32_t>(this->m_created_entity.id));
}

const char* zircon_command_csg_create_primitive::GetName(void)
{
	return "csg create primitive";
}

kotek::entity_t
zircon_command_csg_create_primitive::GetEntityID(void) const noexcept
{
	return this->m_created_entity;
}

void zircon_command_csg_create_primitive::SetEntityID(
	kotek::entity_t id
) noexcept
{
	this->m_created_entity = id;
}

kotek::enum_base_t
zircon_command_csg_create_primitive::GetCommandType(void) noexcept
{
	return static_cast<kotek::enum_base_t>(
		zircon_DEF_COMMAND_TYPE_CSG_CREATE_PRIMITIVE);
}

kotek::size_t zircon_command_csg_create_primitive::Serialize(
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

bool zircon_command_csg_create_primitive::Serialize_Delta(
	zircon_command_delta_writer& writer
) noexcept
{
	bool status = writer.write_u32(
		static_cast<kotek::uint32_t>(this->m_compound_id.id));

	status = status &&
		writer.write_u32(
			static_cast<kotek::uint32_t>(this->m_created_entity.id));

	auto primitive_string =
		kotek::ktk::json::serialize(this->m_primitive_state);

	status = status &&
		writer.write_string(
			primitive_string.data(), primitive_string.size());

	return status && writer.is_valid();
}

bool zircon_command_csg_create_primitive::Deserialize_Delta(
	zircon_command_delta_reader& reader
) noexcept
{
	bool status{};

	const kotek::uint32_t compound_id = reader.read_u32(&status);

	if (status == false)
		return false;

	this->m_compound_id.id = compound_id;

	const kotek::uint32_t entity_id = reader.read_u32(&status);

	if (status == false)
		return false;

	this->m_created_entity.id = entity_id;

	char state_buffer[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON];

	status = reader.read_string(state_buffer, sizeof(state_buffer));

	if (status == false)
		return false;

	{
		kotek::ktk::json::error_code parse_error;

		this->m_primitive_state = kotek::ktk::json::parse(
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
