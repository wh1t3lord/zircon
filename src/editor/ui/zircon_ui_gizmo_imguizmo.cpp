#include "zircon_ui_gizmo_imguizmo.h"

#include <cmath>
#include <cstring>

#include "../../core/zircon_config.h"
#include "../../ecs/zircon_factory.h"
#include "../../ecs/zircon_component_transform.h"
#include "../../ecs/zircon_component_sdk_camera.h"
#include "../../world/zircon_world.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "../commands/zircon_command_history.h"
#include "../commands/zircon_gizmo_edit_commit.h"
#include "../../render/bgfx/zircon_renderer.h"

// the default-orbit fallback camera + the inverse-VP helpers are the grid
// pass's public statics (same reuse the own gizmo makes); the editor
// camera scan itself is replicated below per the pass-independence
// pattern
#include "../../render/bgfx/passes/no_streaming/zircon_render_graph_pass_editor_grid.h"

// the vendored ImGuizmo (task Z3 P2f — the sanctioned third-party
// exception, passes-project-local under src/render/bgfx/passes/imguizmo/).
// Its header includes "imgui.h" quoted, resolved through the PRIVATE
// include dir the editor.ui target carries for exactly this consumer
#include "../../render/bgfx/passes/imguizmo/ImGuizmo.h"

#include "zircon_editor_ui_state.h"

zircon_editor_ui_window_gizmo_imguizmo::
	zircon_editor_ui_window_gizmo_imguizmo(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_renderer_bgfx* p_renderer_bgfx) :
	m_is_show_window{true},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_renderer_bgfx{p_renderer_bgfx},
	m_mode{eMode::kTranslate},
	m_is_snap_enabled{false},
	m_drag{},
	m_frame_start_position{0.0f, 0.0f, 0.0f},
	m_frame_start_scale{1.0f, 1.0f, 1.0f},
	m_frame_start_rotation{0.0f, 0.0f, 0.0f, 1.0f},
	m_was_key_w_down{false},
	m_was_key_e_down{false},
	m_was_key_r_down{false},
	m_was_key_t_down{false},
	m_is_announced{false},
	m_is_overlay_published{false}
{
	KOTEK_ASSERT(
		p_manager_session_editor, "valid pointer is expected");
}

zircon_editor_ui_window_gizmo_imguizmo::
	~zircon_editor_ui_window_gizmo_imguizmo(void)
{
}

void zircon_editor_ui_window_gizmo_imguizmo::Initialize(void) {}

void zircon_editor_ui_window_gizmo_imguizmo::Shutdown(void) {}

void zircon_editor_ui_window_gizmo_imguizmo::Draw(
	kotek::core::ktkMainManager* p_main_manager)
{
	KOTEK_ASSERT(this->m_p_manager_session_editor,
		"you have to pass a valid session editor manager pointer!");

	if (this->m_is_show_window == false)
		return;

	if (this->m_p_manager_session_editor == nullptr ||
		p_main_manager == nullptr)
	{
		return;
	}

	// the variant is active only while its pass token is in the editor
	// slot's set AND enabled (the Render Passes window's skip flag flips
	// without a rebuild, so this is re-read every frame)
	bool is_variant_active = false;

	if (this->m_p_renderer_bgfx)
	{
		const kotek::uint8_t render_graph_id =
			this->m_p_renderer_bgfx->get_render_graph_id_for_session_kind(
				false);

		if (render_graph_id <
			this->m_p_renderer_bgfx->get_render_graph_count())
		{
			const zircon_render_graph_simplified_bgfx_info_t& info =
				this->m_p_renderer_bgfx->get_render_graph_info(
					render_graph_id);

			for (kotek::ktk::size_t i = 0; i < info.pass_names.size(); ++i)
			{
				if (info.pass_enabled[i] &&
					info.pass_names[i] ==
						kZirconConfig_RenderPassEditorGizmoImguizmoName)
				{
					is_variant_active = true;
					break;
				}
			}
		}
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor->get_current_session_id());

	if (p_session == nullptr)
		return;

	if (is_variant_active == false)
	{
		// the overlay POD is NOT touched here while another variant owns
		// it — only the state this window itself published gets cleared,
		// once (see m_is_overlay_published in the header)
		if (this->m_is_overlay_published)
		{
			this->publish_overlay_state(p_session, false);
			this->m_is_overlay_published = false;
		}

		this->m_is_announced = false;

		// toggled off mid-drag: no journaled command, but the preview must
		// not stick — restore the captured start state
		if (this->m_drag.m_is_active)
		{
			zircon_component_transform* p_transform = nullptr;

			zircon_world* p_world = p_session->get_world();

			if (p_world && p_world->is_initialized() &&
				p_world->get_factory() && p_world->get_ecs_context() &&
				p_session->get_ui_state() &&
				p_world->get_factory()->is_valid_entity(
					p_world->get_ecs_context(),
					p_session->get_ui_state()->get_selected_entity()))
			{
				p_transform = static_cast<zircon_component_transform*>(
					p_world->get_factory()->get_component_by_enum(
						p_world->get_ecs_context(),
						p_session->get_ui_state()->get_selected_entity(),
						eZirconComponentType::kzircon_component_transform));
			}

			this->cancel_drag(p_transform);
		}

		return;
	}

	if (this->m_is_announced == false)
	{
		KOTEK_MESSAGE(
			"[editor_gizmo_imguizmo] variant is in the editor pass set and "
			"enabled — ImGuizmo::Manipulate is hosted inside the imgui "
			"frame (task Z3 P2f)");

		this->m_is_announced = true;
	}

	kotek::core::ktkIImguiWrapper* p_wrapper_imgui =
		p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui == nullptr)
		return;

	zircon_world* p_world = p_session->get_world();

	zircon_factory* p_factory =
		p_world && p_world->is_initialized() ? p_world->get_factory()
		                                     : nullptr;

	zircon_ecs_context_t* p_ecs_context =
		p_factory ? p_world->get_ecs_context() : nullptr;

	zircon_editor_ui_state* p_ui_state = p_session->get_ui_state();

	zircon_component_transform* p_transform = nullptr;
	kotek::entity_t selected_entity{kotek::ktk::kInvalidECSEntity};

	if (p_factory && p_ecs_context && p_ui_state)
	{
		selected_entity = p_ui_state->get_selected_entity();

		if (p_factory->is_valid_entity(p_ecs_context, selected_entity))
		{
			p_transform = static_cast<zircon_component_transform*>(
				p_factory->get_component_by_enum(p_ecs_context,
					selected_entity,
					eZirconComponentType::kzircon_component_transform));
		}
	}

	if (p_transform == nullptr)
	{
		// the selection vanished mid-drag — restore, don't commit
		if (this->m_drag.m_is_active)
			this->cancel_drag(nullptr);

		this->publish_overlay_state(p_session, false);
		this->m_is_overlay_published = true;
		return;
	}

	// mode + snap keys (only while no text field eats the keyboard and no
	// drag runs — switching modes mid-drag would change the operation
	// under an active manipulation)
	ImGuiIO& io = p_wrapper_imgui->GetIO();

	const bool is_key_w_down = io.KeysDown[ImGuiKey_W];
	const bool is_key_e_down = io.KeysDown[ImGuiKey_E];
	const bool is_key_r_down = io.KeysDown[ImGuiKey_R];
	const bool is_key_t_down = io.KeysDown[ImGuiKey_T];

	if (io.WantTextInput == false)
	{
		if (this->m_drag.m_is_active == false)
		{
			if (is_key_w_down && this->m_was_key_w_down == false)
				this->m_mode = eMode::kTranslate;
			if (is_key_e_down && this->m_was_key_e_down == false)
				this->m_mode = eMode::kRotate;
			if (is_key_r_down && this->m_was_key_r_down == false)
				this->m_mode = eMode::kScale;
		}
		if (is_key_t_down && this->m_was_key_t_down == false)
			this->m_is_snap_enabled = !this->m_is_snap_enabled;
	}

	this->m_was_key_w_down = is_key_w_down;
	this->m_was_key_e_down = is_key_e_down;
	this->m_was_key_r_down = is_key_r_down;
	this->m_was_key_t_down = is_key_t_down;

	// camera: the editor world's sdk_camera, else the grid pass's default
	// orbit (the same fallback every editor 3D view uses). The bx bytes
	// feed Manipulate UNCHANGED — ImGuizmo's matrix storage/application
	// convention matches bx's (see the header)
	float view[16];
	float projection[16];

	if (this->resolve_editor_camera(p_session, view, projection) == false)
	{
		float aspect_ratio = 4.0f / 3.0f;

		if (io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f)
			aspect_ratio = io.DisplaySize.x / io.DisplaySize.y;

		no_streaming::zircon_render_graph_pass_editor_grid_bgfx::
			build_default_orbit_view_projection(view, projection,
				aspect_ratio, bgfx::getCaps()->homogeneousDepth);
	}

	// the drag-start capture source: the component's TRS BEFORE this
	// frame's Manipulate mutates it
	{
		const kotek::math::vec3f_t& position = p_transform->get_position();
		const kotek::math::vec3f_t& scale = p_transform->get_scale();
		const kotek::math::quatf_t& rotation = p_transform->get_rotation();

		this->m_frame_start_position[0] = position.x();
		this->m_frame_start_position[1] = position.y();
		this->m_frame_start_position[2] = position.z();

		this->m_frame_start_scale[0] = scale.x();
		this->m_frame_start_scale[1] = scale.y();
		this->m_frame_start_scale[2] = scale.z();

		this->m_frame_start_rotation[0] = rotation.x();
		this->m_frame_start_rotation[1] = rotation.y();
		this->m_frame_start_rotation[2] = rotation.z();
		this->m_frame_start_rotation[3] = rotation.w();
	}

	// the host window Manipulate draws into: full viewport, invisible,
	// click-through (ImGuizmo reads the raw IO, so NoInputs costs the
	// drag nothing and keeps the panels under the cursor usable)
	p_wrapper_imgui->SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
	p_wrapper_imgui->SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
	p_wrapper_imgui->SetNextWindowBgAlpha(0.0f);

	constexpr ImGuiWindowFlags _kHostFlags = ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;

	if (p_wrapper_imgui->Begin("##gizmo_imguizmo_host", nullptr,
			_kHostFlags) == false)
	{
		p_wrapper_imgui->End();
		return;
	}

	ImGuizmo::BeginFrame();
	ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);

	float matrix[16];

	compose_trs_matrix(this->m_frame_start_position,
		this->m_frame_start_rotation, this->m_frame_start_scale, matrix);

	ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE mode = ImGuizmo::WORLD;
	float snap_step = zircon_DEF_EDITOR_UI_GIZMO_IMGUIZMO_SNAP_TRANSLATE_STEP;

	switch (this->m_mode)
	{
	case eMode::kRotate:
	{
		operation = ImGuizmo::ROTATE;
		snap_step =
			zircon_DEF_EDITOR_UI_GIZMO_IMGUIZMO_SNAP_ROTATE_STEP_DEGREES;
		break;
	}
	case eMode::kScale:
	{
		// SCALE carries the per-axis handles plus the uniform center
		// handle (MT_SCALE_XYZ), matching the own gizmo's scale mode;
		// scaling is a local-space operation by nature
		operation = ImGuizmo::SCALE;
		mode = ImGuizmo::LOCAL;
		snap_step = zircon_DEF_EDITOR_UI_GIZMO_IMGUIZMO_SNAP_SCALE_STEP;
		break;
	}
	default:
	{
		break;
	}
	}

	const float snap[3] = {snap_step, snap_step, snap_step};

	ImGuizmo::Manipulate(view, projection, operation, mode, matrix,
		nullptr, this->m_is_snap_enabled ? snap : nullptr);

	const bool is_using = ImGuizmo::IsUsing();

	if (is_using && this->m_drag.m_is_active == false)
	{
		// the drag's rising edge: the pre-Manipulate capture of THIS frame
		// is the drag-start state
		this->m_drag.m_is_active = true;

		for (int component = 0; component < 3; ++component)
		{
			this->m_drag.m_start_position[component] =
				this->m_frame_start_position[component];
			this->m_drag.m_start_scale[component] =
				this->m_frame_start_scale[component];
		}

		for (int component = 0; component < 4; ++component)
		{
			this->m_drag.m_start_rotation[component] =
				this->m_frame_start_rotation[component];
		}
	}

	if (is_using)
	{
		// the live preview: the manipulated matrix goes straight into the
		// component (the release edge journals ONE command for the whole
		// drag — the history never sees the intermediate frames)
		float new_position[3];
		float new_scale[3];
		float new_rotation[4];

		if (decompose_trs_matrix(matrix, new_position, new_scale,
				new_rotation))
		{
			p_transform->set_position(kotek::math::vec3f_t(new_position[0],
				new_position[1], new_position[2]));
			p_transform->set_scale(kotek::math::vec3f_t(new_scale[0],
				new_scale[1], new_scale[2]));
			p_transform->set_rotation(kotek::math::quatf_t(new_rotation[0],
				new_rotation[1], new_rotation[2], new_rotation[3]));
		}
	}

	if (is_using == false && this->m_drag.m_is_active)
	{
		// the release edge: the component holds the final preview — the
		// shared helper restores the start state and issues ONE journaled
		// zircon_command_edit_component_state (the own gizmo's exact
		// commit)
		zircon_gizmo_commit_transform_drag_edit(
			this->m_p_manager_session_editor, p_factory,
			p_session->get_command_history(), p_ecs_context,
			selected_entity, this->m_drag.m_start_position,
			this->m_drag.m_start_scale, this->m_drag.m_start_rotation);

		this->m_drag.m_is_active = false;
	}

	this->publish_overlay_state(p_session, true);
	this->m_is_overlay_published = true;

	p_wrapper_imgui->End();
}

int zircon_editor_ui_window_gizmo_imguizmo::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_GizmoImguizmo);
}

void zircon_editor_ui_window_gizmo_imguizmo::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_editor_ui_window_gizmo_imguizmo::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_editor_ui_window_gizmo_imguizmo::Is_Shown(void) const
{
	return this->m_is_show_window;
}

void zircon_editor_ui_window_gizmo_imguizmo::compose_trs_matrix(
	const float* p_position, const float* p_rotation_quat,
	const float* p_scale, float* p_out_matrix) noexcept
{
	KOTEK_ASSERT(p_position, "must be valid storage");
	KOTEK_ASSERT(p_rotation_quat, "must be valid storage");
	KOTEK_ASSERT(p_scale, "must be valid storage");
	KOTEK_ASSERT(p_out_matrix, "must be valid storage");

	const float x = p_rotation_quat[0];
	const float y = p_rotation_quat[1];
	const float z = p_rotation_quat[2];
	const float w = p_rotation_quat[3];

	const float x2 = x + x;
	const float y2 = y + y;
	const float z2 = z + z;

	const float xx = x * x2;
	const float xy = x * y2;
	const float xz = x * z2;
	const float yy = y * y2;
	const float yz = y * z2;
	const float zz = z * z2;
	const float wx = w * x2;
	const float wy = w * y2;
	const float wz = w * z2;

	const float scale_x = p_scale[0];
	const float scale_y = p_scale[1];
	const float scale_z = p_scale[2];

	// column-major bx layout: the columns are the rotated basis vectors
	// scaled per axis, the translation sits at [12..14]. The off-diagonal
	// signs follow bx::mtxFromQuaternion (the engine's rotation sense —
	// NOT glm's mat4_cast, which is its transpose)
	p_out_matrix[0] = (1.0f - (yy + zz)) * scale_x;
	p_out_matrix[1] = (xy - wz) * scale_x;
	p_out_matrix[2] = (xz + wy) * scale_x;
	p_out_matrix[3] = 0.0f;

	p_out_matrix[4] = (xy + wz) * scale_y;
	p_out_matrix[5] = (1.0f - (xx + zz)) * scale_y;
	p_out_matrix[6] = (yz - wx) * scale_y;
	p_out_matrix[7] = 0.0f;

	p_out_matrix[8] = (xz - wy) * scale_z;
	p_out_matrix[9] = (yz + wx) * scale_z;
	p_out_matrix[10] = (1.0f - (xx + yy)) * scale_z;
	p_out_matrix[11] = 0.0f;

	p_out_matrix[12] = p_position[0];
	p_out_matrix[13] = p_position[1];
	p_out_matrix[14] = p_position[2];
	p_out_matrix[15] = 1.0f;
}

bool zircon_editor_ui_window_gizmo_imguizmo::decompose_trs_matrix(
	const float* p_matrix, float* p_out_position, float* p_out_scale,
	float* p_out_rotation_quat) noexcept
{
	KOTEK_ASSERT(p_matrix, "must be valid storage");
	KOTEK_ASSERT(p_out_position, "must be valid storage");
	KOTEK_ASSERT(p_out_scale, "must be valid storage");
	KOTEK_ASSERT(p_out_rotation_quat, "must be valid storage");

	p_out_position[0] = p_matrix[12];
	p_out_position[1] = p_matrix[13];
	p_out_position[2] = p_matrix[14];

	float basis[9];

	for (int column = 0; column < 3; ++column)
	{
		const float length = std::sqrt(
			p_matrix[column * 4 + 0] * p_matrix[column * 4 + 0] +
			p_matrix[column * 4 + 1] * p_matrix[column * 4 + 1] +
			p_matrix[column * 4 + 2] * p_matrix[column * 4 + 2]);

		// a degenerate basis vector carries no rotation
		if (length < 1e-8f)
			return false;

		p_out_scale[column] = length;

		basis[column * 3 + 0] = p_matrix[column * 4 + 0] / length;
		basis[column * 3 + 1] = p_matrix[column * 4 + 1] / length;
		basis[column * 3 + 2] = p_matrix[column * 4 + 2] / length;
	}

	// Shepperd's method: pick the numerically safest branch of the
	// rotation-matrix (column-major basis[col*3+row]) -> quaternion map
	const float trace =
		basis[0] + basis[4] + basis[8];

	float quat[4];

	if (trace > 0.0f)
	{
		const float s = std::sqrt(trace + 1.0f) * 2.0f;

		quat[3] = 0.25f * s;
		quat[0] = (basis[5] - basis[7]) / s;
		quat[1] = (basis[6] - basis[2]) / s;
		quat[2] = (basis[1] - basis[3]) / s;
	}
	else if (basis[0] > basis[4] && basis[0] > basis[8])
	{
		const float s =
			std::sqrt(1.0f + basis[0] - basis[4] - basis[8]) * 2.0f;

		quat[3] = (basis[5] - basis[7]) / s;
		quat[0] = 0.25f * s;
		quat[1] = (basis[3] + basis[1]) / s;
		quat[2] = (basis[6] + basis[2]) / s;
	}
	else if (basis[4] > basis[8])
	{
		const float s =
			std::sqrt(1.0f + basis[4] - basis[0] - basis[8]) * 2.0f;

		quat[3] = (basis[6] - basis[2]) / s;
		quat[0] = (basis[3] + basis[1]) / s;
		quat[1] = 0.25f * s;
		quat[2] = (basis[7] + basis[5]) / s;
	}
	else
	{
		const float s =
			std::sqrt(1.0f + basis[8] - basis[0] - basis[4]) * 2.0f;

		quat[3] = (basis[1] - basis[3]) / s;
		quat[0] = (basis[6] + basis[2]) / s;
		quat[1] = (basis[7] + basis[5]) / s;
		quat[2] = 0.25f * s;
	}

	const float quat_length = std::sqrt(quat[0] * quat[0] +
		quat[1] * quat[1] + quat[2] * quat[2] + quat[3] * quat[3]);

	if (quat_length < 1e-8f)
		return false;

	// the extraction above follows the standard (glm-sense) formulas, but
	// the matrix content is bx's rotation sense (compose_trs_matrix
	// matches bx::mtxFromQuaternion) — the extracted quaternion is the
	// conjugate of the component's quaternion, so flip the vector part
	p_out_rotation_quat[0] = -quat[0] / quat_length;
	p_out_rotation_quat[1] = -quat[1] / quat_length;
	p_out_rotation_quat[2] = -quat[2] / quat_length;
	p_out_rotation_quat[3] = quat[3] / quat_length;

	return true;
}

bool zircon_editor_ui_window_gizmo_imguizmo::resolve_editor_camera(
	zircon_session_editor* p_session, float* p_out_view,
	float* p_out_projection) noexcept
{
	KOTEK_ASSERT(p_out_view, "must be valid storage");
	KOTEK_ASSERT(p_out_projection, "must be valid storage");

	if (p_session == nullptr || p_out_view == nullptr ||
		p_out_projection == nullptr)
	{
		return false;
	}

	zircon_world* p_world = p_session->get_world();

	if (p_world == nullptr || p_world->is_initialized() == false)
		return false;

	zircon_factory* p_factory = p_world->get_factory();
	zircon_ecs_context_t* p_context = p_world->get_ecs_context();

	if (p_factory == nullptr || p_context == nullptr)
		return false;

	kotek::entity_t entity_ids
		[zircon_DEF_EDITOR_UI_GIZMO_IMGUIZMO_MAX_ENTITY_SCAN_COUNT];

	kotek::uint32_t entity_count = p_factory->get_all_entities(p_context,
		p_world->get_entity_count_max_limit(), entity_ids,
		zircon_DEF_EDITOR_UI_GIZMO_IMGUIZMO_MAX_ENTITY_SCAN_COUNT);

	for (kotek::uint32_t entity_index = 0; entity_index < entity_count;
		 ++entity_index)
	{
		if (p_factory->has_component(p_context, entity_ids[entity_index],
				eZirconComponentType::kzircon_component_sdk_camera) == false)
		{
			continue;
		}

		zircon_component_sdk_camera* p_sdk_camera =
			static_cast<zircon_component_sdk_camera*>(
				p_factory->get_component_by_enum(p_context,
					entity_ids[entity_index],
					eZirconComponentType::kzircon_component_sdk_camera));

		if (p_sdk_camera == nullptr)
			continue;

		const zircon_component_camera& camera = p_sdk_camera->get_camera();

		const float* p_camera_view =
			kotek::math::value_ptr(camera.get_view());
		const float* p_camera_projection =
			kotek::math::value_ptr(camera.get_projection());

		for (int element_index = 0; element_index < 16; ++element_index)
		{
			p_out_view[element_index] = p_camera_view[element_index];
			p_out_projection[element_index] =
				p_camera_projection[element_index];
		}

		return true;
	}

	return false;
}

void zircon_editor_ui_window_gizmo_imguizmo::cancel_drag(
	zircon_component_transform* p_transform) noexcept
{
	if (p_transform)
	{
		p_transform->set_position(kotek::math::vec3f_t(
			this->m_drag.m_start_position[0],
			this->m_drag.m_start_position[1],
			this->m_drag.m_start_position[2]));
		p_transform->set_scale(kotek::math::vec3f_t(
			this->m_drag.m_start_scale[0], this->m_drag.m_start_scale[1],
			this->m_drag.m_start_scale[2]));
		p_transform->set_rotation(kotek::math::quatf_t(
			this->m_drag.m_start_rotation[0],
			this->m_drag.m_start_rotation[1],
			this->m_drag.m_start_rotation[2],
			this->m_drag.m_start_rotation[3]));
	}

	this->m_drag.m_is_active = false;
}

void zircon_editor_ui_window_gizmo_imguizmo::publish_overlay_state(
	zircon_session_editor* p_session, bool is_gizmo_active) noexcept
{
	zircon_editor_ui_state* p_ui_state =
		p_session ? p_session->get_ui_state() : nullptr;

	if (p_ui_state == nullptr)
		return;

	zircon_gizmo_overlay_state_t& state =
		p_ui_state->get_gizmo_overlay_state();

	state.m_mode = static_cast<kotek::uint8_t>(this->m_mode);
	state.m_is_snap_enabled = this->m_is_snap_enabled;
	state.m_is_gizmo_active = is_gizmo_active;
	state.m_is_dragging = this->m_drag.m_is_active;

	// the delta/result the overlay prints while dragging: kept cheap —
	// translate shows the position delta/result, scale the scale
	// delta/result, rotate the quaternion result (ImGuizmo reports no
	// per-axis angle, so the rotate delta stays zeroed)
	for (int component = 0; component < 3; ++component)
		state.m_drag_delta[component] = 0.0f;

	for (int component = 0; component < 4; ++component)
		state.m_result[component] = 0.0f;

	if (this->m_drag.m_is_active == false)
		return;

	zircon_world* p_world = p_session ? p_session->get_world() : nullptr;

	if (p_world == nullptr || p_world->is_initialized() == false ||
		p_world->get_factory() == nullptr ||
		p_world->get_ecs_context() == nullptr)
	{
		return;
	}

	zircon_component_transform* p_transform =
		static_cast<zircon_component_transform*>(
			p_world->get_factory()->get_component_by_enum(
				p_world->get_ecs_context(),
				p_ui_state->get_selected_entity(),
				eZirconComponentType::kzircon_component_transform));

	if (p_transform == nullptr)
		return;

	const kotek::math::vec3f_t& position = p_transform->get_position();
	const kotek::math::vec3f_t& scale = p_transform->get_scale();
	const kotek::math::quatf_t& rotation = p_transform->get_rotation();

	switch (this->m_mode)
	{
	case eMode::kTranslate:
	{
		state.m_drag_delta[0] =
			position.x() - this->m_drag.m_start_position[0];
		state.m_drag_delta[1] =
			position.y() - this->m_drag.m_start_position[1];
		state.m_drag_delta[2] =
			position.z() - this->m_drag.m_start_position[2];

		state.m_result[0] = position.x();
		state.m_result[1] = position.y();
		state.m_result[2] = position.z();
		break;
	}
	case eMode::kRotate:
	{
		state.m_result[0] = rotation.x();
		state.m_result[1] = rotation.y();
		state.m_result[2] = rotation.z();
		state.m_result[3] = rotation.w();
		break;
	}
	case eMode::kScale:
	{
		state.m_drag_delta[0] = scale.x() - this->m_drag.m_start_scale[0];
		state.m_drag_delta[1] = scale.y() - this->m_drag.m_start_scale[1];
		state.m_drag_delta[2] = scale.z() - this->m_drag.m_start_scale[2];

		state.m_result[0] = scale.x();
		state.m_result[1] = scale.y();
		state.m_result[2] = scale.z();
		break;
	}
	}
}
