#include "zircon_render_graph_pass_editor_gizmo_own.h"

#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

#include "zircon_render_graph_pass_editor_grid.h"

#include "../../../../ecs/zircon_factory.h"
#include "../../../../ecs/zircon_component_sdk_camera.h"
#include "../../../../world/zircon_world.h"
#include "../../../../editor/session/zircon_session_editor.h"
#include "../../../../editor/session/zircon_session_editor_manager.h"
#include "../../../../editor/ui/zircon_editor_ui_state.h"
#include "../../../../editor/commands/zircon_command_history.h"
#include "../../../../editor/commands/zircon_command_edit_component_state.h"
#include "../../../../editor/commands/zircon_gizmo_edit_commit.h"

#include <cmath>

namespace
{
	constexpr float _kPi = 3.14159265358979323846f;

	// the pick/drag math works on cardinal axes only — index 0/1/2 ->
	// unit X/Y/Z (the kAxisNone slot maps to zero, never used as a
	// direction)
	inline void zircon_gizmo_axis_vector(
		kotek::uint8_t axis, float* p_out) noexcept
	{
		p_out[0] = axis == 0 ? 1.0f : 0.0f;
		p_out[1] = axis == 1 ? 1.0f : 0.0f;
		p_out[2] = axis == 2 ? 1.0f : 0.0f;
	}

	inline void zircon_gizmo_cross(
		const float* p_a, const float* p_b, float* p_out) noexcept
	{
		p_out[0] = p_a[1] * p_b[2] - p_a[2] * p_b[1];
		p_out[1] = p_a[2] * p_b[0] - p_a[0] * p_b[2];
		p_out[2] = p_a[0] * p_b[1] - p_a[1] * p_b[0];
	}

	inline float zircon_gizmo_dot(const float* p_a, const float* p_b) noexcept
	{
		return p_a[0] * p_b[0] + p_a[1] * p_b[1] + p_a[2] * p_b[2];
	}

	inline float zircon_gizmo_length(const float* p_v) noexcept
	{
		return std::sqrt(zircon_gizmo_dot(p_v, p_v));
	}

	inline void zircon_gizmo_normalize(float* p_v) noexcept
	{
		const float length = zircon_gizmo_length(p_v);

		if (length > 1e-8f)
		{
			p_v[0] /= length;
			p_v[1] /= length;
			p_v[2] /= length;
		}
	}

	// both closest-point parameters of a line (point + NORMALIZED
	// direction) and a ray (origin + NORMALIZED direction): p_out_line
	// is along the line direction, p_out_ray along the ray direction
	void zircon_gizmo_closest_params_line_ray(const float* p_line_point,
		const float* p_line_direction, const float* p_ray_origin,
		const float* p_ray_direction, float* p_out_line,
		float* p_out_ray) noexcept
	{
		const float w0[3] = {p_line_point[0] - p_ray_origin[0],
			p_line_point[1] - p_ray_origin[1],
			p_line_point[2] - p_ray_origin[2]};

		const float b = zircon_gizmo_dot(p_line_direction, p_ray_direction);
		const float d = zircon_gizmo_dot(p_line_direction, w0);
		const float e = zircon_gizmo_dot(p_ray_direction, w0);

		const float denominator = 1.0f - b * b;

		if (std::fabs(denominator) < 1e-8f)
		{
			// parallel: the approach distance is constant — pin the ray
			// parameter to the ray point closest to the line point and
			// take the line parameter of that same pair
			*p_out_ray = e;
			*p_out_line = b * e - d;
			return;
		}

		*p_out_line = (b * e - d) / denominator;
		*p_out_ray = (e - b * d) / denominator;
	}

	// position-only vertex of the static handle meshes
	struct zircon_gizmo_vertex_t
	{
		float m_position[3];
	};

	// section vertex/index counts (see build_handle_meshes)
	constexpr kotek::uint32_t _kCircleSegments =
		zircon_DEF_RENDER_PASS_GIZMO_CIRCLE_SEGMENTS;
	constexpr kotek::uint32_t _kCylinderVertexCount = 2 * _kCircleSegments + 1;
	constexpr kotek::uint32_t _kCylinderIndexCount = 9 * _kCircleSegments;
	constexpr kotek::uint32_t _kConeVertexCount = _kCircleSegments + 2;
	constexpr kotek::uint32_t _kConeIndexCount = 6 * _kCircleSegments;
	constexpr kotek::uint32_t _kCubeVertexCount = 8;
	constexpr kotek::uint32_t _kCubeIndexCount = 36;
	constexpr kotek::uint32_t _kRingVertexCount =
		zircon_DEF_RENDER_PASS_GIZMO_RING_RADIAL_SEGMENTS *
		zircon_DEF_RENDER_PASS_GIZMO_RING_TUBE_SEGMENTS;
	constexpr kotek::uint32_t _kRingIndexCount =
		zircon_DEF_RENDER_PASS_GIZMO_RING_RADIAL_SEGMENTS *
		zircon_DEF_RENDER_PASS_GIZMO_RING_TUBE_SEGMENTS * 6;
	constexpr kotek::uint32_t _kQuadVertexCount = 4;
	constexpr kotek::uint32_t _kQuadIndexCount = 6;

	// section order = eZirconRenderPassGizmoMeshSection
	constexpr kotek::uint32_t _kTotalVertexCount =
		(_kCylinderVertexCount + _kConeVertexCount) // arrow
		+ (_kCylinderVertexCount + _kCubeVertexCount) // scale axis
		+ _kRingVertexCount + _kCubeVertexCount + _kQuadVertexCount;
	constexpr kotek::uint32_t _kTotalIndexCount =
		(_kCylinderIndexCount + _kConeIndexCount) +
		(_kCylinderIndexCount + _kCubeIndexCount) + _kRingIndexCount +
		_kCubeIndexCount + _kQuadIndexCount;

	static_assert(_kTotalVertexCount <= 65536,
		"16-bit indices can't address more vertices");

	// cylinder along +X from x=0 to x=length with a bottom cap
	void zircon_gizmo_build_cylinder(zircon_gizmo_vertex_t* p_out_vertices,
		kotek::uint16_t* p_out_indices, kotek::uint16_t base_vertex,
		float length, float radius) noexcept
	{
		for (kotek::uint32_t segment = 0; segment < _kCircleSegments;
			 ++segment)
		{
			const float angle =
				2.0f * _kPi * static_cast<float>(segment) /
				static_cast<float>(_kCircleSegments);
			const float y = std::cos(angle) * radius;
			const float z = std::sin(angle) * radius;

			p_out_vertices[segment].m_position[0] = 0.0f;
			p_out_vertices[segment].m_position[1] = y;
			p_out_vertices[segment].m_position[2] = z;

			p_out_vertices[_kCircleSegments + segment].m_position[0] =
				length;
			p_out_vertices[_kCircleSegments + segment].m_position[1] = y;
			p_out_vertices[_kCircleSegments + segment].m_position[2] = z;
		}

		p_out_vertices[2 * _kCircleSegments].m_position[0] = 0.0f;
		p_out_vertices[2 * _kCircleSegments].m_position[1] = 0.0f;
		p_out_vertices[2 * _kCircleSegments].m_position[2] = 0.0f;

		kotek::uint32_t index_cursor = 0;

		for (kotek::uint16_t segment = 0; segment < _kCircleSegments;
			 ++segment)
		{
			const kotek::uint16_t next = static_cast<kotek::uint16_t>(
				(segment + 1) % _kCircleSegments);

			// side quad
			p_out_indices[index_cursor++] = base_vertex + segment;
			p_out_indices[index_cursor++] =
				base_vertex + _kCircleSegments + segment;
			p_out_indices[index_cursor++] =
				base_vertex + _kCircleSegments + next;

			p_out_indices[index_cursor++] = base_vertex + segment;
			p_out_indices[index_cursor++] =
				base_vertex + _kCircleSegments + next;
			p_out_indices[index_cursor++] = base_vertex + next;

			// bottom cap (faces -X)
			p_out_indices[index_cursor++] =
				base_vertex + 2 * _kCircleSegments;
			p_out_indices[index_cursor++] = base_vertex + next;
			p_out_indices[index_cursor++] = base_vertex + segment;
		}
	}

	// cone along +X: base ring at x=base_x radius radius, apex at
	// x=apex_x, with a base cap
	void zircon_gizmo_build_cone(zircon_gizmo_vertex_t* p_out_vertices,
		kotek::uint16_t* p_out_indices, kotek::uint16_t base_vertex,
		float base_x, float apex_x, float radius) noexcept
	{
		for (kotek::uint32_t segment = 0; segment < _kCircleSegments;
			 ++segment)
		{
			const float angle =
				2.0f * _kPi * static_cast<float>(segment) /
				static_cast<float>(_kCircleSegments);

			p_out_vertices[segment].m_position[0] = base_x;
			p_out_vertices[segment].m_position[1] =
				std::cos(angle) * radius;
			p_out_vertices[segment].m_position[2] =
				std::sin(angle) * radius;
		}

		p_out_vertices[_kCircleSegments].m_position[0] = apex_x;
		p_out_vertices[_kCircleSegments].m_position[1] = 0.0f;
		p_out_vertices[_kCircleSegments].m_position[2] = 0.0f;

		p_out_vertices[_kCircleSegments + 1].m_position[0] = base_x;
		p_out_vertices[_kCircleSegments + 1].m_position[1] = 0.0f;
		p_out_vertices[_kCircleSegments + 1].m_position[2] = 0.0f;

		kotek::uint32_t index_cursor = 0;

		for (kotek::uint16_t segment = 0; segment < _kCircleSegments;
			 ++segment)
		{
			const kotek::uint16_t next = static_cast<kotek::uint16_t>(
				(segment + 1) % _kCircleSegments);

			// side
			p_out_indices[index_cursor++] = base_vertex + segment;
			p_out_indices[index_cursor++] =
				base_vertex + _kCircleSegments;
			p_out_indices[index_cursor++] = base_vertex + next;

			// base cap (faces -X)
			p_out_indices[index_cursor++] =
				base_vertex + _kCircleSegments + 1;
			p_out_indices[index_cursor++] = base_vertex + next;
			p_out_indices[index_cursor++] = base_vertex + segment;
		}
	}

	// axis-aligned cube, half extent half_size, centered at (center_x,0,0)
	void zircon_gizmo_build_cube(zircon_gizmo_vertex_t* p_out_vertices,
		kotek::uint16_t* p_out_indices, kotek::uint16_t base_vertex,
		float center_x, float half_size) noexcept
	{
		for (kotek::uint8_t corner = 0; corner < 8; ++corner)
		{
			p_out_vertices[corner].m_position[0] =
				center_x + ((corner & 1) ? half_size : -half_size);
			p_out_vertices[corner].m_position[1] =
				(corner & 2) ? half_size : -half_size;
			p_out_vertices[corner].m_position[2] =
				(corner & 4) ? half_size : -half_size;
		}

		// 12 triangles over the 8 corners (x-lexicographic corner order)
		const kotek::uint16_t _kCubeIndices[_kCubeIndexCount] = {
			// -X / +X faces
			0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6,
			// -Y / +Y faces
			0, 1, 5, 0, 5, 4, 2, 6, 7, 2, 7, 3,
			// -Z / +Z faces
			0, 4, 6, 0, 6, 2, 1, 3, 7, 1, 7, 5};

		for (kotek::uint32_t index = 0; index < _kCubeIndexCount; ++index)
		{
			p_out_indices[index] = base_vertex + _kCubeIndices[index];
		}
	}

	// torus in the XY plane (normal +Z), centerline radius 1
	void zircon_gizmo_build_ring(zircon_gizmo_vertex_t* p_out_vertices,
		kotek::uint16_t* p_out_indices, kotek::uint16_t base_vertex,
		float tube_radius) noexcept
	{
		constexpr kotek::uint32_t _kRadial =
			zircon_DEF_RENDER_PASS_GIZMO_RING_RADIAL_SEGMENTS;
		constexpr kotek::uint32_t _kTube =
			zircon_DEF_RENDER_PASS_GIZMO_RING_TUBE_SEGMENTS;

		for (kotek::uint32_t radial = 0; radial < _kRadial; ++radial)
		{
			const float angle_radial =
				2.0f * _kPi * static_cast<float>(radial) /
				static_cast<float>(_kRadial);
			const float cos_radial = std::cos(angle_radial);
			const float sin_radial = std::sin(angle_radial);

			for (kotek::uint32_t tube = 0; tube < _kTube; ++tube)
			{
				const float angle_tube = 2.0f * _kPi *
					static_cast<float>(tube) / static_cast<float>(_kTube);
				const float radius = 1.0f + tube_radius *
					std::cos(angle_tube);

				zircon_gizmo_vertex_t& vertex =
					p_out_vertices[radial * _kTube + tube];

				vertex.m_position[0] = radius * cos_radial;
				vertex.m_position[1] = radius * sin_radial;
				vertex.m_position[2] = tube_radius * std::sin(angle_tube);
			}
		}

		kotek::uint32_t index_cursor = 0;

		for (kotek::uint16_t radial = 0; radial < _kRadial; ++radial)
		{
			const kotek::uint16_t radial_next =
				static_cast<kotek::uint16_t>((radial + 1) % _kRadial);

			for (kotek::uint16_t tube = 0; tube < _kTube; ++tube)
			{
				const kotek::uint16_t tube_next =
					static_cast<kotek::uint16_t>((tube + 1) % _kTube);

				const kotek::uint16_t v00 =
					base_vertex + radial * _kTube + tube;
				const kotek::uint16_t v01 =
					base_vertex + radial * _kTube + tube_next;
				const kotek::uint16_t v10 =
					base_vertex + radial_next * _kTube + tube;
				const kotek::uint16_t v11 =
					base_vertex + radial_next * _kTube + tube_next;

				p_out_indices[index_cursor++] = v00;
				p_out_indices[index_cursor++] = v10;
				p_out_indices[index_cursor++] = v11;

				p_out_indices[index_cursor++] = v00;
				p_out_indices[index_cursor++] = v11;
				p_out_indices[index_cursor++] = v01;
			}
		}
	}
} // namespace

namespace no_streaming
{
	// the registered handle set — the pass shell only iterates this
	// table; a new 3D-output handle = a new entry (+ a new mesh section
	// and/or function triple when no existing one fits), zero edits in
	// OnUpdate/OnRender
	const zircon_render_pass_gizmo_handle_t
		_kGizmoHandles
			[zircon_render_graph_pass_editor_gizmo_own_bgfx::kHandleCount] =
		{
			// translate: 3 axis arrows
			{eZirconRenderPassGizmoMode::kTranslate,
				eZirconRenderPassGizmoHandleClass::kAxis,
				eZirconRenderPassGizmoMeshSection::kArrow, 0,
				zircon_kGizmoAxisNone,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_along_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_translate_axis},
			{eZirconRenderPassGizmoMode::kTranslate,
				eZirconRenderPassGizmoHandleClass::kAxis,
				eZirconRenderPassGizmoMeshSection::kArrow, 1,
				zircon_kGizmoAxisNone,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_along_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_translate_axis},
			{eZirconRenderPassGizmoMode::kTranslate,
				eZirconRenderPassGizmoHandleClass::kAxis,
				eZirconRenderPassGizmoMeshSection::kArrow, 2,
				zircon_kGizmoAxisNone,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_along_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_translate_axis},
			// translate: 3 plane quads (XY, YZ, XZ)
			{eZirconRenderPassGizmoMode::kTranslate,
				eZirconRenderPassGizmoHandleClass::kPlane,
				eZirconRenderPassGizmoMeshSection::kQuad, 0, 1,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_quad,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_quad,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_translate_plane},
			{eZirconRenderPassGizmoMode::kTranslate,
				eZirconRenderPassGizmoHandleClass::kPlane,
				eZirconRenderPassGizmoMeshSection::kQuad, 1, 2,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_quad,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_quad,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_translate_plane},
			{eZirconRenderPassGizmoMode::kTranslate,
				eZirconRenderPassGizmoHandleClass::kPlane,
				eZirconRenderPassGizmoMeshSection::kQuad, 0, 2,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_quad,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_quad,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_translate_plane},
			// translate: center cube (camera-plane translation)
			{eZirconRenderPassGizmoMode::kTranslate,
				eZirconRenderPassGizmoHandleClass::kCenter,
				eZirconRenderPassGizmoMeshSection::kCube,
				zircon_kGizmoAxisNone, zircon_kGizmoAxisNone,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_center,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_center,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_translate_plane},
			// rotate: 3 rings (the ring normal IS the rotation axis)
			{eZirconRenderPassGizmoMode::kRotate,
				eZirconRenderPassGizmoHandleClass::kAxis,
				eZirconRenderPassGizmoMeshSection::kRing, 0,
				zircon_kGizmoAxisNone,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_ring,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_ring,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_rotate_ring},
			{eZirconRenderPassGizmoMode::kRotate,
				eZirconRenderPassGizmoHandleClass::kAxis,
				eZirconRenderPassGizmoMeshSection::kRing, 1,
				zircon_kGizmoAxisNone,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_ring,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_ring,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_rotate_ring},
			{eZirconRenderPassGizmoMode::kRotate,
				eZirconRenderPassGizmoHandleClass::kAxis,
				eZirconRenderPassGizmoMeshSection::kRing, 2,
				zircon_kGizmoAxisNone,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_ring,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_ring,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_rotate_ring},
			// scale: 3 axis shaft+cubes
			{eZirconRenderPassGizmoMode::kScale,
				eZirconRenderPassGizmoHandleClass::kAxis,
				eZirconRenderPassGizmoMeshSection::kScaleAxis, 0,
				zircon_kGizmoAxisNone,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_along_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_scale_axis},
			{eZirconRenderPassGizmoMode::kScale,
				eZirconRenderPassGizmoHandleClass::kAxis,
				eZirconRenderPassGizmoMeshSection::kScaleAxis, 1,
				zircon_kGizmoAxisNone,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_along_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_scale_axis},
			{eZirconRenderPassGizmoMode::kScale,
				eZirconRenderPassGizmoHandleClass::kAxis,
				eZirconRenderPassGizmoMeshSection::kScaleAxis, 2,
				zircon_kGizmoAxisNone,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_along_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_axis,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_scale_axis},
			// scale: center cube (uniform)
			{eZirconRenderPassGizmoMode::kScale,
				eZirconRenderPassGizmoHandleClass::kCenter,
				eZirconRenderPassGizmoMeshSection::kCube,
				zircon_kGizmoAxisNone, zircon_kGizmoAxisNone,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					build_model_center,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					intersect_center,
				&zircon_render_graph_pass_editor_gizmo_own_bgfx::
					apply_scale_center},
	};
} // namespace no_streaming

namespace no_streaming
{
	zircon_render_graph_pass_editor_gizmo_own_bgfx::
		zircon_render_graph_pass_editor_gizmo_own_bgfx(void) :
		zircon_render_graph_pass_editor_bgfx(),
		m_program{BGFX_INVALID_HANDLE},
		m_uniform_color{BGFX_INVALID_HANDLE},
		m_vertex_buffer{BGFX_INVALID_HANDLE},
		m_index_buffer{BGFX_INVALID_HANDLE},
		m_layout{},
		m_mesh_sections{},
		m_mode{eZirconRenderPassGizmoMode::kTranslate},
		m_is_snap_enabled{false},
		m_hovered_handle{-1},
		m_drag{},
		m_is_click_candidate{false},
		m_click_press_position{0.0f, 0.0f},
		m_was_key_w_down{false},
		m_was_key_e_down{false},
		m_was_key_r_down{false},
		m_was_key_t_down{false},
		m_was_mouse_down{false},
		m_is_warned_about_missing_program{false}
	{
	}

	zircon_render_graph_pass_editor_gizmo_own_bgfx::
		~zircon_render_graph_pass_editor_gizmo_own_bgfx(void)
	{
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::OnCreateResources(
		kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
		KOTEK_ASSERT(p_manager_main, "must be valid!");

		this->m_p_manager_main = p_manager_main;
		this->m_p_manager_resource = p_manager_resource;

		this->m_layout.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.end();

		this->build_handle_meshes();

		// per-draw solid color; the name mirrors the fragment shader's
		// cbuffer and the cmake uniform table (u_color:vec4:0:1)
		this->m_uniform_color =
			bgfx::createUniform("u_color", bgfx::UniformType::Vec4);

		KOTEK_ASSERT(bgfx::isValid(this->m_uniform_color),
			"failed to create the u_color uniform!");

		// the blob names the Slang pipeline produces from
		// data_game/shaders/slang/gizmo.<stage>.slang; until they exist
		// the pass stays inert (one-time warning)
		bgfx::ShaderHandle shader_vertex =
			this->load_shader_blob("gizmo.vs.bin");
		bgfx::ShaderHandle shader_fragment =
			this->load_shader_blob("gizmo.fs.bin");

		if (bgfx::isValid(shader_vertex) && bgfx::isValid(shader_fragment))
		{
			this->m_program =
				bgfx::createProgram(shader_vertex, shader_fragment, true);

			KOTEK_ASSERT(bgfx::isValid(this->m_program),
				"failed to link the editor_gizmo_own program!");
		}
		else
		{
			if (bgfx::isValid(shader_vertex))
				bgfx::destroy(shader_vertex);
			if (bgfx::isValid(shader_fragment))
				bgfx::destroy(shader_fragment);

			if (this->m_is_warned_about_missing_program == false)
			{
				KOTEK_MESSAGE_WARNING(
					"[editor_gizmo_own] compiled shader blobs are absent "
					"under data_user/shader_cache/bgfx/ (the Slang "
					"pipeline has not produced them yet) — the pass "
					"stays inert");

				this->m_is_warned_about_missing_program = true;
			}
		}
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::
		OnDestroyResources()
	{
		if (bgfx::isValid(this->m_program))
		{
			bgfx::destroy(this->m_program);
			this->m_program = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_uniform_color))
		{
			bgfx::destroy(this->m_uniform_color);
			this->m_uniform_color = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_index_buffer))
		{
			bgfx::destroy(this->m_index_buffer);
			this->m_index_buffer = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_vertex_buffer))
		{
			bgfx::destroy(this->m_vertex_buffer);
			this->m_vertex_buffer = BGFX_INVALID_HANDLE;
		}

		this->m_hovered_handle = -1;
		this->m_drag.m_is_active = false;
		this->m_is_click_candidate = false;
		this->m_is_warned_about_missing_program = false;

		// the overlay window must not keep drawing a dead pass's state —
		// and the cancel arbiter must not probe a stale drag flag of a
		// pass that no longer exists (task Z19)
		frame_context_t frame = this->resolve_frame_context();

		if (frame.m_p_session && frame.m_p_session->get_ui_state())
		{
			frame.m_p_session->get_ui_state()
				->get_gizmo_overlay_state()
				.m_is_gizmo_active = false;
			frame.m_p_session->get_ui_state()
				->get_gizmo_overlay_state()
				.m_is_drag_active = false;
		}
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		frame_context_t frame = this->resolve_frame_context();

		// the imgui pass owns the context and creates it in ITS
		// OnCreateResources; the IO fields are readable between frames
		// (this pass updates before the imgui pass's NewFrame, so the
		// values are the previous frame's — a one-frame latency the
		// hover/drag never notices)
		kotek::core::ktkIImguiWrapper* p_imgui_wrapper = nullptr;

		if (this->m_p_manager_main &&
			this->m_p_manager_main->Get_EngineConfig() &&
			this->m_p_manager_main->Get_EngineConfig()->IsFeatureEnabled(
				kotek::Core::eEngineFeatureSDK::
					kEngine_Feature_SDK_ImGui_Initialized))
		{
			p_imgui_wrapper = this->m_p_manager_main->Get_ImguiWrapper();
		}

		if (frame.m_p_transform == nullptr || p_imgui_wrapper == nullptr)
		{
			// imgui vanished mid-drag: cancel WITHOUT a command, but put
			// the start state back so the preview does not stick
			if (this->m_drag.m_is_active && frame.m_p_transform)
			{
				frame.m_p_transform->set_position(kotek::math::vec3f_t(
					this->m_drag.m_start_position[0],
					this->m_drag.m_start_position[1],
					this->m_drag.m_start_position[2]));
				frame.m_p_transform->set_scale(kotek::math::vec3f_t(
					this->m_drag.m_start_scale[0],
					this->m_drag.m_start_scale[1],
					this->m_drag.m_start_scale[2]));
				frame.m_p_transform->set_rotation(kotek::math::quatf_t(
					this->m_drag.m_start_rotation[0],
					this->m_drag.m_start_rotation[1],
					this->m_drag.m_start_rotation[2],
					this->m_drag.m_start_rotation[3]));
			}

			this->m_drag.m_is_active = false;
			this->m_hovered_handle = -1;
			this->m_is_click_candidate = false;
			this->m_was_mouse_down = false;

			this->publish_overlay_state(frame);
			return;
		}

		ImGuiIO& io = p_imgui_wrapper->GetIO();

		// mode + snap keys: edge-detected by hand (imgui's pressed edges
		// are computed inside NewFrame, which runs after this pass), and
		// only while no text field eats the keyboard
		const bool is_key_w_down = io.KeysDown[ImGuiKey_W];
		const bool is_key_e_down = io.KeysDown[ImGuiKey_E];
		const bool is_key_r_down = io.KeysDown[ImGuiKey_R];
		const bool is_key_t_down = io.KeysDown[ImGuiKey_T];

		if (io.WantTextInput == false)
		{
			// mode keys are ignored mid-drag: the drag context carries
			// its own handle and the render loop draws the current
			// mode's handles — switching mid-drag would show the wrong
			// set
			if (this->m_drag.m_is_active == false)
			{
				if (is_key_w_down && this->m_was_key_w_down == false)
					this->m_mode = eZirconRenderPassGizmoMode::kTranslate;
				if (is_key_e_down && this->m_was_key_e_down == false)
					this->m_mode = eZirconRenderPassGizmoMode::kRotate;
				if (is_key_r_down && this->m_was_key_r_down == false)
					this->m_mode = eZirconRenderPassGizmoMode::kScale;
			}
			if (is_key_t_down && this->m_was_key_t_down == false)
				this->m_is_snap_enabled = !this->m_is_snap_enabled;
		}

		this->m_was_key_w_down = is_key_w_down;
		this->m_was_key_e_down = is_key_e_down;
		this->m_was_key_r_down = is_key_r_down;
		this->m_was_key_t_down = is_key_t_down;

		const bool is_mouse_down = io.MouseDown[0];
		const bool is_mouse_down_edge =
			is_mouse_down && this->m_was_mouse_down == false;
		const bool is_mouse_up_edge =
			is_mouse_down == false && this->m_was_mouse_down;

		this->m_was_mouse_down = is_mouse_down;

		// imgui reports an unavailable mouse as -FLT_MAX
		const bool is_mouse_valid = io.MousePos.x > -1e+30f;

		float view[16];
		float projection[16];
		float inverse_view_projection[16];
		float camera_position[3];
		float gizmo_origin[3];
		float gizmo_scale = 1.0f;

		float viewport_width = 0.0f;
		float viewport_height = 0.0f;

		if (this->m_p_manager_main &&
			this->m_p_manager_main->Get_WindowManager())
		{
			viewport_width = static_cast<float>(
				this->m_p_manager_main->Get_WindowManager()
					->ActiveWindow_GetWidth());
			viewport_height = static_cast<float>(
				this->m_p_manager_main->Get_WindowManager()
					->ActiveWindow_GetHeight());
		}

		const bool is_frame_ready = is_mouse_valid &&
			viewport_width > 0.0f && viewport_height > 0.0f &&
			this->resolve_gizmo_frame(frame, view, projection,
				inverse_view_projection, camera_position, gizmo_origin,
				&gizmo_scale);

		float ray_origin[3] = {0.0f, 0.0f, 0.0f};
		float ray_direction[3] = {0.0f, 0.0f, 0.0f};

		if (is_frame_ready)
		{
			compute_mouse_ray(inverse_view_projection, io.MousePos.x,
				io.MousePos.y, viewport_width, viewport_height,
				ray_origin, ray_direction);
		}

		if (this->m_drag.m_is_active)
		{
			// ESC arbitration (task Z19): the session's cancel arbiter
			// (through the editor imgui pass's adapter) asked to abort
			// the drag — restore the pre-drag component state, journal
			// NOTHING, end the drag; the request flag is consumed here,
			// m_is_drag_active is re-asserted by publish_overlay_state
			// below
			zircon_gizmo_overlay_state_t* p_overlay_state =
				(frame.m_p_session && frame.m_p_session->get_ui_state())
				? &frame.m_p_session->get_ui_state()
					   ->get_gizmo_overlay_state()
				: nullptr;

			if (p_overlay_state && p_overlay_state->m_cancel_drag_requested)
			{
				cancel_drag_edit(frame.m_p_factory, frame.m_p_ecs_context,
					frame.m_selected_entity, this->m_drag.m_start_position,
					this->m_drag.m_start_scale, this->m_drag.m_start_rotation);

				this->m_drag.m_is_active = false;
				p_overlay_state->m_cancel_drag_requested = false;
			}
			else
			{
				if (is_frame_ready)
				{
					apply_drag(this->m_drag, ray_origin, ray_direction,
						this->m_is_snap_enabled);

					// the live preview mutates the component directly; the
					// drag-end commit restores the start state and issues
					// ONE journaled command, so the history sees a single
					// edit
					frame.m_p_transform->set_position(kotek::math::vec3f_t(
						this->m_drag.m_result_position[0],
						this->m_drag.m_result_position[1],
						this->m_drag.m_result_position[2]));
					frame.m_p_transform->set_scale(kotek::math::vec3f_t(
						this->m_drag.m_result_scale[0],
						this->m_drag.m_result_scale[1],
						this->m_drag.m_result_scale[2]));
					frame.m_p_transform->set_rotation(kotek::math::quatf_t(
						this->m_drag.m_result_rotation[0],
						this->m_drag.m_result_rotation[1],
						this->m_drag.m_result_rotation[2],
						this->m_drag.m_result_rotation[3]));
				}

				if (is_mouse_up_edge)
				{
					commit_drag_edit(this->m_p_manager_session_editor,
						frame.m_p_factory,
						frame.m_p_session->get_command_history(),
						frame.m_p_ecs_context, frame.m_selected_entity,
						this->m_drag.m_start_position,
						this->m_drag.m_start_scale,
						this->m_drag.m_start_rotation);

					this->m_drag.m_is_active = false;
				}
			}
		}
		else if (is_frame_ready)
		{
			// hover pre-highlight (not while an imgui window owns the
			// mouse)
			if (io.WantCaptureMouse == false)
			{
				this->m_hovered_handle = pick_handle(ray_origin,
					ray_direction, this->m_mode, gizmo_origin,
					gizmo_scale);
			}
			else
			{
				this->m_hovered_handle = -1;
			}

			if (is_mouse_down_edge && io.WantCaptureMouse == false)
			{
				if (this->m_hovered_handle >= 0)
				{
					const kotek::math::vec3f_t& position =
						frame.m_p_transform->get_position();
					const kotek::math::vec3f_t& scale =
						frame.m_p_transform->get_scale();
					const kotek::math::quatf_t& rotation =
						frame.m_p_transform->get_rotation();

					const float start_position[3] = {position.x(),
						position.y(), position.z()};
					const float start_scale[3] = {
						scale.x(), scale.y(), scale.z()};
					const float start_rotation[4] = {rotation.x(),
						rotation.y(), rotation.z(), rotation.w()};

					begin_drag(this->m_drag,
						&get_handles()[this->m_hovered_handle],
						ray_origin, ray_direction, camera_position,
						gizmo_origin, gizmo_scale, start_position,
						start_scale, start_rotation);

					apply_drag(this->m_drag, ray_origin, ray_direction,
						this->m_is_snap_enabled);
				}
				else
				{
					// a press over empty viewport: maybe a selection
					// click (decided on release by travel)
					this->m_is_click_candidate = true;
					this->m_click_press_position[0] = io.MousePos.x;
					this->m_click_press_position[1] = io.MousePos.y;
				}
			}

			if (this->m_is_click_candidate && is_mouse_down)
			{
				const float travel_x =
					io.MousePos.x - this->m_click_press_position[0];
				const float travel_y =
					io.MousePos.y - this->m_click_press_position[1];

				if (travel_x * travel_x + travel_y * travel_y >
					zircon_DEF_RENDER_PASS_GIZMO_CLICK_MAX_PIXEL_TRAVEL *
						zircon_DEF_RENDER_PASS_GIZMO_CLICK_MAX_PIXEL_TRAVEL)
				{
					this->m_is_click_candidate = false;
				}
			}

			if (is_mouse_up_edge)
			{
				if (this->m_is_click_candidate)
				{
					this->m_is_click_candidate = false;

					kotek::entity_t picked_entity{
						kotek::ktk::kInvalidECSEntity};

					pick_entity(frame.m_p_factory, frame.m_p_ecs_context,
						frame.m_p_world->get_entity_count_max_limit(),
						ray_origin, ray_direction, &picked_entity);

					if (frame.m_p_session->get_ui_state())
					{
						// a miss deselects (the standard editor empty-
						// click behavior)
						frame.m_p_session->get_ui_state()
							->set_selected_entity(picked_entity);
					}
				}
			}
		}

		if (is_mouse_up_edge)
		{
			this->m_is_click_candidate = false;
		}

		this->publish_overlay_state(frame);
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::OnRender(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		if (bgfx::isValid(this->m_program) == false)
		{
			return;
		}

		frame_context_t frame = this->resolve_frame_context();

		if (frame.m_p_transform == nullptr)
		{
			return;
		}

		float view[16];
		float projection[16];
		float inverse_view_projection[16];
		float camera_position[3];
		float gizmo_origin[3];
		float gizmo_scale = 1.0f;

		if (this->resolve_gizmo_frame(frame, view, projection,
				inverse_view_projection, camera_position, gizmo_origin,
				&gizmo_scale) == false)
		{
			return;
		}

		const bgfx::ViewId pass_id =
			static_cast<bgfx::ViewId>(my_id_in_queue);

		bgfx::setViewName(pass_id, "EditorGizmoOwn");
		bgfx::setViewRect(pass_id, 0, 0, bgfx::BackbufferRatio::Equal);
		bgfx::setViewTransform(pass_id, view, projection);

		// the dragged handle out-highlights the hovered one
		int active_handle = -1;

		if (this->m_drag.m_is_active && this->m_drag.m_p_handle)
		{
			active_handle = static_cast<int>(
				this->m_drag.m_p_handle - get_handles());
		}

		// per-axis base colors (X red, Y green, Z blue), the center
		// handles yellow — the gizmo color language every editor uses
		constexpr float _kAxisColors[3][3] = {
			{0.86f, 0.22f, 0.22f},
			{0.25f, 0.75f, 0.25f},
			{0.25f, 0.45f, 0.95f}};
		constexpr float _kCenterColor[3] = {0.95f, 0.85f, 0.30f};

		// depth-test-off overlay: no depth write, alpha blend — the pass
		// runs after the grid so the handles always read over the scene
		constexpr kotek::uint64_t _kState = BGFX_STATE_WRITE_RGB |
			BGFX_STATE_WRITE_A |
			BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
				BGFX_STATE_BLEND_INV_SRC_ALPHA);

		for (kotek::uint8_t handle_index = 0; handle_index < kHandleCount;
			 ++handle_index)
		{
			const zircon_render_pass_gizmo_handle_t& handle =
				get_handles()[handle_index];

			if (handle.m_mode != this->m_mode)
				continue;

			float model[16];

			handle.m_pfn_build_model(handle.m_axis_a, handle.m_axis_b,
				gizmo_origin, gizmo_scale, model);

			// plane quads take the color of their plane's normal axis
			// (the XY quad reads blue), center handles the center color
			const float* p_base_color = _kCenterColor;

			if (handle.m_class ==
				eZirconRenderPassGizmoHandleClass::kPlane)
			{
				// the normal axis is the one NOT spanned by the quad
				kotek::uint8_t normal_axis = 0;

				for (kotek::uint8_t axis = 0; axis < 3; ++axis)
				{
					if (axis != handle.m_axis_a &&
						axis != handle.m_axis_b)
					{
						normal_axis = axis;
						break;
					}
				}

				p_base_color = _kAxisColors[normal_axis];
			}
			else if (handle.m_axis_a != zircon_kGizmoAxisNone)
			{
				p_base_color = _kAxisColors[handle.m_axis_a];
			}

			float boost = 1.0f;

			if (static_cast<int>(handle_index) == active_handle)
			{
				boost = zircon_DEF_RENDER_PASS_GIZMO_COLOR_ACTIVE_BOOST;
			}
			else if (static_cast<int>(handle_index) ==
				this->m_hovered_handle)
			{
				boost = zircon_DEF_RENDER_PASS_GIZMO_COLOR_HOVER_BOOST;
			}

			const float color[4] = {
				p_base_color[0] * boost > 1.0f
					? 1.0f
					: p_base_color[0] * boost,
				p_base_color[1] * boost > 1.0f
					? 1.0f
					: p_base_color[1] * boost,
				p_base_color[2] * boost > 1.0f
					? 1.0f
					: p_base_color[2] * boost,
				1.0f};

			const mesh_section_range_t& section =
				this->m_mesh_sections[static_cast<kotek::uint8_t>(
					handle.m_mesh_section)];

			bgfx::setUniform(this->m_uniform_color, color);
			bgfx::setTransform(model);
			bgfx::setVertexBuffer(0, this->m_vertex_buffer);
			bgfx::setIndexBuffer(this->m_index_buffer,
				section.m_index_offset, section.m_index_count);
			bgfx::setState(_kState);
			bgfx::submit(pass_id, this->m_program);
		}
	}

	const zircon_render_pass_gizmo_handle_t*
	zircon_render_graph_pass_editor_gizmo_own_bgfx::get_handles(
		void) noexcept
	{
		return _kGizmoHandles;
	}
} // namespace no_streaming

namespace no_streaming
{
	float zircon_render_graph_pass_editor_gizmo_own_bgfx::snap_value(
		float value, float step) noexcept
	{
		if (step <= 0.0f)
			return value;

		return std::round(value / step) * step;
	}

	float zircon_render_graph_pass_editor_gizmo_own_bgfx::
		compute_gizmo_scale(const float* p_camera_position,
			const float* p_gizmo_origin,
			float projection_one_over_tan_fovy,
			float viewport_height_pixels) noexcept
	{
		KOTEK_ASSERT(p_camera_position, "must be valid");
		KOTEK_ASSERT(p_gizmo_origin, "must be valid");

		if (p_camera_position == nullptr || p_gizmo_origin == nullptr)
			return 1.0f;

		// 1/tan(30 degrees) — the fallback 60-degree fov
		if (projection_one_over_tan_fovy <= 1e-6f)
			projection_one_over_tan_fovy = 1.7320508f;

		if (viewport_height_pixels < 1.0f)
			viewport_height_pixels = 1.0f;

		const float delta[3] = {p_gizmo_origin[0] - p_camera_position[0],
			p_gizmo_origin[1] - p_camera_position[1],
			p_gizmo_origin[2] - p_camera_position[2]};

		const float distance = zircon_gizmo_length(delta);

		// world height of the viewport at the gizmo's distance is
		// 2*distance*tan(fov/2); one gizmo unit maps to
		// zircon_DEF_RENDER_PASS_GIZMO_SCREEN_EXTENT_PIXELS of it
		const float world_height =
			2.0f * distance / projection_one_over_tan_fovy;

		return world_height *
			(zircon_DEF_RENDER_PASS_GIZMO_SCREEN_EXTENT_PIXELS /
				viewport_height_pixels);
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::
		compute_mouse_ray(const float* p_inverse_view_projection,
			float mouse_x, float mouse_y, float viewport_width,
			float viewport_height, float* p_out_ray_origin,
			float* p_out_ray_direction) noexcept
	{
		KOTEK_ASSERT(p_inverse_view_projection, "must be valid");
		KOTEK_ASSERT(p_out_ray_origin, "must be valid storage");
		KOTEK_ASSERT(p_out_ray_direction, "must be valid storage");
		KOTEK_ASSERT(viewport_width > 0.0f, "must be positive");
		KOTEK_ASSERT(viewport_height > 0.0f, "must be positive");

		if (p_inverse_view_projection == nullptr ||
			p_out_ray_origin == nullptr || p_out_ray_direction == nullptr ||
			viewport_width <= 0.0f || viewport_height <= 0.0f)
		{
			return false;
		}

		// imgui pixels are y-down, the bgfx NDC is y-up
		const float ndc_x = (mouse_x / viewport_width) * 2.0f - 1.0f;
		const float ndc_y = 1.0f - (mouse_y / viewport_height) * 2.0f;

		float point_near[3];
		float point_far[3];

		if (zircon_render_graph_pass_editor_grid_bgfx::compute_world_ray(
				p_inverse_view_projection, ndc_x, ndc_y, point_near,
				point_far) == false)
		{
			return false;
		}

		p_out_ray_origin[0] = point_near[0];
		p_out_ray_origin[1] = point_near[1];
		p_out_ray_origin[2] = point_near[2];

		p_out_ray_direction[0] = point_far[0] - point_near[0];
		p_out_ray_direction[1] = point_far[1] - point_near[1];
		p_out_ray_direction[2] = point_far[2] - point_near[2];

		zircon_gizmo_normalize(p_out_ray_direction);

		return true;
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::
		intersect_ray_sphere(const float* p_ray_origin,
			const float* p_ray_direction, const float* p_sphere_center,
			float sphere_radius, float* p_out_ray_length) noexcept
	{
		KOTEK_ASSERT(p_ray_origin, "must be valid");
		KOTEK_ASSERT(p_ray_direction, "must be valid (normalized)");
		KOTEK_ASSERT(p_sphere_center, "must be valid");
		KOTEK_ASSERT(p_out_ray_length, "must be valid storage");
		KOTEK_ASSERT(sphere_radius >= 0.0f, "must be non-negative");

		if (p_ray_origin == nullptr || p_ray_direction == nullptr ||
			p_sphere_center == nullptr || p_out_ray_length == nullptr ||
			sphere_radius < 0.0f)
		{
			return false;
		}

		const float to_center[3] = {p_sphere_center[0] - p_ray_origin[0],
			p_sphere_center[1] - p_ray_origin[1],
			p_sphere_center[2] - p_ray_origin[2]};

		const float projection =
			zircon_gizmo_dot(to_center, p_ray_direction);

		const float distance_squared =
			zircon_gizmo_dot(to_center, to_center) -
			projection * projection;

		const float radius_squared = sphere_radius * sphere_radius;

		if (distance_squared > radius_squared)
			return false;

		// the nearer intersection; a ray starting inside the sphere
		// counts as a hit at the exit face's near side (0)
		const float half_chord =
			std::sqrt(radius_squared - distance_squared);

		float ray_length = projection - half_chord;

		if (ray_length < 0.0f)
		{
			ray_length = projection + half_chord > 0.0f
				? 0.0f
				: ray_length;
		}

		if (ray_length < 0.0f)
			return false;

		*p_out_ray_length = ray_length;

		return true;
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::
		intersect_ray_plane(const float* p_ray_origin,
			const float* p_ray_direction, const float* p_plane_point,
			const float* p_plane_normal, float* p_out_ray_length) noexcept
	{
		KOTEK_ASSERT(p_ray_origin, "must be valid");
		KOTEK_ASSERT(p_ray_direction, "must be valid (normalized)");
		KOTEK_ASSERT(p_plane_point, "must be valid");
		KOTEK_ASSERT(p_plane_normal, "must be valid (normalized)");
		KOTEK_ASSERT(p_out_ray_length, "must be valid storage");

		if (p_ray_origin == nullptr || p_ray_direction == nullptr ||
			p_plane_point == nullptr || p_plane_normal == nullptr ||
			p_out_ray_length == nullptr)
		{
			return false;
		}

		const float denominator =
			zircon_gizmo_dot(p_ray_direction, p_plane_normal);

		if (std::fabs(denominator) < 1e-8f)
			return false;

		const float to_plane[3] = {p_plane_point[0] - p_ray_origin[0],
			p_plane_point[1] - p_ray_origin[1],
			p_plane_point[2] - p_ray_origin[2]};

		const float ray_length =
			zircon_gizmo_dot(to_plane, p_plane_normal) / denominator;

		if (ray_length <= 0.0f)
			return false;

		*p_out_ray_length = ray_length;

		return true;
	}

	float zircon_render_graph_pass_editor_gizmo_own_bgfx::
		closest_param_line_to_ray(const float* p_line_point,
			const float* p_line_direction, const float* p_ray_origin,
			const float* p_ray_direction) noexcept
	{
		KOTEK_ASSERT(p_line_point, "must be valid");
		KOTEK_ASSERT(p_line_direction, "must be valid (normalized)");
		KOTEK_ASSERT(p_ray_origin, "must be valid");
		KOTEK_ASSERT(p_ray_direction, "must be valid (normalized)");

		float line_param = 0.0f;
		float ray_param = 0.0f;

		zircon_gizmo_closest_params_line_ray(p_line_point,
			p_line_direction, p_ray_origin, p_ray_direction, &line_param,
			&ray_param);

		return line_param;
	}

	float zircon_render_graph_pass_editor_gizmo_own_bgfx::
		closest_distance_ray_to_point(const float* p_ray_origin,
			const float* p_ray_direction, const float* p_point) noexcept
	{
		KOTEK_ASSERT(p_ray_origin, "must be valid");
		KOTEK_ASSERT(p_ray_direction, "must be valid (normalized)");
		KOTEK_ASSERT(p_point, "must be valid");

		const float to_point[3] = {p_point[0] - p_ray_origin[0],
			p_point[1] - p_ray_origin[1],
			p_point[2] - p_ray_origin[2]};

		float ray_param = zircon_gizmo_dot(to_point, p_ray_direction);

		if (ray_param < 0.0f)
			ray_param = 0.0f;

		const float closest[3] = {
			p_ray_origin[0] + p_ray_direction[0] * ray_param,
			p_ray_origin[1] + p_ray_direction[1] * ray_param,
			p_ray_origin[2] + p_ray_direction[2] * ray_param};

		const float delta[3] = {p_point[0] - closest[0],
			p_point[1] - closest[1], p_point[2] - closest[2]};

		return zircon_gizmo_length(delta);
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::
		quat_from_axis_angle(const float* p_axis, float angle_radians,
			float* p_out_quat) noexcept
	{
		KOTEK_ASSERT(p_axis, "must be valid (normalized)");
		KOTEK_ASSERT(p_out_quat, "must be valid storage");

		const float half_angle = angle_radians * 0.5f;
		const float sine = std::sin(half_angle);

		p_out_quat[0] = p_axis[0] * sine;
		p_out_quat[1] = p_axis[1] * sine;
		p_out_quat[2] = p_axis[2] * sine;
		p_out_quat[3] = std::cos(half_angle);
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::quat_multiply(
		const float* p_p, const float* p_q, float* p_out) noexcept
	{
		KOTEK_ASSERT(p_p, "must be valid");
		KOTEK_ASSERT(p_q, "must be valid");
		KOTEK_ASSERT(p_out, "must be valid storage");

		// Hamilton product, x,y,z,w storage
		const float x = p_p[3] * p_q[0] + p_p[0] * p_q[3] +
			p_p[1] * p_q[2] - p_p[2] * p_q[1];
		const float y = p_p[3] * p_q[1] - p_p[0] * p_q[2] +
			p_p[1] * p_q[3] + p_p[2] * p_q[0];
		const float z = p_p[3] * p_q[2] + p_p[0] * p_q[1] -
			p_p[1] * p_q[0] + p_p[2] * p_q[3];
		const float w = p_p[3] * p_q[3] - p_p[0] * p_q[0] -
			p_p[1] * p_q[1] - p_p[2] * p_q[2];

		p_out[0] = x;
		p_out[1] = y;
		p_out[2] = z;
		p_out[3] = w;
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::quat_normalize(
		float* p_quat) noexcept
	{
		KOTEK_ASSERT(p_quat, "must be valid");

		const float length =
			std::sqrt(p_quat[0] * p_quat[0] + p_quat[1] * p_quat[1] +
				p_quat[2] * p_quat[2] + p_quat[3] * p_quat[3]);

		if (length > 1e-8f)
		{
			p_quat[0] /= length;
			p_quat[1] /= length;
			p_quat[2] /= length;
			p_quat[3] /= length;
		}
	}

	int zircon_render_graph_pass_editor_gizmo_own_bgfx::pick_handle(
		const float* p_ray_origin, const float* p_ray_direction,
		eZirconRenderPassGizmoMode mode, const float* p_gizmo_origin,
		float gizmo_scale, float* p_out_ray_length) noexcept
	{
		KOTEK_ASSERT(p_ray_origin, "must be valid");
		KOTEK_ASSERT(p_ray_direction, "must be valid (normalized)");
		KOTEK_ASSERT(p_gizmo_origin, "must be valid");
		KOTEK_ASSERT(gizmo_scale > 0.0f, "must be positive");

		if (p_ray_origin == nullptr || p_ray_direction == nullptr ||
			p_gizmo_origin == nullptr || gizmo_scale <= 0.0f)
		{
			return -1;
		}

		// class priority center > plane > axis: track the nearest hit of
		// each class and take the best class that hit anything
		float best_lengths[3] = {-1.0f, -1.0f, -1.0f};
		int best_handles[3] = {-1, -1, -1};

		for (kotek::uint8_t handle_index = 0; handle_index < kHandleCount;
			 ++handle_index)
		{
			const zircon_render_pass_gizmo_handle_t& handle =
				_kGizmoHandles[handle_index];

			if (handle.m_mode != mode)
				continue;

			float ray_length = 0.0f;

			if (handle.m_pfn_intersect(p_ray_origin, p_ray_direction,
					p_gizmo_origin, gizmo_scale, handle.m_axis_a,
					handle.m_axis_b, &ray_length) == false)
			{
				continue;
			}

			const kotek::uint8_t class_index =
				static_cast<kotek::uint8_t>(handle.m_class);

			if (best_handles[class_index] == -1 ||
				ray_length < best_lengths[class_index])
			{
				best_lengths[class_index] = ray_length;
				best_handles[class_index] = handle_index;
			}
		}

		for (kotek::uint8_t class_index = 0; class_index < 3;
			 ++class_index)
		{
			if (best_handles[class_index] != -1)
			{
				if (p_out_ray_length)
					*p_out_ray_length = best_lengths[class_index];

				return best_handles[class_index];
			}
		}

		return -1;
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::begin_drag(
		zircon_render_pass_gizmo_drag_context_t& context,
		const zircon_render_pass_gizmo_handle_t* p_handle,
		const float* p_ray_origin, const float* p_ray_direction,
		const float* p_camera_position, const float* p_gizmo_origin,
		float gizmo_scale, const float* p_start_position,
		const float* p_start_scale,
		const float* p_start_rotation_quat) noexcept
	{
		KOTEK_ASSERT(p_handle, "must be valid");
		KOTEK_ASSERT(p_ray_origin, "must be valid");
		KOTEK_ASSERT(p_ray_direction, "must be valid (normalized)");
		KOTEK_ASSERT(p_camera_position, "must be valid");
		KOTEK_ASSERT(p_gizmo_origin, "must be valid");
		KOTEK_ASSERT(p_start_position, "must be valid");
		KOTEK_ASSERT(p_start_scale, "must be valid");
		KOTEK_ASSERT(p_start_rotation_quat, "must be valid");

		context = zircon_render_pass_gizmo_drag_context_t{};

		context.m_p_handle = p_handle;
		context.m_gizmo_scale = gizmo_scale;
		context.m_is_active = true;

		for (int component = 0; component < 3; ++component)
		{
			context.m_ray_origin_start[component] = p_ray_origin[component];
			context.m_ray_direction_start[component] =
				p_ray_direction[component];
			context.m_gizmo_origin[component] = p_gizmo_origin[component];
			context.m_start_position[component] =
				p_start_position[component];
			context.m_start_scale[component] = p_start_scale[component];
			context.m_result_position[component] =
				p_start_position[component];
			context.m_result_scale[component] = p_start_scale[component];
		}

		for (int component = 0; component < 4; ++component)
		{
			context.m_start_rotation[component] =
				p_start_rotation_quat[component];
			context.m_result_rotation[component] =
				p_start_rotation_quat[component];
		}

		// the drag plane normal per handle kind: plane quads use their
		// plane's normal, the center handles face the camera, the rings
		// rotate around their axis; the pure-axis handles keep their
		// axis here for a uniform apply-side read
		if (p_handle->m_mesh_section ==
			eZirconRenderPassGizmoMeshSection::kQuad)
		{
			float axis_a[3];
			float axis_b[3];

			zircon_gizmo_axis_vector(p_handle->m_axis_a, axis_a);
			zircon_gizmo_axis_vector(p_handle->m_axis_b, axis_b);

			zircon_gizmo_cross(
				axis_a, axis_b, context.m_drag_plane_normal);
		}
		else if (p_handle->m_axis_a == zircon_kGizmoAxisNone)
		{
			// center handles: the camera-facing direction at drag start
			context.m_drag_plane_normal[0] =
				p_camera_position[0] - p_gizmo_origin[0];
			context.m_drag_plane_normal[1] =
				p_camera_position[1] - p_gizmo_origin[1];
			context.m_drag_plane_normal[2] =
				p_camera_position[2] - p_gizmo_origin[2];

			zircon_gizmo_normalize(context.m_drag_plane_normal);
		}
		else
		{
			zircon_gizmo_axis_vector(
				p_handle->m_axis_a, context.m_drag_plane_normal);
		}
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::apply_drag(
		zircon_render_pass_gizmo_drag_context_t& context,
		const float* p_ray_origin, const float* p_ray_direction,
		bool is_snap_enabled) noexcept
	{
		KOTEK_ASSERT(p_ray_origin, "must be valid");
		KOTEK_ASSERT(p_ray_direction, "must be valid (normalized)");

		if (context.m_is_active == false || context.m_p_handle == nullptr)
			return;

		context.m_p_handle->m_pfn_apply(
			context, p_ray_origin, p_ray_direction, is_snap_enabled);
	}
} // namespace no_streaming

namespace no_streaming
{
	void zircon_render_graph_pass_editor_gizmo_own_bgfx::
		build_model_along_axis(kotek::uint8_t axis_a,
			kotek::uint8_t axis_b, const float* p_gizmo_origin,
			float gizmo_scale, float* p_out_model) noexcept
	{
		KOTEK_ASSERT(axis_a < 3, "the along-axis builders need an axis");
		KOTEK_ASSERT(p_gizmo_origin, "must be valid");
		KOTEK_ASSERT(p_out_model, "must be valid storage");

		// the base meshes extend along +X (arrow, scale shaft+cube):
		// column 0 of the model is the handle axis, columns 1/2 any
		// orthonormal complement (the shapes are radially symmetric)
		float axis[3];
		zircon_gizmo_axis_vector(axis_a, axis);

		const float reference[3] = {axis_a == 1 ? 1.0f : 0.0f,
			axis_a == 1 ? 0.0f : 1.0f, 0.0f};

		float second[3];
		zircon_gizmo_cross(axis, reference, second);
		zircon_gizmo_normalize(second);

		float third[3];
		zircon_gizmo_cross(axis, second, third);

		for (int component = 0; component < 3; ++component)
		{
			p_out_model[component] = axis[component] * gizmo_scale;
			p_out_model[4 + component] = second[component] * gizmo_scale;
			p_out_model[8 + component] = third[component] * gizmo_scale;
			p_out_model[12 + component] = p_gizmo_origin[component];
		}

		p_out_model[3] = 0.0f;
		p_out_model[7] = 0.0f;
		p_out_model[11] = 0.0f;
		p_out_model[15] = 1.0f;
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::build_model_ring(
		kotek::uint8_t axis_a, kotek::uint8_t axis_b,
		const float* p_gizmo_origin, float gizmo_scale,
		float* p_out_model) noexcept
	{
		KOTEK_ASSERT(axis_a < 3, "the ring builder needs an axis");
		KOTEK_ASSERT(p_gizmo_origin, "must be valid");
		KOTEK_ASSERT(p_out_model, "must be valid storage");

		// the base torus lies in the XY plane (normal +Z): column 2 is
		// the ring axis, columns 0/1 any orthonormal complement (the
		// torus is radially symmetric)
		float axis[3];
		zircon_gizmo_axis_vector(axis_a, axis);

		const float reference[3] = {axis_a == 1 ? 1.0f : 0.0f,
			axis_a == 1 ? 0.0f : 1.0f, 0.0f};

		float second[3];
		zircon_gizmo_cross(axis, reference, second);
		zircon_gizmo_normalize(second);

		float first[3];
		zircon_gizmo_cross(second, axis, first);

		for (int component = 0; component < 3; ++component)
		{
			p_out_model[component] = first[component] * gizmo_scale;
			p_out_model[4 + component] = second[component] * gizmo_scale;
			p_out_model[8 + component] = axis[component] * gizmo_scale;
			p_out_model[12 + component] = p_gizmo_origin[component];
		}

		p_out_model[3] = 0.0f;
		p_out_model[7] = 0.0f;
		p_out_model[11] = 0.0f;
		p_out_model[15] = 1.0f;
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::
		build_model_center(kotek::uint8_t axis_a, kotek::uint8_t axis_b,
			const float* p_gizmo_origin, float gizmo_scale,
			float* p_out_model) noexcept
	{
		KOTEK_ASSERT(p_gizmo_origin, "must be valid");
		KOTEK_ASSERT(p_out_model, "must be valid storage");

		for (int element = 0; element < 16; ++element)
			p_out_model[element] = 0.0f;

		p_out_model[0] = gizmo_scale;
		p_out_model[5] = gizmo_scale;
		p_out_model[10] = gizmo_scale;
		p_out_model[12] = p_gizmo_origin[0];
		p_out_model[13] = p_gizmo_origin[1];
		p_out_model[14] = p_gizmo_origin[2];
		p_out_model[15] = 1.0f;
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::build_model_quad(
		kotek::uint8_t axis_a, kotek::uint8_t axis_b,
		const float* p_gizmo_origin, float gizmo_scale,
		float* p_out_model) noexcept
	{
		KOTEK_ASSERT(axis_a < 3 && axis_b < 3,
			"the quad builder needs two axes");
		KOTEK_ASSERT(p_gizmo_origin, "must be valid");
		KOTEK_ASSERT(p_out_model, "must be valid storage");

		// the base quad lies in the XY plane: column 0/1 are the spanned
		// axes, column 2 their normal
		float axis_a_vector[3];
		float axis_b_vector[3];

		zircon_gizmo_axis_vector(axis_a, axis_a_vector);
		zircon_gizmo_axis_vector(axis_b, axis_b_vector);

		float normal[3];
		zircon_gizmo_cross(axis_a_vector, axis_b_vector, normal);

		for (int component = 0; component < 3; ++component)
		{
			p_out_model[component] = axis_a_vector[component] * gizmo_scale;
			p_out_model[4 + component] =
				axis_b_vector[component] * gizmo_scale;
			p_out_model[8 + component] = normal[component] * gizmo_scale;
			p_out_model[12 + component] = p_gizmo_origin[component];
		}

		p_out_model[3] = 0.0f;
		p_out_model[7] = 0.0f;
		p_out_model[11] = 0.0f;
		p_out_model[15] = 1.0f;
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::intersect_axis(
		const float* p_ray_origin, const float* p_ray_direction,
		const float* p_gizmo_origin, float gizmo_scale,
		kotek::uint8_t axis_a, kotek::uint8_t axis_b,
		float* p_out_ray_length) noexcept
	{
		KOTEK_ASSERT(axis_a < 3, "the axis pick needs an axis");

		float axis[3];
		zircon_gizmo_axis_vector(axis_a, axis);

		bool is_hit = false;
		float best_ray_length = 3.402823466e+38f;

		// the shaft: a capped cylinder around [G, G + axis * scale] —
		// closest approach of the ray to the axis line, clamped into the
		// segment
		float line_param = 0.0f;
		float ray_param = 0.0f;

		zircon_gizmo_closest_params_line_ray(p_gizmo_origin, axis,
			p_ray_origin, p_ray_direction, &line_param, &ray_param);

		if (line_param < 0.0f)
			line_param = 0.0f;
		if (line_param > gizmo_scale)
			line_param = gizmo_scale;

		const float on_axis[3] = {p_gizmo_origin[0] + axis[0] * line_param,
			p_gizmo_origin[1] + axis[1] * line_param,
			p_gizmo_origin[2] + axis[2] * line_param};

		// the ray parameter of the ray point closest to the clamped
		// axis point
		const float to_axis[3] = {on_axis[0] - p_ray_origin[0],
			on_axis[1] - p_ray_origin[1], on_axis[2] - p_ray_origin[2]};

		ray_param = zircon_gizmo_dot(to_axis, p_ray_direction);

		if (ray_param > 0.0f)
		{
			const float on_ray[3] = {
				p_ray_origin[0] + p_ray_direction[0] * ray_param,
				p_ray_origin[1] + p_ray_direction[1] * ray_param,
				p_ray_origin[2] + p_ray_direction[2] * ray_param};

			const float delta[3] = {on_ray[0] - on_axis[0],
				on_ray[1] - on_axis[1], on_ray[2] - on_axis[2]};

			if (zircon_gizmo_length(delta) <=
				zircon_DEF_RENDER_PASS_GIZMO_PICK_AXIS_RADIUS *
					gizmo_scale)
			{
				is_hit = true;
				best_ray_length = ray_param;
			}
		}

		// the tip (arrow cone / scale cube): a pick sphere at the unit
		// length mark
		const float tip_center[3] = {
			p_gizmo_origin[0] + axis[0] * gizmo_scale,
			p_gizmo_origin[1] + axis[1] * gizmo_scale,
			p_gizmo_origin[2] + axis[2] * gizmo_scale};

		float tip_ray_length = 0.0f;

		if (intersect_ray_sphere(p_ray_origin, p_ray_direction, tip_center,
				zircon_DEF_RENDER_PASS_GIZMO_PICK_TIP_RADIUS * gizmo_scale,
				&tip_ray_length))
		{
			if (tip_ray_length < best_ray_length)
			{
				best_ray_length = tip_ray_length;
			}

			is_hit = true;
		}

		if (is_hit)
			*p_out_ray_length = best_ray_length;

		return is_hit;
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::intersect_ring(
		const float* p_ray_origin, const float* p_ray_direction,
		const float* p_gizmo_origin, float gizmo_scale,
		kotek::uint8_t axis_a, kotek::uint8_t axis_b,
		float* p_out_ray_length) noexcept
	{
		KOTEK_ASSERT(axis_a < 3, "the ring pick needs an axis");

		float axis[3];
		zircon_gizmo_axis_vector(axis_a, axis);

		float ray_length = 0.0f;

		if (intersect_ray_plane(p_ray_origin, p_ray_direction,
				p_gizmo_origin, axis, &ray_length) == false)
		{
			return false;
		}

		const float hit[3] = {
			p_ray_origin[0] + p_ray_direction[0] * ray_length,
			p_ray_origin[1] + p_ray_direction[1] * ray_length,
			p_ray_origin[2] + p_ray_direction[2] * ray_length};

		const float delta[3] = {hit[0] - p_gizmo_origin[0],
			hit[1] - p_gizmo_origin[1], hit[2] - p_gizmo_origin[2]};

		// the radial band around the unit-radius centerline
		const float radial = zircon_gizmo_length(delta);

		if (std::fabs(radial - gizmo_scale) >
			zircon_DEF_RENDER_PASS_GIZMO_PICK_RING_HALF_WIDTH * gizmo_scale)
		{
			return false;
		}

		*p_out_ray_length = ray_length;

		return true;
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::intersect_quad(
		const float* p_ray_origin, const float* p_ray_direction,
		const float* p_gizmo_origin, float gizmo_scale,
		kotek::uint8_t axis_a, kotek::uint8_t axis_b,
		float* p_out_ray_length) noexcept
	{
		KOTEK_ASSERT(axis_a < 3 && axis_b < 3,
			"the quad pick needs two axes");

		float axis_a_vector[3];
		float axis_b_vector[3];

		zircon_gizmo_axis_vector(axis_a, axis_a_vector);
		zircon_gizmo_axis_vector(axis_b, axis_b_vector);

		float normal[3];
		zircon_gizmo_cross(axis_a_vector, axis_b_vector, normal);

		float ray_length = 0.0f;

		if (intersect_ray_plane(p_ray_origin, p_ray_direction,
				p_gizmo_origin, normal, &ray_length) == false)
		{
			return false;
		}

		const float hit[3] = {
			p_ray_origin[0] + p_ray_direction[0] * ray_length,
			p_ray_origin[1] + p_ray_direction[1] * ray_length,
			p_ray_origin[2] + p_ray_direction[2] * ray_length};

		const float delta[3] = {hit[0] - p_gizmo_origin[0],
			hit[1] - p_gizmo_origin[1], hit[2] - p_gizmo_origin[2]};

		// local quad coordinates in unit gizmo space
		const float local_a =
			zircon_gizmo_dot(delta, axis_a_vector) / gizmo_scale;
		const float local_b =
			zircon_gizmo_dot(delta, axis_b_vector) / gizmo_scale;

		constexpr float _kMin =
			zircon_DEF_RENDER_PASS_GIZMO_QUAD_MIN -
			zircon_DEF_RENDER_PASS_GIZMO_PICK_QUAD_PADDING;
		constexpr float _kMax =
			zircon_DEF_RENDER_PASS_GIZMO_QUAD_MAX +
			zircon_DEF_RENDER_PASS_GIZMO_PICK_QUAD_PADDING;

		if (local_a < _kMin || local_a > _kMax || local_b < _kMin ||
			local_b > _kMax)
		{
			return false;
		}

		*p_out_ray_length = ray_length;

		return true;
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::intersect_center(
		const float* p_ray_origin, const float* p_ray_direction,
		const float* p_gizmo_origin, float gizmo_scale,
		kotek::uint8_t axis_a, kotek::uint8_t axis_b,
		float* p_out_ray_length) noexcept
	{
		return intersect_ray_sphere(p_ray_origin, p_ray_direction,
			p_gizmo_origin,
			zircon_DEF_RENDER_PASS_GIZMO_PICK_CENTER_RADIUS * gizmo_scale,
			p_out_ray_length);
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::
		apply_translate_axis(
			zircon_render_pass_gizmo_drag_context_t& context,
			const float* p_ray_origin, const float* p_ray_direction,
			bool is_snap_enabled) noexcept
	{
		// begin_drag stored the handle axis in the plane-normal slot for
		// the pure-axis handles
		const float* p_axis = context.m_drag_plane_normal;

		const float param_now = closest_param_line_to_ray(
			context.m_gizmo_origin, p_axis, p_ray_origin,
			p_ray_direction);

		const float param_start = closest_param_line_to_ray(
			context.m_gizmo_origin, p_axis, context.m_ray_origin_start,
			context.m_ray_direction_start);

		float delta = param_now - param_start;

		if (is_snap_enabled)
		{
			delta = snap_value(
				delta, zircon_DEF_RENDER_PASS_GIZMO_SNAP_TRANSLATE_STEP);
		}

		for (int component = 0; component < 3; ++component)
		{
			context.m_delta[component] = p_axis[component] * delta;
			context.m_result_position[component] =
				context.m_start_position[component] +
				p_axis[component] * delta;
		}
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::
		apply_translate_plane(
			zircon_render_pass_gizmo_drag_context_t& context,
			const float* p_ray_origin, const float* p_ray_direction,
			bool is_snap_enabled) noexcept
	{
		const float* p_normal = context.m_drag_plane_normal;

		float ray_length_now = 0.0f;
		float ray_length_start = 0.0f;

		// a nearly plane-parallel ray keeps the last result (the drag
		// resumes when the ray leaves the grazing angle)
		if (intersect_ray_plane(p_ray_origin, p_ray_direction,
				context.m_gizmo_origin, p_normal,
				&ray_length_now) == false ||
			intersect_ray_plane(context.m_ray_origin_start,
				context.m_ray_direction_start, context.m_gizmo_origin,
				p_normal, &ray_length_start) == false)
		{
			return;
		}

		for (int component = 0; component < 3; ++component)
		{
			const float hit_now =
				p_ray_origin[component] +
				p_ray_direction[component] * ray_length_now;
			const float hit_start = context.m_ray_origin_start[component] +
				context.m_ray_direction_start[component] *
					ray_length_start;

			float delta = hit_now - hit_start;

			if (is_snap_enabled)
			{
				delta = snap_value(delta,
					zircon_DEF_RENDER_PASS_GIZMO_SNAP_TRANSLATE_STEP);
			}

			context.m_delta[component] = delta;
			context.m_result_position[component] =
				context.m_start_position[component] + delta;
		}
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::
		apply_rotate_ring(
			zircon_render_pass_gizmo_drag_context_t& context,
			const float* p_ray_origin, const float* p_ray_direction,
			bool is_snap_enabled) noexcept
	{
		// begin_drag stored the ring axis in the plane-normal slot
		const float* p_axis = context.m_drag_plane_normal;

		float ray_length_now = 0.0f;
		float ray_length_start = 0.0f;

		if (intersect_ray_plane(p_ray_origin, p_ray_direction,
				context.m_gizmo_origin, p_axis,
				&ray_length_now) == false ||
			intersect_ray_plane(context.m_ray_origin_start,
				context.m_ray_direction_start, context.m_gizmo_origin,
				p_axis, &ray_length_start) == false)
		{
			return;
		}

		float vector_start[3];
		float vector_now[3];

		for (int component = 0; component < 3; ++component)
		{
			vector_start[component] =
				context.m_ray_origin_start[component] +
				context.m_ray_direction_start[component] *
					ray_length_start -
				context.m_gizmo_origin[component];
			vector_now[component] = p_ray_origin[component] +
				p_ray_direction[component] * ray_length_now -
				context.m_gizmo_origin[component];
		}

		if (zircon_gizmo_length(vector_start) < 1e-6f ||
			zircon_gizmo_length(vector_now) < 1e-6f)
		{
			return;
		}

		float crossed[3];
		zircon_gizmo_cross(vector_start, vector_now, crossed);

		// the signed angle start -> now around the ring axis
		float angle_degrees = std::atan2(
								  zircon_gizmo_dot(crossed, p_axis),
								  zircon_gizmo_dot(
									  vector_start, vector_now)) *
			(180.0f / _kPi);

		if (is_snap_enabled)
		{
			angle_degrees = snap_value(angle_degrees,
				zircon_DEF_RENDER_PASS_GIZMO_SNAP_ROTATE_STEP_DEGREES);
		}

		float rotation_delta[4];

		quat_from_axis_angle(
			p_axis, angle_degrees * (_kPi / 180.0f), rotation_delta);

		// world-axis rotation composes from the left: the drag rotation
		// applies AFTER the start rotation
		quat_multiply(rotation_delta, context.m_start_rotation,
			context.m_result_rotation);
		quat_normalize(context.m_result_rotation);

		for (int component = 0; component < 3; ++component)
			context.m_delta[component] = 0.0f;

		context.m_delta[context.m_p_handle->m_axis_a] = angle_degrees;
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::
		apply_scale_axis(
			zircon_render_pass_gizmo_drag_context_t& context,
			const float* p_ray_origin, const float* p_ray_direction,
			bool is_snap_enabled) noexcept
	{
		const float* p_axis = context.m_drag_plane_normal;

		const float param_now = closest_param_line_to_ray(
			context.m_gizmo_origin, p_axis, p_ray_origin,
			p_ray_direction);

		const float param_start = closest_param_line_to_ray(
			context.m_gizmo_origin, p_axis, context.m_ray_origin_start,
			context.m_ray_direction_start);

		float delta = param_now - param_start;

		if (is_snap_enabled)
		{
			delta = snap_value(
				delta, zircon_DEF_RENDER_PASS_GIZMO_SNAP_SCALE_STEP);
		}

		const kotek::uint8_t axis = context.m_p_handle->m_axis_a;

		for (int component = 0; component < 3; ++component)
		{
			context.m_delta[component] = 0.0f;
			context.m_result_scale[component] =
				context.m_start_scale[component];
		}

		context.m_delta[axis] = delta;
		context.m_result_scale[axis] =
			context.m_start_scale[axis] + delta;

		if (context.m_result_scale[axis] <
			zircon_DEF_RENDER_PASS_GIZMO_SCALE_MIN)
		{
			context.m_result_scale[axis] =
				zircon_DEF_RENDER_PASS_GIZMO_SCALE_MIN;
		}
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::
		apply_scale_center(
			zircon_render_pass_gizmo_drag_context_t& context,
			const float* p_ray_origin, const float* p_ray_direction,
			bool is_snap_enabled) noexcept
	{
		// the uniform scale ratio is the ray-to-gizmo distance change:
		// dragging the view away from the center scales up, toward it
		// scales down
		const float distance_start = closest_distance_ray_to_point(
			context.m_ray_origin_start, context.m_ray_direction_start,
			context.m_gizmo_origin);

		if (distance_start < 1e-6f)
			return;

		const float distance_now = closest_distance_ray_to_point(
			p_ray_origin, p_ray_direction, context.m_gizmo_origin);

		const float ratio = distance_now / distance_start;

		for (int component = 0; component < 3; ++component)
		{
			float delta =
				context.m_start_scale[component] * (ratio - 1.0f);

			if (is_snap_enabled)
			{
				delta = snap_value(
					delta, zircon_DEF_RENDER_PASS_GIZMO_SNAP_SCALE_STEP);
			}

			context.m_delta[component] = delta;
			context.m_result_scale[component] =
				context.m_start_scale[component] + delta;

			if (context.m_result_scale[component] <
				zircon_DEF_RENDER_PASS_GIZMO_SCALE_MIN)
			{
				context.m_result_scale[component] =
					zircon_DEF_RENDER_PASS_GIZMO_SCALE_MIN;
			}
		}
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::pick_entity(
		zircon_factory* p_factory, zircon_ecs_context_t* p_context,
		kotek::uint32_t entity_count_max_limit,
		const float* p_ray_origin, const float* p_ray_direction,
		kotek::entity_t* p_out_entity) noexcept
	{
		KOTEK_ASSERT(p_factory, "must be valid");
		KOTEK_ASSERT(p_context, "must be valid");
		KOTEK_ASSERT(p_ray_origin, "must be valid");
		KOTEK_ASSERT(p_ray_direction, "must be valid (normalized)");
		KOTEK_ASSERT(p_out_entity, "must be valid storage");

		if (p_factory == nullptr || p_context == nullptr ||
			p_ray_origin == nullptr || p_ray_direction == nullptr ||
			p_out_entity == nullptr)
		{
			return false;
		}

		kotek::entity_t entity_ids
			[zircon_DEF_RENDER_PASS_EDITOR_GIZMO_OWN_MAX_ENTITY_SCAN_COUNT];

		kotek::uint32_t entity_count = p_factory->get_all_entities(
			p_context, entity_count_max_limit, entity_ids,
			zircon_DEF_RENDER_PASS_EDITOR_GIZMO_OWN_MAX_ENTITY_SCAN_COUNT);

		float best_ray_length = 3.402823466e+38f;
		bool is_found = false;
		kotek::entity_t best_entity{kotek::ktk::kInvalidECSEntity};

		for (kotek::uint32_t entity_index = 0;
			 entity_index < entity_count; ++entity_index)
		{
			const kotek::entity_t& entity = entity_ids[entity_index];

			if (p_factory->has_component(p_context, entity,
					eZirconComponentType::kzircon_component_transform) ==
				false)
			{
				continue;
			}

			// the editor's own camera entity is not a pickable scene
			// object (grabbing it would move the view the gizmo renders
			// through)
			if (p_factory->has_component(p_context, entity,
					eZirconComponentType::kzircon_component_sdk_camera))
			{
				continue;
			}

			zircon_component_transform* p_transform =
				static_cast<zircon_component_transform*>(
					p_factory->get_component_by_enum(p_context, entity,
						eZirconComponentType::kzircon_component_transform));

			if (p_transform == nullptr)
				continue;

			const kotek::math::vec3f_t& position =
				p_transform->get_position();

			float sphere_center[3] = {
				position.x(), position.y(), position.z()};
			float sphere_radius =
				zircon_DEF_RENDER_PASS_GIZMO_SELECT_SPHERE_RADIUS;

			if (p_factory->has_component(p_context, entity,
					eZirconComponentType::
						kzircon_component_bounding_sphere))
			{
				zircon_component_bounding_sphere* p_bounding_sphere =
					static_cast<zircon_component_bounding_sphere*>(
						p_factory->get_component_by_enum(p_context, entity,
							eZirconComponentType::
								kzircon_component_bounding_sphere));

				if (p_bounding_sphere &&
					p_bounding_sphere->is_enabled())
				{
					const kotek::math::vec3f_t& center =
						p_bounding_sphere->get_center();
					const kotek::math::vec3f_t& scale =
						p_transform->get_scale();

					sphere_center[0] += center.x();
					sphere_center[1] += center.y();
					sphere_center[2] += center.z();

					// the sphere scales with the largest axis (the
					// conservative uniform fit)
					float max_scale = std::fabs(scale.x());
					max_scale = max_scale > std::fabs(scale.y())
						? max_scale
						: std::fabs(scale.y());
					max_scale = max_scale > std::fabs(scale.z())
						? max_scale
						: std::fabs(scale.z());

					sphere_radius =
						p_bounding_sphere->get_radius() * max_scale;
				}
			}

			float ray_length = 0.0f;

			if (intersect_ray_sphere(p_ray_origin, p_ray_direction,
					sphere_center, sphere_radius, &ray_length) &&
				ray_length < best_ray_length)
			{
				best_ray_length = ray_length;
				best_entity = entity;
				is_found = true;
			}
		}

		if (is_found)
			*p_out_entity = best_entity;

		return is_found;
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::commit_drag_edit(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory,
		zircon_editor_command_history* p_history,
		zircon_ecs_context_t* p_context, kotek::entity_t entity,
		const float* p_start_position, const float* p_start_scale,
		const float* p_start_rotation_quat) noexcept
	{
		// the implementation moved to the shared
		// zircon_gizmo_commit_transform_drag_edit (task Z3 P2f — the
		// ImGuizmo variant's hosting window issues the identical commit);
		// this static stays as the own pass's surface so the Z6/P2e tests
		// keep pinning it
		return zircon_gizmo_commit_transform_drag_edit(
			p_manager_session_editor, p_factory, p_history, p_context,
			entity, p_start_position, p_start_scale,
			p_start_rotation_quat);
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::cancel_drag_edit(
		zircon_factory* p_factory, zircon_ecs_context_t* p_context,
		kotek::entity_t entity, const float* p_start_position,
		const float* p_start_scale,
		const float* p_start_rotation_quat) noexcept
	{
		KOTEK_ASSERT(p_factory, "must be valid");
		KOTEK_ASSERT(p_context, "must be valid");
		KOTEK_ASSERT(p_start_position, "must be valid");
		KOTEK_ASSERT(p_start_scale, "must be valid");
		KOTEK_ASSERT(p_start_rotation_quat, "must be valid");

		if (p_factory == nullptr || p_context == nullptr ||
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

		// the whole cancel: put the drag-START capture back — unlike
		// commit_drag_edit nothing is serialized and no command is
		// placement-new'd, so the journal never sees the aborted drag
		p_transform->set_position(kotek::math::vec3f_t(p_start_position[0],
			p_start_position[1], p_start_position[2]));
		p_transform->set_scale(kotek::math::vec3f_t(p_start_scale[0],
			p_start_scale[1], p_start_scale[2]));
		p_transform->set_rotation(
			kotek::math::quatf_t(p_start_rotation_quat[0],
				p_start_rotation_quat[1], p_start_rotation_quat[2],
				p_start_rotation_quat[3]));

		return true;
	}
} // namespace no_streaming

namespace no_streaming
{
	zircon_render_graph_pass_editor_gizmo_own_bgfx::frame_context_t
	zircon_render_graph_pass_editor_gizmo_own_bgfx::resolve_frame_context(
		void) noexcept
	{
		frame_context_t result{};

		result.m_selected_entity =
			kotek::entity_t{kotek::ktk::kInvalidECSEntity};

		if (this->m_p_manager_session_editor)
		{
			// the same lookup the grid/imgui passes do: the current id
			// is set at session creation, long before the graph's first
			// frame
			result.m_p_session =
				this->m_p_manager_session_editor->get_session(
					this->m_p_manager_session_editor
						->get_current_session_id());
		}

		if (result.m_p_session == nullptr)
			return result;

		result.m_p_world = result.m_p_session->get_world();

		if (result.m_p_world == nullptr ||
			result.m_p_world->is_initialized() == false)
		{
			result.m_p_world = nullptr;
			return result;
		}

		result.m_p_factory = result.m_p_world->get_factory();
		result.m_p_ecs_context = result.m_p_world->get_ecs_context();

		if (result.m_p_factory == nullptr ||
			result.m_p_ecs_context == nullptr)
		{
			return result;
		}

		zircon_editor_ui_state* p_ui_state =
			result.m_p_session->get_ui_state();

		if (p_ui_state == nullptr)
			return result;

		result.m_selected_entity = p_ui_state->get_selected_entity();

		if (result.m_p_factory->is_valid_entity(result.m_p_ecs_context,
				result.m_selected_entity) == false)
		{
			result.m_selected_entity =
				kotek::entity_t{kotek::ktk::kInvalidECSEntity};
			return result;
		}

		// a selected entity without a transform has nothing for the
		// gizmo to grab
		result.m_p_transform =
			static_cast<zircon_component_transform*>(
				result.m_p_factory->get_component_by_enum(
					result.m_p_ecs_context, result.m_selected_entity,
					eZirconComponentType::kzircon_component_transform));

		return result;
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::
		resolve_gizmo_frame(const frame_context_t& context,
			float* p_out_view, float* p_out_projection,
			float* p_out_inverse_view_projection,
			float* p_out_camera_position, float* p_out_gizmo_origin,
			float* p_out_gizmo_scale) noexcept
	{
		KOTEK_ASSERT(p_out_view, "must be valid storage");
		KOTEK_ASSERT(p_out_projection, "must be valid storage");
		KOTEK_ASSERT(p_out_inverse_view_projection,
			"must be valid storage");
		KOTEK_ASSERT(p_out_camera_position, "must be valid storage");
		KOTEK_ASSERT(p_out_gizmo_origin, "must be valid storage");
		KOTEK_ASSERT(p_out_gizmo_scale, "must be valid storage");

		if (context.m_p_transform == nullptr)
			return false;

		if (this->resolve_editor_camera(p_out_view, p_out_projection) ==
			false)
		{
			float aspect_ratio = 4.0f / 3.0f;

			if (this->m_p_manager_main &&
				this->m_p_manager_main->Get_WindowManager())
			{
				int window_width = this->m_p_manager_main
									   ->Get_WindowManager()
									   ->ActiveWindow_GetWidth();
				int window_height = this->m_p_manager_main
										->Get_WindowManager()
										->ActiveWindow_GetHeight();

				if (window_width > 0 && window_height > 0)
				{
					aspect_ratio = static_cast<float>(window_width) /
						static_cast<float>(window_height);
				}
			}

			zircon_render_graph_pass_editor_grid_bgfx::
				build_default_orbit_view_projection(p_out_view,
					p_out_projection, aspect_ratio,
					bgfx::getCaps()->homogeneousDepth);
		}

		zircon_render_graph_pass_editor_grid_bgfx::
			compose_inverse_view_projection(p_out_view, p_out_projection,
				p_out_inverse_view_projection);

		zircon_render_graph_pass_editor_grid_bgfx::compute_camera_position(
			p_out_view, p_out_camera_position);

		const kotek::math::vec3f_t& position =
			context.m_p_transform->get_position();

		p_out_gizmo_origin[0] = position.x();
		p_out_gizmo_origin[1] = position.y();
		p_out_gizmo_origin[2] = position.z();

		float viewport_height = 1.0f;

		if (this->m_p_manager_main &&
			this->m_p_manager_main->Get_WindowManager())
		{
			viewport_height = static_cast<float>(
				this->m_p_manager_main->Get_WindowManager()
					->ActiveWindow_GetHeight());
		}

		*p_out_gizmo_scale = compute_gizmo_scale(p_out_camera_position,
			p_out_gizmo_origin, p_out_projection[5], viewport_height);

		// defense in depth: user-derived matrices are validated at the
		// resolve, so a degenerate scale here means an internal bug — the
		// clamp keeps the frame alive (pick_handle's assert guards the
		// internal contract); NaN compares false and lands here too
		if ((*p_out_gizmo_scale > 1e-6f) == false)
		{
			*p_out_gizmo_scale = 1.0f;
		}

		return true;
	}

	bool zircon_render_graph_pass_editor_gizmo_own_bgfx::
		resolve_editor_camera(
			float* p_out_view, float* p_out_projection) noexcept
	{
		KOTEK_ASSERT(p_out_view, "must be valid storage");
		KOTEK_ASSERT(p_out_projection, "must be valid storage");

		if (p_out_view == nullptr || p_out_projection == nullptr)
			return false;

		zircon_session_editor* p_session = nullptr;

		if (this->m_p_manager_session_editor)
		{
			p_session = this->m_p_manager_session_editor->get_session(
				this->m_p_manager_session_editor
					->get_current_session_id());
		}

		zircon_world* p_world =
			p_session ? p_session->get_world() : nullptr;

		if (p_world == nullptr || p_world->is_initialized() == false)
			return false;

		zircon_factory* p_factory = p_world->get_factory();
		zircon_ecs_context_t* p_context = p_world->get_ecs_context();

		if (p_factory == nullptr || p_context == nullptr)
			return false;

		kotek::entity_t entity_ids
			[zircon_DEF_RENDER_PASS_EDITOR_GIZMO_OWN_MAX_ENTITY_SCAN_COUNT];

		kotek::uint32_t entity_count = p_factory->get_all_entities(
			p_context, p_world->get_entity_count_max_limit(), entity_ids,
			zircon_DEF_RENDER_PASS_EDITOR_GIZMO_OWN_MAX_ENTITY_SCAN_COUNT);

		for (kotek::uint32_t entity_index = 0;
			 entity_index < entity_count; ++entity_index)
		{
			if (p_factory->has_component(p_context,
					entity_ids[entity_index],
					eZirconComponentType::
						kzircon_component_sdk_camera) == false)
			{
				continue;
			}

			zircon_component_sdk_camera* p_sdk_camera =
				static_cast<zircon_component_sdk_camera*>(
					p_factory->get_component_by_enum(p_context,
						entity_ids[entity_index],
						eZirconComponentType::
							kzircon_component_sdk_camera));

			if (p_sdk_camera == nullptr)
				continue;

			const zircon_component_camera& camera =
				p_sdk_camera->get_camera();

			const float* p_camera_view =
				kotek::math::value_ptr(camera.get_view());
			const float* p_camera_projection =
				kotek::math::value_ptr(camera.get_projection());

			// the camera component is USER data (scene content): a
			// default-constructed or corrupt camera (zero/NaN matrices)
			// must not poison the frame — skip it and let the caller
			// fall back to the default orbit (the 2026-09-04 viewport-
			// click assert chain)
			if (zircon_render_graph_pass_editor_grid_bgfx::
					is_matrix_usable(p_camera_view) == false ||
				zircon_render_graph_pass_editor_grid_bgfx::
					is_matrix_usable(p_camera_projection) == false)
			{
				continue;
			}

			for (int element_index = 0; element_index < 16;
				 ++element_index)
			{
				p_out_view[element_index] = p_camera_view[element_index];
				p_out_projection[element_index] =
					p_camera_projection[element_index];
			}

			return true;
		}

		return false;
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::
		build_handle_meshes(void) noexcept
	{
		zircon_gizmo_vertex_t vertices[_kTotalVertexCount];
		kotek::uint16_t indices[_kTotalIndexCount];

		kotek::uint32_t vertex_cursor = 0;
		kotek::uint32_t index_cursor = 0;

		constexpr kotek::uint8_t _kSectionCount =
			static_cast<kotek::uint8_t>(
				eZirconRenderPassGizmoMeshSection::kCount);

		for (kotek::uint8_t section_index = 0;
			 section_index < _kSectionCount; ++section_index)
		{
			const eZirconRenderPassGizmoMeshSection section =
				static_cast<eZirconRenderPassGizmoMeshSection>(
					section_index);

			const kotek::uint32_t section_index_offset = index_cursor;

			switch (section)
			{
			case eZirconRenderPassGizmoMeshSection::kArrow:
			{
				zircon_gizmo_build_cylinder(vertices + vertex_cursor,
					indices + index_cursor,
					static_cast<kotek::uint16_t>(vertex_cursor),
					zircon_DEF_RENDER_PASS_GIZMO_ARROW_SHAFT_LENGTH,
					zircon_DEF_RENDER_PASS_GIZMO_ARROW_SHAFT_RADIUS);

				vertex_cursor += _kCylinderVertexCount;
				index_cursor += _kCylinderIndexCount;

				zircon_gizmo_build_cone(vertices + vertex_cursor,
					indices + index_cursor,
					static_cast<kotek::uint16_t>(vertex_cursor),
					zircon_DEF_RENDER_PASS_GIZMO_ARROW_SHAFT_LENGTH, 1.0f,
					zircon_DEF_RENDER_PASS_GIZMO_ARROW_TIP_RADIUS);

				vertex_cursor += _kConeVertexCount;
				index_cursor += _kConeIndexCount;
				break;
			}
			case eZirconRenderPassGizmoMeshSection::kScaleAxis:
			{
				zircon_gizmo_build_cylinder(vertices + vertex_cursor,
					indices + index_cursor,
					static_cast<kotek::uint16_t>(vertex_cursor), 1.0f,
					zircon_DEF_RENDER_PASS_GIZMO_ARROW_SHAFT_RADIUS);

				vertex_cursor += _kCylinderVertexCount;
				index_cursor += _kCylinderIndexCount;

				zircon_gizmo_build_cube(vertices + vertex_cursor,
					indices + index_cursor,
					static_cast<kotek::uint16_t>(vertex_cursor), 1.0f,
					zircon_DEF_RENDER_PASS_GIZMO_CENTER_HALF_SIZE);

				vertex_cursor += _kCubeVertexCount;
				index_cursor += _kCubeIndexCount;
				break;
			}
			case eZirconRenderPassGizmoMeshSection::kRing:
			{
				zircon_gizmo_build_ring(vertices + vertex_cursor,
					indices + index_cursor,
					static_cast<kotek::uint16_t>(vertex_cursor),
					zircon_DEF_RENDER_PASS_GIZMO_RING_TUBE_RADIUS);

				vertex_cursor += _kRingVertexCount;
				index_cursor += _kRingIndexCount;
				break;
			}
			case eZirconRenderPassGizmoMeshSection::kCube:
			{
				zircon_gizmo_build_cube(vertices + vertex_cursor,
					indices + index_cursor,
					static_cast<kotek::uint16_t>(vertex_cursor), 0.0f,
					zircon_DEF_RENDER_PASS_GIZMO_CENTER_HALF_SIZE);

				vertex_cursor += _kCubeVertexCount;
				index_cursor += _kCubeIndexCount;
				break;
			}
			case eZirconRenderPassGizmoMeshSection::kQuad:
			{
				zircon_gizmo_vertex_t* p_quad_vertices =
					vertices + vertex_cursor;

				p_quad_vertices[0].m_position[0] =
					zircon_DEF_RENDER_PASS_GIZMO_QUAD_MIN;
				p_quad_vertices[0].m_position[1] =
					zircon_DEF_RENDER_PASS_GIZMO_QUAD_MIN;
				p_quad_vertices[0].m_position[2] = 0.0f;

				p_quad_vertices[1].m_position[0] =
					zircon_DEF_RENDER_PASS_GIZMO_QUAD_MAX;
				p_quad_vertices[1].m_position[1] =
					zircon_DEF_RENDER_PASS_GIZMO_QUAD_MIN;
				p_quad_vertices[1].m_position[2] = 0.0f;

				p_quad_vertices[2].m_position[0] =
					zircon_DEF_RENDER_PASS_GIZMO_QUAD_MAX;
				p_quad_vertices[2].m_position[1] =
					zircon_DEF_RENDER_PASS_GIZMO_QUAD_MAX;
				p_quad_vertices[2].m_position[2] = 0.0f;

				p_quad_vertices[3].m_position[0] =
					zircon_DEF_RENDER_PASS_GIZMO_QUAD_MIN;
				p_quad_vertices[3].m_position[1] =
					zircon_DEF_RENDER_PASS_GIZMO_QUAD_MAX;
				p_quad_vertices[3].m_position[2] = 0.0f;

				kotek::uint16_t* p_quad_indices = indices + index_cursor;
				const kotek::uint16_t base =
					static_cast<kotek::uint16_t>(vertex_cursor);

				p_quad_indices[0] = base + 0;
				p_quad_indices[1] = base + 1;
				p_quad_indices[2] = base + 2;
				p_quad_indices[3] = base + 0;
				p_quad_indices[4] = base + 2;
				p_quad_indices[5] = base + 3;

				vertex_cursor += _kQuadVertexCount;
				index_cursor += _kQuadIndexCount;
				break;
			}
			default:
			{
				KOTEK_ASSERT(false, "unhandled gizmo mesh section!");
				break;
			}
			}

			this->m_mesh_sections[section_index].m_index_offset =
				section_index_offset;
			this->m_mesh_sections[section_index].m_index_count =
				index_cursor - section_index_offset;
		}

		KOTEK_ASSERT(vertex_cursor == _kTotalVertexCount,
			"the mesh builder under/over-filled the vertex scratch!");
		KOTEK_ASSERT(index_cursor == _kTotalIndexCount,
			"the mesh builder under/over-filled the index scratch!");

		this->m_vertex_buffer = bgfx::createVertexBuffer(
			bgfx::copy(vertices, sizeof(vertices)), this->m_layout);
		this->m_index_buffer = bgfx::createIndexBuffer(
			bgfx::copy(indices, sizeof(indices)));

		KOTEK_ASSERT(bgfx::isValid(this->m_vertex_buffer),
			"failed to create the gizmo handle vertex buffer!");
		KOTEK_ASSERT(bgfx::isValid(this->m_index_buffer),
			"failed to create the gizmo handle index buffer!");
	}

	void zircon_render_graph_pass_editor_gizmo_own_bgfx::
		publish_overlay_state(const frame_context_t& context) noexcept
	{
		zircon_editor_ui_state* p_ui_state =
			context.m_p_session ? context.m_p_session->get_ui_state()
								: nullptr;

		if (p_ui_state == nullptr)
			return;

		zircon_gizmo_overlay_state_t& state =
			p_ui_state->get_gizmo_overlay_state();

		state.m_mode = static_cast<kotek::uint8_t>(this->m_mode);
		state.m_is_snap_enabled = this->m_is_snap_enabled;
		state.m_is_gizmo_active = context.m_p_transform != nullptr;
		state.m_is_dragging = this->m_drag.m_is_active;

		// the ESC-arbitration pair (task Z19): the arbiter's gizmo-drag
		// consumer probes m_is_drag_active and sets
		// m_cancel_drag_requested to abort the drag. The request channel
		// is meaningful only while a drag lives — a stale request (a
		// hot-reload destroyed the pass mid-drag) must never eat the
		// NEXT drag, so publish enforces the invariant at every frame
		// end
		state.m_is_drag_active = this->m_drag.m_is_active;

		if (this->m_drag.m_is_active == false)
		{
			state.m_cancel_drag_requested = false;
		}

		for (int component = 0; component < 3; ++component)
			state.m_drag_delta[component] = this->m_drag.m_delta[component];

		switch (this->m_mode)
		{
		case eZirconRenderPassGizmoMode::kTranslate:
		{
			for (int component = 0; component < 4; ++component)
				state.m_result[component] =
					component < 3
						? this->m_drag.m_result_position[component]
						: 0.0f;
			break;
		}
		case eZirconRenderPassGizmoMode::kRotate:
		{
			for (int component = 0; component < 4; ++component)
				state.m_result[component] =
					this->m_drag.m_result_rotation[component];
			break;
		}
		case eZirconRenderPassGizmoMode::kScale:
		{
			for (int component = 0; component < 4; ++component)
				state.m_result[component] =
					component < 3 ? this->m_drag.m_result_scale[component]
								  : 0.0f;
			break;
		}
		}
	}

	bgfx::ShaderHandle
	zircon_render_graph_pass_editor_gizmo_own_bgfx::load_shader_blob(
		const char* p_shader_file_name) noexcept
	{
		bgfx::ShaderHandle result = BGFX_INVALID_HANDLE;

		KOTEK_ASSERT(p_shader_file_name, "must be valid");
		KOTEK_ASSERT(this->m_p_manager_main,
			"OnCreateResources must run before loading shaders");

		if (p_shader_file_name == nullptr ||
			this->m_p_manager_main == nullptr)
		{
			return result;
		}

		kotek::core::ktkIFileSystem* p_filesystem =
			this->m_p_manager_main->GetFileSystem();

		KOTEK_ASSERT(p_filesystem, "must be valid");

		if (p_filesystem == nullptr)
			return result;

		// the backend-dialect directory of the active renderer (same
		// route as the grid/model_static passes): one blob set per
		// backend under shader_cache/bgfx/
		const char* p_dialect_directory = nullptr;

		switch (bgfx::getRendererType())
		{
		case bgfx::RendererType::Direct3D11:
		{
			p_dialect_directory = "dx11";
			break;
		}
		case bgfx::RendererType::Direct3D12:
		{
			p_dialect_directory = "dx12";
			break;
		}
		case bgfx::RendererType::OpenGLES:
		{
			p_dialect_directory = "essl";
			break;
		}
		case bgfx::RendererType::OpenGL:
		{
			p_dialect_directory = "glsl";
			break;
		}
		case bgfx::RendererType::Vulkan:
		{
			p_dialect_directory = "vulkan";
			break;
		}
		default:
		{
			break;
		}
		}

		if (p_dialect_directory == nullptr)
		{
			KOTEK_MESSAGE_WARNING(
				"[editor_gizmo_own] no shader-blob dialect directory for "
				"renderer '{}' — shader '{}' not loaded",
				bgfx::getRendererName(bgfx::getRendererType()),
				p_shader_file_name);

			return result;
		}

		kotek::static_path_t shader_path;

		p_filesystem->Make_Path(shader_path,
			kotek::core::eFolderIndex::kFolderIndex_DataUser_ShaderCache);

		shader_path /= "bgfx";
		shader_path /= p_dialect_directory;
		shader_path /= p_shader_file_name;

		// an absent blob is an expected pre-pipeline state, not an error —
		// the existence check keeps the (graceful since kotek B0)
		// missing-file read from logging its warning
		if (p_filesystem->Is_Exists(shader_path) == false)
			return result;

		kotek::uint8_t buffer
			[zircon_DEF_RENDER_PASS_EDITOR_GIZMO_OWN_SHADER_BIN_MAX_SIZE];

		kotek::uint8_t* p_buffer = buffer;
		kotek::size_t buffer_length = sizeof(buffer);

		bool read_status =
			p_filesystem->Read_File(shader_path, p_buffer, buffer_length);

		if (read_status && buffer_length)
		{
			// bgfx::copy hands bgfx its own copy, so the filesystem's
			// buffer is not referenced past this call
			result = bgfx::createShader(bgfx::copy(p_buffer,
				static_cast<uint32_t>(buffer_length)));

			KOTEK_ASSERT(bgfx::isValid(result),
				"shader blob '{}' failed to create — corrupt or "
				"wrong-dialect data?",
				p_shader_file_name);
		}
		else
		{
			KOTEK_MESSAGE_WARNING(
				"[editor_gizmo_own] failed to read shader blob '{}'",
				p_shader_file_name);
		}

		return result;
	}
} // namespace no_streaming
