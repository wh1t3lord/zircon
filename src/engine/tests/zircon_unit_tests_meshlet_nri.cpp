#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cmath>
		#include <cstring>

		#include "../../core/zircon_meshlet_cluster_read.h"
		#include "../../render/nri/passes/zircon_nri_passlib.h"

		#ifndef ZIRCON_DEF_UNIT_TEST_MESHLET_NRI
			#define ZIRCON_DEF_UNIT_TEST_MESHLET_NRI 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_MESHLET_NRI == 1

// functional proofs for task Z24 B3b (the nanite path's geometry seam +
// the first real NRI draw): the B3a-format reader (the manifest + the
// cluster bin parse, the soup vertex-buffer layout), the shipped boot
// fixture (loads through the REAL filesystem dispatcher — the exact data
// flow the NRI pass runs at boot) and the NRI passlib registry/lifecycle
// (the meshlet pass registered, the initialize seam, the inert-without-
// content contract). Tier: lightweight (rule 8a — a two-triangle quad
// soup + the 152-byte shipped cluster; no GPU needed).

namespace
{
	// the headless environment (the meshlet B3a fixture's shape): a real
	// filesystem behind a real framework config — the boot dispatcher, no
	// engine session needed
	struct meshlet_nri_test_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;

		void initialize(void)
		{
			this->filesystem.Initialize(&this->framework_config);
		}

		void shutdown(void) { this->filesystem.Shutdown(); }
	};

	// builds a synthetic cluster bin (the B3a layout) of a single quad
	// (2 welded vertices? no — 4 welded vertices, 2 triangles) with known
	// positions/normals through the REAL codecs (the LE store + the
	// position quantum + the octahedral codec) — the byte layout the read
	// must decode
	struct synthetic_cluster_bin
	{
		// a 2x2 quad in the XY plane welded at 4 vertices, z = 0.5
		static constexpr kotek::uint32_t k_vertex_count = 4;
		static constexpr kotek::uint32_t k_triangle_count = 2;
		static constexpr double k_aabb_min[3] = {-1.0, -1.0, 0.5};
		static constexpr double k_aabb_max[3] = {1.0, 1.0, 0.5};

		// corner order: 0=(-1,-1) 1=(1,-1) 2=(-1,1) 3=(1,1)
		static constexpr double k_positions[4][3] = {
			{-1.0, -1.0, 0.5}, {1.0, -1.0, 0.5}, {-1.0, 1.0, 0.5},
			{1.0, 1.0, 0.5}};

		static constexpr kotek::uint8_t k_indices[2][3] = {{0, 1, 3},
			{0, 3, 2}};

		static constexpr kotek::uint32_t size_bytes(void)
		{
			return zircon_meshlet_cluster_header_size +
				k_vertex_count * 3u * 2u + // u16 positions
				k_triangle_count * 2u + // oct normals
				k_triangle_count * 3u * 1u + // u8 indices
				k_triangle_count * 2u; // u16 materials
		}

		static void build(kotek::uint8_t* p_out)
		{
			std::memcpy(p_out, zircon_meshlet_cluster_magic, 4);
			zircon_csg_bake_store_u32(
				p_out + 4, k_vertex_count);
			zircon_csg_bake_store_u32(
				p_out + 8, k_triangle_count);
			zircon_csg_bake_store_u32(p_out + 12, 1u); // u8 at LOD0
			zircon_csg_bake_store_u32(p_out + 16, 0u); // reserved

			kotek::uint8_t* p_cursor = p_out +
				zircon_meshlet_cluster_header_size;

			for (kotek::uint32_t vertex = 0; vertex < k_vertex_count;
				 ++vertex)
			{
				for (int axis = 0; axis < 3; ++axis)
				{
					const kotek::uint16_t quant =
						zircon_meshlet_quantize_position(
							k_positions[vertex][axis], k_aabb_min[axis],
							k_aabb_max[axis] - k_aabb_min[axis]);

					zircon_csg_bake_store_u16(p_cursor, quant);
					p_cursor += 2;
				}
			}

			// both triangles face +z
			const double normal[3] = {0.0, 0.0, 1.0};

			for (kotek::uint32_t triangle = 0;
				 triangle < k_triangle_count; ++triangle)
			{
				zircon_csg_bake_encode_normal_oct_u8(
					normal, p_cursor);
				p_cursor += 2;
			}

			for (kotek::uint32_t triangle = 0;
				 triangle < k_triangle_count; ++triangle)
			{
				for (int corner = 0; corner < 3; ++corner)
				{
					*p_cursor = k_indices[triangle][corner];
					++p_cursor;
				}
			}

			for (kotek::uint32_t triangle = 0;
				 triangle < k_triangle_count; ++triangle)
			{
				zircon_csg_bake_store_u16(p_cursor, 7u); // a material id
				p_cursor += 2;
			}
		}
	};

	// the reader's exact byte expectations for the synthetic quad: the
	// soup vertex float layout [pos.xyz | normal.xyz] and the identity IB
	TEST(Zircon_NriMeshlet, ClusterReadLayoutPins)
	{
		kotek::uint8_t bin[synthetic_cluster_bin::size_bytes()]{};
		synthetic_cluster_bin::build(bin);

		float vb[zircon_meshlet_read_vb_capacity_floats(
			synthetic_cluster_bin::k_triangle_count)]{};
		kotek::uint32_t
			ib[zircon_meshlet_read_ib_capacity(
				synthetic_cluster_bin::k_triangle_count)]{};

		zircon_meshlet_cluster_content_t content{};

		ASSERT_TRUE(zircon_meshlet_read_cluster(bin,
			synthetic_cluster_bin::size_bytes(),
			synthetic_cluster_bin::k_aabb_min,
			synthetic_cluster_bin::k_aabb_max, vb,
			zircon_meshlet_read_vb_capacity_floats(
				synthetic_cluster_bin::k_triangle_count),
			ib,
			zircon_meshlet_read_ib_capacity(
				synthetic_cluster_bin::k_triangle_count),
			content));

		EXPECT_EQ(content.m_welded_vertex_count, 4u);
		EXPECT_EQ(content.m_triangle_count, 2u);
		EXPECT_EQ(content.m_soup_vertex_count, 6u);
		EXPECT_EQ(content.m_index_count, 6u);

		// the half-LSB bound of the position quantum (a flat z axis:
		// dequantizes to the bound exactly)
		const double bound = zircon_meshlet_position_error_bound(2.0);

		for (kotek::uint32_t soup_vertex = 0; soup_vertex < 6u;
			 ++soup_vertex)
		{
			const float* p_vertex =
				vb + soup_vertex *
					ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_FLOATS;

			// the corner this soup vertex carries (the identity
			// expansion): triangle t = soup_vertex / 3, corner index from
			// the baked table
			const kotek::uint32_t triangle = soup_vertex / 3u;
			const kotek::uint32_t corner = soup_vertex % 3u;
			const kotek::uint32_t welded =
				synthetic_cluster_bin::k_indices[triangle][corner];

			for (int axis = 0; axis < 3; ++axis)
			{
				EXPECT_NEAR(p_vertex[axis],
					synthetic_cluster_bin::k_positions[welded][axis],
					bound + 1e-7);
			}

			// the flat +z normal, decoded
			EXPECT_NEAR(p_vertex[3], 0.0, 0.02);
			EXPECT_NEAR(p_vertex[4], 0.0, 0.02);
			EXPECT_NEAR(p_vertex[5], 1.0, 0.02);

			// the identity IB
			EXPECT_EQ(ib[soup_vertex], soup_vertex);
		}
	}

	// the corrupt-content contract: a skew never crashes, never reads
	// past the buffer — every guard returns false
	TEST(Zircon_NriMeshlet, ClusterReadRejectsSkew)
	{
		kotek::uint8_t bin[synthetic_cluster_bin::size_bytes()]{};
		synthetic_cluster_bin::build(bin);

		float vb[zircon_meshlet_read_vb_capacity_floats(2)]{};
		kotek::uint32_t ib[zircon_meshlet_read_ib_capacity(2)]{};
		zircon_meshlet_cluster_content_t content{};

		// a truncated bin
		EXPECT_FALSE(zircon_meshlet_read_cluster(bin,
			synthetic_cluster_bin::size_bytes() - 1u,
			synthetic_cluster_bin::k_aabb_min,
			synthetic_cluster_bin::k_aabb_max, vb,
			zircon_meshlet_read_vb_capacity_floats(2u), ib,
			zircon_meshlet_read_ib_capacity(2u), content));

		// a bad magic
		bin[0] = 'X';
		EXPECT_FALSE(zircon_meshlet_read_cluster(bin,
			synthetic_cluster_bin::size_bytes(),
			synthetic_cluster_bin::k_aabb_min,
			synthetic_cluster_bin::k_aabb_max, vb,
			zircon_meshlet_read_vb_capacity_floats(2u), ib,
			zircon_meshlet_read_ib_capacity(2u), content));
		bin[0] = 'M';

		// an index outside the welded vertex count (corrupt table)
		bin[zircon_meshlet_cluster_header_size + 4u * 3u * 2u + 2u * 2u +
			2u] = 200; // the third index byte (triangle 0, corner 2)
		EXPECT_FALSE(zircon_meshlet_read_cluster(bin,
			synthetic_cluster_bin::size_bytes(),
			synthetic_cluster_bin::k_aabb_min,
			synthetic_cluster_bin::k_aabb_max, vb,
			zircon_meshlet_read_vb_capacity_floats(2u), ib,
			zircon_meshlet_read_ib_capacity(2u), content));

		// an undersized caller buffer (the B0 required-size contract)
		EXPECT_FALSE(zircon_meshlet_read_cluster(bin,
			synthetic_cluster_bin::size_bytes(),
			synthetic_cluster_bin::k_aabb_min,
			synthetic_cluster_bin::k_aabb_max, vb,
			zircon_meshlet_read_vb_capacity_floats(2u) - 1u, ib,
			zircon_meshlet_read_ib_capacity(2u), content));
	}

	// the manifest parse: the header + the level table + the record
	// fields, byte-pinned; the corrupt controls
	TEST(Zircon_NriMeshlet, ManifestParsePins)
	{
		// one LOD0 cluster record, minimal manifest
		kotek::uint8_t manifest[zircon_meshlet_manifest_header_size +
			zircon_meshlet_manifest_level_size +
			zircon_meshlet_manifest_record_size]{};

		std::memcpy(manifest, zircon_meshlet_manifest_magic, 8);
		zircon_csg_bake_store_u32(manifest + 12, 1u); // levels
		zircon_csg_bake_store_u32(manifest + 16, 1u); // clusters
		zircon_csg_bake_store_u32(manifest + 20, 12u); // mesh triangles
		zircon_csg_bake_store_u32(manifest + 24, 0u); // child links

		zircon_csg_bake_store_u32(manifest + 32, 0u); // level 0 first
		zircon_csg_bake_store_u32(manifest + 36, 1u); // level 0 count

		kotek::uint8_t* p_record = manifest + 40;
		zircon_csg_bake_store_u32(p_record + 0, 12u); // triangles
		zircon_csg_bake_store_u32(p_record + 4, 8u); // vertices
		zircon_csg_bake_store_u32(p_record + 16, 0u); // error f32 bits
		zircon_csg_bake_store_u32(p_record + 20, 0x40000000u); // 2.0f
		zircon_csg_bake_store_f64(p_record + 36, -1.0);
		zircon_csg_bake_store_f64(p_record + 60, 1.0);
		zircon_csg_bake_store_u16(p_record + 84, 0u);
		zircon_csg_bake_store_u16(p_record + 86, 3u);

		const kotek::size_t manifest_size = sizeof(manifest);

		zircon_meshlet_read_manifest_header_t header{};

		ASSERT_TRUE(zircon_meshlet_read_manifest_header(
			manifest, manifest_size, header));
		EXPECT_EQ(header.m_lod_level_count, 1u);
		EXPECT_EQ(header.m_cluster_count_total, 1u);
		EXPECT_EQ(header.m_mesh_triangle_count, 12u);

		kotek::uint32_t first = 99;
		kotek::uint32_t count = 99;

		ASSERT_TRUE(zircon_meshlet_read_level_range(
			manifest, manifest_size, 0u, first, count));
		EXPECT_EQ(first, 0u);
		EXPECT_EQ(count, 1u);
		EXPECT_FALSE(zircon_meshlet_read_level_range(
			manifest, manifest_size, 1u, first, count));

		zircon_meshlet_cluster_t record{};

		ASSERT_TRUE(zircon_meshlet_read_cluster_record(
			manifest, manifest_size, 0u, record));
		EXPECT_EQ(record.m_triangle_count, 12u);
		EXPECT_EQ(record.m_vertex_count, 8u);
		EXPECT_EQ(record.m_level, 0u);
		EXPECT_GT(record.m_cone_cutoff, 1.0f); // the never-cull sentinel
		EXPECT_DOUBLE_EQ(record.m_aabb_min[0], -1.0);
		EXPECT_DOUBLE_EQ(record.m_aabb_max[0], 1.0);
		EXPECT_EQ(record.m_material_max, 3u);

		// corrupt controls: bad magic, a truncated manifest, a reserved
		// word, an out-of-range record index
		manifest[0] = 'X';
		EXPECT_FALSE(zircon_meshlet_read_manifest_header(
			manifest, manifest_size, header));
		manifest[0] = 'Z';

		EXPECT_FALSE(zircon_meshlet_read_manifest_header(
			manifest, manifest_size - 1u, header));

		zircon_csg_bake_store_u32(manifest + 28, 1u); // reserved word
		EXPECT_FALSE(zircon_meshlet_read_manifest_header(
			manifest, manifest_size, header));
		zircon_csg_bake_store_u32(manifest + 28, 0u);

		EXPECT_FALSE(zircon_meshlet_read_cluster_record(
			manifest, manifest_size, 1u, record));
	}

	// the entry-name builder: the pack layout's path + the scene-name
	// validation (the path-walk rejection)
	TEST(Zircon_NriMeshlet, EntryNameBuildsAndValidates)
	{
		char name[ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH]{};

		ASSERT_TRUE(zircon_meshlet_read_entry_name(name, sizeof(name),
			"boot", 0u, 0u));
		EXPECT_STREQ(name,
			"meshlets/boot/lod_0/cluster_0.bin");

		EXPECT_FALSE(zircon_meshlet_read_entry_name(name, sizeof(name),
			"../boot", 0u, 0u));
		EXPECT_FALSE(zircon_meshlet_read_entry_name(name, sizeof(name),
			"boot/scene", 0u, 0u));

		// an over-capacity caller buffer (named away from `small` — an
		// rpcndr.h macro on Windows, the K25 lesson)
		char small_buffer[8]{};
		EXPECT_FALSE(zircon_meshlet_read_entry_name(small_buffer,
			sizeof(small_buffer), "boot", 0u, 0u));
	}

	// the SHIPPED boot fixture: the exact bytes the NRI pass consumes at
	// boot, loaded through the REAL dispatcher (native files today; the
	// same calls answer from a mounted pack when one carries the entries
	// — the B2b machinery, not this test's subject)
	TEST(Zircon_NriMeshlet, BootFixtureLoadsThroughDispatcher)
	{
		meshlet_nri_test_env* p_env = new meshlet_nri_test_env{};
		p_env->initialize();

		kotek::static_path_t root_path;
		p_env->filesystem.Make_Path(
			root_path, kotek::core::eFolderIndex::kFolderIndex_Root);

		kotek::static_path_t manifest_path = root_path;
		manifest_path /= "meshlets/boot/manifest.bin";

		kotek::size_t manifest_size = 0;

		ASSERT_TRUE(p_env->filesystem.Get_FileSize(
			manifest_path, manifest_size));

		kotek::uint8_t* p_manifest =
			new kotek::uint8_t[manifest_size + 1];
		kotek::size_t manifest_read_size = manifest_size + 1;

		ASSERT_TRUE(p_env->filesystem.Read_File(
			manifest_path, p_manifest, manifest_read_size));
		EXPECT_EQ(manifest_read_size, manifest_size);

		zircon_meshlet_read_manifest_header_t header{};

		ASSERT_TRUE(zircon_meshlet_read_manifest_header(
			p_manifest, manifest_size, header));
		EXPECT_EQ(header.m_lod_level_count, 1u);
		EXPECT_EQ(header.m_cluster_count_total, 1u);
		EXPECT_EQ(header.m_mesh_triangle_count, 12u);

		zircon_meshlet_cluster_t record{};

		ASSERT_TRUE(zircon_meshlet_read_cluster_record(
			p_manifest, manifest_size, 0u, record));
		EXPECT_EQ(record.m_triangle_count, 12u);
		EXPECT_EQ(record.m_vertex_count, 8u);
		EXPECT_EQ(record.m_level, 0u);
		EXPECT_GT(record.m_cone_cutoff, 1.0f); // never-cull sentinel
		EXPECT_DOUBLE_EQ(record.m_aabb_min[0], -1.0);
		EXPECT_DOUBLE_EQ(record.m_aabb_max[2], 1.0);

		delete[] p_manifest;

		kotek::static_path_t cluster_path = root_path;
		cluster_path /= "meshlets/boot/lod_0/cluster_0.bin";

		kotek::size_t cluster_size = 0;

		ASSERT_TRUE(p_env->filesystem.Get_FileSize(
			cluster_path, cluster_size));

		kotek::uint8_t* p_cluster =
			new kotek::uint8_t[cluster_size + 1];
		kotek::size_t cluster_read_size = cluster_size + 1;

		ASSERT_TRUE(p_env->filesystem.Read_File(
			cluster_path, p_cluster, cluster_read_size));
		EXPECT_EQ(cluster_read_size, cluster_size);

		float vb[zircon_meshlet_read_vb_capacity_floats(
			zircon_meshlet_max_tris_for_level(0u))]{};
		kotek::uint32_t
			ib[zircon_meshlet_read_ib_capacity(
				zircon_meshlet_max_tris_for_level(0u))]{};

		zircon_meshlet_cluster_content_t content{};

		ASSERT_TRUE(zircon_meshlet_read_cluster(p_cluster, cluster_size,
			record.m_aabb_min, record.m_aabb_max, vb,
			zircon_meshlet_read_vb_capacity_floats(
				zircon_meshlet_max_tris_for_level(0u)),
			ib,
			zircon_meshlet_read_ib_capacity(
				zircon_meshlet_max_tris_for_level(0u)),
			content));

		delete[] p_cluster;

		// the unit cube: 12 triangles, 8 welded corners, 36 soup
		// vertices; every soup position is a cube corner (coords in
		// {-1, +1})
		EXPECT_EQ(content.m_triangle_count, 12u);
		EXPECT_EQ(content.m_welded_vertex_count, 8u);
		EXPECT_EQ(content.m_soup_vertex_count, 36u);
		EXPECT_EQ(content.m_index_count, 36u);

		for (kotek::uint32_t soup_vertex = 0; soup_vertex < 36u;
			 ++soup_vertex)
		{
			const float* p_vertex =
				vb + soup_vertex *
					ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_FLOATS;

			for (int axis = 0; axis < 3; ++axis)
			{
				const float value = p_vertex[axis];
				EXPECT_TRUE(value > -1.001f && value < 1.001f)
					<< "soup vertex " << soup_vertex << " axis " << axis
					<< " = " << value;
			}

			// a cube-corner normal: exactly one axis component is +-1
			int dominant_axis = -1;

			for (int axis = 0; axis < 3; ++axis)
			{
				if (std::fabs(std::fabs(p_vertex[3 + axis]) - 1.0f) <
					0.02f)
				{
					EXPECT_EQ(dominant_axis, -1);
					dominant_axis = axis;
				}
			}

			EXPECT_NE(dominant_axis, -1);
		}

		p_env->shutdown();
		delete p_env;
	}

	// the NRI passlib registry + the lifecycle seam: the meshlet pass
	// joins the present pass, both create/destroy through the library,
	// the initialize hook survives a null manager (the inert-without-
	// content contract — the boot's missing-content path)
	TEST(Zircon_NriMeshlet, PasslibRegistryAndLifecycle)
	{
		EXPECT_EQ(zircon_nri_passlib_get_count(), 2u);
		EXPECT_STREQ(zircon_nri_passlib_get_name(0u),
			no_streaming::kZircon_RenderGraphPassPresentNri_Name);
		EXPECT_STREQ(zircon_nri_passlib_get_name(1u),
			no_streaming::kZircon_RenderGraphPassMeshletClusterNri_Name);
		EXPECT_EQ(zircon_nri_passlib_get_name(2u), nullptr);

		// the built-in default set drives BOTH passes, present first
		// (the clear opens the frame; the meshlet draws on top)
		EXPECT_NE(std::strstr(kZircon_NriPasslib_DefaultGamePasses,
					  no_streaming::
						  kZircon_RenderGraphPassMeshletClusterNri_Name),
			nullptr);

		kotek::core::ktkIRenderFramePass* p_present =
			zircon_nri_passlib_create(
				no_streaming::kZircon_RenderGraphPassPresentNri_Name);
		kotek::core::ktkIRenderFramePass* p_meshlet =
			zircon_nri_passlib_create(
				no_streaming::kZircon_RenderGraphPassMeshletClusterNri_Name);

		ASSERT_NE(p_present, nullptr);
		ASSERT_NE(p_meshlet, nullptr);

		EXPECT_STREQ(p_present->Get_Name(),
			no_streaming::kZircon_RenderGraphPassPresentNri_Name);
		EXPECT_STREQ(p_meshlet->Get_Name(),
			no_streaming::kZircon_RenderGraphPassMeshletClusterNri_Name);

		// the lifecycle hook with NO manager: loud-inert, never a crash
		// (the boot's missing-content/backend path is this call with a
		// manager whose services are absent)
		zircon_nri_passlib_initialize(p_meshlet, nullptr);

		// a foreign pass object never receives the hook
		zircon_nri_passlib_initialize(nullptr, nullptr);

		EXPECT_EQ(zircon_nri_passlib_create("no_such_pass"), nullptr);

		zircon_nri_passlib_destroy(p_present);
		zircon_nri_passlib_destroy(p_meshlet);
	}
} // namespace

		#endif
	#endif
#endif
