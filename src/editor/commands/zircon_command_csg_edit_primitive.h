#pragma once

// zircon_command_csg_edit_primitive — the journaled CSG primitive
// field edit (task Z25 A2): dimensions/flags/transform before+after,
// the zircon_command_edit_component_state delta shape specialized to
// the csg primitive + the owning compound (which is marked dirty on
// execute AND undo/redo — the debounced-rebuild trigger; the typed
// edit-component command does not know the compound, so the CSG
// family carries it inside its own delta).
//
// delta = [compound entity id][primitive entity id][state before
// json][state after json]

#include "zircon_command_registry.h"

#include "../../ecs/zircon_csg_defs.h"

class zircon_session_editor_manager;
class zircon_factory;
struct zircon_ecs_context_t;

class zircon_command_csg_edit_primitive
	: public kotek::core::ktkISDKRedoUndo,
	  public zircon_interface_command_delta
{
public:
	zircon_command_csg_edit_primitive(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory,
		kotek::entity_t compound_id,
		kotek::entity_t primitive_id,
		const kotek::ktk::json::value& state_after
	);

	// for command history (journal reconstruction)
	zircon_command_csg_edit_primitive(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	);

	~zircon_command_csg_edit_primitive();

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
	kotek::entity_t m_primitive_id;
	zircon_session_editor_manager* m_p_manager_session_editor;
	zircon_factory* m_p_factory;
	kotek::ktk::json::value m_state_before;
	kotek::ktk::json::value m_state_after;
};
