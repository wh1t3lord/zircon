#pragma once

// zircon_csg_evaluate.h — the brush-CSG evaluation core (task Z25,
// phase A1). The Quake/Valve brush model: a primitive is a CONVEX set
// of planes, the solid is their intersection, and the booleans operate
// on plane sets + polygon soups (never on arbitrary triangle soups):
//
//   union:        clip each brush's face polygons against every other
//                 brush's planes, keep the strictly-outside fragments;
//   subtraction:  A-side keeps the fragments outside B, B contributes
//                 its inside-A fragments with flipped winding (the
//                 cavity walls);
//   intersection: keep only the mutually-inside fragments.
//
// The output is a welded triangle mesh (zircon_csg_mesh_t): positions
// welded on the weld-eps grid (the sort-based cluster weld below),
// flat per-triangle normals and per-triangle material ids — pure math
// on POD, NO bgfx/render/ECS dependencies (the components convert into
// zircon_csg_primitive_desc_t at the boundary; the tests drive the
// core directly).
//
// The core is templated on the scalar (the plan's precision
// configuration): zircon_csg_scalar_t is what the integration uses,
// the unit proofs instantiate float / zircon_csg_scalar_u32f_t /
// double explicitly in every build. Header-only by structural
// decision: the three instantiations coexist and only the used ones
// compile (a .cpp would hold explicit instantiations for compile time
// only — A1 has exactly two consumers, the components' seam and the
// tests).
//
// ------------------------------------------------------------------
// The keep tables (the coplanar contract — the 2026-09-20 design
// session; every case below is test-pinned):
//
// A face fragment that survives clipping (inside-or-on the other
// brush's whole plane set) carries a coplanar mark: kSame (it lies on
// one of the other brush's planes with the same outward normal),
// kOpposed (same geometric plane, flipped normal), or kNone (strictly
// inside). Per operation and side:
//
//   | op        | side  | kSame | kOpposed | kNone |
//   |-----------|-------|-------|----------|-------|
//   | union     | i vs j| i>j keeps (continues), else drop | drop | drop |
//   | intersect | A     | keep  | drop     | keep  |
//   | intersect | B     | drop  | drop     | keep  |
//   | subtract  | A     | drop  | keep     | drop  |
//   | subtract  | B(flip)| drop | drop     | keep  |
//
// Rationale: union drops the internal seam (opposed) and dedups
// coincident surfaces to the highest brush index (the N-way
// generalization of "B keeps the copy": the surviving copy still
// continues through the remaining brushes, so nested coplanar brushes
// compose correctly); subtraction keeps a touching (opposed) face
// because contact without overlap removes nothing; intersection
// requires volume overlap (opposed coplanar = touching = empty by
// policy).
//
// Epsilon policy (named defines in zircon_csg_defs.h, SCALED to the
// brush set): the evaluation computes the primitives' bounding-box
// diagonal D and uses plane_eps = max(1e-5 x D, mode floor) for the
// in-front/behind/on classification, weld_eps = max(1e-4 x D, floor)
// for the weld cluster radius, sliver_area_eps = weld_eps^2 — clipped
// fragments thinner than a weld cell are dropped and COUNTED
// (m_sliver_drop_count), never silently kept. Area tests run in double
// through traits::to_double (exact for u32f and f32) because position
// cross-products overflow the 16.16 range for large coordinates — the
// drop DECISION stays deterministic per mode (IEEE double math is
// correctly rounded).
//
// Documented A1 limitations (all deliberate, all revisited by A2/A3):
//  - kCylinder primitives are box-approximated (their AABB); kWedge is
//    the exact 5-plane ramp brush. The enum is the registration
//    contract; the evaluation switch is the approximation boundary.
//  - N-way compounds = the additive primitives unioned first, then the
//    subtractive ones applied in MEMBER ORDER (the Valve-carve
//    semantic, order-dependent by design). Cavity-wall fragments
//    landing exactly on a previously-applied subtractive brush's
//    boundary are dropped (carve-ordering corner case).
//  - intersection is a brush-PAIR boolean (the plan's tests); it is
//    not a compound semantic.
//  - the evaluation context is ~6 MB (f32) of fixed tables and pools —
//    HEAP-ALLOCATE it (the tests follow the engine fixture rule:
//    `new`/`delete`, never a stack instance).
// ------------------------------------------------------------------

#include "zircon_csg_defs.h"

#include <kotek.core.containers.vector/include/kotek_core_containers_vector.h>

// ------------------------------------------------------------------
// geometry PODs
// ------------------------------------------------------------------
template <typename Scalar>
struct zircon_csg_vec3_t
{
	Scalar m[3];
};

// a brush half-space: the solid side is dot(m_normal, p) <= m_dist
// (outward unit normals; the signed distance of p is dot(n, p) - dist)
template <typename Scalar>
struct zircon_csg_plane_t
{
	zircon_csg_vec3_t<Scalar> m_normal;
	Scalar m_dist;
};

// one face of a pristine primitive soup (indexes the brush's vertex
// ring; m_plane_index names its source plane — the material lives on
// the brush)
struct zircon_csg_face_t
{
	kotek::uint16_t m_vertex_start;
	kotek::uint8_t m_vertex_count;
	kotek::uint8_t m_plane_index;
};

// the convex brush: plane set + polygon soup
template <typename Scalar>
struct zircon_csg_brush_t
{
	kotek::static_vector_t<
		zircon_csg_plane_t<Scalar>, ZIRCON_DEF_CSG_MAX_PLANES_PER_BRUSH>
		m_planes;
	kotek::static_vector_t<
		zircon_csg_vec3_t<Scalar>, ZIRCON_DEF_CSG_MAX_VERTICES_PER_BRUSH>
		m_vertices;
	kotek::static_vector_t<
		zircon_csg_face_t, ZIRCON_DEF_CSG_MAX_FACES_PER_BRUSH>
		m_faces;
	kotek::uint16_t m_material_id;
};

// the ECS/render-agnostic primitive descriptor (the components fill
// this at the boundary; the tests build it directly)
template <typename Scalar>
struct zircon_csg_primitive_desc_t
{
	Scalar m_dimensions[3]; // full extents per axis (meters)
	Scalar m_position[3];
	Scalar m_rotation[4];   // quaternion x, y, z, w
	kotek::uint16_t m_material_id;
	kotek::uint8_t m_type;      // eZirconCsgPrimitiveType
	kotek::uint8_t m_operation; // zircon_DEF_CSG_OPERATION_*
};

// one fragment of the evaluated soup (indexes the soup's flat vertex
// array; the normal is the SOURCE PLANE's outward normal — flat
// shading is the brush's nature)
template <typename Scalar>
struct zircon_csg_polygon_t
{
	kotek::uint32_t m_vertex_start;
	kotek::uint32_t m_vertex_count;
	zircon_csg_vec3_t<Scalar> m_normal;
	kotek::uint16_t m_material_id;
	kotek::uint8_t m_spare[2];
};

template <typename Scalar>
struct zircon_csg_soup_t
{
	kotek::static_vector_t<zircon_csg_vec3_t<Scalar>,
		ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION>
		m_vertices;
	kotek::static_vector_t<zircon_csg_polygon_t<Scalar>,
		ZIRCON_DEF_CSG_MAX_POLYGONS_PER_EVALUATION>
		m_polygons;
};

// the welded output mesh (evaluation precision; the renderer/bake
// converts at its own boundary). Positions are welded on the weld-eps
// grid; indices address m_positions; normals and material ids are
// per-triangle.
template <typename Scalar>
struct zircon_csg_mesh_t
{
	kotek::static_vector_t<zircon_csg_vec3_t<Scalar>,
		ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION>
		m_positions;
	kotek::static_vector_t<kotek::uint32_t,
		ZIRCON_DEF_CSG_MAX_INDICES_PER_EVALUATION>
		m_indices;
	kotek::static_vector_t<zircon_csg_vec3_t<Scalar>,
		ZIRCON_DEF_CSG_MAX_TRIANGLES_PER_EVALUATION>
		m_normals;
	kotek::static_vector_t<kotek::uint16_t,
		ZIRCON_DEF_CSG_MAX_TRIANGLES_PER_EVALUATION>
		m_materials;
};

// a fragment in flight (indexes a piece pool's flat vertex array)
struct zircon_csg_piece_t
{
	kotek::uint32_t m_vertex_start;
	kotek::uint32_t m_vertex_count;
};

enum class eZirconCsgBoolean : kotek::uint8_t
{
	kUnion = 0,
	kSubtract,
	kIntersect
};

enum class eZirconCsgCoplanar : kotek::uint8_t
{
	kNone = 0,
	kSame,
	kOpposed
};

// ------------------------------------------------------------------
// the evaluation context: output mesh + diagnostics + the machinery
// pools. ~6 MB at f32 — HEAP-ALLOCATE ONLY (the fixture rule). Reused
// across evaluations: every entry point reset()s it first.
// ------------------------------------------------------------------
template <typename Scalar>
struct zircon_csg_evaluation_t
{
	using traits_t = zircon_csg_scalar_traits<Scalar>;

	void reset(void) noexcept
	{
		m_mesh.m_positions.clear();
		m_mesh.m_indices.clear();
		m_mesh.m_normals.clear();
		m_mesh.m_materials.clear();
		m_soup.m_vertices.clear();
		m_soup.m_polygons.clear();
		m_soup_b.m_vertices.clear();
		m_soup_b.m_polygons.clear();
		m_brushes.clear();
		m_weld_order.clear();
		m_weld_scratch.clear();
		m_weld_map.clear();
		m_pieces_a.clear();
		m_piece_vertices_a.clear();
		m_pieces_b.clear();
		m_piece_vertices_b.clear();
		m_pieces_c.clear();
		m_piece_vertices_c.clear();
		m_status = eZirconCsgEvaluationStatus::kSuccess;
		m_sliver_drop_count = 0;
		m_weld_degenerate_drop_count = 0;
		m_capacity_drop_count = 0;
		m_plane_eps = traits_t::zero();
		m_weld_eps = traits_t::zero();
		m_sliver_area_eps = 0.0;
	}

	// ---- outputs ----
	zircon_csg_mesh_t<Scalar> m_mesh;
	eZirconCsgEvaluationStatus m_status;
	// fragments dropped by the sliver policy / by post-weld degeneracy
	// / by the capacity guards (the loud-degradation counters)
	kotek::uint32_t m_sliver_drop_count;
	kotek::uint32_t m_weld_degenerate_drop_count;
	kotek::uint32_t m_capacity_drop_count;
	// the scaled epsilons of the LAST evaluation (test-observable)
	Scalar m_plane_eps;
	Scalar m_weld_eps;
	double m_sliver_area_eps;

	// ---- machinery (do not read from outside) ----
	zircon_csg_soup_t<Scalar> m_soup;
	// the ping-pong soup of the subtraction cut
	zircon_csg_soup_t<Scalar> m_soup_b;
	kotek::static_vector_t<zircon_csg_brush_t<Scalar>,
		ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND>
		m_brushes;
	// weld scratch (sort order + merge-sort temp + original->welded)
	kotek::static_vector_t<kotek::uint32_t,
		ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION>
		m_weld_order;
	kotek::static_vector_t<kotek::uint32_t,
		ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION>
		m_weld_scratch;
	kotek::static_vector_t<kotek::uint32_t,
		ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION>
		m_weld_map;
	// the fragment-in-flight ping-pong pools (a/b) + the cavity
	// candidate list (c)
	kotek::static_vector_t<zircon_csg_piece_t,
		ZIRCON_DEF_CSG_MAX_FRAGMENTS_IN_FLIGHT>
		m_pieces_a;
	kotek::static_vector_t<zircon_csg_vec3_t<Scalar>,
		ZIRCON_DEF_CSG_MAX_FRAGMENT_SCRATCH_VERTICES>
		m_piece_vertices_a;
	kotek::static_vector_t<zircon_csg_piece_t,
		ZIRCON_DEF_CSG_MAX_FRAGMENTS_IN_FLIGHT>
		m_pieces_b;
	kotek::static_vector_t<zircon_csg_vec3_t<Scalar>,
		ZIRCON_DEF_CSG_MAX_FRAGMENT_SCRATCH_VERTICES>
		m_piece_vertices_b;
	kotek::static_vector_t<zircon_csg_piece_t,
		ZIRCON_DEF_CSG_MAX_FRAGMENTS_IN_FLIGHT>
		m_pieces_c;
	kotek::static_vector_t<zircon_csg_vec3_t<Scalar>,
		ZIRCON_DEF_CSG_MAX_FRAGMENT_SCRATCH_VERTICES>
		m_piece_vertices_c;
};

// ------------------------------------------------------------------
// scalar/vector helpers (all in-scalar; dot goes through the traits so
// u32f accumulates in i64)
// ------------------------------------------------------------------
template <typename Scalar>
inline zircon_csg_vec3_t<Scalar>
zircon_csg_make_vec3(Scalar x, Scalar y, Scalar z) noexcept
{
	zircon_csg_vec3_t<Scalar> result{x, y, z};
	return result;
}

template <typename Scalar>
inline zircon_csg_vec3_t<Scalar> zircon_csg_vec_add(
	const zircon_csg_vec3_t<Scalar>& left,
	const zircon_csg_vec3_t<Scalar>& right) noexcept
{
	return zircon_csg_make_vec3(
		left.m[0] + right.m[0], left.m[1] + right.m[1],
		left.m[2] + right.m[2]);
}

template <typename Scalar>
inline zircon_csg_vec3_t<Scalar> zircon_csg_vec_sub(
	const zircon_csg_vec3_t<Scalar>& left,
	const zircon_csg_vec3_t<Scalar>& right) noexcept
{
	return zircon_csg_make_vec3(
		left.m[0] - right.m[0], left.m[1] - right.m[1],
		left.m[2] - right.m[2]);
}

template <typename Scalar>
inline zircon_csg_vec3_t<Scalar> zircon_csg_vec_scale(
	const zircon_csg_vec3_t<Scalar>& value, Scalar factor) noexcept
{
	return zircon_csg_make_vec3(
		value.m[0] * factor, value.m[1] * factor, value.m[2] * factor);
}

template <typename Scalar>
inline Scalar zircon_csg_vec_dot(
	const zircon_csg_vec3_t<Scalar>& left,
	const zircon_csg_vec3_t<Scalar>& right) noexcept
{
	return zircon_csg_scalar_traits<Scalar>::dot(left.m, right.m);
}

template <typename Scalar>
inline zircon_csg_vec3_t<Scalar> zircon_csg_vec_cross(
	const zircon_csg_vec3_t<Scalar>& left,
	const zircon_csg_vec3_t<Scalar>& right) noexcept
{
	return zircon_csg_make_vec3(
		left.m[1] * right.m[2] - left.m[2] * right.m[1],
		left.m[2] * right.m[0] - left.m[0] * right.m[2],
		left.m[0] * right.m[1] - left.m[1] * right.m[0]);
}

// rotates v by the quaternion q (x, y, z, w) — the q*v*q^-1 expansion
// v + 2*(qv x (qv x v + w*v)) in scalar ops (u32f-exact for the 90-degree
// quats; irrational rotations quantize deterministically)
template <typename Scalar>
inline zircon_csg_vec3_t<Scalar> zircon_csg_quat_rotate(
	const Scalar* p_quat_4, const zircon_csg_vec3_t<Scalar>& value
) noexcept
{
	using traits_t = zircon_csg_scalar_traits<Scalar>;

	const zircon_csg_vec3_t<Scalar> quat_vector{
		p_quat_4[0], p_quat_4[1], p_quat_4[2]};

	const zircon_csg_vec3_t<Scalar> inner = zircon_csg_vec_add(
		zircon_csg_vec_cross(quat_vector, value),
		zircon_csg_vec_scale(value, p_quat_4[3]));

	return zircon_csg_vec_add(
		value,
		zircon_csg_vec_scale(
			zircon_csg_vec_cross(quat_vector, inner),
			traits_t::from_int(2)));
}

// the signed distance of a point to a plane (positive = outside)
template <typename Scalar>
inline Scalar zircon_csg_plane_distance(
	const zircon_csg_plane_t<Scalar>& plane,
	const zircon_csg_vec3_t<Scalar>& point) noexcept
{
	return zircon_csg_vec_dot(plane.m_normal, point) - plane.m_dist;
}

// the Newell vector area of a (convex) ring, in DOUBLE: position
// cross-products overflow the 16.16 range for large coordinates, so
// area tests convert through traits::to_double (exact for u32f/f32)
// and compare against the double sliver epsilon — deterministic per
// mode by IEEE correct rounding
template <typename Scalar>
inline double zircon_csg_ring_area_double(
	const zircon_csg_vec3_t<Scalar>* p_ring, kotek::uint32_t count
) noexcept
{
	using traits_t = zircon_csg_scalar_traits<Scalar>;

	double sum[3] = {0.0, 0.0, 0.0};

	for (kotek::uint32_t i = 0; i < count; ++i)
	{
		const kotek::uint32_t next = (i + 1) % count;

		const double ax = traits_t::to_double(p_ring[i].m[0]);
		const double ay = traits_t::to_double(p_ring[i].m[1]);
		const double az = traits_t::to_double(p_ring[i].m[2]);
		const double bx = traits_t::to_double(p_ring[next].m[0]);
		const double by = traits_t::to_double(p_ring[next].m[1]);
		const double bz = traits_t::to_double(p_ring[next].m[2]);

		sum[0] += ay * bz - az * by;
		sum[1] += az * bx - ax * bz;
		sum[2] += ax * by - ay * bx;
	}

	return 0.5 * std::sqrt(
		sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
}

// ------------------------------------------------------------------
// brush construction (bakes the primitive transform into the planes
// and the soup; the quaternion is normalized defensively — a zero
// quat degrades to identity, never a NaN)
// ------------------------------------------------------------------
namespace zircon_csg_detail
{
	// appends one face (auto-flipped to match its plane's outward
	// normal — the winding contract is enforced here, not by hand);
	// plane_index names the face's source plane (pushed already)
	template <typename Scalar>
	inline void zircon_csg_brush_push_face(
		zircon_csg_brush_t<Scalar>& out_brush,
		const zircon_csg_vec3_t<Scalar>* p_ring,
		kotek::uint8_t vertex_count, kotek::uint8_t plane_index) noexcept
	{
		if (vertex_count < 3)
			return;

		if (out_brush.m_faces.size() >=
		        ZIRCON_DEF_CSG_MAX_FACES_PER_BRUSH ||
		    out_brush.m_vertices.size() + vertex_count >
		        ZIRCON_DEF_CSG_MAX_VERTICES_PER_BRUSH)
		{
			KOTEK_MESSAGE_WARNING(
				"[csg] brush face/vertex capacity exceeded — face "
				"dropped (raise ZIRCON_DEF_CSG_MAX_*)");
			return;
		}

		const zircon_csg_vec3_t<Scalar>& plane_normal =
			out_brush.m_planes[plane_index].m_normal;

		// the geometric normal of the ring (the first fan cross)
		// decides the flip
		bool flip = false;

		{
			const zircon_csg_vec3_t<Scalar> edge_a =
				zircon_csg_vec_sub(p_ring[1], p_ring[0]);
			const zircon_csg_vec3_t<Scalar> edge_b =
				zircon_csg_vec_sub(p_ring[2], p_ring[1]);

			const zircon_csg_vec3_t<Scalar> geometric_normal =
				zircon_csg_vec_cross(edge_a, edge_b);

			if (zircon_csg_vec_dot(geometric_normal, plane_normal) <
			    zircon_csg_scalar_traits<Scalar>::zero())
			{
				flip = true;
			}
		}

		zircon_csg_face_t face;
		face.m_vertex_start = static_cast<kotek::uint16_t>(
			out_brush.m_vertices.size());
		face.m_vertex_count = vertex_count;
		face.m_plane_index = plane_index;

		if (flip)
		{
			for (kotek::uint8_t i = 0; i < vertex_count; ++i)
			{
				out_brush.m_vertices.push_back(
					p_ring[vertex_count - 1 - i]);
			}
		}
		else
		{
			for (kotek::uint8_t i = 0; i < vertex_count; ++i)
			{
				out_brush.m_vertices.push_back(p_ring[i]);
			}
		}

		out_brush.m_faces.push_back(face);
	}

	template <typename Scalar>
	inline void zircon_csg_brush_push_plane(
		zircon_csg_brush_t<Scalar>& out_brush,
		const zircon_csg_vec3_t<Scalar>& normal, Scalar dist) noexcept
	{
		if (out_brush.m_planes.size() >=
		    ZIRCON_DEF_CSG_MAX_PLANES_PER_BRUSH)
		{
			KOTEK_MESSAGE_WARNING(
				"[csg] brush plane capacity exceeded — plane dropped "
				"(raise ZIRCON_DEF_CSG_MAX_PLANES_PER_BRUSH)");
			return;
		}

		out_brush.m_planes.push_back({normal, dist});
	}

	// the axis-aligned box (the kBox path AND the kCylinder
	// box-approximation): six planes, six quads
	template <typename Scalar>
	inline void zircon_csg_build_box_planes_and_faces(
		zircon_csg_brush_t<Scalar>& out_brush, Scalar half_x,
		Scalar half_y, Scalar half_z) noexcept
	{
		using traits_t = zircon_csg_scalar_traits<Scalar>;

		const Scalar zero = traits_t::zero();
		const Scalar one = traits_t::one();

		const zircon_csg_vec3_t<Scalar> normal_x =
			zircon_csg_make_vec3(one, zero, zero);
		const zircon_csg_vec3_t<Scalar> normal_neg_x =
			zircon_csg_make_vec3(-one, zero, zero);
		const zircon_csg_vec3_t<Scalar> normal_y =
			zircon_csg_make_vec3(zero, one, zero);
		const zircon_csg_vec3_t<Scalar> normal_neg_y =
			zircon_csg_make_vec3(zero, -one, zero);
		const zircon_csg_vec3_t<Scalar> normal_z =
			zircon_csg_make_vec3(zero, zero, one);
		const zircon_csg_vec3_t<Scalar> normal_neg_z =
			zircon_csg_make_vec3(zero, zero, -one);

		zircon_csg_brush_push_plane(out_brush, normal_x, half_x);
		zircon_csg_brush_push_plane(out_brush, normal_neg_x, half_x);
		zircon_csg_brush_push_plane(out_brush, normal_y, half_y);
		zircon_csg_brush_push_plane(out_brush, normal_neg_y, half_y);
		zircon_csg_brush_push_plane(out_brush, normal_z, half_z);
		zircon_csg_brush_push_plane(out_brush, normal_neg_z, half_z);

		const Scalar nx = -half_x;
		const Scalar ny = -half_y;
		const Scalar nz = -half_z;

		{
			const zircon_csg_vec3_t<Scalar> ring[4] = {
				zircon_csg_make_vec3(half_x, ny, nz),
				zircon_csg_make_vec3(half_x, half_y, nz),
				zircon_csg_make_vec3(half_x, half_y, half_z),
				zircon_csg_make_vec3(half_x, ny, half_z)};
			zircon_csg_brush_push_face(out_brush, ring, 4, 0);
		}
		{
			const zircon_csg_vec3_t<Scalar> ring[4] = {
				zircon_csg_make_vec3(nx, ny, nz),
				zircon_csg_make_vec3(nx, ny, half_z),
				zircon_csg_make_vec3(nx, half_y, half_z),
				zircon_csg_make_vec3(nx, half_y, nz)};
			zircon_csg_brush_push_face(out_brush, ring, 4, 1);
		}
		{
			const zircon_csg_vec3_t<Scalar> ring[4] = {
				zircon_csg_make_vec3(nx, half_y, nz),
				zircon_csg_make_vec3(nx, half_y, half_z),
				zircon_csg_make_vec3(half_x, half_y, half_z),
				zircon_csg_make_vec3(half_x, half_y, nz)};
			zircon_csg_brush_push_face(out_brush, ring, 4, 2);
		}
		{
			const zircon_csg_vec3_t<Scalar> ring[4] = {
				zircon_csg_make_vec3(nx, ny, nz),
				zircon_csg_make_vec3(half_x, ny, nz),
				zircon_csg_make_vec3(half_x, ny, half_z),
				zircon_csg_make_vec3(nx, ny, half_z)};
			zircon_csg_brush_push_face(out_brush, ring, 4, 3);
		}
		{
			const zircon_csg_vec3_t<Scalar> ring[4] = {
				zircon_csg_make_vec3(nx, ny, half_z),
				zircon_csg_make_vec3(half_x, ny, half_z),
				zircon_csg_make_vec3(half_x, half_y, half_z),
				zircon_csg_make_vec3(nx, half_y, half_z)};
			zircon_csg_brush_push_face(out_brush, ring, 4, 4);
		}
		{
			const zircon_csg_vec3_t<Scalar> ring[4] = {
				zircon_csg_make_vec3(nx, ny, nz),
				zircon_csg_make_vec3(nx, half_y, nz),
				zircon_csg_make_vec3(half_x, half_y, nz),
				zircon_csg_make_vec3(half_x, ny, nz)};
			zircon_csg_brush_push_face(out_brush, ring, 4, 5);
		}
	}

	// the wedge (kWedge): the right-triangle cross-section in XY
	// (legs along -x and -y from the box corner, the diagonal from
	// (+half_x, -half_y) to (-half_x, +half_y)) extruded along Z —
	// five planes, two triangles + three quads
	template <typename Scalar>
	inline void zircon_csg_build_wedge_planes_and_faces(
		zircon_csg_brush_t<Scalar>& out_brush, Scalar half_x,
		Scalar half_y, Scalar half_z) noexcept
	{
		using traits_t = zircon_csg_scalar_traits<Scalar>;

		const Scalar zero = traits_t::zero();
		const Scalar one = traits_t::one();

		const zircon_csg_vec3_t<Scalar> normal_neg_x =
			zircon_csg_make_vec3(-one, zero, zero);
		const zircon_csg_vec3_t<Scalar> normal_neg_y =
			zircon_csg_make_vec3(zero, -one, zero);
		const zircon_csg_vec3_t<Scalar> normal_z =
			zircon_csg_make_vec3(zero, zero, one);
		const zircon_csg_vec3_t<Scalar> normal_neg_z =
			zircon_csg_make_vec3(zero, zero, -one);

		// the diagonal plane: through the box's diagonal edge, normal
		// in the XY plane = normalize(2*half_y, 2*half_x, 0), dist = 0
		// (it passes the origin — the box is centered)
		const Scalar diagonal_length = traits_t::sqrt(
			half_y * half_y + half_x * half_x);

		Scalar inv_length = zero;

		if (diagonal_length > zero)
		{
			inv_length = traits_t::one() / diagonal_length;
		}
		else
		{
			// a degenerate wedge dimension is rejected upstream; the
			// guard keeps the math total
			KOTEK_MESSAGE_WARNING(
				"[csg] wedge with a zero diagonal — degenerate");
		}

		const zircon_csg_vec3_t<Scalar> normal_diagonal =
			zircon_csg_make_vec3(
				half_y * inv_length, half_x * inv_length, zero);

		zircon_csg_brush_push_plane(out_brush, normal_neg_x, half_x);
		zircon_csg_brush_push_plane(out_brush, normal_neg_y, half_y);
		zircon_csg_brush_push_plane(out_brush, normal_diagonal, zero);
		zircon_csg_brush_push_plane(out_brush, normal_z, half_z);
		zircon_csg_brush_push_plane(out_brush, normal_neg_z, half_z);

		const Scalar nx = -half_x;
		const Scalar ny = -half_y;
		const Scalar nz = -half_z;

		// bottom (y = -half_y) -> plane 1
		{
			const zircon_csg_vec3_t<Scalar> ring[4] = {
				zircon_csg_make_vec3(nx, ny, nz),
				zircon_csg_make_vec3(half_x, ny, nz),
				zircon_csg_make_vec3(half_x, ny, half_z),
				zircon_csg_make_vec3(nx, ny, half_z)};
			zircon_csg_brush_push_face(out_brush, ring, 4, 1);
		}
		// back (x = -half_x) -> plane 0
		{
			const zircon_csg_vec3_t<Scalar> ring[4] = {
				zircon_csg_make_vec3(nx, ny, nz),
				zircon_csg_make_vec3(nx, ny, half_z),
				zircon_csg_make_vec3(nx, half_y, half_z),
				zircon_csg_make_vec3(nx, half_y, nz)};
			zircon_csg_brush_push_face(out_brush, ring, 4, 0);
		}
		// the diagonal slope -> plane 2
		{
			const zircon_csg_vec3_t<Scalar> ring[4] = {
				zircon_csg_make_vec3(half_x, ny, nz),
				zircon_csg_make_vec3(nx, half_y, nz),
				zircon_csg_make_vec3(nx, half_y, half_z),
				zircon_csg_make_vec3(half_x, ny, half_z)};
			zircon_csg_brush_push_face(out_brush, ring, 4, 2);
		}
		// the two triangle sides (z = +-half_z) -> planes 3 / 4
		{
			const zircon_csg_vec3_t<Scalar> ring[3] = {
				zircon_csg_make_vec3(nx, ny, half_z),
				zircon_csg_make_vec3(half_x, ny, half_z),
				zircon_csg_make_vec3(nx, half_y, half_z)};
			zircon_csg_brush_push_face(out_brush, ring, 3, 3);
		}
		{
			const zircon_csg_vec3_t<Scalar> ring[3] = {
				zircon_csg_make_vec3(nx, ny, nz),
				zircon_csg_make_vec3(nx, half_y, nz),
				zircon_csg_make_vec3(half_x, ny, nz)};
			zircon_csg_brush_push_face(out_brush, ring, 3, 4);
		}
	}
} // namespace zircon_csg_detail

// builds the convex brush of one primitive, baking the transform:
// planes get the rotated normal + the shifted dist, the soup vertices
// the full rotation + translation. false = invalid arguments (a
// non-positive dimension, an unknown type) or a brush capacity guard
// fired (the partial brush is still returned, loudly).
template <typename Scalar>
inline bool zircon_csg_build_brush(
	const zircon_csg_primitive_desc_t<Scalar>& desc,
	zircon_csg_brush_t<Scalar>& out_brush) noexcept
{
	using traits_t = zircon_csg_scalar_traits<Scalar>;

	out_brush.m_planes.clear();
	out_brush.m_vertices.clear();
	out_brush.m_faces.clear();
	out_brush.m_material_id = desc.m_material_id;

	const Scalar zero = traits_t::zero();

	if (desc.m_dimensions[0] <= zero || desc.m_dimensions[1] <= zero ||
	    desc.m_dimensions[2] <= zero)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] primitive with a non-positive dimension — rejected"
		);
		return false;
	}

	if (desc.m_type >=
	    static_cast<kotek::uint8_t>(eZirconCsgPrimitiveType::kCount))
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] unknown primitive type {} — rejected",
			static_cast<kotek::uint32_t>(desc.m_type));
		return false;
	}

	// defensive quat normalization (a zero quat degrades to identity)
	Scalar rotation[4] = {
		desc.m_rotation[0], desc.m_rotation[1], desc.m_rotation[2],
		desc.m_rotation[3]};

	{
		const Scalar length_squared =
			rotation[0] * rotation[0] + rotation[1] * rotation[1] +
			rotation[2] * rotation[2] + rotation[3] * rotation[3];

		const Scalar one = traits_t::one();

		if (length_squared <= zero)
		{
			rotation[0] = zero;
			rotation[1] = zero;
			rotation[2] = zero;
			rotation[3] = one;
		}
		else
		{
			const Scalar difference =
				traits_t::abs(length_squared - one);

			// renormalize only when it visibly drifts (identity and
			// the exact 90-degree quats must stay bit-exact for the
			// u32f determinism argument)
			if (difference > traits_t::from_double(0.001))
			{
				const Scalar inv_length =
					one / traits_t::sqrt(length_squared);

				rotation[0] = rotation[0] * inv_length;
				rotation[1] = rotation[1] * inv_length;
				rotation[2] = rotation[2] * inv_length;
				rotation[3] = rotation[3] * inv_length;
			}
		}
	}

	const Scalar two = traits_t::from_int(2);
	const Scalar half_x = desc.m_dimensions[0] / two;
	const Scalar half_y = desc.m_dimensions[1] / two;
	const Scalar half_z = desc.m_dimensions[2] / two;

	switch (static_cast<eZirconCsgPrimitiveType>(desc.m_type))
	{
	case eZirconCsgPrimitiveType::kBox:
	{
		zircon_csg_detail::zircon_csg_build_box_planes_and_faces(
			out_brush, half_x, half_y, half_z);
		break;
	}
	case eZirconCsgPrimitiveType::kWedge:
	{
		zircon_csg_detail::zircon_csg_build_wedge_planes_and_faces(
			out_brush, half_x, half_y, half_z);
		break;
	}
	case eZirconCsgPrimitiveType::kCylinder:
	{
		// A1: the cylinder is box-approximated (its AABB) — the
		// documented approximation boundary (the enum is the
		// registration contract; an N-gon prism is the A2+ shape)
		zircon_csg_detail::zircon_csg_build_box_planes_and_faces(
			out_brush, half_x, half_y, half_z);
		break;
	}
	default:
	{
		// unreachable: the type was validated above
		return false;
	}
	}

	// bake the transform
	const zircon_csg_vec3_t<Scalar> position{
		desc.m_position[0], desc.m_position[1], desc.m_position[2]};

	for (kotek::size_t plane_index = 0;
	     plane_index < out_brush.m_planes.size(); ++plane_index)
	{
		zircon_csg_plane_t<Scalar>& plane =
			out_brush.m_planes[plane_index];

		const zircon_csg_vec3_t<Scalar> rotated_normal =
			zircon_csg_quat_rotate(rotation, plane.m_normal);

		plane.m_normal = rotated_normal;
		plane.m_dist =
			plane.m_dist + zircon_csg_vec_dot(rotated_normal, position);
	}

	for (kotek::size_t vertex_index = 0;
	     vertex_index < out_brush.m_vertices.size(); ++vertex_index)
	{
		zircon_csg_vec3_t<Scalar>& vertex =
			out_brush.m_vertices[vertex_index];

		vertex = zircon_csg_vec_add(
			zircon_csg_quat_rotate(rotation, vertex), position);
	}

	return true;
}

// ------------------------------------------------------------------
// the ring split (the Quake eps classification): splits one convex
// ring against one plane into the strictly-outside front ring and the
// inside-or-on back ring. Crossing points are computed ONCE and shared
// bitwise by both rings; on-plane vertices (|d| <= eps) close both
// rings. Buffers are caller-owned, capacity
// ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES (a split adds at most one
// vertex — the caller pre-checks in_count < capacity).
// ------------------------------------------------------------------
template <typename Scalar>
inline void zircon_csg_split_ring(
	const zircon_csg_vec3_t<Scalar>* p_in, kotek::uint32_t in_count,
	const zircon_csg_plane_t<Scalar>& plane, Scalar plane_eps,
	zircon_csg_vec3_t<Scalar>* p_out_front,
	kotek::uint32_t& out_front_count,
	zircon_csg_vec3_t<Scalar>* p_out_back,
	kotek::uint32_t& out_back_count, bool& out_has_strict_front,
	bool& out_all_on) noexcept
{
	Scalar distances[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];
	signed char classes[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];

	const Scalar negative_eps = -plane_eps;

	kotek::uint32_t on_count = 0;

	for (kotek::uint32_t i = 0; i < in_count; ++i)
	{
		const Scalar distance =
			zircon_csg_plane_distance(plane, p_in[i]);

		distances[i] = distance;

		if (distance > plane_eps)
		{
			classes[i] = 1;
		}
		else if (distance < negative_eps)
		{
			classes[i] = -1;
		}
		else
		{
			classes[i] = 0;
			++on_count;
		}
	}

	out_front_count = 0;
	out_back_count = 0;
	out_has_strict_front = false;
	out_all_on = (on_count == in_count);

	for (kotek::uint32_t i = 0; i < in_count; ++i)
	{
		const kotek::uint32_t next = (i + 1) % in_count;

		if (classes[i] > 0)
		{
			p_out_front[out_front_count++] = p_in[i];
			out_has_strict_front = true;
		}
		else if (classes[i] == 0)
		{
			p_out_front[out_front_count++] = p_in[i];
			p_out_back[out_back_count++] = p_in[i];
		}
		else
		{
			p_out_back[out_back_count++] = p_in[i];
		}

		// a strict sign change crosses the plane: one shared point.
		// The crossing is a pure function of (a, b, plane) — the two
		// collinear subsegments of one original edge (produced by
		// different clip histories) must yield the SAME seam vertex
		// bitwise. The 2026-09-21 u32f lesson: computing t in the
		// fixed-point domain quantizes it differently per subsegment
		// length (the seam scattered across 3 quanta and the weld
		// could not pair the slanted micro-edges). So the crossing is
		// computed through an IEEE-754 double hop (the endpoints and
		// distances convert exactly in every mode; the division and
		// the lerp are correctly rounded — identical on every IEEE
		// platform) and quantized ONCE by the mode's from_double rule.
		if ((classes[i] > 0 && classes[next] < 0) ||
		    (classes[i] < 0 && classes[next] > 0))
		{
			using traits_t = zircon_csg_scalar_traits<Scalar>;

			const double distance_a =
				traits_t::to_double(distances[i]);
			const double distance_b =
				traits_t::to_double(distances[next]);

			const double t =
				distance_a / (distance_a - distance_b);

			zircon_csg_vec3_t<Scalar> crossing;

			for (int axis = 0; axis < 3; ++axis)
			{
				const double a_component =
					traits_t::to_double(p_in[i].m[axis]);
				const double b_component =
					traits_t::to_double(p_in[next].m[axis]);

				crossing.m[axis] = traits_t::from_double(
					a_component + t * (b_component - a_component));
			}

			p_out_front[out_front_count++] = crossing;
			p_out_back[out_back_count++] = crossing;
		}
	}
}

// ------------------------------------------------------------------
// the piece machinery (fragments in flight)
// ------------------------------------------------------------------
namespace zircon_csg_detail
{
	template <typename Scalar>
	struct zircon_csg_piece_pool_t
	{
		kotek::static_vector_t<zircon_csg_piece_t,
			ZIRCON_DEF_CSG_MAX_FRAGMENTS_IN_FLIGHT>* p_pieces;
		kotek::static_vector_t<zircon_csg_vec3_t<Scalar>,
			ZIRCON_DEF_CSG_MAX_FRAGMENT_SCRATCH_VERTICES>* p_vertices;
	};

	// appends a ring as a piece; false = the capacity guard fired
	// (loud, counted by the caller)
	template <typename Scalar>
	inline bool zircon_csg_pool_append(
		zircon_csg_piece_pool_t<Scalar>& pool,
		const zircon_csg_vec3_t<Scalar>* p_ring,
		kotek::uint32_t vertex_count) noexcept
	{
		if (pool.p_pieces->size() >=
		        ZIRCON_DEF_CSG_MAX_FRAGMENTS_IN_FLIGHT ||
		    pool.p_vertices->size() + vertex_count >
		        ZIRCON_DEF_CSG_MAX_FRAGMENT_SCRATCH_VERTICES)
		{
			return false;
		}

		zircon_csg_piece_t piece;
		piece.m_vertex_start = static_cast<kotek::uint32_t>(
			pool.p_vertices->size());
		piece.m_vertex_count = vertex_count;

		for (kotek::uint32_t i = 0; i < vertex_count; ++i)
		{
			pool.p_vertices->push_back(p_ring[i]);
		}

		pool.p_pieces->push_back(piece);
		return true;
	}

	// splits one piece against a whole brush: the strictly-outside
	// fronts append to out_pool (the sliver policy applied), the
	// inside-or-on survivor (when any) lands in the caller's survivor
	// ring with its coplanar mark — the keep tables live in the
	// callers (the header banner). p_piece_normal is the SOURCE FACE's
	// outward normal (all fragments of a face share it; it decides the
	// kSame/kOpposed mark against an all-on plane).
	template <typename Scalar>
	inline void zircon_csg_split_piece_vs_brush(
		zircon_csg_evaluation_t<Scalar>& evaluation,
		const zircon_csg_vec3_t<Scalar>* p_piece_ring,
		kotek::uint32_t piece_vertex_count,
		const zircon_csg_vec3_t<Scalar>* p_piece_normal,
		const zircon_csg_brush_t<Scalar>& brush,
		zircon_csg_piece_pool_t<Scalar>& out_pool,
		zircon_csg_vec3_t<Scalar>* p_out_survivor_ring,
		kotek::uint32_t& out_survivor_count,
		eZirconCsgCoplanar& out_survivor_coplanar) noexcept
	{
		// the ping-pong split buffers (a split adds at most one vertex
		// per plane — the pre-check below keeps the invariant)
		zircon_csg_vec3_t<Scalar>
			buffer_a[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];
		zircon_csg_vec3_t<Scalar>
			buffer_b[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];

		const zircon_csg_vec3_t<Scalar>* p_current = p_piece_ring;
		kotek::uint32_t current_count = piece_vertex_count;

		out_survivor_count = 0;
		out_survivor_coplanar = eZirconCsgCoplanar::kNone;

		for (kotek::size_t plane_index = 0;
		     plane_index < brush.m_planes.size(); ++plane_index)
		{
			if (current_count == 0)
				break;

			if (current_count >= ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES - 1)
			{
				// the ring would outgrow the split buffers — a
				// pathological fragment; drop it loudly (counted)
				++evaluation.m_capacity_drop_count;
				evaluation.m_status =
					eZirconCsgEvaluationStatus::kError_CapacityPolygons;
				return;
			}

			zircon_csg_vec3_t<Scalar>* p_front = buffer_a;
			zircon_csg_vec3_t<Scalar>* p_back = buffer_b;

			kotek::uint32_t front_count = 0;
			kotek::uint32_t back_count = 0;
			bool has_strict_front = false;
			bool all_on = false;

			zircon_csg_split_ring(p_current, current_count,
				brush.m_planes[plane_index], evaluation.m_plane_eps,
				p_front, front_count, p_back, back_count,
				has_strict_front, all_on);

			if (has_strict_front && front_count >= 3)
			{
				const double front_area =
					zircon_csg_ring_area_double(p_front, front_count);

				if (front_area >= evaluation.m_sliver_area_eps)
				{
					if (zircon_csg_pool_append(
							out_pool, p_front, front_count) == false)
					{
						++evaluation.m_capacity_drop_count;
						evaluation.m_status = eZirconCsgEvaluationStatus::
							kError_CapacityPolygons;
					}
				}
				else
				{
					++evaluation.m_sliver_drop_count;
				}
			}

			if (back_count < 3)
			{
				// fully consumed as outside fronts (or a degenerate
				// no-area remnant) — nothing survives inside-or-on
				// this brush
				return;
			}

			if (all_on && out_survivor_coplanar ==
			                  eZirconCsgCoplanar::kNone)
			{
				// the first all-on plane marks the survivor; the mark
				// stays valid downstream because the back-ring of an
				// all-on ring keeps every vertex ON that plane (the
				// crossing points of on-plane edges lie on the plane).
				// The mark distinguishes TRUE coplanar faces — the
				// normals are (anti)parallel, 0.999 admits the
				// "near-parallel planes within eps" case by design —
				// from thin boundary strips (perpendicular normals),
				// which follow the kNone rule.
				using traits_t = zircon_csg_scalar_traits<Scalar>;

				const Scalar normal_dot = zircon_csg_vec_dot(
					*p_piece_normal,
					brush.m_planes[plane_index].m_normal);

				const Scalar parallel = traits_t::from_double(0.999);

				if (normal_dot > parallel)
				{
					out_survivor_coplanar = eZirconCsgCoplanar::kSame;
				}
				else if (normal_dot < -parallel)
				{
					out_survivor_coplanar =
						eZirconCsgCoplanar::kOpposed;
				}
			}

			// continue with the inside-or-on part
			for (kotek::uint32_t i = 0; i < back_count; ++i)
			{
				p_out_survivor_ring[i] = p_back[i];
			}

			p_current = p_out_survivor_ring;
			current_count = back_count;
		}

		out_survivor_count = current_count;
	}
} // namespace zircon_csg_detail


// ------------------------------------------------------------------
// the boolean pipelines (the keep tables of the header banner)
// ------------------------------------------------------------------
namespace zircon_csg_detail
{
	// appends one polygon to a soup (capacity-guarded, loud)
	template <typename Scalar>
	inline bool zircon_csg_soup_append(
		zircon_csg_evaluation_t<Scalar>& evaluation,
		zircon_csg_soup_t<Scalar>& soup,
		const zircon_csg_vec3_t<Scalar>* p_ring,
		kotek::uint32_t vertex_count,
		const zircon_csg_vec3_t<Scalar>& normal,
		kotek::uint16_t material_id) noexcept
	{
		if (vertex_count < 3)
			return false;

		if (soup.m_vertices.size() + vertex_count >
		        ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION ||
		    soup.m_polygons.size() >=
		        ZIRCON_DEF_CSG_MAX_POLYGONS_PER_EVALUATION)
		{
			++evaluation.m_capacity_drop_count;
			evaluation.m_status =
				soup.m_polygons.size() >=
					ZIRCON_DEF_CSG_MAX_POLYGONS_PER_EVALUATION
				? eZirconCsgEvaluationStatus::kError_CapacityPolygons
				: eZirconCsgEvaluationStatus::kError_CapacityVertices;
			return false;
		}

		zircon_csg_polygon_t<Scalar> polygon;
		polygon.m_vertex_start = static_cast<kotek::uint32_t>(
			soup.m_vertices.size());
		polygon.m_vertex_count = vertex_count;
		polygon.m_normal = normal;
		polygon.m_material_id = material_id;
		polygon.m_spare[0] = 0;
		polygon.m_spare[1] = 0;

		for (kotek::uint32_t i = 0; i < vertex_count; ++i)
		{
			soup.m_vertices.push_back(p_ring[i]);
		}

		soup.m_polygons.push_back(polygon);
		return true;
	}

	// validates one descriptor (dims > 0, a known type) — shared by the
	// two public entry points
	template <typename Scalar>
	inline eZirconCsgEvaluationStatus zircon_csg_validate_desc(
		const zircon_csg_primitive_desc_t<Scalar>& desc) noexcept
	{
		using traits_t = zircon_csg_scalar_traits<Scalar>;

		if (traits_t::to_double(desc.m_dimensions[0]) <= 0.0 ||
		    traits_t::to_double(desc.m_dimensions[1]) <= 0.0 ||
		    traits_t::to_double(desc.m_dimensions[2]) <= 0.0)
		{
			KOTEK_MESSAGE_WARNING(
				"[csg] primitive with a non-positive dimension");
			return eZirconCsgEvaluationStatus::kError_InvalidArguments;
		}

		if (desc.m_type >=
		    static_cast<kotek::uint8_t>(eZirconCsgPrimitiveType::kCount))
		{
			KOTEK_MESSAGE_WARNING(
				"[csg] unknown primitive type {}",
				static_cast<kotek::uint32_t>(desc.m_type));
			return eZirconCsgEvaluationStatus::kError_UnknownPrimitiveType;
		}

		return eZirconCsgEvaluationStatus::kSuccess;
	}

	// scales the epsilons to the primitive set (the policy of the
	// defs): plane/weld eps = max(relative x bounding diagonal, the
	// mode floor), sliver area = weld^2
	template <typename Scalar>
	inline void zircon_csg_compute_epsilons(
		const zircon_csg_primitive_desc_t<Scalar>* p_primitives,
		kotek::uint32_t primitive_count,
		zircon_csg_evaluation_t<Scalar>& evaluation) noexcept
	{
		using traits_t = zircon_csg_scalar_traits<Scalar>;

		double span = 0.0;

		for (kotek::uint32_t i = 0; i < primitive_count; ++i)
		{
			const double half_x =
				std::fabs(traits_t::to_double(
					p_primitives[i].m_dimensions[0])) *
				0.5;
			const double half_y =
				std::fabs(traits_t::to_double(
					p_primitives[i].m_dimensions[1])) *
				0.5;
			const double half_z =
				std::fabs(traits_t::to_double(
					p_primitives[i].m_dimensions[2])) *
				0.5;

			const double half_diagonal = std::sqrt(
				half_x * half_x + half_y * half_y + half_z * half_z);

			const double position_x = traits_t::to_double(
				p_primitives[i].m_position[0]);
			const double position_y = traits_t::to_double(
				p_primitives[i].m_position[1]);
			const double position_z = traits_t::to_double(
				p_primitives[i].m_position[2]);

			const double reach = std::sqrt(
									 position_x * position_x +
									 position_y * position_y +
									 position_z * position_z) +
				half_diagonal;

			if (2.0 * reach > span)
			{
				span = 2.0 * reach;
			}
		}

		const double plane_floor =
			traits_t::to_double(traits_t::plane_eps_floor());
		const double weld_floor =
			traits_t::to_double(traits_t::weld_eps_floor());

		double plane_eps = ZIRCON_DEF_CSG_EPS_PLANE_RELATIVE * span;
		double weld_eps = ZIRCON_DEF_CSG_EPS_WELD_RELATIVE * span;

		if (plane_eps < plane_floor)
			plane_eps = plane_floor;
		if (weld_eps < weld_floor)
			weld_eps = weld_floor;

		evaluation.m_plane_eps = traits_t::from_double(plane_eps);
		evaluation.m_weld_eps = traits_t::from_double(weld_eps);
		evaluation.m_sliver_area_eps = weld_eps * weld_eps;
	}

	// the N-way union for one face of brush brush_index_i: the
	// strictly-outside fronts continue through the remaining ADDITIVE
	// brushes, the inside-or-on survivor follows the union keep table,
	// and whatever survives appends to the soup
	template <typename Scalar>
	inline void zircon_csg_union_face(
		zircon_csg_evaluation_t<Scalar>& evaluation,
		kotek::uint32_t brush_index_i, kotek::uint32_t face_index,
		const kotek::uint32_t* p_additive_indices,
		kotek::uint32_t additive_count) noexcept
	{
		const zircon_csg_brush_t<Scalar>& brush_i =
			evaluation.m_brushes[brush_index_i];
		const zircon_csg_face_t& face = brush_i.m_faces[face_index];
		const zircon_csg_vec3_t<Scalar>& face_normal =
			brush_i.m_planes[face.m_plane_index].m_normal;

		zircon_csg_piece_pool_t<Scalar> pool_a{
			&evaluation.m_pieces_a, &evaluation.m_piece_vertices_a};
		zircon_csg_piece_pool_t<Scalar> pool_b{
			&evaluation.m_pieces_b, &evaluation.m_piece_vertices_b};

		pool_a.p_pieces->clear();
		pool_a.p_vertices->clear();

		if (zircon_csg_pool_append(
				pool_a, &brush_i.m_vertices[face.m_vertex_start],
				face.m_vertex_count) == false)
		{
			++evaluation.m_capacity_drop_count;
			evaluation.m_status =
				eZirconCsgEvaluationStatus::kError_CapacityPolygons;
			return;
		}

		zircon_csg_piece_pool_t<Scalar>* p_in = &pool_a;
		zircon_csg_piece_pool_t<Scalar>* p_out = &pool_b;

		zircon_csg_vec3_t<Scalar>
			survivor_ring[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];

		for (kotek::uint32_t additive = 0;
		     additive < additive_count && p_in->p_pieces->empty() == false;
		     ++additive)
		{
			const kotek::uint32_t j = p_additive_indices[additive];

			if (j == brush_index_i)
				continue;

			p_out->p_pieces->clear();
			p_out->p_vertices->clear();

			const zircon_csg_brush_t<Scalar>& brush_j =
				evaluation.m_brushes[j];

			for (kotek::uint32_t piece_index = 0;
			     piece_index < p_in->p_pieces->size(); ++piece_index)
			{
				const zircon_csg_piece_t& piece =
					(*p_in->p_pieces)[piece_index];

				kotek::uint32_t survivor_count = 0;
				eZirconCsgCoplanar coplanar = eZirconCsgCoplanar::kNone;

				zircon_csg_split_piece_vs_brush(evaluation,
					&(*p_in->p_vertices)[piece.m_vertex_start],
					piece.m_vertex_count, &face_normal, brush_j, *p_out,
					survivor_ring, survivor_count, coplanar);

				if (survivor_count >= 3 &&
				    coplanar == eZirconCsgCoplanar::kSame &&
				    brush_index_i > j)
				{
					// the dedup'd coplanar copy continues (the
					// highest-index coplanar brush owns the surface)
					if (zircon_csg_pool_append(*p_out, survivor_ring,
						    survivor_count) == false)
					{
						++evaluation.m_capacity_drop_count;
						evaluation.m_status = eZirconCsgEvaluationStatus::
							kError_CapacityPolygons;
					}
				}
				// kNone: strictly inside — covered. kOpposed: the
				// internal seam. kSame with i < j: the other owns it.
			}

			zircon_csg_piece_pool_t<Scalar>* p_swap = p_in;
			p_in = p_out;
			p_out = p_swap;
		}

		for (kotek::uint32_t piece_index = 0;
		     piece_index < p_in->p_pieces->size(); ++piece_index)
		{
			const zircon_csg_piece_t& piece =
				(*p_in->p_pieces)[piece_index];

			zircon_csg_soup_append(evaluation, evaluation.m_soup,
				&(*p_in->p_vertices)[piece.m_vertex_start],
				piece.m_vertex_count, face_normal, brush_i.m_material_id);
		}
	}

	// cuts the accumulated soup by one subtractive brush (the A-side
	// keep table: the strictly-outside fronts and the coplanar-OPPOSED
	// survivor stay, the rest is removed). Ping-pongs the soup.
	template <typename Scalar>
	inline void zircon_csg_subtract_cut_soup(
		zircon_csg_evaluation_t<Scalar>& evaluation,
		const zircon_csg_brush_t<Scalar>& subtractive) noexcept
	{
		evaluation.m_soup_b.m_vertices.clear();
		evaluation.m_soup_b.m_polygons.clear();

		zircon_csg_piece_pool_t<Scalar> front_pool{
			&evaluation.m_pieces_a, &evaluation.m_piece_vertices_a};

		zircon_csg_vec3_t<Scalar>
			survivor_ring[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];

		const kotek::size_t polygon_count =
			evaluation.m_soup.m_polygons.size();

		for (kotek::size_t polygon_index = 0;
		     polygon_index < polygon_count; ++polygon_index)
		{
			const zircon_csg_polygon_t<Scalar>& polygon =
				evaluation.m_soup.m_polygons[polygon_index];

			front_pool.p_pieces->clear();
			front_pool.p_vertices->clear();

			kotek::uint32_t survivor_count = 0;
			eZirconCsgCoplanar coplanar = eZirconCsgCoplanar::kNone;

			zircon_csg_split_piece_vs_brush(evaluation,
				&evaluation.m_soup.m_vertices[polygon.m_vertex_start],
				polygon.m_vertex_count, &polygon.m_normal, subtractive,
				front_pool, survivor_ring, survivor_count, coplanar);

			for (kotek::uint32_t piece_index = 0;
			     piece_index < front_pool.p_pieces->size();
			     ++piece_index)
			{
				const zircon_csg_piece_t& piece =
					(*front_pool.p_pieces)[piece_index];

				zircon_csg_soup_append(evaluation, evaluation.m_soup_b,
					&(*front_pool.p_vertices)[piece.m_vertex_start],
					piece.m_vertex_count, polygon.m_normal,
					polygon.m_material_id);
			}

			if (survivor_count >= 3 &&
			    coplanar == eZirconCsgCoplanar::kOpposed)
			{
				// contact without overlap removes nothing
				zircon_csg_soup_append(evaluation, evaluation.m_soup_b,
					survivor_ring, survivor_count, polygon.m_normal,
					polygon.m_material_id);
			}
		}

		evaluation.m_soup = evaluation.m_soup_b;
	}

	// the cavity walls of one subtractive brush: its faces' fragments
	// that are inside-or-on ANY additive brush (and not coplanar with
	// the capturing additive's planes) and strictly outside every
	// previously applied subtractive — appended FLIPPED (reversed
	// winding + negated normal, the B-side keep table)
	template <typename Scalar>
	inline void zircon_csg_subtract_cavity_walls(
		zircon_csg_evaluation_t<Scalar>& evaluation,
		kotek::uint32_t subtractive_index,
		const kotek::uint32_t* p_additive_indices,
		kotek::uint32_t additive_count,
		const kotek::uint32_t* p_previous_subtractive_indices,
		kotek::uint32_t previous_subtractive_count) noexcept
	{
		const zircon_csg_brush_t<Scalar>& brush_s =
			evaluation.m_brushes[subtractive_index];

		zircon_csg_piece_pool_t<Scalar> pool_a{
			&evaluation.m_pieces_a, &evaluation.m_piece_vertices_a};
		zircon_csg_piece_pool_t<Scalar> pool_b{
			&evaluation.m_pieces_b, &evaluation.m_piece_vertices_b};
		zircon_csg_piece_pool_t<Scalar> pool_c{
			&evaluation.m_pieces_c, &evaluation.m_piece_vertices_c};

		zircon_csg_vec3_t<Scalar>
			survivor_ring[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];

		for (kotek::size_t face_index = 0;
		     face_index < brush_s.m_faces.size(); ++face_index)
		{
			const zircon_csg_face_t& face = brush_s.m_faces[face_index];
			const zircon_csg_vec3_t<Scalar>& face_normal =
				brush_s.m_planes[face.m_plane_index].m_normal;

			pool_a.p_pieces->clear();
			pool_a.p_vertices->clear();
			pool_c.p_pieces->clear();
			pool_c.p_vertices->clear();

			if (zircon_csg_pool_append(pool_a,
				    &brush_s.m_vertices[face.m_vertex_start],
				    face.m_vertex_count) == false)
			{
				++evaluation.m_capacity_drop_count;
				evaluation.m_status = eZirconCsgEvaluationStatus::
					kError_CapacityPolygons;
				continue;
			}

			// stage 1: hunt the additive that contains each fragment
			// (fronts keep hunting, the inside-or-on survivor of an
			// additive becomes a candidate unless it is coplanar with
			// the capturing additive — the B-side table)
			zircon_csg_piece_pool_t<Scalar>* p_in = &pool_a;
			zircon_csg_piece_pool_t<Scalar>* p_out = &pool_b;

			for (kotek::uint32_t additive = 0;
			     additive < additive_count &&
			     p_in->p_pieces->empty() == false;
			     ++additive)
			{
				const zircon_csg_brush_t<Scalar>& brush_j =
					evaluation.m_brushes[p_additive_indices[additive]];

				p_out->p_pieces->clear();
				p_out->p_vertices->clear();

				for (kotek::uint32_t piece_index = 0;
				     piece_index < p_in->p_pieces->size(); ++piece_index)
				{
					const zircon_csg_piece_t& piece =
						(*p_in->p_pieces)[piece_index];

					kotek::uint32_t survivor_count = 0;
					eZirconCsgCoplanar coplanar =
						eZirconCsgCoplanar::kNone;

					zircon_csg_split_piece_vs_brush(evaluation,
						&(*p_in->p_vertices)[piece.m_vertex_start],
						piece.m_vertex_count, &face_normal, brush_j,
						*p_out, survivor_ring, survivor_count, coplanar);

					if (survivor_count >= 3 &&
					    coplanar == eZirconCsgCoplanar::kNone)
					{
						if (zircon_csg_pool_append(pool_c, survivor_ring,
							    survivor_count) == false)
						{
							++evaluation.m_capacity_drop_count;
							evaluation.m_status =
								eZirconCsgEvaluationStatus::
									kError_CapacityPolygons;
						}
					}
				}

				zircon_csg_piece_pool_t<Scalar>* p_swap = p_in;
				p_in = p_out;
				p_out = p_swap;
			}

			// stage 2: the candidates clipped strictly outside every
			// previously applied subtractive (the survivor is dropped
			// — the documented carve-ordering simplification)
			for (kotek::uint32_t previous = 0;
			     previous < previous_subtractive_count &&
			     pool_c.p_pieces->empty() == false;
			     ++previous)
			{
				const zircon_csg_brush_t<Scalar>& brush_t =
					evaluation.m_brushes
						[p_previous_subtractive_indices[previous]];

				pool_b.p_pieces->clear();
				pool_b.p_vertices->clear();

				for (kotek::uint32_t piece_index = 0;
				     piece_index < pool_c.p_pieces->size(); ++piece_index)
				{
					const zircon_csg_piece_t& piece =
						(*pool_c.p_pieces)[piece_index];

					kotek::uint32_t survivor_count = 0;
					eZirconCsgCoplanar coplanar =
						eZirconCsgCoplanar::kNone;

					zircon_csg_split_piece_vs_brush(evaluation,
						&(*pool_c.p_vertices)[piece.m_vertex_start],
						piece.m_vertex_count, &face_normal, brush_t,
						pool_b, survivor_ring, survivor_count, coplanar);
					// the survivor is dropped on purpose (see above)
				}

				zircon_csg_piece_pool_t<Scalar> swap_temp = pool_b;
				pool_b = pool_c;
				pool_c = swap_temp;
			}

			// emit the surviving candidates FLIPPED
			using traits_t = zircon_csg_scalar_traits<Scalar>;

			const zircon_csg_vec3_t<Scalar> flipped_normal =
				zircon_csg_vec_scale(
					face_normal, -traits_t::one());

			for (kotek::uint32_t piece_index = 0;
			     piece_index < pool_c.p_pieces->size(); ++piece_index)
			{
				const zircon_csg_piece_t& piece =
					(*pool_c.p_pieces)[piece_index];

				zircon_csg_vec3_t<Scalar>
					flipped_ring[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];

				for (kotek::uint32_t i = 0; i < piece.m_vertex_count;
				     ++i)
				{
					flipped_ring[i] =
						(*pool_c.p_vertices)[piece.m_vertex_start +
							piece.m_vertex_count - 1 - i];
				}

				zircon_csg_soup_append(evaluation, evaluation.m_soup,
					flipped_ring, piece.m_vertex_count, flipped_normal,
					brush_s.m_material_id);
			}
		}
	}

	// the brush-PAIR intersection (the plan's boolean; not a compound
	// semantic): each brush's faces keep their mutually-inside
	// fragments (the intersect keep table — the A side wins the
	// coplanar-same dedup, so touching = empty by policy)
	template <typename Scalar>
	inline void zircon_csg_intersect_pair(
		zircon_csg_evaluation_t<Scalar>& evaluation) noexcept
	{
		zircon_csg_piece_pool_t<Scalar> discard_pool{
			&evaluation.m_pieces_a, &evaluation.m_piece_vertices_a};

		zircon_csg_vec3_t<Scalar>
			survivor_ring[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];

		for (kotek::uint32_t side = 0; side < 2; ++side)
		{
			const zircon_csg_brush_t<Scalar>& brush_x =
				evaluation.m_brushes[side];
			const zircon_csg_brush_t<Scalar>& brush_y =
				evaluation.m_brushes[1 - side];
			const bool is_a_side = (side == 0);

			for (kotek::size_t face_index = 0;
			     face_index < brush_x.m_faces.size(); ++face_index)
			{
				const zircon_csg_face_t& face =
					brush_x.m_faces[face_index];
				const zircon_csg_vec3_t<Scalar>& face_normal =
					brush_x.m_planes[face.m_plane_index].m_normal;

				discard_pool.p_pieces->clear();
				discard_pool.p_vertices->clear();

				kotek::uint32_t survivor_count = 0;
				eZirconCsgCoplanar coplanar = eZirconCsgCoplanar::kNone;

				zircon_csg_split_piece_vs_brush(evaluation,
					&brush_x.m_vertices[face.m_vertex_start],
					face.m_vertex_count, &face_normal, brush_y,
					discard_pool, survivor_ring, survivor_count,
					coplanar);
				// the fronts are discarded — only the mutually-inside
				// fragment counts

				if (survivor_count >= 3 &&
				    (coplanar == eZirconCsgCoplanar::kNone ||
				        (coplanar == eZirconCsgCoplanar::kSame &&
				            is_a_side)))
				{
					zircon_csg_soup_append(evaluation, evaluation.m_soup,
						survivor_ring, survivor_count, face_normal,
						brush_x.m_material_id);
				}
			}
		}
	}

	// the deterministic merge sort of vertex indices by (x, y, z)
	// (bottom-up, stable — equal keys keep the index order; hand-rolled
	// per the wrappers-only rule, no std::sort)
	template <typename Scalar>
	inline int zircon_csg_compare_vertices(
		const zircon_csg_vec3_t<Scalar>& left,
		const zircon_csg_vec3_t<Scalar>& right) noexcept
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			if (left.m[axis] < right.m[axis])
				return -1;
			if (left.m[axis] > right.m[axis])
				return 1;
		}

		return 0;
	}

	template <typename Scalar>
	inline void zircon_csg_merge_sort_indices(
		const zircon_csg_vec3_t<Scalar>* p_vertices,
		kotek::uint32_t* p_order, kotek::uint32_t* p_scratch,
		kotek::uint32_t count) noexcept
	{
		for (kotek::uint32_t i = 0; i < count; ++i)
		{
			p_order[i] = i;
		}

		for (kotek::uint32_t width = 1; width < count; width <<= 1)
		{
			for (kotek::uint32_t start = 0; start < count;
			     start += 2 * width)
			{
				const kotek::uint32_t middle =
					start + width < count ? start + width : count;
				const kotek::uint32_t end =
					start + 2 * width < count ? start + 2 * width : count;

				kotek::uint32_t left = start;
				kotek::uint32_t right = middle;
				kotek::uint32_t output = start;

				while (left < middle && right < end)
				{
					if (zircon_csg_compare_vertices(
							p_vertices[p_order[left]],
							p_vertices[p_order[right]]) <= 0)
					{
						p_scratch[output++] = p_order[left++];
					}
					else
					{
						p_scratch[output++] = p_order[right++];
					}
				}

				while (left < middle)
				{
					p_scratch[output++] = p_order[left++];
				}

				while (right < end)
				{
					p_scratch[output++] = p_order[right++];
				}
			}

			for (kotek::uint32_t i = 0; i < count; ++i)
			{
				p_order[i] = p_scratch[i];
			}
		}
	}

	// the sort-based cluster weld + fan triangulation into m_mesh.
	// Weld key = the position within weld_eps per axis of the cluster's
	// first (x-sorted) member. The x-sort makes within-eps-x runs
	// consecutive, so the scan breaks when x exceeds eps; a candidate
	// inside the x band whose y or z mismatches is SKIPPED, not
	// terminal (y/z are not sorted within the band) — it becomes its
	// own cluster representative later (the u32f lesson of
	// 2026-09-21: two seam vertices computed through different split
	// histories can sit quanta apart in x with another vertex's x
	// between them). Comparing against the cluster's FIRST member (not
	// the previous one) bounds the cluster diameter to 2 eps with no
	// chaining; the representative is the first member in sort order,
	// so the welded positions are deterministic BITS.
	template <typename Scalar>
	inline void zircon_csg_weld_and_build(
		zircon_csg_evaluation_t<Scalar>& evaluation) noexcept
	{
		using traits_t = zircon_csg_scalar_traits<Scalar>;

		const kotek::uint32_t vertex_count =
			static_cast<kotek::uint32_t>(
				evaluation.m_soup.m_vertices.size());

		if (vertex_count == 0)
			return;

		evaluation.m_weld_order.clear();
		evaluation.m_weld_scratch.clear();
		evaluation.m_weld_map.clear();

		for (kotek::uint32_t i = 0; i < vertex_count; ++i)
		{
			evaluation.m_weld_order.push_back(0);
			evaluation.m_weld_scratch.push_back(0);
			// the unassigned sentinel
			evaluation.m_weld_map.push_back(0xffffffffu);
		}

		zircon_csg_merge_sort_indices(
			evaluation.m_soup.m_vertices.data(),
			evaluation.m_weld_order.data(),
			evaluation.m_weld_scratch.data(), vertex_count);

		// the cluster weld: every unassigned vertex in sort order opens
		// a cluster and absorbs every later unassigned vertex within
		// eps on ALL axes (x within eps first — the sort key)
		{
			for (kotek::uint32_t i = 0; i < vertex_count; ++i)
			{
				const kotek::uint32_t representative_index =
					evaluation.m_weld_order[i];

				if (evaluation.m_weld_map[representative_index] !=
				    0xffffffffu)
				{
					// already absorbed by an earlier cluster
					continue;
				}

				const zircon_csg_vec3_t<Scalar>& representative =
					evaluation.m_soup.m_vertices[representative_index];

				const kotek::uint32_t welded_id =
					static_cast<kotek::uint32_t>(
						evaluation.m_mesh.m_positions.size());

				evaluation.m_mesh.m_positions.push_back(representative);
				evaluation.m_weld_map[representative_index] = welded_id;

				for (kotek::uint32_t j = i + 1; j < vertex_count; ++j)
				{
					const kotek::uint32_t candidate_index =
						evaluation.m_weld_order[j];
					const zircon_csg_vec3_t<Scalar>& candidate =
						evaluation.m_soup.m_vertices[candidate_index];

					// x is the sort key: past the band the scan ends
					if (traits_t::difference_within(candidate.m[0],
						    representative.m[0],
						    evaluation.m_weld_eps) == false)
					{
						break;
					}

					if (evaluation.m_weld_map[candidate_index] !=
					    0xffffffffu)
					{
						continue;
					}

					if (traits_t::difference_within(candidate.m[1],
						    representative.m[1],
						    evaluation.m_weld_eps) &&
					    traits_t::difference_within(candidate.m[2],
						    representative.m[2],
						    evaluation.m_weld_eps))
					{
						evaluation.m_weld_map[candidate_index] =
							welded_id;
					}
				}
			}
		}

		// rewrite the polygons on welded ids + fan triangulate
		for (kotek::size_t polygon_index = 0;
		     polygon_index < evaluation.m_soup.m_polygons.size();
		     ++polygon_index)
		{
			const zircon_csg_polygon_t<Scalar>& polygon =
				evaluation.m_soup.m_polygons[polygon_index];

			kotek::uint32_t
				ring[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];
			kotek::uint32_t ring_count = 0;

			for (kotek::uint32_t i = 0; i < polygon.m_vertex_count; ++i)
			{
				const kotek::uint32_t welded_id =
					evaluation.m_weld_map
						[polygon.m_vertex_start + i];

				if (ring_count > 0 && ring[ring_count - 1] == welded_id)
					continue;

				ring[ring_count++] = welded_id;
			}

			while (ring_count > 1 && ring[0] == ring[ring_count - 1])
			{
				--ring_count;
			}

			// the distinct-id count (a degenerate ring has < 3)
			kotek::uint32_t distinct_count = 0;

			for (kotek::uint32_t i = 0; i < ring_count; ++i)
			{
				bool seen = false;

				for (kotek::uint32_t k = 0; k < i; ++k)
				{
					if (ring[k] == ring[i])
					{
						seen = true;
						break;
					}
				}

				if (seen == false)
					++distinct_count;
			}

			if (distinct_count < 3)
			{
				++evaluation.m_weld_degenerate_drop_count;
				continue;
			}

			// the post-weld area (zero-area spurs are dropped too)
			zircon_csg_vec3_t<Scalar>
				position_ring[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];

			for (kotek::uint32_t i = 0; i < ring_count; ++i)
			{
				position_ring[i] =
					evaluation.m_mesh.m_positions[ring[i]];
			}

			if (zircon_csg_ring_area_double(
					position_ring, ring_count) <
			    evaluation.m_sliver_area_eps)
			{
				++evaluation.m_weld_degenerate_drop_count;
				continue;
			}

			// the fan (convex ring): (0, k, k+1) — triangle and index
			// caps hold by construction (triangles <= soup vertices),
			// guarded defensively anyway
			for (kotek::uint32_t k = 1; k + 1 < ring_count; ++k)
			{
				if (evaluation.m_mesh.m_normals.size() >=
				        ZIRCON_DEF_CSG_MAX_TRIANGLES_PER_EVALUATION ||
				    evaluation.m_mesh.m_indices.size() + 3 >
				        ZIRCON_DEF_CSG_MAX_INDICES_PER_EVALUATION)
				{
					++evaluation.m_capacity_drop_count;
					evaluation.m_status = eZirconCsgEvaluationStatus::
						kError_CapacityVertices;
					break;
				}

				evaluation.m_mesh.m_indices.push_back(ring[0]);
				evaluation.m_mesh.m_indices.push_back(ring[k]);
				evaluation.m_mesh.m_indices.push_back(ring[k + 1]);
				evaluation.m_mesh.m_normals.push_back(
					polygon.m_normal);
				evaluation.m_mesh.m_materials.push_back(
					polygon.m_material_id);
			}
		}
	}
} // namespace zircon_csg_detail

// ------------------------------------------------------------------
// the public entry points
// ------------------------------------------------------------------

// evaluates a whole compound: the additive primitives are unioned
// N-way (each face clipped against every other additive brush), then
// the subtractive primitives carve the union soup in MEMBER ORDER (the
// Valve-carve semantic — order-dependent by design, documented in the
// banner). A compound of only subtractive primitives evaluates to the
// empty mesh (there is nothing to bite). The mesh lands in
// evaluation.m_mesh; the status reports loud degradations (capacity
// guards) alongside the hard errors.
template <typename Scalar>
inline eZirconCsgEvaluationStatus zircon_csg_evaluate_compound(
	const zircon_csg_primitive_desc_t<Scalar>* p_primitives,
	kotek::uint32_t primitive_count,
	zircon_csg_evaluation_t<Scalar>& evaluation) noexcept
{
	evaluation.reset();

	if (p_primitives == nullptr)
	{
		KOTEK_MESSAGE_WARNING("[csg] null primitive array");
		return eZirconCsgEvaluationStatus::kError_InvalidArguments;
	}

	if (primitive_count == 0)
	{
		KOTEK_MESSAGE_WARNING("[csg] empty compound");
		return eZirconCsgEvaluationStatus::kError_EmptyCompound;
	}

	if (primitive_count > ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] {} primitives exceed the compound cap {}",
			primitive_count,
			static_cast<kotek::uint32_t>(
				ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND));
		return eZirconCsgEvaluationStatus::kError_TooManyPrimitives;
	}

	kotek::uint32_t
		additive_indices[ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND];
	kotek::uint32_t
		subtractive_indices[ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND];
	kotek::uint32_t additive_count = 0;
	kotek::uint32_t subtractive_count = 0;

	for (kotek::uint32_t i = 0; i < primitive_count; ++i)
	{
		const eZirconCsgEvaluationStatus validation =
			zircon_csg_detail::zircon_csg_validate_desc(
				p_primitives[i]);

		if (validation != eZirconCsgEvaluationStatus::kSuccess)
			return validation;

		if (p_primitives[i].m_operation &
		    zircon_DEF_CSG_OPERATION_SUBTRACTIVE)
		{
			subtractive_indices[subtractive_count++] = i;
		}
		else
		{
			additive_indices[additive_count++] = i;
		}
	}

	if (additive_count == 0)
	{
		// a pure-subtractive compound is the empty mesh by design
		return evaluation.m_status;
	}

	zircon_csg_detail::zircon_csg_compute_epsilons(
		p_primitives, primitive_count, evaluation);

	for (kotek::uint32_t i = 0; i < primitive_count; ++i)
	{
		zircon_csg_brush_t<Scalar> brush;

		if (zircon_csg_build_brush(p_primitives[i], brush) == false)
		{
			// unreachable past the validation — kept for safety
			return eZirconCsgEvaluationStatus::kError_InvalidArguments;
		}

		evaluation.m_brushes.push_back(brush);
	}

	for (kotek::uint32_t additive = 0; additive < additive_count;
	     ++additive)
	{
		const kotek::uint32_t brush_index = additive_indices[additive];
		const zircon_csg_brush_t<Scalar>& brush =
			evaluation.m_brushes[brush_index];

		for (kotek::uint32_t face_index = 0;
		     face_index < brush.m_faces.size(); ++face_index)
		{
			zircon_csg_detail::zircon_csg_union_face(evaluation,
				brush_index, face_index, additive_indices,
				additive_count);
		}
	}

	kotek::uint32_t
		previous_subtractives[ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND];
	kotek::uint32_t previous_subtractive_count = 0;

	for (kotek::uint32_t subtractive = 0;
	     subtractive < subtractive_count; ++subtractive)
	{
		const kotek::uint32_t brush_index =
			subtractive_indices[subtractive];

		zircon_csg_detail::zircon_csg_subtract_cut_soup(
			evaluation, evaluation.m_brushes[brush_index]);

		zircon_csg_detail::zircon_csg_subtract_cavity_walls(evaluation,
			brush_index, additive_indices, additive_count,
			previous_subtractives, previous_subtractive_count);

		previous_subtractives[previous_subtractive_count++] =
			brush_index;
	}

	zircon_csg_detail::zircon_csg_weld_and_build(evaluation);

	return evaluation.m_status;
}

// the brush-PAIR boolean (the plan's unit proofs): kUnion and
// kSubtract ride the compound machinery (the pair's operation flags
// are overridden — kUnion = the union of the two, kSubtract = first
// minus second, regardless of the descriptors' own flags);
// kIntersect is the pairwise inside-clip (not a compound semantic).
template <typename Scalar>
inline eZirconCsgEvaluationStatus zircon_csg_evaluate_pair(
	const zircon_csg_primitive_desc_t<Scalar>& first,
	const zircon_csg_primitive_desc_t<Scalar>& second,
	eZirconCsgBoolean operation,
	zircon_csg_evaluation_t<Scalar>& evaluation) noexcept
{
	if (operation != eZirconCsgBoolean::kIntersect)
	{
		zircon_csg_primitive_desc_t<Scalar> descs[2] = {first, second};

		descs[0].m_operation = 0;

		if (operation == eZirconCsgBoolean::kUnion)
		{
			descs[1].m_operation = 0;
		}
		else
		{
			descs[1].m_operation = zircon_DEF_CSG_OPERATION_SUBTRACTIVE;
		}

		return zircon_csg_evaluate_compound(descs, 2, evaluation);
	}

	evaluation.reset();

	const eZirconCsgEvaluationStatus validation_first =
		zircon_csg_detail::zircon_csg_validate_desc(first);
	if (validation_first != eZirconCsgEvaluationStatus::kSuccess)
		return validation_first;

	const eZirconCsgEvaluationStatus validation_second =
		zircon_csg_detail::zircon_csg_validate_desc(second);
	if (validation_second != eZirconCsgEvaluationStatus::kSuccess)
		return validation_second;

	const zircon_csg_primitive_desc_t<Scalar> descs[2] = {first,
		second};

	zircon_csg_detail::zircon_csg_compute_epsilons(descs, 2, evaluation);

	for (kotek::uint32_t i = 0; i < 2; ++i)
	{
		zircon_csg_brush_t<Scalar> brush;

		if (zircon_csg_build_brush(descs[i], brush) == false)
		{
			return eZirconCsgEvaluationStatus::kError_InvalidArguments;
		}

		evaluation.m_brushes.push_back(brush);
	}

	zircon_csg_detail::zircon_csg_intersect_pair(evaluation);

	zircon_csg_detail::zircon_csg_weld_and_build(evaluation);

	return evaluation.m_status;
}
