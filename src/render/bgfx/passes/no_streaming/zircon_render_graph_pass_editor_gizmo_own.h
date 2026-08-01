#pragma once

#include "../../zircon_render_graph_pass_editor.h"

class zircon_factory;
class zircon_session_editor;
class zircon_session_editor_manager;
class zircon_world;
class zircon_component_transform;
class zircon_editor_command_history;
struct zircon_ecs_context_t;

// editor pass "editor_gizmo_own" (task Z3 P2e) capacities and tunables —
// named per the memory-budget rule
// upper bound of one compiled shader blob read through the kotek
// filesystem (the gizmo pair is ~1-3 KB; 4x headroom)
#define zircon_DEF_RENDER_PASS_EDITOR_GIZMO_OWN_SHADER_BIN_MAX_SIZE 16384
// entity scan cap of the editor-camera lookup AND of the click-select
// sphere pick (same bound the grid pass uses)
#define zircon_DEF_RENDER_PASS_EDITOR_GIZMO_OWN_MAX_ENTITY_SCAN_COUNT 256

// constant screen size: the gizmo's unit length (arrow length, ring
// radius) always maps to this many screen pixels, whatever the camera
// distance (scale = distance * world-per-pixel * this)
#define zircon_DEF_RENDER_PASS_GIZMO_SCREEN_EXTENT_PIXELS 96.0f

// handle dimensions in unit gizmo space (the whole gizmo is scaled by the
// screen-size factor at draw/pick time)
#define zircon_DEF_RENDER_PASS_GIZMO_ARROW_SHAFT_LENGTH 0.75f
#define zircon_DEF_RENDER_PASS_GIZMO_ARROW_SHAFT_RADIUS 0.025f
#define zircon_DEF_RENDER_PASS_GIZMO_ARROW_TIP_RADIUS 0.07f
#define zircon_DEF_RENDER_PASS_GIZMO_CENTER_HALF_SIZE 0.07f
#define zircon_DEF_RENDER_PASS_GIZMO_QUAD_MIN 0.35f
#define zircon_DEF_RENDER_PASS_GIZMO_QUAD_MAX 0.65f
#define zircon_DEF_RENDER_PASS_GIZMO_RING_TUBE_RADIUS 0.02f

// pick tolerances in unit gizmo space (deliberately generous over the
// visual dimensions — a gizmo handle must be easy to catch)
#define zircon_DEF_RENDER_PASS_GIZMO_PICK_AXIS_RADIUS 0.07f
#define zircon_DEF_RENDER_PASS_GIZMO_PICK_TIP_RADIUS 0.10f
#define zircon_DEF_RENDER_PASS_GIZMO_PICK_CENTER_RADIUS 0.13f
#define zircon_DEF_RENDER_PASS_GIZMO_PICK_RING_HALF_WIDTH 0.08f
#define zircon_DEF_RENDER_PASS_GIZMO_PICK_QUAD_PADDING 0.03f

// a mouse press+release whose travel stays under this many pixels is a
// click (selection), not a drag attempt
#define zircon_DEF_RENDER_PASS_GIZMO_CLICK_MAX_PIXEL_TRAVEL 5.0f
// click-select fallback: entities without a bounding_sphere component are
// tested against a sphere of this radius (meters) at the transform
// position
#define zircon_DEF_RENDER_PASS_GIZMO_SELECT_SPHERE_RADIUS 0.5f

// highlight multipliers over the per-axis base colors
#define zircon_DEF_RENDER_PASS_GIZMO_COLOR_HOVER_BOOST 1.35f
#define zircon_DEF_RENDER_PASS_GIZMO_COLOR_ACTIVE_BOOST 1.70f

// snap steps (toggled with the snap key — T — see the pass comment)
#define zircon_DEF_RENDER_PASS_GIZMO_SNAP_TRANSLATE_STEP 0.25f
#define zircon_DEF_RENDER_PASS_GIZMO_SNAP_ROTATE_STEP_DEGREES 15.0f
#define zircon_DEF_RENDER_PASS_GIZMO_SNAP_SCALE_STEP 0.1f
// lower clamp for any scale component during a scale drag
#define zircon_DEF_RENDER_PASS_GIZMO_SCALE_MIN 0.01f

// mesh tessellation of the static handle geometry
#define zircon_DEF_RENDER_PASS_GIZMO_CIRCLE_SEGMENTS 24
#define zircon_DEF_RENDER_PASS_GIZMO_RING_RADIAL_SEGMENTS 32
#define zircon_DEF_RENDER_PASS_GIZMO_RING_TUBE_SEGMENTS 4

namespace no_streaming
{
	// gizmo modes, shared with the ui_state overlay struct (the overlay
	// window maps them to labels); kept as a plain uint8_t-compatible
	// enum so the POD state in zircon_editor_ui_state stays free of
	// render headers
	enum class eZirconRenderPassGizmoMode : kotek::uint8_t
	{
		kTranslate = 0,
		kRotate,
		kScale
	};

	// handle priority classes (picking: a center hit beats a plane hit
	// beats an axis hit regardless of ray length; the nearest ray length
	// wins WITHIN a class)
	enum class eZirconRenderPassGizmoHandleClass : kotek::uint8_t
	{
		kCenter = 0,
		kPlane,
		kAxis
	};

	// the static mesh sections of the shared handle vertex/index buffer
	enum class eZirconRenderPassGizmoMeshSection : kotek::uint8_t
	{
		// cylinder + cone along +X, total length 1
		kArrow = 0,
		// cylinder + tip cube along +X (the scale axis handle)
		kScaleAxis,
		// torus in the XY plane (normal +Z), radius 1
		kRing,
		// axis-aligned cube, half extent
		// zircon_DEF_RENDER_PASS_GIZMO_CENTER_HALF_SIZE
		kCube,
		// quad in the XY plane spanning [QUAD_MIN..QUAD_MAX]^2
		kQuad,
		kCount
	};

	// axis slots inside a handle descriptor; kAxisNone marks "no axis"
	// (the center handles)
	constexpr kotek::uint8_t zircon_kGizmoAxisNone = 3;

	struct zircon_render_pass_gizmo_drag_context_t;

	// the handle table's function surface (task Z3 P2e "customizable"):
	// a new 3D-output handle registers a table entry with these three
	// functions (+ mesh section + axis data) and never touches the pass
	// shell. All functions are pure float-array math — the unit tests
	// drive them directly, no bgfx needed.

	// draw: composes the handle's model matrix from the gizmo origin and
	// the constant-screen-size scale (column-major, bx layout)
	using zircon_gizmo_build_model_fn = void (*)(
		kotek::uint8_t axis_a, kotek::uint8_t axis_b,
		const float* p_gizmo_origin, float gizmo_scale,
		float* p_out_model) noexcept;

	// intersect: analytic ray-vs-handle in world space; the ray direction
	// is normalized; on a hit writes the ray length of the nearest
	// intersection (used for nearest-within-class ordering)
	using zircon_gizmo_intersect_fn = bool (*)(
		const float* p_ray_origin, const float* p_ray_direction,
		const float* p_gizmo_origin, float gizmo_scale,
		kotek::uint8_t axis_a, kotek::uint8_t axis_b,
		float* p_out_ray_length) noexcept;

	// apply: advances the drag from the current mouse ray; reads the
	// captured start state from the context and writes the delta +
	// absolute result fields (the overlay reads them from there)
	using zircon_gizmo_apply_fn = void (*)(
		zircon_render_pass_gizmo_drag_context_t& context,
		const float* p_ray_origin, const float* p_ray_direction,
		bool is_snap_enabled) noexcept;

	struct zircon_render_pass_gizmo_handle_t
	{
		eZirconRenderPassGizmoMode m_mode;
		eZirconRenderPassGizmoHandleClass m_class;
		eZirconRenderPassGizmoMeshSection m_mesh_section;
		kotek::uint8_t m_axis_a;
		kotek::uint8_t m_axis_b;
		zircon_gizmo_build_model_fn m_pfn_build_model;
		zircon_gizmo_intersect_fn m_pfn_intersect;
		zircon_gizmo_apply_fn m_pfn_apply;
	};

	// everything a drag needs, captured at mouse-down (the live preview
	// mutates the component per frame; on release the pass restores the
	// start state and issues ONE journaled command with the final state —
	// see commit_drag_edit)
	struct zircon_render_pass_gizmo_drag_context_t
	{
		const zircon_render_pass_gizmo_handle_t* m_p_handle;
		// world rays, normalized direction (start = at mouse-down)
		float m_ray_origin_start[3];
		float m_ray_direction_start[3];
		// the drag plane normal: plane quads -> axis_a x axis_b, center
		// translate -> the camera-facing direction at drag start, ring ->
		// the ring axis; unused by the pure-axis handles
		float m_drag_plane_normal[3];
		// the selected entity's transform at drag start (quat x,y,z,w)
		float m_start_position[3];
		float m_start_scale[3];
		float m_start_rotation[4];
		// the gizmo's world anchor/extent at drag start
		float m_gizmo_origin[3];
		float m_gizmo_scale;
		// last computed drag output (what the overlay shows)
		float m_delta[3];
		float m_result_position[3];
		float m_result_scale[3];
		float m_result_rotation[4];
		bool m_is_active;
	};

	// the own-gizmo editor pass (task Z3 P2e): translate (3 axis arrows +
	// 3 plane quads + center cube), rotate (3 rings), scale (3 axis
	// shaft+cubes + center cube) over the editor session's selected
	// entity (zircon_editor_ui_state::get_selected_entity — the object
	// list window owns list selection, this pass adds click-select in the
	// viewport). Handles are bgfx-drawn static meshes, per-axis colors,
	// constant screen size, depth-test-off overlay rendering between the
	// grid and imgui. Picking is analytic only (no GPU picking). Modes
	// switch with W/E/R, snapping toggles with T (imgui IO, only while no
	// text field captures the keyboard). Every drag END issues one
	// zircon_command_edit_component_state through the session's command
	// history, so gizmo edits undo/redo with the Z6 journal.
	class zircon_render_graph_pass_editor_gizmo_own_bgfx
		: public zircon_render_graph_pass_editor_bgfx
	{
	public:
		zircon_render_graph_pass_editor_gizmo_own_bgfx(void);
		~zircon_render_graph_pass_editor_gizmo_own_bgfx(void);

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;
		void OnDestroyResources() override;
		void OnUpdate(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;
		void OnRender(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;

		// --- the registered handle set (the "small static table" of the
		// plan; 7 translate + 3 rotate + 4 scale handles)
		static constexpr kotek::uint8_t kHandleCount = 14;
		static const zircon_render_pass_gizmo_handle_t*
		get_handles(void) noexcept;

		// --- pure-static math surface (the unit tests pin these) ---

		// rounds value to the nearest multiple of step (0.3 on a 0.25
		// step -> 0.25); a non-positive step passes the value through
		static float snap_value(float value, float step) noexcept;

		// the constant-screen-size factor: how big one unit of gizmo
		// space is in world units at the gizmo's distance.
		// projection_one_over_tan_fovy is projection[5] of a standard
		// perspective matrix (1/tan(fovY/2)); a non-positive value falls
		// back to a 60-degree fov
		static float compute_gizmo_scale(const float* p_camera_position,
			const float* p_gizmo_origin, float projection_one_over_tan_fovy,
			float viewport_height_pixels) noexcept;

		// mouse pixel position (imgui space, y-down) -> world ray through
		// the inverse view-projection (the grid pass's compute_world_ray
		// convention); the direction comes out normalized
		static bool compute_mouse_ray(
			const float* p_inverse_view_projection, float mouse_x,
			float mouse_y, float viewport_width, float viewport_height,
			float* p_out_ray_origin, float* p_out_ray_direction) noexcept;

		// ray (normalized direction) vs sphere; writes the nearest
		// positive ray length
		static bool intersect_ray_sphere(const float* p_ray_origin,
			const float* p_ray_direction, const float* p_sphere_center,
			float sphere_radius, float* p_out_ray_length) noexcept;

		// ray vs plane (point + normalized normal); false when parallel
		// or behind the camera
		static bool intersect_ray_plane(const float* p_ray_origin,
			const float* p_ray_direction, const float* p_plane_point,
			const float* p_plane_normal, float* p_out_ray_length) noexcept;

		// parameter along a line (point + NORMALIZED direction) of the
		// closest point between the line and the ray — the axis-drag
		// workhorse
		static float closest_param_line_to_ray(const float* p_line_point,
			const float* p_line_direction, const float* p_ray_origin,
			const float* p_ray_direction) noexcept;

		// closest distance between the ray and a fixed point (the
		// center-scale drag ratio source)
		static float closest_distance_ray_to_point(
			const float* p_ray_origin, const float* p_ray_direction,
			const float* p_point) noexcept;

		// quaternion helpers (x,y,z,w storage, right-handed, matching the
		// bx/glm conventions the transform component uses)
		static void quat_from_axis_angle(const float* p_axis,
			float angle_radians, float* p_out_quat) noexcept;
		// out = p * q (p applied AFTER q in the column-vector reading)
		static void quat_multiply(const float* p_p, const float* p_q,
			float* p_out) noexcept;
		static void quat_normalize(float* p_quat) noexcept;

		// handle picking over the registered table of one mode: class
		// priority center > plane > axis, nearest ray length within the
		// winning class. Returns the handle index or -1
		static int pick_handle(const float* p_ray_origin,
			const float* p_ray_direction,
			eZirconRenderPassGizmoMode mode, const float* p_gizmo_origin,
			float gizmo_scale, float* p_out_ray_length = nullptr) noexcept;

		// fills the drag context's start capture at mouse-down (the
		// per-kind plane normal is derived from the handle/camera here)
		static void begin_drag(
			zircon_render_pass_gizmo_drag_context_t& context,
			const zircon_render_pass_gizmo_handle_t* p_handle,
			const float* p_ray_origin, const float* p_ray_direction,
			const float* p_camera_position, const float* p_gizmo_origin,
			float gizmo_scale, const float* p_start_position,
			const float* p_start_scale, const float* p_start_rotation_quat)
			noexcept;

		// dispatches the context's handle apply function
		static void apply_drag(
			zircon_render_pass_gizmo_drag_context_t& context,
			const float* p_ray_origin, const float* p_ray_direction,
			bool is_snap_enabled) noexcept;

		// click-select: nearest entity whose pick sphere the ray hits.
		// The sphere is the bounding_sphere component's (center offset by
		// the transform position, radius scaled by the largest scale
		// component) when present, else a
		// zircon_DEF_RENDER_PASS_GIZMO_SELECT_SPHERE_RADIUS sphere at the
		// transform position. False = nothing hit (the caller deselects)
		static bool pick_entity(zircon_factory* p_factory,
			zircon_ecs_context_t* p_context,
			kotek::uint32_t entity_count_max_limit,
			const float* p_ray_origin, const float* p_ray_direction,
			kotek::entity_t* p_out_entity) noexcept;

		// the drag-END commit (the exact shape the Z6 stress suite
		// drives): the component currently holds the previewed final
		// state — serialize it as state_after, restore the captured start
		// state so the command's Execute captures the true before, then
		// placement-new one zircon_command_edit_component_state into the
		// history pool and ExecuteCommand it. False when any link of the
		// chain is missing (the preview was already restored by the
		// caller's drag teardown in that case)
		static bool commit_drag_edit(
			zircon_session_editor_manager* p_manager_session_editor,
			zircon_factory* p_factory,
			zircon_editor_command_history* p_history,
			zircon_ecs_context_t* p_context, kotek::entity_t entity,
			const float* p_start_position, const float* p_start_scale,
			const float* p_start_rotation_quat) noexcept;

		// --- the handle function implementations (registered in the
		// table; public so the table can name them and the unit tests
		// can pin them one by one)
		static void build_model_along_axis(kotek::uint8_t axis_a,
			kotek::uint8_t axis_b, const float* p_gizmo_origin,
			float gizmo_scale, float* p_out_model) noexcept;
		static void build_model_ring(kotek::uint8_t axis_a,
			kotek::uint8_t axis_b, const float* p_gizmo_origin,
			float gizmo_scale, float* p_out_model) noexcept;
		static void build_model_center(kotek::uint8_t axis_a,
			kotek::uint8_t axis_b, const float* p_gizmo_origin,
			float gizmo_scale, float* p_out_model) noexcept;
		static void build_model_quad(kotek::uint8_t axis_a,
			kotek::uint8_t axis_b, const float* p_gizmo_origin,
			float gizmo_scale, float* p_out_model) noexcept;

		static bool intersect_axis(const float* p_ray_origin,
			const float* p_ray_direction, const float* p_gizmo_origin,
			float gizmo_scale, kotek::uint8_t axis_a,
			kotek::uint8_t axis_b, float* p_out_ray_length) noexcept;
		static bool intersect_ring(const float* p_ray_origin,
			const float* p_ray_direction, const float* p_gizmo_origin,
			float gizmo_scale, kotek::uint8_t axis_a,
			kotek::uint8_t axis_b, float* p_out_ray_length) noexcept;
		static bool intersect_quad(const float* p_ray_origin,
			const float* p_ray_direction, const float* p_gizmo_origin,
			float gizmo_scale, kotek::uint8_t axis_a,
			kotek::uint8_t axis_b, float* p_out_ray_length) noexcept;
		static bool intersect_center(const float* p_ray_origin,
			const float* p_ray_direction, const float* p_gizmo_origin,
			float gizmo_scale, kotek::uint8_t axis_a,
			kotek::uint8_t axis_b, float* p_out_ray_length) noexcept;

		static void apply_translate_axis(
			zircon_render_pass_gizmo_drag_context_t& context,
			const float* p_ray_origin, const float* p_ray_direction,
			bool is_snap_enabled) noexcept;
		static void apply_translate_plane(
			zircon_render_pass_gizmo_drag_context_t& context,
			const float* p_ray_origin, const float* p_ray_direction,
			bool is_snap_enabled) noexcept;
		static void apply_rotate_ring(
			zircon_render_pass_gizmo_drag_context_t& context,
			const float* p_ray_origin, const float* p_ray_direction,
			bool is_snap_enabled) noexcept;
		static void apply_scale_axis(
			zircon_render_pass_gizmo_drag_context_t& context,
			const float* p_ray_origin, const float* p_ray_direction,
			bool is_snap_enabled) noexcept;
		static void apply_scale_center(
			zircon_render_pass_gizmo_drag_context_t& context,
			const float* p_ray_origin, const float* p_ray_direction,
			bool is_snap_enabled) noexcept;

	private:
		// per-section draw ranges inside the shared static index buffer
		struct mesh_section_range_t
		{
			kotek::uint32_t m_index_offset;
			kotek::uint32_t m_index_count;
		};

		// --- frame plumbing (all private, no test surface) ---

		bgfx::ShaderHandle load_shader_blob(
			const char* p_shader_file_name) noexcept;

		// same editor-camera resolution as the grid pass (the sdk_camera
		// component scan), replicated per the pass-independence pattern
		bool resolve_editor_camera(
			float* p_out_view, float* p_out_projection) noexcept;

		// the editor session + world + factory + the selected entity's
		// transform, or nullptrs (every field stays null when the chain
		// breaks)
		struct frame_context_t
		{
			zircon_session_editor* m_p_session;
			zircon_world* m_p_world;
			zircon_factory* m_p_factory;
			zircon_ecs_context_t* m_p_ecs_context;
			zircon_component_transform* m_p_transform;
			kotek::entity_t m_selected_entity;
		};

		frame_context_t resolve_frame_context(void) noexcept;

		// the gizmo anchor for the current selection: origin + constant
		// screen-size scale (fills view/projection/camera position too —
		// the pick and the draw share one camera resolve per call site)
		bool resolve_gizmo_frame(const frame_context_t& context,
			float* p_out_view, float* p_out_projection,
			float* p_out_inverse_view_projection,
			float* p_out_camera_position, float* p_out_gizmo_origin,
			float* p_out_gizmo_scale) noexcept;

		// the static handle meshes (all sections in one vertex/index
		// buffer pair, ranges in m_mesh_sections)
		void build_handle_meshes(void) noexcept;

		// publishes the overlay state the gizmo overlay window draws
		// (mode/snap always, delta+result while dragging)
		void publish_overlay_state(const frame_context_t& context) noexcept;

	private:
		bgfx::ProgramHandle m_program;
		bgfx::UniformHandle m_uniform_color;
		bgfx::VertexBufferHandle m_vertex_buffer;
		bgfx::IndexBufferHandle m_index_buffer;
		bgfx::VertexLayout m_layout;

		mesh_section_range_t m_mesh_sections[static_cast<kotek::uint8_t>(
			eZirconRenderPassGizmoMeshSection::kCount)];

		eZirconRenderPassGizmoMode m_mode;
		bool m_is_snap_enabled;
		// table index of the hovered handle, -1 = none
		int m_hovered_handle;
		zircon_render_pass_gizmo_drag_context_t m_drag;

		// click-vs-drag disambiguation (a press with no handle under the
		// cursor becomes a selection click on release when the travel
		// stayed under the pixel threshold)
		bool m_is_click_candidate;
		float m_click_press_position[2];

		// own edge detection for the mode/snap keys (the imgui IO key
		// array is level state; the pass sees it a frame late, so imgui's
		// own pressed-edges are unusable here)
		bool m_was_key_w_down;
		bool m_was_key_e_down;
		bool m_was_key_r_down;
		bool m_was_key_t_down;
		bool m_was_mouse_down;

		bool m_is_warned_about_missing_program;
	};
} // namespace no_streaming
