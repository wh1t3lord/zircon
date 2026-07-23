#pragma once

#include "zircon_command_definitions.h"
#include "zircon_command_registry.h"
#include "../../ecs/zircon_factory_definitions.h"

class zircon_factory;
class zircon_editor_command_history;
class zircon_world;
class zircon_session_editor_manager;
struct zircon_ecs_context_t;

enum eZirconComponentType;

/// @brief \~english one captured component state of a deleted
/// entity: type + byte range inside the command's states buffer
struct zircon_delete_entity_component_state_entry
{
	kotek::uint16_t m_component_type;
	kotek::uint16_t m_state_offset;
	kotek::uint16_t m_state_size;
	kotek::uint16_t m_reserved;
};

/// @brief \~english deletes an entity; its inverse data (component
/// types + their serialized states before the deletion) is captured
/// by Execute itself, so undo/redo works from the journal without
/// any cross-command lookups. The states buffer is fixed (see
/// zircon_DEF_COMMAND_DELETE_ENTITY_STATE_BUFFER_SIZE), an overflow
/// degrades to a warning instead of an exception (the old design had
/// the same class of limit, see the old 4096 bytes json buffer)
class zircon_command_delete_entity
	: public kotek::core::ktkISDKRedoUndo,
	  public zircon_interface_command_delta
{
public:
	zircon_command_delete_entity(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory,
		kotek::entity_t entity_to_delete
	);

	~zircon_command_delete_entity();

	void Execute(void) override;
	void Undo(void) override;
	const char* GetName() override;

	kotek::entity_t GetEntityID(void) const noexcept override;
	void SetEntityID(kotek::entity_t id) noexcept override;

	kotek::enum_base_t GetCommandType() noexcept override;

	/// @brief \~english deprecated file serialization from the old
	/// streaming design, the journal uses Serialize_Delta instead
	kotek::size_t Serialize(kotek::core::ktkFileHandleType file
	) noexcept override;

	/// @brief \~english delta = entity id + per component
	/// [type, json state]
	bool Serialize_Delta(
		zircon_command_delta_writer& writer
	) noexcept override;
	bool Deserialize_Delta(
		zircon_command_delta_reader& reader
	) noexcept override;

private:
	/// @brief \~english captures component types and their states
	/// into m_components / m_states_buffer; called by Execute before
	/// the entity is destroyed
	void capture_component_states(
		zircon_ecs_context_t* p_ecs_context
	) noexcept;

private:
	kotek::entity_t m_entity_created;
	kotek::entity_t m_entity_previous_id;
	zircon_session_editor_manager* m_p_manager_session_editor;
	zircon_factory* m_p_factory;
	kotek::static_vector_t<
		eZirconComponentType,
		zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>
		m_components;
	kotek::static_vector_t<
		zircon_delete_entity_component_state_entry,
		zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>
		m_states_index;
	kotek::uint32_t m_states_used;
	unsigned char m_states_buffer
		[zircon_DEF_COMMAND_DELETE_ENTITY_STATE_BUFFER_SIZE];
};
