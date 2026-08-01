#pragma once

class zircon_session_editor;
class zircon_session_editor_manager;
class zircon_renderer_bgfx;
class zircon_world;
class zircon_factory;
class zircon_component_transform;
struct zircon_ecs_context_t;

// editor window "gizmo_imguizmo" (task Z3 P2f) capacities/tunables —
// named per the memory-budget rule
// entity scan cap of the editor-camera lookup (the same bound the grid
// and own-gizmo passes use)
#define zircon_DEF_EDITOR_UI_GIZMO_IMGUIZMO_MAX_ENTITY_SCAN_COUNT 256
// snap steps, toggled with T — they deliberately mirror the own gizmo's
// zircon_DEF_RENDER_PASS_GIZMO_SNAP_* values so both variants feel
// identical; defined separately because this window must not include a
// passes-project header (module direction: editor.ui never depends on
// concrete passes)
#define zircon_DEF_EDITOR_UI_GIZMO_IMGUIZMO_SNAP_TRANSLATE_STEP 0.25f
#define zircon_DEF_EDITOR_UI_GIZMO_IMGUIZMO_SNAP_ROTATE_STEP_DEGREES 15.0f
#define zircon_DEF_EDITOR_UI_GIZMO_IMGUIZMO_SNAP_SCALE_STEP 0.1f

/// @brief \~english the ImGuizmo gizmo variant's host (task Z3 P2f). The
/// imgui frame opens AND closes inside the imgui pass's OnUpdate, so the
/// variant's pass (no_streaming::zircon_render_graph_pass_editor_gizmo_
/// imguizmo_bgfx) is an inert registration token and the actual
/// ImGuizmo::Manipulate runs HERE — a ui_element's Draw executes inside
/// that frame (see zircon_render_graph_pass_editor_imgui_bgfx::OnUpdate).
///
/// Activation: the window reads the editor render-graph slot's name list
/// through zircon_renderer_bgfx::get_render_graph_info and Manipulates
/// only while kZirconConfig_RenderPassEditorGizmoImguizmoName is in the
/// set AND enabled AND a transform-bearing selection exists (the object
/// list window / own-gizmo click-select drive
/// zircon_editor_ui_state::set_selected_entity). Otherwise it draws
/// nothing and costs nothing.
///
/// Hosting: Manipulate must draw into a current window's draw list, so
/// the window opens a full-viewport, fully transparent, no-input host
/// window ("##gizmo_imguizmo_host") and calls Manipulate inside it;
/// ImGuizmo reads the raw IO, so the no-input flags do not break
/// dragging. The window sits FIRST in the editor's ui element list so
/// the gizmo renders under the docked panels (the own gizmo renders
/// under all of imgui — this is the closest layering imgui allows).
///
/// Write-back: while ImGuizmo::IsUsing() the manipulated matrix is
/// decomposed and written into the selected entity's transform component
/// as the live preview; on the release edge the window issues ONE
/// journaled zircon_command_edit_component_state through the shared
/// zircon_gizmo_commit_transform_drag_edit (the exact commit the own
/// gizmo's pass makes) — every ImGuizmo drag undoes/redoes with the Z6
/// journal. Modes switch with W/E/R, snapping toggles with T (only while
/// no text field captures the keyboard), same as the own gizmo.
///
/// The vendored ImGuizmo is the sanctioned third-party exception (the
/// P2f plan): it makes raw ImGui:: calls internally, which is why it
/// stays passes-project-local; this window only talks to the ImGuizmo::
/// entry points, all direct imgui UI calls go through the wrapper.
class zircon_editor_ui_window_gizmo_imguizmo
	: public kotek::core::ktkISDKUIElement
{
public:
	/// @param p_renderer_bgfx nullptr when the active renderer is not
	/// bgfx (NRI) — the window then stays inert (the variant is a bgfx
	/// editor pass)
	zircon_editor_ui_window_gizmo_imguizmo(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_renderer_bgfx* p_renderer_bgfx);
	~zircon_editor_ui_window_gizmo_imguizmo(void);

	void Initialize(void) override;
	void Shutdown(void) override;
	void Draw(kotek::core::ktkMainManager* main_manager) override;

	int Get_ID(void) const override;

	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

	// --- pure-static math surface (the unit tests pin these; column-major
	// bx layout with the translation at [12..14] — byte-identical to what
	// the model_static pass feeds bgfx::setTransform, because the gizmo
	// must overlay the RENDERED model). ImGuizmo needs no convention
	// bridge: its matrix_t stores/reads the flat float[16] exactly like
	// bx/glm column-major and applies it as M*v (verified against its
	// vec_t::TransformPoint and operator*), so the engine's bx view/
	// projection bytes feed Manipulate unchanged. What DOES differ
	// between bx and glm is the quaternion->matrix CONTENT (bx rotates
	// the opposite sense: bx::mtxFromQuaternion gives m[1]==-1 for a
	// 90-degree-Z quaternion where glm gives +1 — pinned by
	// RenderPassModelStaticCollectDrawItems), so the two functions below
	// follow the bx formulas, NOT glm's ---

	// out = T * R * S composed exactly the way the renderer composes a
	// model matrix (bx::mtxFromQuaternion's layout with the scale folded
	// into the rotation columns, the translation at [12..14])
	static void compose_trs_matrix(const float* p_position,
		const float* p_rotation_quat, const float* p_scale,
		float* p_out_matrix) noexcept;

	// the inverse of compose_trs_matrix: translation from the fourth
	// column, per-axis scale from the column lengths, the quaternion from
	// the scale-normalized 3x3 (Shepperd's method, normalized output; the
	// extraction is conjugated vs. glm's quat_cast — the matrix content
	// is bx's rotation sense, see above). False when a scale column
	// degenerates (a zero-length basis vector carries no rotation) —
	// negative scales are not recovered (the gizmos clamp scale away from
	// zero/negative anyway)
	static bool decompose_trs_matrix(const float* p_matrix,
		float* p_out_position, float* p_out_scale,
		float* p_out_rotation_quat) noexcept;

private:
	// the mode values mirror no_streaming::eZirconRenderPassGizmoMode —
	// the shared overlay POD (zircon_gizmo_overlay_state_t::m_mode)
	// documents 0/1/2 = translate/rotate/scale; redeclared here so this
	// header stays free of passes-project headers
	enum class eMode : kotek::uint8_t
	{
		kTranslate = 0,
		kRotate,
		kScale
	};

	// the drag-start capture (the live preview mutates the component; on
	// the release edge the commit helper restores this state and journals
	// one edit command with the final state as after)
	struct drag_context_t
	{
		bool m_is_active;
		float m_start_position[3];
		float m_start_scale[3];
		float m_start_rotation[4];
	};

	// view/projection of the editor camera (the sdk_camera component scan,
	// replicated per the pass-independence pattern the grid/own-gizmo
	// passes follow); false when the world or the camera is not there —
	// the caller then falls back to the grid pass's default orbit
	bool resolve_editor_camera(zircon_session_editor* p_session,
		float* p_out_view, float* p_out_projection) noexcept;

	// restores the drag-start state into the transform and drops the drag
	// (the cancel path: variant toggled off / selection lost mid-drag)
	void cancel_drag(zircon_component_transform* p_transform) noexcept;

	// feeds the shared gizmo overlay POD the P2e overlay window draws
	// (mode/snap always while active, delta+result while dragging)
	void publish_overlay_state(zircon_session_editor* p_session,
		bool is_gizmo_active) noexcept;

private:
	bool m_is_show_window;
	zircon_session_editor_manager* m_p_manager_session_editor;
	zircon_renderer_bgfx* m_p_renderer_bgfx;

	eMode m_mode;
	bool m_is_snap_enabled;
	drag_context_t m_drag;

	// the component's TRS captured every frame BEFORE Manipulate runs —
	// on the drag's rising edge this is the true drag-start state
	float m_frame_start_position[3];
	float m_frame_start_scale[3];
	float m_frame_start_rotation[4];

	// own edge detection for the mode/snap keys (the overlay POD reads
	// level state through the wrapper's IO, same as the own gizmo pass)
	bool m_was_key_w_down;
	bool m_was_key_e_down;
	bool m_was_key_r_down;
	bool m_was_key_t_down;

	// one-time boot evidence that the variant is live (the log line the
	// P2f verification looks for)
	bool m_is_announced;

	// overlay-POD ownership: true while THIS window is the publisher of
	// zircon_gizmo_overlay_state_t. The own gizmo pass owns the same POD
	// when it is the active variant, so an inactive window must not
	// touch it — EXCEPT once on the active->inactive transition, to
	// clear the state this window last published (otherwise a removed/
	// disabled variant leaves a stale "gizmo active" overlay behind)
	bool m_is_overlay_published;
};
