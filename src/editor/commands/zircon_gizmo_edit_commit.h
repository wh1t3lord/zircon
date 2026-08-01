#pragma once

class zircon_session_editor_manager;
class zircon_factory;
class zircon_editor_command_history;
struct zircon_ecs_context_t;

/// @brief \~english the gizmo drag-END commit shared by both editor gizmo
/// implementations (task Z3 P2e's own gizmo pass and P2f's ImGuizmo
/// variant): during a drag the live preview mutates the transform
/// component directly; on mouse release the component therefore holds the
/// FINAL state. This helper serializes that state as the command's
/// state_after, restores the captured drag-start state so the command's
/// Execute captures the true before, then placement-news ONE
/// zircon_command_edit_component_state into the history pool and
/// ExecuteCommands it — the exact dance the Z6 stress suite drives. The
/// start arrays are the drag-context capture: position/scale xyz and the
/// rotation quaternion (x,y,z,w). Returns false when any link of the
/// chain is missing (the caller's drag teardown has already restored the
/// start state in that case)
bool zircon_gizmo_commit_transform_drag_edit(
	zircon_session_editor_manager* p_manager_session_editor,
	zircon_factory* p_factory,
	zircon_editor_command_history* p_history,
	zircon_ecs_context_t* p_context, kotek::entity_t entity,
	const float* p_start_position, const float* p_start_scale,
	const float* p_start_rotation_quat) noexcept;
