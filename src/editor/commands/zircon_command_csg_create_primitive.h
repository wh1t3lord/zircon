#pragma once

// zircon_command_csg_create_primitive — the journaled "add a CSG
// primitive" command (task Z25 A2): creates a primitive entity, gives
// it a zircon_component_csg_primitive (the creation payload is the
// component's serialized state — the factory's json-create call does
// the whole installation, the same state the journal roundtrips),
// registers it in the target compound's member list and marks the
// compound dirty (the debounced-rebuild trigger — dirty on execute
// AND undo/redo, the rebuild keys on dirtiness, not the command
// kind). Undo removes the member-list entry and destroys the entity
// (the history's reincarnation observation handles the fresh id on
// re-execution, exactly like zircon_command_create_entity).
//
// delta = [compound entity id][created entity id][the primitive
// component's serialized json]

#include "zircon_command_registry.h"

#include "../../ecs/zircon_csg_defs.h"

class zircon_session_editor_manager;
class zircon_factory;
struct zircon_ecs_context_t;

class zircon_command_csg_create_primitive
	: public kotek::core::ktkISDKRedoUndo,
	  public zircon_interface_command_delta
{
public:
	// the UI-facing ctor: takes the creation parameters, builds the
	// serialized component state the execute path installs
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
	);

	// for command history (journal reconstruction)
	zircon_command_csg_create_primitive(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	);

	~zircon_command_csg_create_primitive();

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
	kotek::entity_t m_compound_id;
	kotek::entity_t m_created_entity;
	zircon_session_editor_manager* m_p_manager_session_editor;
	zircon_factory* m_p_factory;
	kotek::ktk::json::value m_primitive_state;
};
