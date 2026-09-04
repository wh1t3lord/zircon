#include "zircon_editor_sdk_camera.h"

#include "../../ecs/zircon_factory.h"
#include "../../core/zircon_defs.h"

void zircon_sdk_camera_drive_euler(
	float& inout_yaw_degrees,
	float& inout_pitch_degrees,
	float delta_yaw_degrees,
	float delta_pitch_degrees,
	kotek::ktk::math::vec3f_t& out_front
) noexcept
{
	inout_yaw_degrees += delta_yaw_degrees;
	inout_pitch_degrees += delta_pitch_degrees;

	if (inout_pitch_degrees > ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES)
		inout_pitch_degrees = ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES;

	if (inout_pitch_degrees < -ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES)
		inout_pitch_degrees = -ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES;

	out_front.x() =
		cos(kotek::ktk::math::convert_to_radians(inout_yaw_degrees)) *
		cos(kotek::ktk::math::convert_to_radians(inout_pitch_degrees));
	out_front.y() =
		sin(kotek::ktk::math::convert_to_radians(inout_pitch_degrees));
	out_front.z() =
		sin(kotek::ktk::math::convert_to_radians(inout_yaw_degrees)) *
		cos(kotek::ktk::math::convert_to_radians(inout_pitch_degrees));
}

void zircon_sdk_camera_drive_quaternion(
	kotek::ktk::math::quatf_t& inout_rotation,
	float delta_yaw_degrees,
	float delta_pitch_degrees,
	kotek::ktk::math::vec3f_t& out_front
) noexcept
{
	const kotek::ktk::math::vec3f_t forward(0.0f, 0.0f, -1.0f);

	if (delta_yaw_degrees != 0.0f || delta_pitch_degrees != 0.0f)
	{
		// the euler path clamps the absolute pitch; here the pitch is
		// measured from the rotated forward (front.y == sin(pitch) by
		// construction) and the delta is shrunk so the result stays
		// inside the same window — the camera approaches the pole but
		// never crosses it, and moving the mouse back works immediately
		// (no stuck-at-the-limit like a naive discard-the-delta clamp)
		kotek::ktk::math::vec3f_t front_current =
			kotek::ktk::math::get_math_rotate(inout_rotation, forward);

		float front_y = front_current.y();

		if (front_y > 1.0f)
			front_y = 1.0f;

		if (front_y < -1.0f)
			front_y = -1.0f;

		float pitch_current_degrees =
			kotek::ktk::math::convert_to_degrees(
				kotek::ktk::math::get_math_asin(front_y)
			);

		float pitch_target_degrees =
			pitch_current_degrees + delta_pitch_degrees;

		if (pitch_target_degrees > ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES)
			pitch_target_degrees = ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES;

		if (pitch_target_degrees < -ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES)
			pitch_target_degrees = -ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES;

		float applied_pitch_degrees =
			pitch_target_degrees - pitch_current_degrees;

		// yaw about world +Y; the negative angle matches the euler
		// path's forward (its yaw accumulates into sin/cos with the
		// opposite handedness to a right-handed rotation about +Y), so
		// both representations produce the same front for the same
		// delta sequence
		kotek::ktk::math::quatf_t yaw_delta =
			kotek::ktk::math::get_math_angle_axis(
				kotek::ktk::math::convert_to_radians(-delta_yaw_degrees),
				kotek::ktk::math::vec3f_t(0.0f, 1.0f, 0.0f)
			);

		kotek::ktk::math::vec3f_t right_axis =
			kotek::ktk::math::get_math_rotate(
				inout_rotation, kotek::ktk::math::vec3f_t(1.0f, 0.0f, 0.0f)
			);

		kotek::ktk::math::quatf_t pitch_delta =
			kotek::ktk::math::get_math_angle_axis(
				kotek::ktk::math::convert_to_radians(applied_pitch_degrees),
				right_axis
			);

		// premultiply (the deltas apply in the current orientation's
		// frame) and renormalize — hundreds of small drags accumulate
		// float error otherwise
		inout_rotation = kotek::ktk::math::get_math_normalize(
			yaw_delta * pitch_delta * inout_rotation
		);
	}

	out_front = kotek::ktk::math::get_math_rotate(inout_rotation, forward);
}

kotek::entity_t zircon_editor_ensure_sdk_bootstrap_entity(
	zircon_factory* p_factory,
	zircon_ecs_context_t* p_context,
	kotek::uint32_t entity_count_max_limit
) noexcept
{
	KOTEK_ASSERT(p_factory, "must be valid");
	KOTEK_ASSERT(p_context, "must be valid");

	if (!p_factory || !p_context)
	{
		KOTEK_MESSAGE_WARNING(
			"editor sdk bootstrap: no factory or ecs context — the "
			"editor camera stays uncreated"
		);
		return kotek::ktk::kInvalidECSEntity;
	}

	// pico has no view<>, so scan for an sdk_camera owner (the same
	// scan the camera driver does per frame) — an existing camera
	// (a scene that loaded its own) makes the bootstrap a no-op
	kotek::entity_t entities[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};
	kotek::uint32_t entities_count = p_factory->get_all_entities(
		p_context,
		entity_count_max_limit,
		entities,
		ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT
	);

	for (kotek::uint32_t entity_index = 0;
	     entity_index < entities_count; ++entity_index)
	{
		if (p_factory->has_component(
				p_context,
				entities[entity_index],
				eZirconComponentType::kzircon_component_sdk_camera
			))
		{
			return entities[entity_index];
		}
	}

	kotek::entity_t id = p_factory->create_entity(p_context);

	KOTEK_ASSERT(
		ecs_is_invalid_entity(id) == false,
		"editor sdk bootstrap: failed to create the entity"
	);

	if (ecs_is_invalid_entity(id))
	{
		return kotek::ktk::kInvalidECSEntity;
	}

	// default-constructed on purpose: the camera ctor carries the old
	// driver's defaults (yaw=-90, pitch=0) and the transform ctor is the
	// identity; sdk_input gets the engine input manager wired by the
	// factory's create_component
	bool was_camera = p_factory->create_component(
		p_context, id, eZirconComponentType::kzircon_component_sdk_camera
	);
	bool was_input = p_factory->create_component(
		p_context, id, eZirconComponentType::kzircon_component_sdk_input
	);
	bool was_transform = p_factory->create_component(
		p_context, id, eZirconComponentType::kzircon_component_transform
	);

	KOTEK_ASSERT(
		was_camera && was_input && was_transform,
		"editor sdk bootstrap: failed to create the components"
	);

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE(
		"editor sdk bootstrap: created camera entity id={}",
		static_cast<kotek::uint64_t>(id.id)
	);
#endif

	return id;
}
