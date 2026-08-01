#include "zircon_gizmo_edit_commit.h"

#include "../../ecs/zircon_factory.h"
#include "../../ecs/zircon_component_transform.h"
#include "zircon_command_history.h"
#include "zircon_command_edit_component_state.h"

bool zircon_gizmo_commit_transform_drag_edit(
	zircon_session_editor_manager* p_manager_session_editor,
	zircon_factory* p_factory, zircon_editor_command_history* p_history,
	zircon_ecs_context_t* p_context, kotek::entity_t entity,
	const float* p_start_position, const float* p_start_scale,
	const float* p_start_rotation_quat) noexcept
{
	KOTEK_ASSERT(p_manager_session_editor, "must be valid");
	KOTEK_ASSERT(p_factory, "must be valid");
	KOTEK_ASSERT(p_history, "must be valid");
	KOTEK_ASSERT(p_context, "must be valid");
	KOTEK_ASSERT(p_start_position, "must be valid");
	KOTEK_ASSERT(p_start_scale, "must be valid");
	KOTEK_ASSERT(p_start_rotation_quat, "must be valid");

	if (p_manager_session_editor == nullptr || p_factory == nullptr ||
		p_history == nullptr || p_context == nullptr ||
		p_start_position == nullptr || p_start_scale == nullptr ||
		p_start_rotation_quat == nullptr)
	{
		return false;
	}

	zircon_component_transform* p_transform =
		static_cast<zircon_component_transform*>(
			p_factory->get_component_by_enum(p_context, entity,
				eZirconComponentType::kzircon_component_transform));

	if (p_transform == nullptr)
		return false;

	// the component currently holds the previewed final state — the same
	// dance the Z6 stress suite does: serialize the after state, restore
	// the start state, then let the command's Execute capture the true
	// before and apply the after
	kotek::ktk::json::value state_after =
		zircon_serialize_component(p_transform);

	p_transform->set_position(kotek::math::vec3f_t(p_start_position[0],
		p_start_position[1], p_start_position[2]));
	p_transform->set_scale(kotek::math::vec3f_t(p_start_scale[0],
		p_start_scale[1], p_start_scale[2]));
	p_transform->set_rotation(
		kotek::math::quatf_t(p_start_rotation_quat[0],
			p_start_rotation_quat[1], p_start_rotation_quat[2],
			p_start_rotation_quat[3]));

	unsigned char* p_memory =
		p_history->allocate_memory_for_command(
			sizeof(zircon_command_edit_component_state),
			"zircon_command_edit_component_state");

	if (p_memory == nullptr)
		return false;

	zircon_command_edit_component_state* p_command =
		new (p_memory) zircon_command_edit_component_state(
			p_manager_session_editor, p_factory, entity,
			zircon_translate_component_type_enum_to_string(
				eZirconComponentType::kzircon_component_transform),
			state_after);

	p_history->ExecuteCommand(p_command);

	return true;
}
