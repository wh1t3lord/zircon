#pragma once

#include "../../zircon_render_graph_pass_editor.h"

// editor pass "editor_grid" (task Z3 P2d) capacities — named per the
// memory-budget rule
// upper bound of one compiled shader blob read through the kotek
// filesystem (the grid pair is ~1-4 KB today; 4x headroom)
#define zircon_DEF_RENDER_PASS_EDITOR_GRID_SHADER_BIN_MAX_SIZE 16384
// entity scan cap of the editor-camera lookup (the sdk camera is among the
// first entities of an editor world; a world that hides it past the cap
// just gets the fallback orbit)
#define zircon_DEF_RENDER_PASS_EDITOR_GRID_MAX_ENTITY_SCAN_COUNT 256

namespace no_streaming
{
	// the procedural infinite grid: one fullscreen triangle (no vertex
	// buffer — bgfx::setVertexCount(3) + SV_VertexID expansion in the
	// shader), the fragment stage reconstructs the world XZ-plane ray per
	// pixel from the inverse view-projection and draws the grid
	// analytically (1 m cells + 10 m major lines, colored X/Z axes,
	// distance fade). Sits between the present clear and imgui in the
	// editor set: alpha-blended, depth-tested, no depth write, so the
	// scene geometry of the later passes draws OVER it. View/projection
	// come from the editor camera (the sdk_camera component of the editor
	// session's world); without one the pass falls back to the same
	// default orbit the game model_static pass uses. POD/handles only;
	// every bgfx handle is released in OnDestroyResources.
	class zircon_render_graph_pass_editor_grid_bgfx
		: public zircon_render_graph_pass_editor_bgfx
	{
	public:
		zircon_render_graph_pass_editor_grid_bgfx(void);
		~zircon_render_graph_pass_editor_grid_bgfx(void);

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

		// the fallback camera: the same default orbit the game
		// model_static pass documents (eye (4,3,4) -> origin, 60-degree
		// fov, bx column-major layout) — also the unit-test fixture for
		// the ray math below. homogeneous_depth is bgfx::getCaps()->
		// homogeneousDepth at runtime (the tests pass false — no bgfx
		// there) and only selects the projection's depth mapping
		static void build_default_orbit_view_projection(
			float* p_out_view, float* p_out_projection, float aspect_ratio,
			bool homogeneous_depth) noexcept;

		// composes inverse(P*V) exactly the way bgfx fills the predefined
		// u_invViewProj from setViewTransform (bx::float4x4_mul(view,
		// proj) inverted) — the shader's clip -> world mapping
		static void compose_inverse_view_projection(const float* p_view,
			const float* p_projection,
			float* p_out_inverse_view_projection) noexcept;

		// camera world position of a rigid view matrix: the translation
		// of the inverse view, -(R^T * t) in the bx/glm column-major
		// layout (translation lives at [12..14])
		static void compute_camera_position(const float* p_view,
			float* p_out_position) noexcept;

		// the shader's per-pixel ray, mirrored in C++ for the unit tests
		// (grid.fs.slang carries the same formulas): unprojects the NDC
		// point at z=-1 and z=+1 through inverse(P*V) — any two distinct
		// depths stay on the same view ray whatever the projection's
		// depth convention. Returns false when a homogeneous w
		// degenerates (a broken projection)
		static bool compute_world_ray(
			const float* p_inverse_view_projection, float ndc_x,
			float ndc_y, float* p_out_point_near,
			float* p_out_point_far) noexcept;

		// ray ∩ the y=0 grid plane; false when the ray is parallel to the
		// plane or the plane lies behind the camera
		static bool intersect_xz_plane(const float* p_ray_origin,
			const float* p_ray_direction, float* p_out_hit) noexcept;

		// validity gate for matrices sourced from USER data (the
		// sdk_camera component is scene content, not programmer state):
		// every element finite and at least one non-zero — a
		// default-constructed or corrupt camera falls back to the default
		// orbit instead of NaN-ing every consumer downstream (the
		// 2026-09-04 "clicked an entity, gizmo scale assert" chain)
		static bool is_matrix_usable(const float* p_matrix) noexcept;

	private:
		// reads one compiled shader blob from
		// data_user/shader_cache/bgfx/<active renderer>/ through the kotek
		// filesystem (same route as the game model_static pass). An
		// absent file is not an error: the handle stays invalid and the
		// pass no-ops
		bgfx::ShaderHandle load_shader_blob(
			const char* p_shader_file_name) noexcept;

		// view/projection of the editor camera (the sdk_camera component
		// of the editor session's world); false when the session, the
		// world, or the camera is not there yet — the caller then falls
		// back to the default orbit
		bool resolve_editor_camera(
			float* p_out_view, float* p_out_projection) noexcept;

	private:
		bgfx::ProgramHandle m_program;
		bgfx::UniformHandle m_uniform_camera_pos;
		// cached BGFX_CAPS_VERTEX_ID support bit (the fullscreen triangle
		// needs it); an unsupported renderer keeps the pass inert
		bool m_is_vertex_id_supported;
		// the pass is inert (renders nothing) until its compiled program
		// exists — tracks the one-time warning so the log is not spammed
		bool m_is_warned_about_missing_program;
	};
} // namespace no_streaming
