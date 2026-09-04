#pragma once

#include <kotek.core.math/include/kotek_core_math.h>
#include <kotek.core.ecs/include/kotek_core_ecs.h>

class zircon_factory;
struct zircon_ecs_context_t;

// the editor camera chain helpers (task Z20): the bootstrap entity the
// per-frame driver (zircon_session_editor::update_component_camera_sdk)
// needs and the drive math of both rotation representations — free
// functions so the unit tests drive the exact code the session runs
// (headless: no imgui, no window, no real input device)

/// @brief \~english the pitch window of BOTH representations (the old
/// euler driver hardcoded the 89.0 literals — the named constant keeps
/// the euler and quaternion paths clamped identically)
#define ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES 89.0f

/// @brief \~english the euler representation (yaw/pitch accumulation,
/// the pre-Z20 driver math untouched): deltas are in degrees (mouse
/// delta * sensitivity at the call site), pitch is clamped to
/// +/-ZIRCON_DEF_SDK_CAMERA_MAX_PITCH_DEGREES and out_front receives
/// the resulting forward vector
void zircon_sdk_camera_drive_euler(
	float& inout_yaw_degrees,
	float& inout_pitch_degrees,
	float delta_yaw_degrees,
	float delta_pitch_degrees,
	kotek::ktk::math::vec3f_t& out_front
) noexcept;

/// @brief \~english the quaternion representation
/// (kSDK_Feature_SDKCamera_Rotation_Quaternion): the deltas accumulate
/// into the rotation as small axis-angle quaternions — yaw about world
/// +Y, pitch about the camera's current local right — premultiplied and
/// normalized; the pitch delta is shrunk so the resulting pitch stays
/// inside the same +/-89 window as the euler path (front.y == sin(pitch)
/// by construction), so the pole is never crossed and no flip/NaN
/// appears. identity rotation == the euler defaults (yaw=-90, pitch=0,
/// forward -Z)
void zircon_sdk_camera_drive_quaternion(
	kotek::ktk::math::quatf_t& inout_rotation,
	float delta_yaw_degrees,
	float delta_pitch_degrees,
	kotek::ktk::math::vec3f_t& out_front
) noexcept;

/// @brief \~english idempotently ensures the ONE editor bootstrap
/// entity (sdk_camera + sdk_input + transform, defaults) the camera
/// driver needs: when NO entity carries sdk_camera a fresh one is
/// created through direct factory calls (NOT journaled — bootstrap is
/// not undoable history; the entity is then deletable through the
/// normal journaled delete), otherwise the existing owner is returned
/// untouched (a scene that loaded its own camera never gets a second
/// one — the driver asserts on more than one sdk_camera). Returns
/// kotek::ktk::kInvalidECSEntity on a null factory/context.
kotek::entity_t zircon_editor_ensure_sdk_bootstrap_entity(
	zircon_factory* p_factory,
	zircon_ecs_context_t* p_context,
	kotek::uint32_t entity_count_max_limit
) noexcept;
