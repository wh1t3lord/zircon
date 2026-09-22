#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cmath>
		#include <cstring>
		#include <filesystem>

		#include "../../core/zircon_meshlet_clusterize.h"

		#include <kotek.core.filesystem.pack/include/kotek_kpack_format.h>

		#ifndef ZIRCON_DEF_UNIT_TEST_MESHLET
			#define ZIRCON_DEF_UNIT_TEST_MESHLET 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_MESHLET == 1

// functional proofs for task Z24 B3a (the meshlet clusterizer, the
// nanite path's content side): the seeded region growth (the budgets,
// the compactness, the conservation), the normal-cone backface math
// (the flat/clustered/never-cull cases), the placeholder LOD hierarchy
// (the exact halving, the parent/child link coverage), the pack
// roundtrip (the manifest + the bins through the dispatcher), the
// streaming order (the coarsest level first), the DETERMINISM contract
// (two bakes byte-identical) and the capacity guards (loud graceful).
// Tier: lightweight (rule 8a — tiny soups, small packs).

namespace
{
	// the headless environment (the pack_boot / csg-bake fixture's
	// shape): a real filesystem behind a real framework config — the
	// boot dispatcher, no engine session needed
	struct meshlet_test_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;

		void initialize(void)
		{
			this->filesystem.Initialize(&this->framework_config);
		}

		void shutdown(void) { this->filesystem.Shutdown(); }
	};

	// builds <root>/data_game/packs/<name>
	kotek::static_path_t meshlet_make_pack_path(
		meshlet_test_env& env, const char* p_pack_name)
	{
		kotek::static_path_t packs_folder;
		env.filesystem.Make_Path(packs_folder,
			kotek::core::eFolderIndex::kFolderIndex_DataGame);
		packs_folder /= kotek::core::kKpackPacksFolderName;

		kotek::static_path_t pack_path = packs_folder;
		pack_path /= p_pack_name;
		return pack_path;
	}

	void meshlet_ensure_packs_folder(meshlet_test_env& env)
	{
		kotek::static_path_t packs_folder;
		env.filesystem.Make_Path(packs_folder,
			kotek::core::eFolderIndex::kFolderIndex_DataGame);
		packs_folder /= kotek::core::kKpackPacksFolderName;

		std::error_code ec;
		std::filesystem::create_directories(
			std::filesystem::path(packs_folder.c_str()), ec);
	}

	void meshlet_remove_pack(const kotek::static_path_t& pack_path)
	{
		std::error_code ec;
		std::filesystem::remove(
			std::filesystem::path(pack_path.c_str()), ec);

		// the packs folder leaves with the last test pack (the
		// shipped tree has no data_game/packs — the pack_boot /
		// embedded-defaults fixtures pin that invariant)
		ec.clear();
		std::filesystem::remove(
			std::filesystem::path(pack_path.c_str()).parent_path(), ec);
	}

	// reads a whole file through the dispatcher into a heap buffer
	// (out_size receives the byte count; the caller deletes[])
	kotek::uint8_t* meshlet_read_through_dispatcher(meshlet_test_env& env,
		const kotek::static_path_t& absolute_path, kotek::size_t& out_size
		)
	{
		out_size = 0;

		kotek::size_t file_size = 0;

		if (env.filesystem.Get_FileSize(absolute_path, file_size) == false)
			return nullptr;

		kotek::uint8_t* p_buffer = new kotek::uint8_t[file_size + 1];
		kotek::size_t read_size = file_size + 1;

		if (env.filesystem.Read_File(
				absolute_path, p_buffer, read_size) == false ||
			read_size != file_size)
		{
			delete[] p_buffer;
			return nullptr;
		}

		out_size = read_size;
		return p_buffer;
	}

	// the absolute dispatcher path of a baked entry
	kotek::static_path_t meshlet_entry_path(
		meshlet_test_env& env, const char* p_relative_name)
	{
		kotek::static_path_t path;
		env.filesystem.Make_Path(
			path, kotek::core::eFolderIndex::kFolderIndex_Root);
		path /= p_relative_name;
		return path;
	}

	// a welded m x n quad grid in the XY plane (z = 0), normals +Z,
	// materials (i + j) % 2 — a compact connected soup with a pinned
	// vertex/triangle count
	struct meshlet_grid_fixture_t
	{
		static constexpr kotek::uint32_t kQuadsX = 8;
		static constexpr kotek::uint32_t kQuadsY = 8;
		static constexpr kotek::uint32_t kVertexCount =
			(kQuadsX + 1) * (kQuadsY + 1);
		static constexpr kotek::uint32_t kTriangleCount =
			kQuadsX * kQuadsY * 2;

		double m_positions[kVertexCount * 3];
		kotek::uint32_t m_indices[kTriangleCount * 3];
		double m_normals[kTriangleCount * 3];
		kotek::uint16_t m_materials[kTriangleCount];

		meshlet_grid_fixture_t(void)
		{
			for (kotek::uint32_t y = 0; y <= kQuadsY; ++y)
			{
				for (kotek::uint32_t x = 0; x <= kQuadsX; ++x)
				{
					const kotek::uint32_t vertex =
						y * (kQuadsX + 1) + x;

					m_positions[vertex * 3 + 0] =
						static_cast<double>(x);
					m_positions[vertex * 3 + 1] =
						static_cast<double>(y);
					m_positions[vertex * 3 + 2] = 0.0;
				}
			}

			for (kotek::uint32_t y = 0; y < kQuadsY; ++y)
			{
				for (kotek::uint32_t x = 0; x < kQuadsX; ++x)
				{
					const kotek::uint32_t quad = y * kQuadsX + x;
					const kotek::uint32_t v00 =
						y * (kQuadsX + 1) + x;
					const kotek::uint32_t v10 = v00 + 1;
					const kotek::uint32_t v01 = v00 + (kQuadsX + 1);
					const kotek::uint32_t v11 = v01 + 1;

					m_indices[quad * 6 + 0] = v00;
					m_indices[quad * 6 + 1] = v10;
					m_indices[quad * 6 + 2] = v11;
					m_indices[quad * 6 + 3] = v00;
					m_indices[quad * 6 + 4] = v11;
					m_indices[quad * 6 + 5] = v01;

					for (kotek::uint8_t half = 0; half < 2; ++half)
					{
						for (int axis = 0; axis < 3; ++axis)
						{
							m_normals[(quad * 2 + half) * 3 + axis] =
								axis == 2 ? 1.0 : 0.0;
						}
					}

					m_materials[quad * 2 + 0] =
						static_cast<kotek::uint16_t>((x + y) & 1);
					m_materials[quad * 2 + 1] =
						static_cast<kotek::uint16_t>((x + y) & 1);
				}
			}
		}

		zircon_meshlet_mesh_input_t make_input(void) const
		{
			zircon_meshlet_mesh_input_t input{};
			input.p_positions = m_positions;
			input.m_vertex_count = kVertexCount;
			input.p_indices = m_indices;
			input.p_normals = m_normals;
			input.p_materials = m_materials;
			input.m_triangle_count = kTriangleCount;
			return input;
		}
	};

	// the six cube faces as 12 welded triangles (8 vertices), the
	// per-triangle normals the geometric face normals
	struct meshlet_cube_fixture_t
	{
		static constexpr kotek::uint32_t kVertexCount = 8;
		static constexpr kotek::uint32_t kTriangleCount = 12;

		double m_positions[kVertexCount * 3];
		kotek::uint32_t m_indices[kTriangleCount * 3];
		double m_normals[kTriangleCount * 3];
		kotek::uint16_t m_materials[kTriangleCount];

		// p_face_normals: null = the closed cube (all six faces); a
		// 3-entry array of the kept axes signs (e.g. {(1,0,0),(0,1,0),
		// (0,0,1)} = the open +X+Y+Z corner) builds the partial cube
		meshlet_cube_fixture_t(const double (*p_face_normals)[3]
			) 
		{
			for (kotek::uint32_t corner = 0; corner < 8; ++corner)
			{
				m_positions[corner * 3 + 0] =
					static_cast<double>(corner & 1);
				m_positions[corner * 3 + 1] =
					static_cast<double>((corner >> 1) & 1);
				m_positions[corner * 3 + 2] =
					static_cast<double>((corner >> 2) & 1);
			}

			// the faces: {axis, sign} — +X, -X, +Y, -Y, +Z, -Z
			static const double k_face_dirs[6][3] = {{1.0, 0.0, 0.0},
				{-1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, -1.0, 0.0},
				{0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}};

			// the face quads as corner indices (CCW from outside)
			static const kotek::uint32_t k_face_quads[6][4] = {
				{1, 3, 7, 5}, // +X
				{0, 4, 6, 2}, // -X
				{2, 6, 7, 3}, // +Y
				{0, 1, 5, 4}, // -Y
				{4, 5, 7, 6}, // +Z
				{0, 2, 3, 1}, // -Z
			};

			kotek::uint32_t triangle = 0;

			for (kotek::uint32_t face = 0; face < 6; ++face)
			{
				if (p_face_normals != nullptr)
				{
					bool is_kept = false;

					for (kotek::uint32_t kept = 0; kept < 3; ++kept)
					{
						if (p_face_normals[kept][0] ==
								k_face_dirs[face][0] &&
							p_face_normals[kept][1] ==
								k_face_dirs[face][1] &&
							p_face_normals[kept][2] ==
								k_face_dirs[face][2])
						{
							is_kept = true;
							break;
						}
					}

					if (is_kept == false)
						continue;
				}

				const kotek::uint32_t* p_quad = k_face_quads[face];

				m_indices[triangle * 3 + 0] = p_quad[0];
				m_indices[triangle * 3 + 1] = p_quad[1];
				m_indices[triangle * 3 + 2] = p_quad[2];
				m_indices[triangle * 3 + 3] = p_quad[0];
				m_indices[triangle * 3 + 4] = p_quad[2];
				m_indices[triangle * 3 + 5] = p_quad[3];

				for (kotek::uint8_t half = 0; half < 2; ++half)
				{
					for (int axis = 0; axis < 3; ++axis)
					{
						m_normals[(triangle + half) * 3 + axis] =
							k_face_dirs[face][axis];
					}
				}

				m_materials[triangle + 0] = 0;
				m_materials[triangle + 1] = 0;
				triangle += 2;
			}

			// the partial-cube case leaves the tail unused — the input
			// descriptor reports the real count
			m_triangle_count = triangle;
		}

		kotek::uint32_t m_triangle_count;

		zircon_meshlet_mesh_input_t make_input(void) const
		{
			zircon_meshlet_mesh_input_t input{};
			input.p_positions = m_positions;
			input.m_vertex_count = kVertexCount;
			input.p_indices = m_indices;
			input.p_normals = m_normals;
			input.p_materials = m_materials;
			input.m_triangle_count = m_triangle_count;
			return input;
		}
	};

	// the disconnected single-triangle soup (count configurable — the
	// LOD-hierarchy + capacity fixtures): every triangle owns its 3
	// vertices, nothing is shared
	struct meshlet_disconnected_fixture_t
	{
		static constexpr kotek::uint32_t kMaxTriangles = 4200;

		double m_positions[kMaxTriangles * 9];
		kotek::uint32_t m_indices[kMaxTriangles * 3];
		double m_normals[kMaxTriangles * 3];
		kotek::uint16_t m_materials[kMaxTriangles];

		meshlet_disconnected_fixture_t(kotek::uint32_t triangle_count
			) 
		{
			for (kotek::uint32_t triangle = 0; triangle < triangle_count;
				 ++triangle)
			{
				const double base =
					static_cast<double>(triangle) * 4.0;

				m_positions[triangle * 9 + 0] = base;
				m_positions[triangle * 9 + 1] = 0.0;
				m_positions[triangle * 9 + 2] = 0.0;
				m_positions[triangle * 9 + 3] = base + 1.0;
				m_positions[triangle * 9 + 4] = 0.0;
				m_positions[triangle * 9 + 5] = 0.0;
				m_positions[triangle * 9 + 6] = base;
				m_positions[triangle * 9 + 7] = 1.0;
				m_positions[triangle * 9 + 8] = 0.0;

				m_indices[triangle * 3 + 0] = triangle * 3 + 0;
				m_indices[triangle * 3 + 1] = triangle * 3 + 1;
				m_indices[triangle * 3 + 2] = triangle * 3 + 2;

				for (int axis = 0; axis < 3; ++axis)
				{
					m_normals[triangle * 3 + axis] =
						axis == 2 ? 1.0 : 0.0;
				}

				m_materials[triangle] =
					static_cast<kotek::uint16_t>(triangle & 7);
			}
		}

		zircon_meshlet_mesh_input_t make_input(
			kotek::uint32_t triangle_count) const
		{
			zircon_meshlet_mesh_input_t input{};
			input.p_positions = m_positions;
			input.m_vertex_count = triangle_count * 3;
			input.p_indices = m_indices;
			input.p_normals = m_normals;
			input.p_materials = m_materials;
			input.m_triangle_count = triangle_count;
			return input;
		}
	};

	// the per-level cluster range from the level table
	void meshlet_level_range(const zircon_meshlet_lod_set_t& set,
		kotek::uint32_t level, kotek::uint32_t& out_first,
		kotek::uint32_t& out_count)
	{
		out_first = set.m_level_first_cluster[level];
		out_count =
			set.m_level_first_cluster[level + 1u] - out_first;
	}

	// the cluster's triangles share vertices (the region-growth
	// compactness): a union-find over the cluster's triangles via the
	// shared mesh vertices resolves into exactly one component
	bool meshlet_cluster_is_compact(
		const zircon_meshlet_mesh_input_t& mesh,
		const zircon_meshlet_lod_set_t& set,
		const zircon_meshlet_cluster_t& info) noexcept
	{
		if (info.m_triangle_count <= 1u)
			return true;

		// the tiny-fixture union-find over the cluster's triangles
		kotek::uint32_t parent[256];

		for (kotek::uint32_t index = 0; index < info.m_triangle_count;
			 ++index)
		{
			parent[index] = index;
		}

		auto find_root = [&parent](kotek::uint32_t index) {
			while (parent[index] != index)
			{
				parent[index] = parent[parent[index]];
				index = parent[index];
			}

			return index;
		};

		const kotek::uint32_t* p_tris =
			set.m_cluster_triangles.data() + info.m_first_triangle;

		for (kotek::uint32_t a = 0; a < info.m_triangle_count; ++a)
		{
			for (kotek::uint32_t b = a + 1;
				 b < info.m_triangle_count; ++b)
			{
				bool shares = false;

				for (kotek::uint8_t ca = 0; ca < 3 && shares == false;
					 ++ca)
				{
					const kotek::uint32_t va =
						mesh.p_indices[p_tris[a] * 3 + ca];

					for (kotek::uint8_t cb = 0; cb < 3; ++cb)
					{
						if (mesh.p_indices[p_tris[b] * 3 + cb] == va)
						{
							shares = true;
							break;
						}
					}
				}

				if (shares)
				{
					const kotek::uint32_t ra = find_root(a);
					const kotek::uint32_t rb = find_root(b);
					parent[ra] = rb;
				}
			}
		}

		const kotek::uint32_t root = find_root(0);

		for (kotek::uint32_t index = 1; index < info.m_triangle_count;
			 ++index)
		{
			if (find_root(index) != root)
				return false;
		}

		return true;
	}

	// one parsed manifest (the test-side view of the pack metadata)
	struct meshlet_parsed_manifest_t
	{
		kotek::uint32_t m_lod_level_count;
		kotek::uint32_t m_cluster_count_total;
		kotek::uint32_t m_mesh_triangle_count;
		kotek::uint32_t m_link_count_total;
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS>
			m_level_first;
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE>
			m_level_count;
	};

	bool meshlet_parse_manifest(const kotek::uint8_t* p_manifest,
		kotek::size_t manifest_size,
		meshlet_parsed_manifest_t& out_parsed) noexcept
	{
		out_parsed = meshlet_parsed_manifest_t{};

		if (manifest_size < zircon_meshlet_manifest_header_size ||
			std::memcmp(p_manifest, zircon_meshlet_manifest_magic, 8) !=
				0)
		{
			return false;
		}

		out_parsed.m_lod_level_count =
			zircon_csg_bake_load_u32(p_manifest + 12);
		out_parsed.m_cluster_count_total =
			zircon_csg_bake_load_u32(p_manifest + 16);
		out_parsed.m_mesh_triangle_count =
			zircon_csg_bake_load_u32(p_manifest + 20);
		out_parsed.m_link_count_total =
			zircon_csg_bake_load_u32(p_manifest + 24);

		const kotek::uint32_t expected_size =
			zircon_meshlet_manifest_header_size +
			out_parsed.m_lod_level_count *
				zircon_meshlet_manifest_level_size +
			out_parsed.m_cluster_count_total *
				zircon_meshlet_manifest_record_size +
			out_parsed.m_link_count_total *
				sizeof(kotek::uint32_t);

		if (manifest_size != expected_size)
			return false;

		kotek::uint32_t cursor = zircon_meshlet_manifest_header_size;

		for (kotek::uint32_t level = 0;
			 level < out_parsed.m_lod_level_count; ++level)
		{
			out_parsed.m_level_first.push_back(
				zircon_csg_bake_load_u32(p_manifest + cursor));
			out_parsed.m_level_count.push_back(
				zircon_csg_bake_load_u32(p_manifest + cursor + 4));
			cursor += zircon_meshlet_manifest_level_size;
		}

		return true;
	}
} // namespace

// the region growth on a DISCONNECTED soup: no shared vertices — no
// merging (the growth is adjacency-driven, the documented v1 metric).
// 10 welded QUADS are built by hand on top of the disconnected fixture
// (each quad's two triangles re-pointed at their shared 4 vertices):
// exactly one cluster per quad, the budgets trivially respected, the
// triangle count conserved at every level
TEST(Zircon_Game, MeshletClusterizeDisconnectedQuads)
{
	constexpr kotek::uint32_t kQuadCount = 10;

	meshlet_disconnected_fixture_t fixture(kQuadCount * 2);

	// weld the quad pairs: quad q owns vertices 4q..4q+3, its two
	// triangles live at fixture slots 2q and 2q+1
	for (kotek::uint32_t quad = 0; quad < kQuadCount; ++quad)
	{
		fixture.m_indices[(quad * 2 + 0) * 3 + 0] = quad * 4 + 0;
		fixture.m_indices[(quad * 2 + 0) * 3 + 1] = quad * 4 + 1;
		fixture.m_indices[(quad * 2 + 0) * 3 + 2] = quad * 4 + 2;
		fixture.m_indices[(quad * 2 + 1) * 3 + 0] = quad * 4 + 0;
		fixture.m_indices[(quad * 2 + 1) * 3 + 1] = quad * 4 + 2;
		fixture.m_indices[(quad * 2 + 1) * 3 + 2] = quad * 4 + 3;
	}

	const zircon_meshlet_mesh_input_t input =
		fixture.make_input(kQuadCount * 2);

	// heap: the set's tables are sized at the caps (the fixture rule)
	auto* p_set = new zircon_meshlet_lod_set_t();
	ASSERT_TRUE(zircon_meshlet_clusterize_lod_hierarchy(input, *p_set));

	// LOD0: exactly one cluster per quad (20 triangles in 10 clusters
	// of 2), the 124/62 budgets trivially respected
	kotek::uint32_t level_first = 0;
	kotek::uint32_t level_count = 0;
	meshlet_level_range(*p_set, 0u, level_first, level_count);

	EXPECT_EQ(level_count, kQuadCount);

	for (kotek::uint32_t index = 0; index < level_count; ++index)
	{
		const zircon_meshlet_cluster_t& info =
			p_set->m_clusters[level_first + index];

		EXPECT_EQ(info.m_triangle_count, 2u);
		EXPECT_LE(info.m_vertex_count, 62u);
		EXPECT_LE(info.m_triangle_count, 124u);
	}

	// the conservation at every level: the cluster triangle counts sum
	// to the mesh total (the geometry is unchanged by the placeholder
	// pairing)
	for (kotek::uint32_t level = 0;
		 level < p_set->m_level_first_cluster.size() - 1u; ++level)
	{
		meshlet_level_range(*p_set, level, level_first, level_count);

		kotek::uint32_t triangle_sum = 0;

		for (kotek::uint32_t index = 0; index < level_count; ++index)
		{
			triangle_sum +=
				p_set->m_clusters[level_first + index]
					.m_triangle_count;
		}

		EXPECT_EQ(triangle_sum, input.m_triangle_count)
			<< "level " << level << " lost triangles";
	}

	delete p_set;
}

// the region growth on a CONNECTED soup: the welded 8x8 quad grid —
// the budgets hold (the 62-vertex budget binds before the 124-triangle
// one on a plane), every cluster's triangles are vertex-connected (the
// compactness the region growth exists for), every triangle lands in
// exactly one cluster per level, and the exact LOD0 count is pinned
// (the deterministic algorithm's signature — a change to the growth
// order is a deliberate format-affecting decision, never silent)
TEST(Zircon_Game, MeshletClusterizeWeldedGridCompactness)
{
	meshlet_grid_fixture_t fixture;
	const zircon_meshlet_mesh_input_t input = fixture.make_input();

	auto* p_set = new zircon_meshlet_lod_set_t();
	ASSERT_TRUE(zircon_meshlet_clusterize_lod_hierarchy(input, *p_set));

	kotek::uint32_t level_first = 0;
	kotek::uint32_t level_count = 0;
	meshlet_level_range(*p_set, 0u, level_first, level_count);

	// the real growth happened: the 128-triangle grid splits into 2
	// compact clusters (the 62-vertex budget binds at 95 triangles for
	// the seed patch, the remainder follows) — the exact count is the
	// pinned deterministic-algorithm signature: a change to the growth
	// order is a deliberate format-affecting decision, never silent
	EXPECT_GT(level_count, 1u);
	EXPECT_EQ(level_count, 2u);

	kotek::uint32_t seen[128];

	for (kotek::uint32_t level = 0;
		 level < p_set->m_level_first_cluster.size() - 1u; ++level)
	{
		meshlet_level_range(*p_set, level, level_first, level_count);

		for (kotek::uint32_t index = 0;
			 index < input.m_triangle_count; ++index)
		{
			seen[index] = 0u;
		}

		for (kotek::uint32_t index = 0; index < level_count; ++index)
		{
			const zircon_meshlet_cluster_t& info =
				p_set->m_clusters[level_first + index];

			EXPECT_LE(info.m_vertex_count, 62u << level);
			EXPECT_LE(info.m_triangle_count, 124u << level);
			EXPECT_EQ(info.m_level, level);

			// the compactness: the cluster's triangles share vertices
			// (one union-find component)
			EXPECT_TRUE(meshlet_cluster_is_compact(input, *p_set, info))
				<< "level " << level << " cluster " << index
				<< " is not vertex-connected";

			// the AABB is exact from the member vertices
			for (int axis = 0; axis < 3; ++axis)
			{
				double expected_min = 1e30;
				double expected_max = -1e30;

				for (kotek::uint32_t order = 0;
					 order < info.m_triangle_count; ++order)
				{
					const kotek::uint32_t triangle =
						p_set->m_cluster_triangles
							[info.m_first_triangle + order];

					for (kotek::uint8_t corner = 0; corner < 3;
						 ++corner)
					{
						const kotek::uint32_t vertex =
							input.p_indices[triangle * 3 + corner];
						const double value =
							input.p_positions[vertex * 3 + axis];

						if (value < expected_min)
							expected_min = value;
						if (value > expected_max)
							expected_max = value;
					}
				}

				EXPECT_DOUBLE_EQ(info.m_aabb_min[axis], expected_min);
				EXPECT_DOUBLE_EQ(info.m_aabb_max[axis], expected_max);
			}

			for (kotek::uint32_t order = 0;
				 order < info.m_triangle_count; ++order)
			{
				const kotek::uint32_t triangle =
					p_set->m_cluster_triangles[info.m_first_triangle +
						order];

				++seen[triangle];
			}
		}

		// every triangle in EXACTLY one cluster at this level
		for (kotek::uint32_t index = 0;
			 index < input.m_triangle_count; ++index)
		{
			EXPECT_EQ(seen[index], 1u)
				<< "level " << level << " triangle " << index
				<< " seen " << seen[index] << " times";
		}
	}

	delete p_set;
}

// the normal-cone backface math (the banner's derivation, pinned):
// the flat quad cone rejects the away-pointing hemisphere, the open
// cube corner cone carries the exact sin(theta) cutoff, the closed
// cube spans the sphere and gets the never-cull sentinel
TEST(Zircon_Game, MeshletClusterizeConeBackface)
{
	// the flat quad: one cluster, axis +Z, cutoff 0 — the predicate
	// culls exactly the dot(d, axis) > 0 hemisphere
	{
		meshlet_grid_fixture_t fixture;
		const zircon_meshlet_mesh_input_t input =
			fixture.make_input();

		auto* p_set = new zircon_meshlet_lod_set_t();
		ASSERT_TRUE(
			zircon_meshlet_clusterize_lod_hierarchy(input, *p_set));

		const zircon_meshlet_cluster_t& info = p_set->m_clusters[0];

		EXPECT_NEAR(info.m_cone_axis[0], 0.0f, 1e-6f);
		EXPECT_NEAR(info.m_cone_axis[1], 0.0f, 1e-6f);
		EXPECT_NEAR(info.m_cone_axis[2], 1.0f, 1e-6f);
		EXPECT_NEAR(info.m_cone_cutoff, 0.0f, 1e-6f);

		// looking along +Z: the whole cluster is back-facing
		EXPECT_TRUE(zircon_meshlet_cone_culls(
			0.0f, 0.0f, 1.0f, info.m_cone_axis, info.m_cone_cutoff));
		// looking along -Z: front-facing
		EXPECT_FALSE(zircon_meshlet_cone_culls(
			0.0f, 0.0f, -1.0f, info.m_cone_axis, info.m_cone_cutoff));
		// looking along +X: dot == 0, not > 0 — boundary, kept
		EXPECT_FALSE(zircon_meshlet_cone_culls(
			1.0f, 0.0f, 0.0f, info.m_cone_axis, info.m_cone_cutoff));

		delete p_set;
	}

	// the open +X+Y+Z cube corner: 6 triangles, one cluster; the axis
	// is (1,1,1)/sqrt(3), the cutoff is sin(theta) = sqrt(2/3)
	{
		static const double k_kept[3][3] = {
			{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};

		meshlet_cube_fixture_t fixture(k_kept);
		const zircon_meshlet_mesh_input_t input =
			fixture.make_input();
		ASSERT_EQ(input.m_triangle_count, 6u);

		auto* p_set = new zircon_meshlet_lod_set_t();
		ASSERT_TRUE(
			zircon_meshlet_clusterize_lod_hierarchy(input, *p_set));
		ASSERT_EQ(p_set->m_clusters.size(), 1u);

		const zircon_meshlet_cluster_t& info = p_set->m_clusters[0];

		const double kInvSqrt3 = 0.5773502691896258;
		EXPECT_NEAR(
			info.m_cone_axis[0], kInvSqrt3, 1e-5f);
		EXPECT_NEAR(
			info.m_cone_axis[1], kInvSqrt3, 1e-5f);
		EXPECT_NEAR(
			info.m_cone_axis[2], kInvSqrt3, 1e-5f);
		EXPECT_NEAR(info.m_cone_cutoff, std::sqrt(2.0 / 3.0), 1e-5f);

		// looking out through the corner: the cone is entirely
		// back-facing
		EXPECT_TRUE(zircon_meshlet_cone_culls(
			static_cast<float>(kInvSqrt3),
			static_cast<float>(kInvSqrt3),
			static_cast<float>(kInvSqrt3), info.m_cone_axis,
			info.m_cone_cutoff));
		// looking into the corner: front-facing
		EXPECT_FALSE(zircon_meshlet_cone_culls(
			-static_cast<float>(kInvSqrt3),
			-static_cast<float>(kInvSqrt3),
			-static_cast<float>(kInvSqrt3), info.m_cone_axis,
			info.m_cone_cutoff));

		delete p_set;
	}

	// the closed cube: the six face normals cancel — the mean is
	// degenerate, the never-cull sentinel answers every direction
	{
		meshlet_cube_fixture_t fixture(nullptr);
		const zircon_meshlet_mesh_input_t input =
			fixture.make_input();
		ASSERT_EQ(input.m_triangle_count, 12u);

		auto* p_set = new zircon_meshlet_lod_set_t();
		ASSERT_TRUE(
			zircon_meshlet_clusterize_lod_hierarchy(input, *p_set));
		ASSERT_EQ(p_set->m_clusters.size(), 1u);

		const zircon_meshlet_cluster_t& info = p_set->m_clusters[0];

		EXPECT_GT(info.m_cone_cutoff, 1.0f);
		EXPECT_EQ(info.m_cone_axis[0], 0.0f);
		EXPECT_EQ(info.m_cone_axis[1], 0.0f);
		EXPECT_EQ(info.m_cone_axis[2], 0.0f);

		// no view direction is ever cone-culled
		for (kotek::uint32_t index = 0; index < 64; ++index)
		{
			const double z = 1.0 - 2.0 * (index + 0.5) / 64.0;
			const double radius = std::sqrt(1.0 - z * z);
			const double phi = index * 2.399963229728653;
			const float direction[3] = {
				static_cast<float>(radius * std::cos(phi)),
				static_cast<float>(radius * std::sin(phi)),
				static_cast<float>(z)};

			EXPECT_FALSE(zircon_meshlet_cone_culls(direction[0],
				direction[1], direction[2], info.m_cone_axis,
				info.m_cone_cutoff));
		}

		delete p_set;
	}
}

// the placeholder LOD hierarchy: 63 disconnected triangles give 63
// LOD0 clusters — the greedy pairing halves the count EXACTLY per
// level (63, 32, 16, 8, 4, 2) until the six-level cap, the parent/
// child links cover every leaf exactly once, the parents inherit the
// children's geometry unchanged (the error metric is exactly zero)
TEST(Zircon_Game, MeshletLodHierarchyLinks)
{
	constexpr kotek::uint32_t kTriangleCount = 63;

	meshlet_disconnected_fixture_t fixture(kTriangleCount);
	const zircon_meshlet_mesh_input_t input =
		fixture.make_input(kTriangleCount);

	auto* p_set = new zircon_meshlet_lod_set_t();
	ASSERT_TRUE(zircon_meshlet_clusterize_lod_hierarchy(input, *p_set));

	// the level cap: 6 levels, the counts halved exactly per level
	ASSERT_EQ(p_set->m_level_first_cluster.size() - 1u, 6u);

	const kotek::uint32_t expected[6] = {63, 32, 16, 8, 4, 2};

	for (kotek::uint32_t level = 0; level < 6; ++level)
	{
		kotek::uint32_t level_first = 0;
		kotek::uint32_t level_count = 0;
		meshlet_level_range(*p_set, level, level_first, level_count);

		EXPECT_EQ(level_count, expected[level]) << "level " << level;
	}

	// every LOD0 cluster appears in EXACTLY one parent's child run;
	// the link table's size is the total clusters minus the top level
	// (every non-root cluster is exactly one parent's child)
	const kotek::uint32_t leaf_count =
		p_set->m_level_first_cluster[1u] -
		p_set->m_level_first_cluster[0u];
	const kotek::uint32_t total_clusters =
		static_cast<kotek::uint32_t>(p_set->m_clusters.size());
	EXPECT_EQ(leaf_count, 63u);
	EXPECT_EQ(p_set->m_child_links.size(),
		total_clusters - expected[5]);

	kotek::uint32_t leaf_hits[63];

	for (kotek::uint32_t index = 0; index < leaf_count; ++index)
	{
		leaf_hits[index] = 0u;
	}

	for (kotek::uint32_t cluster = 0;
		 cluster < p_set->m_clusters.size(); ++cluster)
	{
		const zircon_meshlet_cluster_t& info =
			p_set->m_clusters[cluster];

		if (info.m_level == 0u)
		{
			EXPECT_EQ(info.m_child_count, 0u);
			continue;
		}

		EXPECT_LE(info.m_child_count, 2u);
		EXPECT_EQ(info.m_error_metric, 0.0f);

		// the children live in the PREVIOUS level's range (level-major
		// index space)
		const kotek::uint32_t prev_begin =
			p_set->m_level_first_cluster[info.m_level - 1u];
		const kotek::uint32_t prev_end =
			p_set->m_level_first_cluster[info.m_level];

		// the parent's triangle count is the sum of its children's
		kotek::uint32_t child_tris = 0;

		for (kotek::uint32_t link = 0; link < info.m_child_count;
			 ++link)
		{
			const kotek::uint32_t child =
				p_set->m_child_links[info.m_first_child_link + link];

			ASSERT_GE(child, prev_begin);
			ASSERT_LT(child, prev_end);

			// the level-1 parents own the leaves — the coverage count
			if (info.m_level == 1u)
			{
				const kotek::uint32_t leaf = child - prev_begin;
				ASSERT_LT(leaf, leaf_count);
				++leaf_hits[leaf];
			}

			child_tris += p_set->m_clusters[child].m_triangle_count;
		}

		EXPECT_EQ(info.m_triangle_count, child_tris);

		// the parent's AABB is the union of its children's AABBs
		for (int axis = 0; axis < 3; ++axis)
		{
			double expected_min = 1e30;
			double expected_max = -1e30;

			for (kotek::uint32_t link = 0; link < info.m_child_count;
				 ++link)
			{
				const zircon_meshlet_cluster_t& child_info =
					p_set->m_clusters[p_set->m_child_links
							[info.m_first_child_link + link]];

				if (child_info.m_aabb_min[axis] < expected_min)
					expected_min = child_info.m_aabb_min[axis];
				if (child_info.m_aabb_max[axis] > expected_max)
					expected_max = child_info.m_aabb_max[axis];
			}

			EXPECT_DOUBLE_EQ(info.m_aabb_min[axis], expected_min);
			EXPECT_DOUBLE_EQ(info.m_aabb_max[axis], expected_max);
		}
	}

	for (kotek::uint32_t index = 0; index < leaf_count; ++index)
	{
		EXPECT_EQ(leaf_hits[index], 1u)
			<< "leaf " << index << " is linked " << leaf_hits[index]
			<< " times";
	}

	delete p_set;
}

// the pack roundtrip: clusterize -> pack -> a FRESH filesystem mounts
// the pack at Initialize (the boot dispatcher) -> the manifest + the
// bins read back through the priority chain, byte-consistent with the
// in-memory hierarchy, the bins' positions within the documented
// half-LSB bound of the source vertices
TEST(Zircon_Game, MeshletPackRoundtrip)
{
	meshlet_grid_fixture_t fixture;
	const zircon_meshlet_mesh_input_t input = fixture.make_input();

	meshlet_test_env& env = *new meshlet_test_env();
	env.initialize();

	const kotek::static_path_t pack_path =
		meshlet_make_pack_path(env, "meshlet_rt.kpack");
	meshlet_remove_pack(pack_path);
	meshlet_ensure_packs_folder(env);

	// the in-memory reference hierarchy
	auto* p_reference = new zircon_meshlet_lod_set_t();
	ASSERT_TRUE(
		zircon_meshlet_clusterize_lod_hierarchy(input, *p_reference));

	zircon_meshlet_bake_result_t result{};
	ASSERT_TRUE(zircon_meshlet_clusterize_pack(&env.filesystem,
		"data_game/packs/meshlet_rt.kpack", input, "rt", result));

	EXPECT_EQ(result.m_emitted_lod_level_count,
		p_reference->m_level_first_cluster.size() - 1u);
	EXPECT_EQ(result.m_emitted_cluster_count,
		p_reference->m_clusters.size());
	EXPECT_EQ(result.m_emitted_child_link_count,
		p_reference->m_child_links.size());
	EXPECT_EQ(result.m_mesh_triangle_count, input.m_triangle_count);

	// a FRESH reader mounts the pack at Initialize
	meshlet_test_env& reader = *new meshlet_test_env();
	reader.initialize();

	kotek::size_t manifest_size = 0;
	kotek::uint8_t* p_manifest = meshlet_read_through_dispatcher(reader,
		meshlet_entry_path(reader, "meshlets/rt/manifest.bin"),
		manifest_size);
	ASSERT_TRUE(p_manifest != nullptr);

	auto* p_parsed = new meshlet_parsed_manifest_t();
	ASSERT_TRUE(meshlet_parse_manifest(
		p_manifest, manifest_size, *p_parsed));

	EXPECT_EQ(p_parsed->m_mesh_triangle_count,
		input.m_triangle_count);
	EXPECT_EQ(p_parsed->m_lod_level_count,
		p_reference->m_level_first_cluster.size() - 1u);
	EXPECT_EQ(p_parsed->m_cluster_count_total,
		p_reference->m_clusters.size());
	EXPECT_EQ(p_parsed->m_link_count_total,
		p_reference->m_child_links.size());

	for (kotek::uint32_t level = 0;
		 level < p_parsed->m_lod_level_count; ++level)
	{
		EXPECT_EQ(p_parsed->m_level_first[level],
			p_reference->m_level_first_cluster[level]);
		EXPECT_EQ(p_parsed->m_level_count[level],
			p_reference->m_level_first_cluster[level + 1u] -
				p_reference->m_level_first_cluster[level]);
	}

	// every cluster bin: the header fields consistent with the
	// manifest record, the positions within the half-LSB bound of the
	// source vertices, the indices inside the cluster, the materials
	// conserved
	for (kotek::uint32_t level = 0;
		 level < p_parsed->m_lod_level_count; ++level)
	{
		const kotek::uint32_t level_first =
			p_parsed->m_level_first[level];
		const kotek::uint32_t level_count =
			p_parsed->m_level_count[level];

		for (kotek::uint32_t index = 0; index < level_count; ++index)
		{
			const zircon_meshlet_cluster_t& reference =
				p_reference->m_clusters[level_first + index];

			kotek::static_cstring_t<
				ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH>
				entry_name;
			entry_name.assign("meshlets/rt/lod_");
			char level_digits[2] = {static_cast<char>('0' + level),
				'\0'};
			entry_name += level_digits;
			entry_name += "/cluster_";

			char index_digits[10];
			kotek::uint32_t digit_count = 0;
			kotek::uint32_t index_value = index;

			do
			{
				index_digits[digit_count++] =
					static_cast<char>('0' + (index_value % 10u));
				index_value /= 10u;
			} while (index_value != 0u);

			while (digit_count > 0)
			{
				char symbol[2] = {index_digits[--digit_count], '\0'};
				entry_name += symbol;
			}

			entry_name += ".bin";

			kotek::size_t bin_size = 0;
			kotek::uint8_t* p_bin = meshlet_read_through_dispatcher(
				reader, meshlet_entry_path(reader, entry_name.c_str()),
				bin_size);
			ASSERT_TRUE(p_bin != nullptr)
				<< "the bin " << entry_name.c_str() << " is missing";

			ASSERT_GE(bin_size, zircon_meshlet_cluster_header_size);
			EXPECT_EQ(std::memcmp(p_bin, zircon_meshlet_cluster_magic,
						  4),
				0);

			const kotek::uint32_t bin_verts =
				zircon_csg_bake_load_u32(p_bin + 4);
			const kotek::uint32_t bin_tris =
				zircon_csg_bake_load_u32(p_bin + 8);
			const kotek::uint32_t index_width =
				zircon_csg_bake_load_u32(p_bin + 12);

			EXPECT_EQ(bin_verts, reference.m_vertex_count);
			EXPECT_EQ(bin_tris, reference.m_triangle_count);
			EXPECT_EQ(index_width, level == 0u ? 1u : 2u);

			const kotek::uint32_t* p_tris =
				p_reference->m_cluster_triangles.data() +
				reference.m_first_triangle;

			// the expected weld: the cluster's deterministic first-use
			// order (the bin must reproduce it exactly — the slot per
			// corner vertex is the weld's bijection proof)
			kotek::int32_t expected_slot[128];
			kotek::uint32_t expected_count = 0;

			for (kotek::uint32_t vertex = 0;
				 vertex < input.m_vertex_count; ++vertex)
			{
				expected_slot[vertex] = -1;
			}

			for (kotek::uint32_t order = 0;
				 order < reference.m_triangle_count; ++order)
			{
				for (kotek::uint8_t corner = 0; corner < 3; ++corner)
				{
					const kotek::uint32_t vertex =
						input.p_indices[p_tris[order] * 3 + corner];

					if (expected_slot[vertex] < 0)
					{
						expected_slot[vertex] =
							static_cast<kotek::int32_t>(
								expected_count);
						++expected_count;
					}
				}
			}

			EXPECT_EQ(expected_count, bin_verts);

			kotek::uint32_t cursor =
				zircon_meshlet_cluster_header_size;

			// the positions: every dequantized vertex is within the
			// half-LSB bound of its source vertex
			for (kotek::uint32_t slot = 0; slot < bin_verts; ++slot)
			{
				double decoded[3];

				for (int axis = 0; axis < 3; ++axis)
				{
					zircon_meshlet_position_quant_t quantum = 0;

					if constexpr (sizeof(
									  zircon_meshlet_position_quant_t) ==
						2)
					{
						quantum =
							static_cast<zircon_meshlet_position_quant_t>(
								zircon_csg_bake_load_u16(p_bin +
									cursor));
						cursor += 2;
					}
					else
					{
						quantum =
							static_cast<zircon_meshlet_position_quant_t>(
								p_bin[cursor]);
						cursor += 1;
					}

					const double extent = reference.m_aabb_max[axis] -
						reference.m_aabb_min[axis];

					decoded[axis] = zircon_meshlet_dequantize_position(
						quantum, reference.m_aabb_min[axis], extent);
				}

				// the slot's source vertex: the weld order is the
				// cluster's first-use order — the reference set does
				// not carry the map, so match by proximity (the bound
				// is far below the grid spacing)
				bool is_found = false;

				for (kotek::uint32_t vertex = 0;
					 vertex < input.m_vertex_count; ++vertex)
				{
					bool is_match = true;

					for (int axis = 0; axis < 3; ++axis)
					{
						const double extent =
							reference.m_aabb_max[axis] -
							reference.m_aabb_min[axis];
						const double bound =
							zircon_meshlet_position_error_bound(extent) +
							1e-9;

						if (std::fabs(
								decoded[axis] -
								input.p_positions[vertex * 3 + axis]) >
							bound)
						{
							is_match = false;
							break;
						}
					}

					if (is_match)
					{
						is_found = true;
						break;
					}
				}

				EXPECT_TRUE(is_found) << "the bin vertex " << slot
									  << " matches no source vertex";
			}

			// the octahedral normals (skipped cursor-wise: 2 bytes per
			// triangle)
			cursor += bin_tris * 2;

			// the indices: inside the cluster, addressing exactly the
			// deterministic weld slot of the corner's vertex
			for (kotek::uint32_t order = 0; order < bin_tris;
				 ++order)
			{
				for (kotek::uint8_t corner = 0; corner < 3; ++corner)
				{
					kotek::uint32_t slot = 0;

					if (index_width == 1u)
					{
						slot = p_bin[cursor];
						cursor += 1;
					}
					else
					{
						slot = zircon_csg_bake_load_u16(p_bin + cursor);
						cursor += 2;
					}

					EXPECT_LT(slot, bin_verts);

					const kotek::uint32_t triangle = p_tris[order];
					const kotek::uint32_t corner_vertex =
						input.p_indices[triangle * 3 + corner];

					ASSERT_EQ(expected_slot[corner_vertex] >= 0, true);
					EXPECT_EQ(slot,
						static_cast<kotek::uint32_t>(
							expected_slot[corner_vertex]));
				}
			}

			// the materials
			for (kotek::uint32_t order = 0; order < bin_tris; ++order)
			{
				const kotek::uint16_t material =
					zircon_csg_bake_load_u16(p_bin + cursor);
				cursor += 2;

				EXPECT_EQ(material,
					input.p_materials[p_tris[order]]);
			}

			EXPECT_EQ(cursor, bin_size);

			delete[] p_bin;
		}
	}

	delete p_parsed;
	delete[] p_manifest;

	reader.shutdown();
	delete &reader;

	delete p_reference;
	env.shutdown();
	delete &env;

	meshlet_remove_pack(pack_path);
}

// the streaming order: the pack's entry table carries the manifest
// first, then the cluster bins level-DESCENDING (the coarsest level
// first — the documented progressive-loading posture)
TEST(Zircon_Game, MeshletPackStreamingOrder)
{
	constexpr kotek::uint32_t kTriangleCount = 4;

	meshlet_disconnected_fixture_t fixture(kTriangleCount);
	const zircon_meshlet_mesh_input_t input =
		fixture.make_input(kTriangleCount);

	meshlet_test_env& env = *new meshlet_test_env();
	env.initialize();

	const kotek::static_path_t pack_path =
		meshlet_make_pack_path(env, "meshlet_order.kpack");
	meshlet_remove_pack(pack_path);
	meshlet_ensure_packs_folder(env);

	zircon_meshlet_bake_result_t result{};
	ASSERT_TRUE(zircon_meshlet_clusterize_pack(&env.filesystem,
		"data_game/packs/meshlet_order.kpack", input, "order",
		result));

	// 4 disconnected triangles: LOD0 = 4, LOD1 = 2, LOD2 = 1 — 3
	// levels, 7 bins + the manifest = 8 entries
	ASSERT_EQ(result.m_emitted_lod_level_count, 3u);
	ASSERT_EQ(result.m_emitted_cluster_count, 7u);

	kotek::size_t pack_size = 0;
	kotek::uint8_t* p_pack = meshlet_read_through_dispatcher(
		env, pack_path, pack_size);
	ASSERT_TRUE(p_pack != nullptr);

	ASSERT_GE(pack_size, 20u + 8u * 45u);
	EXPECT_EQ(std::memcmp(p_pack, kotek::core::kKpackMagic, 8), 0);

	const kotek::uint32_t entry_count =
		zircon_csg_bake_load_u32(p_pack + 8);
	ASSERT_EQ(entry_count, 8u);

	const char* expected_names[8] = {"meshlets/order/manifest.bin",
		"meshlets/order/lod_2/cluster_0.bin",
		"meshlets/order/lod_1/cluster_0.bin",
		"meshlets/order/lod_1/cluster_1.bin",
		"meshlets/order/lod_0/cluster_0.bin",
		"meshlets/order/lod_0/cluster_1.bin",
		"meshlets/order/lod_0/cluster_2.bin",
		"meshlets/order/lod_0/cluster_3.bin"};

	for (kotek::uint32_t index = 0; index < entry_count; ++index)
	{
		const kotek::uint64_t name_hash =
			zircon_csg_bake_load_u64(p_pack + 20 + index * 45);

		EXPECT_EQ(name_hash,
			kotek::core::kpack_hash_name(expected_names[index],
				std::strlen(expected_names[index])))
			<< "entry " << index << " is out of the streaming order";
	}

	delete[] p_pack;

	env.shutdown();
	delete &env;

	meshlet_remove_pack(pack_path);
}

// the DETERMINISM contract: two bakes of the same soup produce
// BYTE-IDENTICAL packs (the index-ordered decisions + the fixed
// rounding rules + the deterministic encoder)
TEST(Zircon_Game, MeshletBakeDeterminism)
{
	meshlet_grid_fixture_t fixture;
	const zircon_meshlet_mesh_input_t input = fixture.make_input();

	meshlet_test_env& env = *new meshlet_test_env();
	env.initialize();

	const kotek::static_path_t first_path =
		meshlet_make_pack_path(env, "meshlet_det_a.kpack");
	const kotek::static_path_t second_path =
		meshlet_make_pack_path(env, "meshlet_det_b.kpack");
	meshlet_remove_pack(first_path);
	meshlet_remove_pack(second_path);
	meshlet_ensure_packs_folder(env);

	zircon_meshlet_bake_result_t first_result{};
	ASSERT_TRUE(zircon_meshlet_clusterize_pack(&env.filesystem,
		"data_game/packs/meshlet_det_a.kpack", input, "det",
		first_result));

	zircon_meshlet_bake_result_t second_result{};
	ASSERT_TRUE(zircon_meshlet_clusterize_pack(&env.filesystem,
		"data_game/packs/meshlet_det_b.kpack", input, "det",
		second_result));

	EXPECT_EQ(first_result.m_emitted_cluster_count,
		second_result.m_emitted_cluster_count);
	EXPECT_EQ(first_result.m_emitted_lod_level_count,
		second_result.m_emitted_lod_level_count);
	EXPECT_EQ(first_result.m_emitted_child_link_count,
		second_result.m_emitted_child_link_count);

	kotek::size_t first_size = 0;
	kotek::uint8_t* p_first =
		meshlet_read_through_dispatcher(env, first_path, first_size);
	ASSERT_TRUE(p_first != nullptr);

	kotek::size_t second_size = 0;
	kotek::uint8_t* p_second =
		meshlet_read_through_dispatcher(env, second_path, second_size);
	ASSERT_TRUE(p_second != nullptr);

	ASSERT_EQ(first_size, second_size);
	EXPECT_EQ(std::memcmp(p_first, p_second, first_size), 0);

	delete[] p_first;
	delete[] p_second;

	env.shutdown();
	delete &env;

	meshlet_remove_pack(first_path);
	meshlet_remove_pack(second_path);
}

// the capacity guards: a disconnected soup over the scene cluster
// budget is a LOUD HARD failure (the pack would be unloadable — no
// file is written); an invalid scene name and an out-of-bounds mesh
// are loud caller errors
TEST(Zircon_Game, MeshletCapacityGuards)
{
	meshlet_test_env& env = *new meshlet_test_env();
	env.initialize();

	const kotek::static_path_t pack_path =
		meshlet_make_pack_path(env, "meshlet_capacity.kpack");
	meshlet_remove_pack(pack_path);
	meshlet_ensure_packs_folder(env);

	// 4091 disconnected triangles: each is its own cluster — one over
	// the ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE (= 4090) budget
	meshlet_disconnected_fixture_t fixture(
		ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE + 1);
	const zircon_meshlet_mesh_input_t input = fixture.make_input(
		ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE + 1);

	zircon_meshlet_bake_result_t result{};
	EXPECT_FALSE(zircon_meshlet_clusterize_pack(&env.filesystem,
		"data_game/packs/meshlet_capacity.kpack", input, "capacity",
		result));

	// no pack was written
	kotek::size_t file_size = 0;
	EXPECT_FALSE(env.filesystem.Get_FileSize(pack_path, file_size));

	// an invalid scene name (a path walk) is a hard caller error
	zircon_meshlet_bake_result_t bad_name_result{};
	EXPECT_FALSE(zircon_meshlet_clusterize_pack(&env.filesystem,
		"data_game/packs/meshlet_capacity.kpack", input, "../escape",
		bad_name_result));

	// a mesh outside the supported bounds is a hard caller error (the
	// tables are sized at the caps; the count check fires before any
	// table is touched)
	{
		double position = 0.0;
		kotek::uint32_t index = 0;
		double normal = 0.0;
		kotek::uint16_t material = 0;

		zircon_meshlet_mesh_input_t oversized{};
		oversized.p_positions = &position;
		oversized.m_vertex_count = 1;
		oversized.p_indices = &index;
		oversized.p_normals = &normal;
		oversized.p_materials = &material;
		oversized.m_triangle_count =
			ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES + 1;

		zircon_meshlet_bake_result_t oversized_result{};
		EXPECT_FALSE(zircon_meshlet_clusterize_pack(&env.filesystem,
			"data_game/packs/meshlet_capacity.kpack", oversized,
			"oversized", oversized_result));
	}

	env.shutdown();
	delete &env;

	meshlet_remove_pack(pack_path);
}

		#endif
	#endif
#endif
