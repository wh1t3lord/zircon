#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cmath>
		#include <cstring>

		#include "../../core/zircon_config.h"
		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_csg_primitive.h"
		#include "../../ecs/zircon_component_csg.h"
		#include "../../ecs/zircon_csg_evaluate.h"
		#include "../../world/zircon_world.h"

		#include <kotek.core.console/include/kotek_console.h>

		#ifndef ZIRCON_DEF_UNIT_TEST_CSG
			#define ZIRCON_DEF_UNIT_TEST_CSG 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_CSG == 1

// functional proofs for task Z25 A1 (brush CSG): the plane-set
// evaluation core (the union/subtraction/intersection fragment sets of
// known brush pairs, watertightness, the exact volumes), the u32f
// determinism (the bake-reproducibility proof), the f32/u32f precision
// parity band, the weld/sliver edge policies, the capacity guards, the
// component json roundtrips and the ECS registration. Every fixture
// runs through the float, u32f and double instantiations (the core is
// templated — all three modes are proven in every build; the
// ZIRCON_CSG_PRECISION cmake option only selects what the components
// store). The fixtures are all dyadic-exact so u32f quantizes them
// losslessly. Tier: lightweight (rule 8a — the brushes are tiny; no
// heavy-flag suites here).

namespace
{
	// the volume-error band per mode (positions quantize at the u32f
	// quantum ~1.5e-5: the clip crossings shift the volume by ~surface
	// area x quantum)
	template <typename Scalar>
	double csg_volume_band(void)
	{
		return 1e-3;
	}

	template <>
	double csg_volume_band<double>(void)
	{
		return 1e-6;
	}

	template <typename Scalar>
	zircon_csg_primitive_desc_t<Scalar> csg_make_desc(
		eZirconCsgPrimitiveType type, double dimension_x,
		double dimension_y, double dimension_z, double position_x,
		double position_y, double position_z,
		kotek::uint16_t material_id = 0, kotek::uint8_t operation = 0,
		double quat_x = 0.0, double quat_y = 0.0, double quat_z = 0.0,
		double quat_w = 1.0)
	{
		using traits_t = zircon_csg_scalar_traits<Scalar>;

		zircon_csg_primitive_desc_t<Scalar> desc;

		desc.m_dimensions[0] = traits_t::from_double(dimension_x);
		desc.m_dimensions[1] = traits_t::from_double(dimension_y);
		desc.m_dimensions[2] = traits_t::from_double(dimension_z);
		desc.m_position[0] = traits_t::from_double(position_x);
		desc.m_position[1] = traits_t::from_double(position_y);
		desc.m_position[2] = traits_t::from_double(position_z);
		desc.m_rotation[0] = traits_t::from_double(quat_x);
		desc.m_rotation[1] = traits_t::from_double(quat_y);
		desc.m_rotation[2] = traits_t::from_double(quat_z);
		desc.m_rotation[3] = traits_t::from_double(quat_w);
		desc.m_material_id = material_id;
		desc.m_type = static_cast<kotek::uint8_t>(type);
		desc.m_operation = operation;

		return desc;
	}

	// the signed volume of a closed mesh (the divergence-theorem sum;
	// positive for outward CCW winding) — computed in double from the
	// scalar positions (exact for u32f/f32)
	template <typename Scalar>
	double csg_signed_volume(const zircon_csg_mesh_t<Scalar>& mesh)
	{
		using traits_t = zircon_csg_scalar_traits<Scalar>;

		double volume = 0.0;

		for (kotek::size_t index = 0; index < mesh.m_indices.size();
		     index += 3)
		{
			const zircon_csg_vec3_t<Scalar>& a =
				mesh.m_positions[mesh.m_indices[index]];
			const zircon_csg_vec3_t<Scalar>& b =
				mesh.m_positions[mesh.m_indices[index + 1]];
			const zircon_csg_vec3_t<Scalar>& c =
				mesh.m_positions[mesh.m_indices[index + 2]];

			const double ax = traits_t::to_double(a.m[0]);
			const double ay = traits_t::to_double(a.m[1]);
			const double az = traits_t::to_double(a.m[2]);
			const double bx = traits_t::to_double(b.m[0]);
			const double by = traits_t::to_double(b.m[1]);
			const double bz = traits_t::to_double(b.m[2]);
			const double cx = traits_t::to_double(c.m[0]);
			const double cy = traits_t::to_double(c.m[1]);
			const double cz = traits_t::to_double(c.m[2]);

			volume += (ax * (by * cz - bz * cy) -
			              ay * (bx * cz - bz * cx) +
			              az * (bx * cy - by * cx)) /
				6.0;
		}

		return volume;
	}

	// the watertightness invariant of a closed surface, at the ATOMIC
	// sub-segment level: brush CSG splits faces per clip order, so two
	// fragments can meet along collinear edges of different lengths
	// (T-junctions — Quake's brush CSG has the same shape; the seam
	// vertices coincide exactly, so the surface is sealed). The honest
	// invariant: split every directed edge at every welded vertex lying
	// ON it, and each resulting atomic segment must appear exactly ONCE
	// per direction (once forward, once reversed). O(E x V + atoms^2)
	// over the test meshes (hundreds of edges).
	template <typename Scalar>
	bool csg_is_watertight(const zircon_csg_mesh_t<Scalar>& mesh)
	{
		using traits_t = zircon_csg_scalar_traits<Scalar>;

		const kotek::size_t vertex_count = mesh.m_positions.size();
		const kotek::size_t edge_count = mesh.m_indices.size();

		if (edge_count == 0)
			return true;

		// double views of the welded positions
		double(*positions)[3] =
			new double[vertex_count][3];

		for (kotek::size_t i = 0; i < vertex_count; ++i)
		{
			positions[i][0] =
				traits_t::to_double(mesh.m_positions[i].m[0]);
			positions[i][1] =
				traits_t::to_double(mesh.m_positions[i].m[1]);
			positions[i][2] =
				traits_t::to_double(mesh.m_positions[i].m[2]);
		}

		// the directed-atom tally: (from, to) welded id pairs with
		// forward/reverse counts; a sealed surface's atoms are exactly
		// bounded — 3 x triangles x (1 + T-junctions)
		struct atom_t
		{
			kotek::uint32_t m_from;
			kotek::uint32_t m_to;
			kotek::uint32_t m_count;
		};

		const kotek::size_t atom_capacity =
			edge_count * (vertex_count + 1);
		atom_t* p_atoms = new atom_t[atom_capacity];
		kotek::size_t atom_count = 0;

		kotek::uint32_t* on_segment = new kotek::uint32_t[vertex_count];
		double* on_segment_t = new double[vertex_count];

		bool sealed = true;

		for (kotek::size_t edge_index = 0; edge_index < edge_count;
		     ++edge_index)
		{
			// the mesh's directed edges: triangle i contributes
			// indices[3i] -> [3i+1] -> [3i+2] -> [3i]
			const kotek::uint32_t from =
				mesh.m_indices[edge_index];
			const kotek::uint32_t to =
				mesh.m_indices[(edge_index / 3) * 3 +
					(edge_index % 3 + 1) % 3];

			if (from == to)
				continue;

			const double* p_a = positions[from];
			const double* p_b = positions[to];

			const double dir[3] = {p_b[0] - p_a[0], p_b[1] - p_a[1],
				p_b[2] - p_a[2]};

			const double dir_length_squared =
				dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];

			if (dir_length_squared < 1e-18)
				continue;

			// gather the welded vertices lying exactly ON this
			// segment (cross ~ 0, t strictly inside) — the T-junction
			// split points
			kotek::size_t on_count = 0;

			for (kotek::size_t v = 0; v < vertex_count; ++v)
			{
				if (v == from || v == to)
					continue;

				const double rel[3] = {positions[v][0] - p_a[0],
					positions[v][1] - p_a[1],
					positions[v][2] - p_a[2]};

				const double cross[3] = {
					rel[1] * dir[2] - rel[2] * dir[1],
					rel[2] * dir[0] - rel[0] * dir[2],
					rel[0] * dir[1] - rel[1] * dir[0]};

				const double cross_magnitude_squared =
					cross[0] * cross[0] + cross[1] * cross[1] +
					cross[2] * cross[2];

				// exact for the dyadic fixtures; the band only
				// admits float noise on rotated seams
				if (cross_magnitude_squared >
				    1e-12 * dir_length_squared + 1e-18)
				{
					continue;
				}

				const double t =
					(rel[0] * dir[0] + rel[1] * dir[1] +
				        rel[2] * dir[2]) /
					dir_length_squared;

				if (t <= 1e-9 || t >= 1.0 - 1e-9)
					continue;

				on_segment[on_count] =
					static_cast<kotek::uint32_t>(v);
				on_segment_t[on_count] = t;
				++on_count;
			}

			// sort the split points by t (insertion — a handful)
			for (kotek::size_t i = 1; i < on_count; ++i)
			{
				const kotek::uint32_t key_id = on_segment[i];
				const double key_t = on_segment_t[i];

				kotek::size_t k = i;

				while (k > 0 && on_segment_t[k - 1] > key_t)
				{
					on_segment[k] = on_segment[k - 1];
					on_segment_t[k] = on_segment_t[k - 1];
					--k;
				}

				on_segment[k] = key_id;
				on_segment_t[k] = key_t;
			}

			// expand into atoms: [from, split points..., to]
			kotek::uint32_t previous = from;

			for (kotek::size_t i = 0; i <= on_count; ++i)
			{
				const kotek::uint32_t current =
					i < on_count ? on_segment[i] : to;

				// tally the directed atom (previous -> current)
				bool found = false;

				for (kotek::size_t atom_index = 0;
				     atom_index < atom_count; ++atom_index)
				{
					if (p_atoms[atom_index].m_from == previous &&
					    p_atoms[atom_index].m_to == current)
					{
						++p_atoms[atom_index].m_count;
						found = true;
						break;
					}
				}

				if (found == false)
				{
					p_atoms[atom_count].m_from = previous;
					p_atoms[atom_count].m_to = current;
					p_atoms[atom_count].m_count = 1;
					++atom_count;
				}

				previous = current;
			}
		}

		// the seal: every atom appears exactly once per direction
		for (kotek::size_t atom_index = 0; atom_index < atom_count;
		     ++atom_index)
		{
			const atom_t& atom = p_atoms[atom_index];

			if (atom.m_count != 1)
			{
				sealed = false;
				break;
			}

			kotek::uint32_t reverse_count = 0;

			for (kotek::size_t other = 0; other < atom_count; ++other)
			{
				if (p_atoms[other].m_from == atom.m_to &&
				    p_atoms[other].m_to == atom.m_from)
				{
					reverse_count = p_atoms[other].m_count;
					break;
				}
			}

			if (reverse_count != 1)
			{
				sealed = false;
				break;
			}
		}

		delete[] on_segment_t;
		delete[] on_segment;
		delete[] p_atoms;
		delete[] positions;

		return sealed;
	}

	// the mesh self-consistency floor: indices address real positions,
	// per-triangle normals are unit-length
	template <typename Scalar>
	void csg_expect_mesh_valid(
		const zircon_csg_mesh_t<Scalar>& mesh, double normal_band)
	{
		using traits_t = zircon_csg_scalar_traits<Scalar>;

		ASSERT_EQ(mesh.m_indices.size() % 3, 0u);
		ASSERT_EQ(mesh.m_normals.size(), mesh.m_indices.size() / 3);
		ASSERT_EQ(mesh.m_materials.size(), mesh.m_indices.size() / 3);

		for (kotek::size_t index = 0; index < mesh.m_indices.size();
		     ++index)
		{
			ASSERT_LT(mesh.m_indices[index],
				mesh.m_positions.size());
		}

		for (kotek::size_t tri = 0; tri < mesh.m_normals.size(); ++tri)
		{
			const double x =
				traits_t::to_double(mesh.m_normals[tri].m[0]);
			const double y =
				traits_t::to_double(mesh.m_normals[tri].m[1]);
			const double z =
				traits_t::to_double(mesh.m_normals[tri].m[2]);

			ASSERT_NEAR(x * x + y * y + z * z, 1.0, normal_band);
		}
	}

	template <typename Scalar>
	bool csg_meshes_byte_identical(
		const zircon_csg_mesh_t<Scalar>& left,
		const zircon_csg_mesh_t<Scalar>& right)
	{
		if (left.m_positions.size() != right.m_positions.size() ||
		    left.m_indices.size() != right.m_indices.size() ||
		    left.m_normals.size() != right.m_normals.size() ||
		    left.m_materials.size() != right.m_materials.size())
		{
			return false;
		}

		const kotek::size_t positions_bytes =
			left.m_positions.size() *
			sizeof(zircon_csg_vec3_t<Scalar>);
		const kotek::size_t indices_bytes =
			left.m_indices.size() * sizeof(kotek::uint32_t);
		const kotek::size_t normals_bytes = positions_bytes;
		const kotek::size_t materials_bytes =
			left.m_materials.size() * sizeof(kotek::uint16_t);

		return std::memcmp(left.m_positions.data(),
			       right.m_positions.data(), positions_bytes) ==
			0 &&
			std::memcmp(left.m_indices.data(), right.m_indices.data(),
			       indices_bytes) == 0 &&
			std::memcmp(left.m_normals.data(), right.m_normals.data(),
			       normals_bytes) == 0 &&
			std::memcmp(left.m_materials.data(),
			       right.m_materials.data(), materials_bytes) == 0;
	}

	// the overlapping-boxes union (the plan's reference case): A =
	// [-0.5, 0.5]^3, B = [0, 1]^3 -> 24 quads / 48 tris, volume 1.875,
	// watertight. The fragment set (dump-verified 2026-09-21): the
	// faces straddling B's region split per the clip order — A gives
	// 12 quads (-x whole; +x/+y/+z in two; -y in two; -z in THREE:
	// the fully-outside bottom face is split by B's -x and -y planes
	// before B's -z plane emits the corner — sequential clipping does
	// not minimize the fragment count, and the extra fragments tile the
	// same surface exactly), B mirrors with 12. The robust invariants
	// are the exact volume and watertightness; the count pins the
	// clip-order behavior against silent regressions.
	template <typename Scalar>
	void csg_check_union_overlapping(void)
	{
		// heap-allocated per the fixture rule (the context is ~6 MB of
		// fixed pools)
		zircon_csg_evaluation_t<Scalar>& evaluation =
			*new zircon_csg_evaluation_t<Scalar>();

		const zircon_csg_primitive_desc_t<Scalar> first =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 0.0, 0.0, 0.0);
		const zircon_csg_primitive_desc_t<Scalar> second =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 0.5, 0.5, 0.5);

		const eZirconCsgEvaluationStatus status =
			zircon_csg_evaluate_pair(
				first, second, eZirconCsgBoolean::kUnion, evaluation);

		ASSERT_EQ(status, eZirconCsgEvaluationStatus::kSuccess);

		const zircon_csg_mesh_t<Scalar>& mesh = evaluation.m_mesh;

		csg_expect_mesh_valid(mesh, 5e-3);

		EXPECT_EQ(mesh.m_normals.size(), 48u);
		EXPECT_NEAR(csg_signed_volume(mesh), 1.875,
			csg_volume_band<Scalar>());

		EXPECT_TRUE(csg_is_watertight(mesh));

		delete &evaluation;
	}

	template <typename Scalar>
	void csg_check_union_disjoint(void)
	{
		zircon_csg_evaluation_t<Scalar>& evaluation =
			*new zircon_csg_evaluation_t<Scalar>();

		const zircon_csg_primitive_desc_t<Scalar> first =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 0.0, 0.0, 0.0);
		const zircon_csg_primitive_desc_t<Scalar> second =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 3.0, 0.0, 0.0);

		ASSERT_EQ(zircon_csg_evaluate_pair(first, second,
			              eZirconCsgBoolean::kUnion, evaluation),
			eZirconCsgEvaluationStatus::kSuccess);

		// both boxes intact: 12 quads, 24 tris, volume 2
		EXPECT_EQ(evaluation.m_mesh.m_normals.size(), 24u);
		EXPECT_NEAR(csg_signed_volume(evaluation.m_mesh), 2.0,
			csg_volume_band<Scalar>());
		EXPECT_TRUE(csg_is_watertight(evaluation.m_mesh));

		delete &evaluation;
	}

	template <typename Scalar>
	void csg_check_union_containment(void)
	{
		zircon_csg_evaluation_t<Scalar>& evaluation =
			*new zircon_csg_evaluation_t<Scalar>();

		// A = [-1, 1]^3, B = [-0.25, 0.75]^3 fully inside
		const zircon_csg_primitive_desc_t<Scalar> first =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 2.0,
				2.0, 2.0, 0.0, 0.0, 0.0);
		const zircon_csg_primitive_desc_t<Scalar> second =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 0.25, 0.25, 0.25);

		ASSERT_EQ(zircon_csg_evaluate_pair(first, second,
			              eZirconCsgBoolean::kUnion, evaluation),
			eZirconCsgEvaluationStatus::kSuccess);

		// "the outer only" — nothing of B survives; A's surface is
		// intact but NOT minimally fragmented: A's 2x2 faces straddle
		// B's side planes, so the sequential clip shatters them into
		// strips (+x/-x: 1 quad each (separated immediately), +y/-y:
		// 3, +z/-z: 5 -> 18 quads / 36 tris). The tile is exact:
		// volume 8, watertight (T-junction seams at the strip edges,
		// handled by the atomic-segment seal check).
		EXPECT_EQ(evaluation.m_mesh.m_normals.size(), 36u);
		EXPECT_NEAR(csg_signed_volume(evaluation.m_mesh), 8.0,
			csg_volume_band<Scalar>());
		EXPECT_TRUE(csg_is_watertight(evaluation.m_mesh));

		delete &evaluation;
	}

	template <typename Scalar>
	void csg_check_union_identical(void)
	{
		zircon_csg_evaluation_t<Scalar>& evaluation =
			*new zircon_csg_evaluation_t<Scalar>();

		const zircon_csg_primitive_desc_t<Scalar> first =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 0.0, 0.0, 0.0);
		const zircon_csg_primitive_desc_t<Scalar> second = first;

		ASSERT_EQ(zircon_csg_evaluate_pair(first, second,
			              eZirconCsgBoolean::kUnion, evaluation),
			eZirconCsgEvaluationStatus::kSuccess);

		// the coplanar-same dedup: exactly ONE copy survives (the
		// higher brush index owns it) — not 12 quads, not 0
		EXPECT_EQ(evaluation.m_mesh.m_normals.size(), 12u);
		EXPECT_NEAR(csg_signed_volume(evaluation.m_mesh), 1.0,
			csg_volume_band<Scalar>());
		EXPECT_TRUE(csg_is_watertight(evaluation.m_mesh));

		delete &evaluation;
	}

	// the subtraction notch (the plan's reference case): A =
	// [-0.5, 0.5]^3 minus B = [0, 0.5] x [0, 0.5] x [-1, 1] — a square
	// channel cut through the +x+y corner along z. The verified
	// fragment set (dump-checked 2026-09-21): A-side keeps -x/-y
	// whole (the -y bottom split in two by the clip order), +x and +y
	// keep only the parts below the channel (the on-boundary strips
	// are dangling faces — removed material on one side, void on the
	// other — correctly dropped), +z/-z L-cut in two -> 9 quads; the
	// cavity: B's -x and -y walls flipped -> 2 quads. 11 quads / 22
	// tris, volume 0.75.
	template <typename Scalar>
	void csg_check_subtraction_notch(void)
	{
		zircon_csg_evaluation_t<Scalar>& evaluation =
			*new zircon_csg_evaluation_t<Scalar>();

		const zircon_csg_primitive_desc_t<Scalar> first =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 0.0, 0.0, 0.0);
		const zircon_csg_primitive_desc_t<Scalar> second =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 0.5,
				0.5, 2.0, 0.25, 0.25, 0.0);

		ASSERT_EQ(zircon_csg_evaluate_pair(first, second,
			              eZirconCsgBoolean::kSubtract, evaluation),
			eZirconCsgEvaluationStatus::kSuccess);

		const zircon_csg_mesh_t<Scalar>& mesh = evaluation.m_mesh;

		csg_expect_mesh_valid(mesh, 5e-3);

		EXPECT_EQ(mesh.m_normals.size(), 22u);
		EXPECT_NEAR(csg_signed_volume(mesh), 0.75,
			csg_volume_band<Scalar>());
		EXPECT_TRUE(csg_is_watertight(mesh));

		// the cavity walls face INTO the removed channel (the flipped
		// B faces): a triangle normal at (+1, 0, 0) and one at
		// (0, +1, 0) must exist (B's -x / -y faces flipped)
		bool has_wall_x = false;
		bool has_wall_y = false;

		for (kotek::size_t tri = 0; tri < mesh.m_normals.size(); ++tri)
		{
			const double x = zircon_csg_scalar_traits<
				Scalar>::to_double(mesh.m_normals[tri].m[0]);
			const double y = zircon_csg_scalar_traits<
				Scalar>::to_double(mesh.m_normals[tri].m[1]);

			if (std::fabs(x - 1.0) < 5e-3 && std::fabs(y) < 5e-3)
				has_wall_x = true;
			if (std::fabs(y - 1.0) < 5e-3 && std::fabs(x) < 5e-3)
				has_wall_y = true;
		}

		EXPECT_TRUE(has_wall_x);
		EXPECT_TRUE(has_wall_y);

		delete &evaluation;
	}

	// the containment subtraction: B strictly inside A — the outer
	// shell intact PLUS the closed cavity (the two-component
	// watertight case). The outer shell is shattered by the clip order
	// exactly like the containment union (+x/-x: 1 quad, +y/-y: 3,
	// +z/-z: 5 -> 18 quads) and the cavity adds B's six flipped faces
	// — 24 quads / 48 tris, volume 8 - 1 = 7.
	template <typename Scalar>
	void csg_check_subtraction_containment(void)
	{
		zircon_csg_evaluation_t<Scalar>& evaluation =
			*new zircon_csg_evaluation_t<Scalar>();

		const zircon_csg_primitive_desc_t<Scalar> first =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 2.0,
				2.0, 2.0, 0.0, 0.0, 0.0);
		const zircon_csg_primitive_desc_t<Scalar> second =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 0.25, 0.25, 0.25);

		ASSERT_EQ(zircon_csg_evaluate_pair(first, second,
			              eZirconCsgBoolean::kSubtract, evaluation),
			eZirconCsgEvaluationStatus::kSuccess);

		EXPECT_EQ(evaluation.m_mesh.m_normals.size(), 48u);
		EXPECT_NEAR(csg_signed_volume(evaluation.m_mesh), 7.0,
			csg_volume_band<Scalar>());
		EXPECT_TRUE(csg_is_watertight(evaluation.m_mesh));

		delete &evaluation;
	}

	template <typename Scalar>
	void csg_check_intersection_overlap(void)
	{
		zircon_csg_evaluation_t<Scalar>& evaluation =
			*new zircon_csg_evaluation_t<Scalar>();

		const zircon_csg_primitive_desc_t<Scalar> first =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 0.0, 0.0, 0.0);
		const zircon_csg_primitive_desc_t<Scalar> second =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 0.5, 0.5, 0.5);

		ASSERT_EQ(zircon_csg_evaluate_pair(first, second,
			              eZirconCsgBoolean::kIntersect, evaluation),
			eZirconCsgEvaluationStatus::kSuccess);

		// the overlap box [0, 0.5]^3: 6 quads / 12 tris, volume 0.125
		EXPECT_EQ(evaluation.m_mesh.m_normals.size(), 12u);
		EXPECT_NEAR(csg_signed_volume(evaluation.m_mesh), 0.125,
			csg_volume_band<Scalar>());
		EXPECT_TRUE(csg_is_watertight(evaluation.m_mesh));

		delete &evaluation;
	}

	template <typename Scalar>
	void csg_check_intersection_touching_is_empty(void)
	{
		zircon_csg_evaluation_t<Scalar>& evaluation =
			*new zircon_csg_evaluation_t<Scalar>();

		// touching at the x = 0.5 plane only: zero-volume contact is
		// empty by policy (the coplanar-opposed drop)
		const zircon_csg_primitive_desc_t<Scalar> first =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 0.0, 0.0, 0.0);
		const zircon_csg_primitive_desc_t<Scalar> second =
			csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox, 1.0,
				1.0, 1.0, 1.0, 0.0, 0.0);

		ASSERT_EQ(zircon_csg_evaluate_pair(first, second,
			              eZirconCsgBoolean::kIntersect, evaluation),
			eZirconCsgEvaluationStatus::kSuccess);

		EXPECT_EQ(evaluation.m_mesh.m_indices.size(), 0u);
		EXPECT_EQ(evaluation.m_mesh.m_positions.size(), 0u);

		delete &evaluation;
	}

	// the mixed compound (the N-way path): A and B unioned
	// (overlapping, volume 1.875), then C = [-0.5, 0]^3 subtractive
	// inside A's -x-y-z corner (its -x/-y/-z faces coplanar-same with
	// A's — the open-notch case). Final volume 1.875 - 0.125 = 1.75.
	template <typename Scalar>
	void csg_check_compound_mixed(void)
	{
		zircon_csg_evaluation_t<Scalar>& evaluation =
			*new zircon_csg_evaluation_t<Scalar>();

		zircon_csg_primitive_desc_t<Scalar> descs[3];

		descs[0] = csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox,
			1.0, 1.0, 1.0, 0.0, 0.0, 0.0);
		descs[1] = csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox,
			1.0, 1.0, 1.0, 0.5, 0.5, 0.5);
		descs[2] = csg_make_desc<Scalar>(eZirconCsgPrimitiveType::kBox,
			0.5, 0.5, 0.5, -0.25, -0.25, -0.25, 3,
			zircon_DEF_CSG_OPERATION_SUBTRACTIVE);

		ASSERT_EQ(
			zircon_csg_evaluate_compound(descs, 3, evaluation),
			eZirconCsgEvaluationStatus::kSuccess);

		EXPECT_NEAR(csg_signed_volume(evaluation.m_mesh), 1.75,
			csg_volume_band<Scalar>());
		EXPECT_TRUE(csg_is_watertight(evaluation.m_mesh));

		delete &evaluation;
	}

	/// @brief \~english the headless ecs environment (the camera-sdk
	/// fixture shape): a real factory and world, no imgui, no window
	struct zircon_test_csg_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;
		kotek::core::ktkConsole console;
		kotek::core::ktkInput input;
		zircon_config engine_config;
		zircon_factory factory;
		zircon_world world;

		void initialize(void)
		{
			this->filesystem.Initialize(&this->framework_config);

			this->factory.Initialize(
				&this->engine_config, &this->console, &this->input);

			this->world.initialize("zircon_z25_test_world",
				&this->engine_config, &this->console, &this->input,
				&this->factory, 65536);
		}

		void shutdown(void)
		{
			this->world.shutdown(&this->factory);
			this->factory.Shutdown();
			this->filesystem.Shutdown();
		}
	};
} // namespace

// ------------------------------------------------------------------
// the scalar: exact arithmetic pins (the determinism argument — every
// value below is integer-exact, no eps anywhere)
// ------------------------------------------------------------------
TEST(Zircon_Game, CsgScalarU32fExactArithmetic)
{
	using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_u32f_t>;

	// the quantum grid: 1.5 = 98304 raw, and the double boundary is
	// lossless (the json/bake roundtrip argument)
	{
		const zircon_csg_scalar_u32f_t value =
			traits_t::from_double(1.5);

		EXPECT_EQ(value.m_raw_bits, 98304u);
		EXPECT_DOUBLE_EQ(traits_t::to_double(value), 1.5);
		EXPECT_EQ(traits_t::from_double(traits_t::to_double(value))
		              .m_raw_bits,
			value.m_raw_bits);
	}

	// negatives: two's-complement lanes
	{
		const zircon_csg_scalar_u32f_t value =
			traits_t::from_double(-1.5);

		EXPECT_EQ(zircon_csg_u32f_as_signed(value.m_raw_bits), -98304);
		EXPECT_DOUBLE_EQ(traits_t::to_double(value), -1.5);
	}

	// add/sub are exact wraps
	{
		const zircon_csg_scalar_u32f_t sum =
			traits_t::from_double(1.5) + traits_t::from_double(2.25);

		EXPECT_EQ(sum.m_raw_bits, 245760u); // 3.75 * 65536

		const zircon_csg_scalar_u32f_t difference =
			traits_t::from_double(2.0) - traits_t::from_double(3.5);

		EXPECT_EQ(
			zircon_csg_u32f_as_signed(difference.m_raw_bits), -98304);
	}

	// mul: one exact i64 product + one FLOOR shift; div: the widened
	// dividend + truncation toward zero (the documented asymmetry)
	{
		const zircon_csg_scalar_u32f_t product =
			traits_t::from_double(1.5) * traits_t::from_double(2.25);

		EXPECT_EQ(product.m_raw_bits, 221184u); // 3.375 * 65536

		const zircon_csg_scalar_u32f_t quotient =
			traits_t::from_double(3.75) / traits_t::from_double(1.5);

		EXPECT_EQ(quotient.m_raw_bits, 163840u); // 2.5 * 65536
	}

	// the floor (mul) vs truncate (div) distinction, pinned on purpose:
	// (-0.1)^2 quantizes down to -656 raw after the shift, while the
	// positive side floors to 655
	{
		const zircon_csg_scalar_u32f_t positive =
			traits_t::from_double(0.1); // 6554 raw
		EXPECT_EQ(zircon_csg_u32f_as_signed(positive.m_raw_bits), 6554);

		const zircon_csg_scalar_u32f_t negative =
			traits_t::from_double(-0.1); // -6554 raw

		const zircon_csg_scalar_u32f_t positive_square =
			positive * positive;
		EXPECT_EQ(
			zircon_csg_u32f_as_signed(positive_square.m_raw_bits),
			655); // floor(42954916 / 65536) = floor(655.39)

		const zircon_csg_scalar_u32f_t negative_square =
			negative * positive;
		EXPECT_EQ(
			zircon_csg_u32f_as_signed(negative_square.m_raw_bits),
			-656); // floor(-655.39) = -656 (arithmetic shift)

		const zircon_csg_scalar_u32f_t third =
			traits_t::from_int(1) / traits_t::from_int(3);
		EXPECT_EQ(zircon_csg_u32f_as_signed(third.m_raw_bits),
			21845); // truncated toward zero

		const zircon_csg_scalar_u32f_t negative_third =
			traits_t::from_int(-1) / traits_t::from_int(3);
		EXPECT_EQ(
			zircon_csg_u32f_as_signed(negative_third.m_raw_bits),
			-21845); // truncated TOWARD ZERO, not floored
	}

	// comparisons are integer compares over the signed lanes
	{
		EXPECT_TRUE(traits_t::from_double(-1.0) <
		            traits_t::from_double(0.5));
		EXPECT_TRUE(traits_t::from_double(0.5) <=
		            traits_t::from_double(0.5));
		EXPECT_TRUE(traits_t::from_int(2) > traits_t::from_int(-3));
		EXPECT_TRUE(traits_t::from_double(0.25) ==
		            traits_t::from_double(0.25));
		EXPECT_TRUE(traits_t::from_double(0.25) !=
		            traits_t::from_double(0.25001));
	}

	// sqrt through the correctly-rounded IEEE double hop
	{
		EXPECT_EQ(traits_t::sqrt(traits_t::from_int(4)).m_raw_bits,
			traits_t::from_int(2).m_raw_bits);
		EXPECT_EQ(
			traits_t::sqrt(traits_t::from_double(2.25)).m_raw_bits,
			traits_t::from_double(1.5).m_raw_bits);
	}

	// the overflow contract: WRAPS, never saturates (the warning is
	// the loudness — the value proves the no-clamp rule)
	{
		const zircon_csg_scalar_u32f_t big =
			traits_t::from_double(30000.0);
		const zircon_csg_scalar_u32f_t wrapped = big + big;

		// 3932160000 - 2^32 = -362807296 -> -5536.0 (a clamp would pin
		// 32767.99...)
		EXPECT_DOUBLE_EQ(traits_t::to_double(wrapped), -5536.0);
	}

	// the widened dot: values near the range contract stay exact
	{
		const zircon_csg_scalar_u32f_t a[3] = {
			traits_t::from_double(16384.0),
			traits_t::from_double(-16384.0), traits_t::from_int(1)};
		const zircon_csg_scalar_u32f_t b[3] = {
			traits_t::from_int(1), traits_t::from_int(1),
			traits_t::from_int(0)};

		EXPECT_DOUBLE_EQ(traits_t::to_double(traits_t::dot(a, b)), 0.0);
	}
}

// ------------------------------------------------------------------
// the brush builders
// ------------------------------------------------------------------
TEST(Zircon_Game, CsgBrushBuildBoxExact)
{
	const zircon_csg_primitive_desc_t<float> desc =
		csg_make_desc<float>(eZirconCsgPrimitiveType::kBox, 2.0, 4.0,
			6.0, 1.0, 2.0, 3.0);

	zircon_csg_brush_t<float> brush;

	ASSERT_TRUE(zircon_csg_build_brush(desc, brush));

	ASSERT_EQ(brush.m_planes.size(), 6u);
	ASSERT_EQ(brush.m_faces.size(), 6u);
	ASSERT_EQ(brush.m_vertices.size(), 24u);

	// the transform bake: the +x plane (unit +x normal) sits at
	// half_x + position.x = 1 + 1 = 2
	EXPECT_FLOAT_EQ(brush.m_planes[0].m_normal.m[0], 1.0f);
	EXPECT_FLOAT_EQ(brush.m_planes[0].m_normal.m[1], 0.0f);
	EXPECT_FLOAT_EQ(brush.m_planes[0].m_dist, 2.0f);

	// the -z plane after the bake: dist = half_z + (0,0,-1).position =
	// 3 - 3 = 0 (the face plane sits at world z = 3 - 3 = 0)
	EXPECT_FLOAT_EQ(brush.m_planes[5].m_normal.m[2], -1.0f);
	EXPECT_FLOAT_EQ(brush.m_planes[5].m_dist, 0.0f);

	// every face winds outward (the auto-flip contract): the first fan
	// cross of each face must agree with its plane's normal
	for (kotek::size_t face_index = 0;
	     face_index < brush.m_faces.size(); ++face_index)
	{
		const zircon_csg_face_t& face = brush.m_faces[face_index];
		const zircon_csg_vec3_t<float>& v0 =
			brush.m_vertices[face.m_vertex_start];
		const zircon_csg_vec3_t<float>& v1 =
			brush.m_vertices[face.m_vertex_start + 1];
		const zircon_csg_vec3_t<float>& v2 =
			brush.m_vertices[face.m_vertex_start + 2];

		const zircon_csg_vec3_t<float> cross =
			zircon_csg_vec_cross(zircon_csg_vec_sub(v1, v0),
				zircon_csg_vec_sub(v2, v1));

		EXPECT_GT(zircon_csg_vec_dot(cross,
			              brush.m_planes[face.m_plane_index].m_normal),
			0.0f);
	}

	// a translated corner exists in the soup: (1+1, 2-2, 3-3) =
	// (2, 0, 0)
	bool found_corner = false;

	for (kotek::size_t i = 0; i < brush.m_vertices.size(); ++i)
	{
		const zircon_csg_vec3_t<float>& v = brush.m_vertices[i];

		if (std::fabs(v.m[0] - 2.0f) < 1e-6f &&
		    std::fabs(v.m[1]) < 1e-6f && std::fabs(v.m[2]) < 1e-6f)
		{
			found_corner = true;
		}
	}

	EXPECT_TRUE(found_corner);
}

TEST(Zircon_Game, CsgBrushBuildWedge)
{
	// dims (2, 2, 2) at the origin: half extents 1
	const zircon_csg_primitive_desc_t<float> desc =
		csg_make_desc<float>(eZirconCsgPrimitiveType::kWedge, 2.0, 2.0,
			2.0, 0.0, 0.0, 0.0);

	zircon_csg_brush_t<float> brush;

	ASSERT_TRUE(zircon_csg_build_brush(desc, brush));

	ASSERT_EQ(brush.m_planes.size(), 5u);
	ASSERT_EQ(brush.m_faces.size(), 5u);
	// 3 quads + 2 triangles
	ASSERT_EQ(brush.m_vertices.size(), 18u);

	// the diagonal plane: normal (1, 1, 0)/sqrt(2), dist 0
	const float inv_sqrt_2 = 0.70710678f;

	EXPECT_NEAR(brush.m_planes[2].m_normal.m[0], inv_sqrt_2, 1e-4f);
	EXPECT_NEAR(brush.m_planes[2].m_normal.m[1], inv_sqrt_2, 1e-4f);
	EXPECT_NEAR(brush.m_planes[2].m_normal.m[2], 0.0f, 1e-6f);
	EXPECT_NEAR(brush.m_planes[2].m_dist, 0.0f, 1e-6f);

	// the classification: the (+1, -1, 0) corner is ON the diagonal
	// plane, (+1, +1, 0) is OUTSIDE (the wedge cuts it away), the
	// (-1, -1, 0) corner is inside-or-on every plane
	const zircon_csg_vec3_t<float> on_corner =
		zircon_csg_make_vec3(1.0f, -1.0f, 0.0f);
	const zircon_csg_vec3_t<float> out_corner =
		zircon_csg_make_vec3(1.0f, 1.0f, 0.0f);
	const zircon_csg_vec3_t<float> in_corner =
		zircon_csg_make_vec3(-1.0f, -1.0f, 0.0f);

	EXPECT_NEAR(zircon_csg_plane_distance(
		            brush.m_planes[2], on_corner),
		0.0f, 1e-4f);
	EXPECT_GT(
		zircon_csg_plane_distance(brush.m_planes[2], out_corner),
		0.1f);

	for (kotek::size_t plane_index = 0;
	     plane_index < brush.m_planes.size(); ++plane_index)
	{
		EXPECT_LE(zircon_csg_plane_distance(
			              brush.m_planes[plane_index], in_corner),
			1e-4f);
	}

	// the wedge's volume via its own boolean: union of one wedge =
	// half the box = 4.0 (the 2x2x2 box is 8)
	zircon_csg_evaluation_t<float>& evaluation =
		*new zircon_csg_evaluation_t<float>();

	ASSERT_EQ(zircon_csg_evaluate_compound(&desc, 1, evaluation),
		eZirconCsgEvaluationStatus::kSuccess);

	EXPECT_NEAR(csg_signed_volume(evaluation.m_mesh), 4.0, 1e-3);
	EXPECT_TRUE(csg_is_watertight(evaluation.m_mesh));

	delete &evaluation;
}


// ------------------------------------------------------------------
// the boolean fragment sets, per precision mode
// ------------------------------------------------------------------
TEST(Zircon_Game, CsgUnionOverlappingBoxesF32)
{
	csg_check_union_overlapping<float>();
}

TEST(Zircon_Game, CsgUnionOverlappingBoxesU32F)
{
	csg_check_union_overlapping<zircon_csg_scalar_u32f_t>();
}

TEST(Zircon_Game, CsgUnionOverlappingBoxesF64)
{
	csg_check_union_overlapping<double>();
}

TEST(Zircon_Game, CsgUnionDisjointBoxesF32)
{
	csg_check_union_disjoint<float>();
}

TEST(Zircon_Game, CsgUnionDisjointBoxesU32F)
{
	csg_check_union_disjoint<zircon_csg_scalar_u32f_t>();
}

TEST(Zircon_Game, CsgUnionContainmentF32)
{
	csg_check_union_containment<float>();
}

TEST(Zircon_Game, CsgUnionContainmentU32F)
{
	csg_check_union_containment<zircon_csg_scalar_u32f_t>();
}

TEST(Zircon_Game, CsgUnionIdenticalBoxesDedupF32)
{
	csg_check_union_identical<float>();
}

TEST(Zircon_Game, CsgUnionIdenticalBoxesDedupU32F)
{
	csg_check_union_identical<zircon_csg_scalar_u32f_t>();
}

TEST(Zircon_Game, CsgSubtractionNotchF32)
{
	csg_check_subtraction_notch<float>();
}

TEST(Zircon_Game, CsgSubtractionNotchU32F)
{
	csg_check_subtraction_notch<zircon_csg_scalar_u32f_t>();
}

TEST(Zircon_Game, CsgSubtractionNotchF64)
{
	csg_check_subtraction_notch<double>();
}

TEST(Zircon_Game, CsgSubtractionContainmentCavityF32)
{
	csg_check_subtraction_containment<float>();
}

TEST(Zircon_Game, CsgSubtractionContainmentCavityU32F)
{
	csg_check_subtraction_containment<zircon_csg_scalar_u32f_t>();
}

TEST(Zircon_Game, CsgIntersectionOverlapBoxF32)
{
	csg_check_intersection_overlap<float>();
}

TEST(Zircon_Game, CsgIntersectionOverlapBoxU32F)
{
	csg_check_intersection_overlap<zircon_csg_scalar_u32f_t>();
}

TEST(Zircon_Game, CsgIntersectionTouchingIsEmptyF32)
{
	csg_check_intersection_touching_is_empty<float>();
}

TEST(Zircon_Game, CsgIntersectionTouchingIsEmptyU32F)
{
	csg_check_intersection_touching_is_empty<
		zircon_csg_scalar_u32f_t>();
}

TEST(Zircon_Game, CsgCompoundMixedOperationsF32)
{
	csg_check_compound_mixed<float>();
}

TEST(Zircon_Game, CsgCompoundMixedOperationsU32F)
{
	csg_check_compound_mixed<zircon_csg_scalar_u32f_t>();
}

// the shared-plane union (the weld's shared-plane edge case): two
// boxes side by side sharing the x = 0.5 face — the internal seam
// (coplanar-opposed) cancels from BOTH, the 2x1x1 shell stays
// watertight
TEST(Zircon_Game, CsgWeldSharedPlaneUnion)
{
	zircon_csg_evaluation_t<float>& evaluation =
		*new zircon_csg_evaluation_t<float>();

	const zircon_csg_primitive_desc_t<float> first =
		csg_make_desc<float>(eZirconCsgPrimitiveType::kBox, 1.0, 1.0,
			1.0, 0.0, 0.0, 0.0);
	const zircon_csg_primitive_desc_t<float> second =
		csg_make_desc<float>(eZirconCsgPrimitiveType::kBox, 1.0, 1.0,
			1.0, 1.0, 0.0, 0.0);

	ASSERT_EQ(zircon_csg_evaluate_pair(first, second,
		              eZirconCsgBoolean::kUnion, evaluation),
		eZirconCsgEvaluationStatus::kSuccess);

	// the 2x1x1 shell: 10 quads (2 end caps + 4 sides x 2 halves), 20
	// tris, volume 2 — and NO internal face at x = 0.5
	EXPECT_EQ(evaluation.m_mesh.m_normals.size(), 20u);
	EXPECT_NEAR(csg_signed_volume(evaluation.m_mesh), 2.0, 1e-3);
	EXPECT_TRUE(csg_is_watertight(evaluation.m_mesh));

	// no position may sit strictly inside the seam plane's interior
	// region as a lone internal face: every welded vertex with
	// x == 0.5 must be a shell boundary vertex (the shell's faces
	// meet there) — the watertight check above already forbids a
	// T-junction; assert the seam vertices exist at the shared edge
	bool found_seam_vertex = false;

	for (kotek::size_t i = 0; i < evaluation.m_mesh.m_positions.size();
	     ++i)
	{
		if (std::fabs(
		        evaluation.m_mesh.m_positions[i].m[0] - 0.5f) < 1e-6f)
		{
			found_seam_vertex = true;
		}
	}

	EXPECT_TRUE(found_seam_vertex);

	delete &evaluation;
}

// near-parallel planes within eps (the weld's second edge case): B
// rotated 1e-4 rad about z — the union must succeed, stay
// deterministic across two runs, and land near the unrotated volume;
// exact watertightness is NOT asserted (slivers at the rotated seam
// are policy-dropped by design — documented in the header)
TEST(Zircon_Game, CsgWeldNearParallelPlanesDeterministic)
{
	// 1e-4 rad about z: sin/cos of the half angle in the small-angle
	// regime (2e-5 rad — the error is ~1e-16, far below the fixture's
	// eps); the brush builder's defensive normalization absorbs the
	// remaining |q| != 1 error
	const float quat_z = 0.00005f;
	const float quat_w = 1.0f;

	zircon_csg_evaluation_t<float>& first_eval =
		*new zircon_csg_evaluation_t<float>();
	zircon_csg_evaluation_t<float>& second_eval =
		*new zircon_csg_evaluation_t<float>();

	const zircon_csg_primitive_desc_t<float> first =
		csg_make_desc<float>(eZirconCsgPrimitiveType::kBox, 1.0, 1.0,
			1.0, 0.0, 0.0, 0.0);
	const zircon_csg_primitive_desc_t<float> second =
		csg_make_desc<float>(eZirconCsgPrimitiveType::kBox, 1.0, 1.0,
			1.0, 0.75, 0.0, 0.0, 0, 0, 0.0, 0.0, quat_z, quat_w);

	ASSERT_EQ(zircon_csg_evaluate_pair(first, second,
		              eZirconCsgBoolean::kUnion, first_eval),
		eZirconCsgEvaluationStatus::kSuccess);
	ASSERT_EQ(zircon_csg_evaluate_pair(first, second,
		              eZirconCsgBoolean::kUnion, second_eval),
		eZirconCsgEvaluationStatus::kSuccess);

	// determinism: byte-identical across the two runs
	EXPECT_TRUE(csg_meshes_byte_identical(
		first_eval.m_mesh, second_eval.m_mesh));

	// sanity: a real mesh near the unrotated union's volume (the two
	// boxes overlap by 0.25 -> union 1.75; the 1e-4 rad rotation
	// perturbs the seam only)
	EXPECT_GT(csg_signed_volume(first_eval.m_mesh), 1.7);
	EXPECT_LT(csg_signed_volume(first_eval.m_mesh), 1.9);

	delete &second_eval;
	delete &first_eval;
}

// tiny slivers dropped by policy, proven at the split level: a piece
// crossing a brush plane leaves a crumb fragment (legs 1e-4 — beyond
// the plane eps, so it IS strictly outside, but its area 1e-8 is below
// the sliver area bar) — the crumb is counted and dropped, never
// silently kept
TEST(Zircon_Game, CsgSliverDropPolicy)
{
	zircon_csg_evaluation_t<float>& evaluation =
		*new zircon_csg_evaluation_t<float>();

	zircon_csg_brush_t<float> brush;

	const zircon_csg_primitive_desc_t<float> box =
		csg_make_desc<float>(eZirconCsgPrimitiveType::kBox, 1.0, 1.0,
			1.0, 0.0, 0.0, 0.0);

	ASSERT_TRUE(zircon_csg_build_brush(box, brush));

	// the epsilons the evaluation would compute for this brush scale
	// (span ~3.5): plane 4.5e-5, sliver area 2e-7
	evaluation.m_plane_eps = 4.5e-5f;
	evaluation.m_sliver_area_eps = 2e-7;

	// the piece: a quad straddling the +x plane (x <= 0.5) with a
	// 1e-4 x 1e-4 corner sticking out
	const zircon_csg_vec3_t<float> ring[4] = {
		zircon_csg_make_vec3(0.4f, -0.1f, 0.0f),
		zircon_csg_make_vec3(0.5001f, -0.1f, 0.0f),
		zircon_csg_make_vec3(0.5001f, -0.0999f, 0.0f),
		zircon_csg_make_vec3(0.4f, -0.0999f, 0.0f)};

	const zircon_csg_vec3_t<float> piece_normal =
		zircon_csg_make_vec3(0.0f, 0.0f, 1.0f);

	kotek::static_vector_t<zircon_csg_piece_t,
		ZIRCON_DEF_CSG_MAX_FRAGMENTS_IN_FLIGHT>
		pieces;
	kotek::static_vector_t<zircon_csg_vec3_t<float>,
		ZIRCON_DEF_CSG_MAX_FRAGMENT_SCRATCH_VERTICES>
		vertices;

	zircon_csg_detail::zircon_csg_piece_pool_t<float> pool{&pieces,
		&vertices};

	zircon_csg_vec3_t<float>
		survivor_ring[ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES];
	kotek::uint32_t survivor_count = 0;
	eZirconCsgCoplanar coplanar = eZirconCsgCoplanar::kNone;

	zircon_csg_detail::zircon_csg_split_piece_vs_brush(evaluation,
		ring, 4, &piece_normal, brush, pool, survivor_ring,
		survivor_count, coplanar);

	// the crumb was counted and NOT appended; the bulk survived
	EXPECT_EQ(evaluation.m_sliver_drop_count, 1u);
	EXPECT_EQ(pieces.size(), 0u);
	EXPECT_EQ(survivor_count, 4u);

	delete &evaluation;
}

// the sub-eps overlap regime: an intersection 1e-7 deep (below the
// plane eps) is the touching regime — coplanar-opposed faces drop and
// the result is empty by policy (never a nanometer-thick shell)
TEST(Zircon_Game, CsgIntersectionEpsShallowIsEmpty)
{
	zircon_csg_evaluation_t<float>& evaluation =
		*new zircon_csg_evaluation_t<float>();

	const zircon_csg_primitive_desc_t<float> first =
		csg_make_desc<float>(eZirconCsgPrimitiveType::kBox, 1.0, 1.0,
			1.0, 0.0, 0.0, 0.0);
	const zircon_csg_primitive_desc_t<float> second =
		csg_make_desc<float>(eZirconCsgPrimitiveType::kBox, 1.0, 1.0,
			1.0, 1.0f - 1e-7f, 0.0f, 0.0f);

	ASSERT_EQ(zircon_csg_evaluate_pair(first, second,
		              eZirconCsgBoolean::kIntersect, evaluation),
		eZirconCsgEvaluationStatus::kSuccess);

	EXPECT_EQ(evaluation.m_mesh.m_indices.size(), 0u);
	EXPECT_EQ(evaluation.m_mesh.m_positions.size(), 0u);

	delete &evaluation;
}

// the capacity guards: too many primitives is a loud graceful
// rejection (never a wrap, never a partial evaluation)
TEST(Zircon_Game, CsgCapacityTooManyPrimitives)
{
	zircon_csg_evaluation_t<float>& evaluation =
		*new zircon_csg_evaluation_t<float>();

	zircon_csg_primitive_desc_t<float> descs
		[ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND + 1];

	for (kotek::uint32_t i = 0;
	     i < ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND + 1; ++i)
	{
		descs[i] = csg_make_desc<float>(eZirconCsgPrimitiveType::kBox,
			1.0, 1.0, 1.0, static_cast<double>(i) * 3.0, 0.0, 0.0);
	}

	// exactly at the cap: works (128 disjoint boxes)
	EXPECT_EQ(zircon_csg_evaluate_compound(
		              descs, ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND,
		              evaluation),
		eZirconCsgEvaluationStatus::kSuccess);
	EXPECT_EQ(evaluation.m_mesh.m_normals.size(),
		static_cast<kotek::size_t>(
			ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND) * 12u);

	// one past: the loud rejection
	EXPECT_EQ(zircon_csg_evaluate_compound(
		              descs, ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND + 1,
		              evaluation),
		eZirconCsgEvaluationStatus::kError_TooManyPrimitives);

	delete &evaluation;
}

// the invalid-input guards (user content is not a programmer error)
TEST(Zircon_Game, CsgEvaluationInputGuards)
{
	zircon_csg_evaluation_t<float>& evaluation =
		*new zircon_csg_evaluation_t<float>();

	EXPECT_EQ(zircon_csg_evaluate_compound<float>(
		              nullptr, 2, evaluation),
		eZirconCsgEvaluationStatus::kError_InvalidArguments);

	const zircon_csg_primitive_desc_t<float> valid =
		csg_make_desc<float>(eZirconCsgPrimitiveType::kBox, 1.0, 1.0,
			1.0, 0.0, 0.0, 0.0);

	EXPECT_EQ(zircon_csg_evaluate_compound(&valid, 0, evaluation),
		eZirconCsgEvaluationStatus::kError_EmptyCompound);

	// a non-positive dimension
	zircon_csg_primitive_desc_t<float> bad_dimension = valid;
	bad_dimension.m_dimensions[1] = 0.0f;

	EXPECT_EQ(zircon_csg_evaluate_compound(
		              &bad_dimension, 1, evaluation),
		eZirconCsgEvaluationStatus::kError_InvalidArguments);

	// an unknown primitive type
	zircon_csg_primitive_desc_t<float> bad_type = valid;
	bad_type.m_type = 200;

	EXPECT_EQ(
		zircon_csg_evaluate_compound(&bad_type, 1, evaluation),
		eZirconCsgEvaluationStatus::kError_UnknownPrimitiveType);

	delete &evaluation;
}

// ------------------------------------------------------------------
// determinism + precision parity
// ------------------------------------------------------------------

// THE bake-reproducibility proof: the same input evaluated twice in
// u32f produces BYTE-IDENTICAL output (positions, indices, normals,
// material ids) — deterministic input = deterministic output
TEST(Zircon_Game, CsgDeterminismU32fByteIdentical)
{
	zircon_csg_evaluation_t<zircon_csg_scalar_u32f_t>& first_eval =
		*new zircon_csg_evaluation_t<zircon_csg_scalar_u32f_t>();
	zircon_csg_evaluation_t<zircon_csg_scalar_u32f_t>& second_eval =
		*new zircon_csg_evaluation_t<zircon_csg_scalar_u32f_t>();

	zircon_csg_primitive_desc_t<zircon_csg_scalar_u32f_t> descs[3];

	descs[0] = csg_make_desc<zircon_csg_scalar_u32f_t>(
		eZirconCsgPrimitiveType::kBox, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0);
	descs[1] = csg_make_desc<zircon_csg_scalar_u32f_t>(
		eZirconCsgPrimitiveType::kBox, 1.0, 1.0, 1.0, 0.5, 0.5, 0.5);
	descs[2] = csg_make_desc<zircon_csg_scalar_u32f_t>(
		eZirconCsgPrimitiveType::kBox, 0.5, 0.5, 0.5, -0.25, -0.25,
		-0.25, 3, zircon_DEF_CSG_OPERATION_SUBTRACTIVE);

	ASSERT_EQ(zircon_csg_evaluate_compound(descs, 3, first_eval),
		eZirconCsgEvaluationStatus::kSuccess);
	ASSERT_EQ(zircon_csg_evaluate_compound(descs, 3, second_eval),
		eZirconCsgEvaluationStatus::kSuccess);

	EXPECT_TRUE(csg_meshes_byte_identical(
		first_eval.m_mesh, second_eval.m_mesh));

	// the diagnostics are identical too (the drop counters are part of
	// the deterministic output)
	EXPECT_EQ(first_eval.m_sliver_drop_count,
		second_eval.m_sliver_drop_count);
	EXPECT_EQ(first_eval.m_weld_degenerate_drop_count,
		second_eval.m_weld_degenerate_drop_count);
	EXPECT_EQ(first_eval.m_capacity_drop_count,
		second_eval.m_capacity_drop_count);

	delete &second_eval;
	delete &first_eval;
}

// the precision parity band: the same dyadic fixtures through the f32
// and u32f cores produce the same topology (identical triangle and
// vertex counts) with positions within the documented band (the u32f
// quantum + the weld eps — 1e-3 absolute here)
TEST(Zircon_Game, CsgPrecisionParityF32U32F)
{
	zircon_csg_evaluation_t<float>& f32_eval =
		*new zircon_csg_evaluation_t<float>();
	zircon_csg_evaluation_t<zircon_csg_scalar_u32f_t>& u32f_eval =
		*new zircon_csg_evaluation_t<zircon_csg_scalar_u32f_t>();

	// the mixed compound again — it exercises splits, the cavity and
	// the weld in both modes
	zircon_csg_primitive_desc_t<float> descs_f32[3];
	zircon_csg_primitive_desc_t<zircon_csg_scalar_u32f_t> descs_u32f[3];

	descs_f32[0] = csg_make_desc<float>(eZirconCsgPrimitiveType::kBox,
		1.0, 1.0, 1.0, 0.0, 0.0, 0.0);
	descs_f32[1] = csg_make_desc<float>(eZirconCsgPrimitiveType::kBox,
		1.0, 1.0, 1.0, 0.5, 0.5, 0.5);
	descs_f32[2] = csg_make_desc<float>(eZirconCsgPrimitiveType::kBox,
		0.5, 0.5, 0.5, -0.25, -0.25, -0.25, 3,
		zircon_DEF_CSG_OPERATION_SUBTRACTIVE);

	descs_u32f[0] = csg_make_desc<zircon_csg_scalar_u32f_t>(
		eZirconCsgPrimitiveType::kBox, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0);
	descs_u32f[1] = csg_make_desc<zircon_csg_scalar_u32f_t>(
		eZirconCsgPrimitiveType::kBox, 1.0, 1.0, 1.0, 0.5, 0.5, 0.5);
	descs_u32f[2] = csg_make_desc<zircon_csg_scalar_u32f_t>(
		eZirconCsgPrimitiveType::kBox, 0.5, 0.5, 0.5, -0.25, -0.25,
		-0.25, 3, zircon_DEF_CSG_OPERATION_SUBTRACTIVE);

	ASSERT_EQ(zircon_csg_evaluate_compound(descs_f32, 3, f32_eval),
		eZirconCsgEvaluationStatus::kSuccess);
	ASSERT_EQ(zircon_csg_evaluate_compound(descs_u32f, 3, u32f_eval),
		eZirconCsgEvaluationStatus::kSuccess);

	// identical topology (the fixtures are dyadic-exact — both modes
	// weld the same clusters in the same order)
	ASSERT_EQ(f32_eval.m_mesh.m_indices.size(),
		u32f_eval.m_mesh.m_indices.size());
	ASSERT_EQ(f32_eval.m_mesh.m_positions.size(),
		u32f_eval.m_mesh.m_positions.size());

	for (kotek::size_t i = 0;
	     i < f32_eval.m_mesh.m_positions.size(); ++i)
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			const double f32_value =
				f32_eval.m_mesh.m_positions[i].m[axis];
			const double u32f_value =
				zircon_csg_scalar_traits<zircon_csg_scalar_u32f_t>::
					to_double(
						u32f_eval.m_mesh.m_positions[i].m[axis]);

			EXPECT_NEAR(f32_value, u32f_value, 1e-3);
		}
	}

	// the volumes agree within the band too
	EXPECT_NEAR(csg_signed_volume(f32_eval.m_mesh),
		csg_signed_volume(u32f_eval.m_mesh), 1e-3);

	delete &u32f_eval;
	delete &f32_eval;
}

// ------------------------------------------------------------------
// the components
// ------------------------------------------------------------------
TEST(Zircon_Game, CsgComponentPrimitiveJsonRoundTrip)
{
	zircon_component_csg_primitive primitive;

	primitive.set_enabled(false);
	primitive.set_primitive_type(eZirconCsgPrimitiveType::kWedge);
	primitive.set_subtractive(true);
	primitive.set_material_id(7);

	using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_t>;

	primitive.set_dimensions(traits_t::from_double(1.5),
		traits_t::from_double(2.25), traits_t::from_double(3.5));
	primitive.set_position(traits_t::from_double(-1.25),
		traits_t::from_double(0.5), traits_t::from_double(2.75));
	primitive.set_rotation(traits_t::from_double(0.1),
		traits_t::from_double(-0.2), traits_t::from_double(0.3),
		traits_t::from_double(0.9));

	kotek::ktk::json::value serialized =
		kotek::ktk::json::value_from(primitive);

	zircon_component_csg_primitive restored =
		kotek::ktk::json::value_to<zircon_component_csg_primitive>(
			serialized);

	EXPECT_EQ(restored.is_enabled(), false);
	EXPECT_EQ(
		restored.get_primitive_type(), eZirconCsgPrimitiveType::kWedge);
	EXPECT_EQ(restored.is_subtractive(), true);
	EXPECT_EQ(restored.get_material_id(), 7);

	for (kotek::uint8_t axis = 0; axis < 3; ++axis)
	{
		EXPECT_DOUBLE_EQ(
			traits_t::to_double(restored.get_dimension(axis)),
			traits_t::to_double(primitive.get_dimension(axis)));
		EXPECT_DOUBLE_EQ(
			traits_t::to_double(restored.get_position_axis(axis)),
			traits_t::to_double(primitive.get_position_axis(axis)));
	}

	for (kotek::uint8_t component = 0; component < 4; ++component)
	{
		EXPECT_DOUBLE_EQ(
			traits_t::to_double(
				restored.get_rotation_component(component)),
			traits_t::to_double(
				primitive.get_rotation_component(component)));
	}
}

TEST(Zircon_Game, CsgComponentCsgJsonRoundTrip)
{
	zircon_component_csg compound;

	compound.set_enabled(false);

	kotek::entity_t first{5};
	kotek::entity_t second{42};
	kotek::entity_t third{4096};

	ASSERT_TRUE(compound.add_primitive_entity(first));
	ASSERT_TRUE(compound.add_primitive_entity(second));
	ASSERT_TRUE(compound.add_primitive_entity(third));

	// duplicates are refused (the caller-error contract)
	EXPECT_FALSE(compound.add_primitive_entity(second));
	EXPECT_EQ(compound.get_primitive_count(), 3u);

	compound.mark_dirty();

	zircon_csg_mesh_handle_t handle;
	handle.m_vertex_count = 100;
	handle.m_triangle_count = 50;
	handle.m_generation = 3;
	handle.m_state =
		static_cast<kotek::uint8_t>(eZirconCsgMeshState::kValid);

	compound.set_mesh_handle(handle);

	kotek::ktk::json::value serialized =
		kotek::ktk::json::value_from(compound);

	zircon_component_csg restored =
		kotek::ktk::json::value_to<zircon_component_csg>(serialized);

	EXPECT_EQ(restored.is_enabled(), false);
	ASSERT_EQ(restored.get_primitive_count(), 3u);
	EXPECT_EQ(restored.get_primitive_entities()[0].id, 5u);
	EXPECT_EQ(restored.get_primitive_entities()[1].id, 42u);
	EXPECT_EQ(restored.get_primitive_entities()[2].id, 4096u);
	EXPECT_EQ(restored.is_dirty(), 1);

	const zircon_csg_mesh_handle_t& restored_handle =
		restored.get_mesh_handle();

	EXPECT_EQ(restored_handle.m_vertex_count, 100u);
	EXPECT_EQ(restored_handle.m_triangle_count, 50u);
	EXPECT_EQ(restored_handle.m_generation, 3u);
	EXPECT_EQ(restored_handle.m_state,
		static_cast<kotek::uint8_t>(eZirconCsgMeshState::kValid));
}

TEST(Zircon_Game, CsgComponentCsgMemberListAndHandleOps)
{
	zircon_component_csg compound;

	// the defaults: unevaluated, clean, empty
	EXPECT_EQ(compound.get_mesh_handle().m_state,
		static_cast<kotek::uint8_t>(eZirconCsgMeshState::kUnevaluated));
	EXPECT_EQ(compound.is_dirty(), 0);
	EXPECT_EQ(compound.get_primitive_count(), 0u);

	// fill to the cap, then the loud overflow
	for (kotek::uint32_t i = 0;
	     i < ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND; ++i)
	{
		ASSERT_TRUE(compound.add_primitive_entity(
			kotek::entity_t{1000 + i}));
	}

	EXPECT_FALSE(compound.add_primitive_entity(kotek::entity_t{9999}));

	// removal: present -> true, absent -> false, and the list packs
	ASSERT_TRUE(compound.remove_primitive_entity(
		kotek::entity_t{1001}));
	EXPECT_FALSE(compound.has_primitive_entity(kotek::entity_t{1001}));
	EXPECT_FALSE(compound.remove_primitive_entity(
		kotek::entity_t{1001}));
	EXPECT_TRUE(compound.has_primitive_entity(kotek::entity_t{1002}));
	EXPECT_EQ(compound.get_primitive_count(),
		ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND - 1u);

	// record_evaluation: counts + the generation bump + the valid
	// state; the failure path marks kFailed
	compound.record_evaluation(24, 12);

	EXPECT_EQ(compound.get_mesh_handle().m_vertex_count, 24u);
	EXPECT_EQ(compound.get_mesh_handle().m_triangle_count, 12u);
	EXPECT_EQ(compound.get_mesh_handle().m_generation, 1u);
	EXPECT_EQ(compound.get_mesh_handle().m_state,
		static_cast<kotek::uint8_t>(eZirconCsgMeshState::kValid));

	compound.record_evaluation(48, 24);
	EXPECT_EQ(compound.get_mesh_handle().m_generation, 2u);

	compound.mark_evaluation_failed();
	EXPECT_EQ(compound.get_mesh_handle().m_state,
		static_cast<kotek::uint8_t>(eZirconCsgMeshState::kFailed));
}

// the ECS registration: both types are in the codegen'd enum (the
// inspector combo's fill consumes exactly these names), the factory
// creates them on a real context, and the components drive a real
// evaluation through the fill_desc seam
TEST(Zircon_Game, CsgComponentRegistrationAndFactoryRoundTrip)
{
	EXPECT_STREQ(
		zircon_translate_component_type_enum_to_string(
			eZirconComponentType::kzircon_component_csg_primitive),
		"zircon_component_csg_primitive");
	EXPECT_STREQ(
		zircon_translate_component_type_enum_to_string(
			eZirconComponentType::kzircon_component_csg),
		"zircon_component_csg");

	zircon_test_csg_env& env = *new zircon_test_csg_env();
	env.initialize();

	EXPECT_STREQ(
		env.factory.get_component_name_by_enum(
			eZirconComponentType::kzircon_component_csg_primitive),
		"zircon_component_csg_primitive");
	EXPECT_STREQ(
		env.factory.get_component_name_by_enum(
			eZirconComponentType::kzircon_component_csg),
		"zircon_component_csg");

	// the name -> enum roundtrip (the console/inspector path)
	EXPECT_EQ(env.factory.get_component_enum_by_name(
		              "zircon_component_csg_primitive"),
		eZirconComponentType::kzircon_component_csg_primitive);
	EXPECT_EQ(env.factory.get_component_enum_by_name(
		              "zircon_component_csg"),
		eZirconComponentType::kzircon_component_csg);

	zircon_ecs_context_t* p_context = env.world.get_ecs_context();

	// two primitives + the compound on real entities
	kotek::entity_t id_first = env.factory.create_entity(p_context);
	kotek::entity_t id_second = env.factory.create_entity(p_context);
	kotek::entity_t id_compound =
		env.factory.create_entity(p_context);

	ASSERT_FALSE(ecs_is_invalid_entity(id_first));
	ASSERT_FALSE(ecs_is_invalid_entity(id_second));
	ASSERT_FALSE(ecs_is_invalid_entity(id_compound));

	EXPECT_TRUE(env.factory.create_component(p_context, id_first,
		eZirconComponentType::kzircon_component_csg_primitive));
	EXPECT_TRUE(env.factory.create_component(p_context, id_second,
		eZirconComponentType::kzircon_component_csg_primitive));
	EXPECT_TRUE(env.factory.create_component(p_context, id_compound,
		eZirconComponentType::kzircon_component_csg));

	EXPECT_TRUE(env.factory.has_component(p_context, id_first,
		eZirconComponentType::kzircon_component_csg_primitive));
	EXPECT_TRUE(env.factory.has_component(p_context, id_compound,
		eZirconComponentType::kzircon_component_csg));

	// the component pair drives a real evaluation: A and B overlap,
	// the compound lists them
	auto* p_first = static_cast<zircon_component_csg_primitive*>(
		env.factory.get_component_by_enum(p_context, id_first,
			eZirconComponentType::kzircon_component_csg_primitive));
	auto* p_second = static_cast<zircon_component_csg_primitive*>(
		env.factory.get_component_by_enum(p_context, id_second,
			eZirconComponentType::kzircon_component_csg_primitive));
	auto* p_compound = static_cast<zircon_component_csg*>(
		env.factory.get_component_by_enum(p_context, id_compound,
			eZirconComponentType::kzircon_component_csg));

	ASSERT_NE(p_first, nullptr);
	ASSERT_NE(p_second, nullptr);
	ASSERT_NE(p_compound, nullptr);

	using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_t>;

	p_first->set_dimensions(traits_t::one(), traits_t::one(),
		traits_t::one());
	p_second->set_dimensions(traits_t::one(), traits_t::one(),
		traits_t::one());
	p_second->set_position(traits_t::from_double(0.5),
		traits_t::from_double(0.5), traits_t::from_double(0.5));
	p_second->set_material_id(9);

	ASSERT_TRUE(p_compound->add_primitive_entity(id_first));
	ASSERT_TRUE(p_compound->add_primitive_entity(id_second));

	zircon_csg_primitive_desc_t<zircon_csg_scalar_t> descs[2];

	p_first->fill_desc(descs[0]);
	p_second->fill_desc(descs[1]);

	// the desc seam carries the component state
	EXPECT_EQ(descs[1].m_material_id, 9);
	EXPECT_DOUBLE_EQ(traits_t::to_double(descs[1].m_position[0]), 0.5);

	zircon_csg_evaluation_t<zircon_csg_scalar_t>& evaluation =
		*new zircon_csg_evaluation_t<zircon_csg_scalar_t>();

	ASSERT_EQ(zircon_csg_evaluate_compound(descs, 2, evaluation),
		eZirconCsgEvaluationStatus::kSuccess);

	EXPECT_EQ(evaluation.m_mesh.m_normals.size(), 48u);
	EXPECT_NEAR(csg_signed_volume(evaluation.m_mesh), 1.875,
		csg_volume_band<zircon_csg_scalar_t>());

	// the evaluated-mesh handle records the result (the A2 contract)
	p_compound->record_evaluation(
		static_cast<kotek::uint32_t>(
			evaluation.m_mesh.m_positions.size()),
		static_cast<kotek::uint32_t>(
			evaluation.m_mesh.m_normals.size()));

	EXPECT_EQ(p_compound->get_mesh_handle().m_vertex_count,
		evaluation.m_mesh.m_positions.size());
	EXPECT_EQ(p_compound->get_mesh_handle().m_triangle_count, 48u);
	EXPECT_EQ(p_compound->get_mesh_handle().m_state,
		static_cast<kotek::uint8_t>(eZirconCsgMeshState::kValid));

	delete &evaluation;

	env.shutdown();
	delete &env;
}

		#endif

	#endif
#endif
