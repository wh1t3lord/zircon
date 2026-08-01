#include "zircon_ui_gizmo_overlay.h"

#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "zircon_editor_ui_state.h"

zircon_editor_ui_window_gizmo_overlay::
	zircon_editor_ui_window_gizmo_overlay(
		zircon_session_editor_manager* p_manager_session_editor) :
	m_is_show_window{true},
	m_p_manager_session_editor{p_manager_session_editor}
{
	KOTEK_ASSERT(
		p_manager_session_editor, "valid pointer is expected");
}

zircon_editor_ui_window_gizmo_overlay::
	~zircon_editor_ui_window_gizmo_overlay(void)
{
}

void zircon_editor_ui_window_gizmo_overlay::Initialize(void) {}

void zircon_editor_ui_window_gizmo_overlay::Shutdown(void) {}

void zircon_editor_ui_window_gizmo_overlay::Draw(
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

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor
				->get_current_session_id());

	if (p_session == nullptr || p_session->get_ui_state() == nullptr)
		return;

	const zircon_gizmo_overlay_state_t& state =
		p_session->get_ui_state()->get_gizmo_overlay_state();

	// nothing selected (or the gizmo pass is not in the editor set) —
	// the overlay stays out of the way
	if (state.m_is_gizmo_active == false)
		return;

	kotek::core::ktkIImguiWrapper* p_wrapper_imgui =
		p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui == nullptr)
		return;

	// a pinned, click-through overlay in the viewport's top-left corner
	// (below the top bar)
	p_wrapper_imgui->SetNextWindowPos(ImVec2(14.0f, 48.0f),
		ImGuiCond_Always);
	p_wrapper_imgui->SetNextWindowBgAlpha(0.55f);

	constexpr ImGuiWindowFlags _kOverlayFlags = ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

	if (p_wrapper_imgui->Begin("##gizmo_overlay", nullptr,
			_kOverlayFlags) == false)
	{
		p_wrapper_imgui->End();
		return;
	}

	const char* p_mode_label = "translate";
	const char* p_mode_axis_hint = "";

	switch (state.m_mode)
	{
	case 1:
	{
		p_mode_label = "rotate";
		break;
	}
	case 2:
	{
		p_mode_label = "scale";
		break;
	}
	default:
	{
		break;
	}
	}

	p_wrapper_imgui->Text("gizmo: %s   snap: %s", p_mode_label,
		state.m_is_snap_enabled ? "on" : "off");
	p_wrapper_imgui->TextDisabled("W move  E rotate  R scale  T snap");

	if (state.m_is_dragging)
	{
		switch (state.m_mode)
		{
		case 0:
		{
			p_wrapper_imgui->Text("delta (%+.3f, %+.3f, %+.3f)",
				static_cast<double>(state.m_drag_delta[0]),
				static_cast<double>(state.m_drag_delta[1]),
				static_cast<double>(state.m_drag_delta[2]));
			p_wrapper_imgui->Text("pos   (%.3f, %.3f, %.3f)",
				static_cast<double>(state.m_result[0]),
				static_cast<double>(state.m_result[1]),
				static_cast<double>(state.m_result[2]));
			break;
		}
		case 1:
		{
			// the rotate delta is the dragged axis's angle in degrees
			// (exactly one component is non-zero)
			const char* p_axis_names[3] = {"X", "Y", "Z"};
			kotek::uint8_t dragged_axis = 0;

			for (kotek::uint8_t axis = 0; axis < 3; ++axis)
			{
				if (state.m_drag_delta[axis] != 0.0f)
				{
					dragged_axis = axis;
					break;
				}
			}

			p_mode_axis_hint = p_axis_names[dragged_axis];

			p_wrapper_imgui->Text("delta %+.2f deg around %s",
				static_cast<double>(state.m_drag_delta[dragged_axis]),
				p_mode_axis_hint);
			p_wrapper_imgui->Text("rot   (%.3f, %.3f, %.3f, %.3f)",
				static_cast<double>(state.m_result[0]),
				static_cast<double>(state.m_result[1]),
				static_cast<double>(state.m_result[2]),
				static_cast<double>(state.m_result[3]));
			break;
		}
		case 2:
		{
			p_wrapper_imgui->Text("delta (%+.3f, %+.3f, %+.3f)",
				static_cast<double>(state.m_drag_delta[0]),
				static_cast<double>(state.m_drag_delta[1]),
				static_cast<double>(state.m_drag_delta[2]));
			p_wrapper_imgui->Text("scale (%.3f, %.3f, %.3f)",
				static_cast<double>(state.m_result[0]),
				static_cast<double>(state.m_result[1]),
				static_cast<double>(state.m_result[2]));
			break;
		}
		default:
		{
			break;
		}
		}
	}

	p_wrapper_imgui->End();
}

int zircon_editor_ui_window_gizmo_overlay::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_GizmoOverlay);
}

void zircon_editor_ui_window_gizmo_overlay::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_editor_ui_window_gizmo_overlay::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_editor_ui_window_gizmo_overlay::Is_Shown(void) const
{
	return this->m_is_show_window;
}
