#pragma once

// zircon_command_csg_delete_primitive — the journaled "remove a CSG
// primitive" command (task Z25 A2): captures the primitive's
// serialized component state (the self-contained undo delta),
// unregisters it from the compound's member list, destroys the
// entity and marks the compound dirty. Undo recreates the entity,
// reinstalls the state and re-registers it (the fresh entity id is
// reported to the history through the GetEntityID observation, the
// zircon_command_delete_entity pattern).
//
// delta = [compound entity id][primitive entity id][the captured
// primitive component json]

#include "zircon_command_registry.h"

#include "../../ecs/zircon_csg_defs.h"

class zircon_session_editor_manager;
class zircon_factory;
struct zircon_ecs_context_t;

class zircon_command_csg_delete_primitive
	: public kotek::core::ktkISDKRedoUndo,
	  public zircon_interface_command_delta
{
public:
	zircon_command_csg_delete_primitive(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory,
		kotek::entity_t compound_id,
		kotek::entity_t primitive_id
	);

	// for command history (journal reconstruction)
	zircon_command_csg_delete_primitive(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	);

	~zircon_command_csg_delete_primitive();

	void Execute(void) override;
	void Undo(void) override;
	const char* GetName(void) override;

	kotek::entity_t GetEntityID(void) const noexcept override;
	void SetEntityID(kotek::entity_t id) noexcept override;

	kotek::enum_base_t GetCommandType(void) noexcept override;

	/// @brief \~english deprecated file serialization from the old
	/// streaming design, the journal uses Serialize_Delta instead
	kotek::size_t Serialize(kotek::core::ktkFileHandleType file
	) noexcept override;

	bool Serialize_Delta(
		zircon_command_delta_writer& writer
	) noexcept override;
	bool Deserialize_Delta(
		zircon_command_delta_reader& reader
	) noexcept override;

private:
	// the captured primitive state (the undo delta), serialized once
	// at Execute time like zircon_command_delete_entity captures its
	// component states
	void capture_primitive_state(
		zircon_ecs_context_t* p_context
	) noexcept;

private:
	kotek::entity_t m_compound_id;
	kotek::entity_t m_primitive_id;
	zircon_session_editor_manager* m_p_manager_session_editor;
	zircon_factory* m_p_factory;
	kotek::static_cstring_t<zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON>
		m_state_buffer;
};
