#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cmath>
		#include <cstring>

		#include "../../core/zircon_meshlet_cluster_read.h"
		#include "../../core/zircon_meshlet_cull.h"
		#include "../../core/zircon_config.h"
		#include "../../render/nri/passes/zircon_nri_passlib.h"

		#ifndef ZIRCON_DEF_UNIT_TEST_MESHLET_NRI
			#define ZIRCON_DEF_UNIT_TEST_MESHLET_NRI 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_MESHLET_NRI == 1

// functional proofs for task Z24 B3b (the nanite path's geometry seam +
// the first real NRI draw) + B3c (the GPU-side finish — the cluster
// cull + the LOD cut): the B3a-format reader (the manifest + the
// cluster bin parse, the soup vertex-buffer layout), the cull/LOD logic
// as pure statics (the frustum + the AABB test, the LOD-cut predicate,
// the indirect command + the GPU record byte layouts, the CPU mirror's
// exact cull+compact), the shipped boot fixture (loads through the REAL
// filesystem dispatcher — the exact data flow the NRI pass runs at
// boot) and the NRI passlib registry/lifecycle (the meshlet pass
// registered, the initialize seam, the inert-without-content contract).
// Tier: lightweight (rule 8a — small synthetic soups + the 152-byte
// shipped cluster; no GPU needed).

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

	// ---- task Z24 B3c: the cull/LOD logic as pure C++ statics (the
	// kernel's mirror — the same classification the GPU runs, pinned
	// headlessly; tier: lightweight, rule 8a)

	// the frustum plane extraction: the identity view-projection yields
	// the exact six half-spaces (x>=-1, x<=1, y>=-1, y<=1, z>=0, z<=1
	// in world space)
	TEST(Zircon_NriMeshlet, CullPlaneExtractionPins)
	{
		const float identity_vp[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

		float planes[24]{};
		zircon_meshlet_cull_extract_frustum_planes(identity_vp, planes);

		// left = (1,0,0,1), right = (-1,0,0,1), bottom = (0,1,0,1),
		// top = (0,-1,0,1), near = (0,0,1,0), far = (0,0,-1,1) —
		// every plane is already unit length
		const float expected[24] = {1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f};

		for (int index = 0; index < 24; ++index)
		{
			EXPECT_FLOAT_EQ(planes[index], expected[index])
				<< "plane component " << index;
		}
	}

	// the AABB-vs-frustum test on the identity-clip planes: inside,
	// outside per plane, straddling, a degenerate point box
	TEST(Zircon_NriMeshlet, CullAabbTestPins)
	{
		float planes[24]{};
		zircon_meshlet_cull_extract_frustum_planes(
			nullptr, planes); // the null guard: no write, no crash

		const float identity_vp[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			1.0f};
		zircon_meshlet_cull_extract_frustum_planes(identity_vp, planes);

		// fully inside
		{
			const float mn[3] = {-0.5f, -0.5f, 0.25f};
			const float mx[3] = {0.5f, 0.5f, 0.75f};
			EXPECT_TRUE(zircon_meshlet_cull_test_aabb(planes, mn, mx));
		}

		// outside the left plane (x >= 2)
		{
			const float mn[3] = {2.0f, -0.5f, 0.25f};
			const float mx[3] = {3.0f, 0.5f, 0.75f};
			EXPECT_FALSE(zircon_meshlet_cull_test_aabb(planes, mn, mx));
		}

		// straddling the left plane but the p-vertex inside -> visible
		{
			const float mn[3] = {-2.0f, -0.5f, 0.25f};
			const float mx[3] = {0.5f, 0.5f, 0.75f};
			EXPECT_TRUE(zircon_meshlet_cull_test_aabb(planes, mn, mx));
		}

		// behind the near plane (z < 0)
		{
			const float mn[3] = {-0.5f, -0.5f, -2.0f};
			const float mx[3] = {0.5f, 0.5f, -0.5f};
			EXPECT_FALSE(zircon_meshlet_cull_test_aabb(planes, mn, mx));
		}

		// beyond the far plane (z > 1)
		{
			const float mn[3] = {-0.5f, -0.5f, 2.0f};
			const float mx[3] = {0.5f, 0.5f, 3.0f};
			EXPECT_FALSE(zircon_meshlet_cull_test_aabb(planes, mn, mx));
		}

		// a degenerate point box on the near plane boundary
		{
			const float mn[3] = {0.0f, 0.0f, 0.0f};
			const float mx[3] = {0.0f, 0.0f, 0.0f};
			EXPECT_TRUE(zircon_meshlet_cull_test_aabb(planes, mn, mx));
		}
	}

	// the LOD cut: the projected-error math (the distance floor, the
	// scale), the parent halves, the exact cut predicate
	TEST(Zircon_NriMeshlet, LodCutSelectionPins)
	{
		const float scale = 540.0f; // ~1080p, 60 deg fov
		const float threshold = ZIRCON_DEF_NRI_MESHLET_LOD_ERROR_THRESHOLD;

		// the projection: 1 world unit at 10 m on a 540px scale
		EXPECT_FLOAT_EQ(zircon_meshlet_cull_projected_error(1.0f, 10.0f,
							  scale),
			54.0f);

		// zero error projects to zero — the B3a placeholder's shape
		EXPECT_FLOAT_EQ(
			zircon_meshlet_cull_projected_error(0.0f, 10.0f, scale),
			0.0f);

		// the distance floor: a zero-distance cluster saturates, never
		// divides by zero
		EXPECT_FLOAT_EQ(zircon_meshlet_cull_projected_error(1.0f, 0.0f,
							  scale),
			1.0f * scale / ZIRCON_DEF_MESHLET_CULL_MIN_DISTANCE);

		// the FLT_MAX parent sentinel is never acceptable (the natural
		// +inf arithmetic — a root is decided by its own error alone)
		EXPECT_FALSE(zircon_meshlet_cull_parent_acceptable(
			ZIRCON_DEF_MESHLET_CULL_NO_PARENT_ERROR, 10.0f, scale,
			threshold));

		// the exact predicate: own pass + parent fail = on the cut
		EXPECT_TRUE(zircon_meshlet_cull_is_on_cut(0.0f,
			ZIRCON_DEF_MESHLET_CULL_NO_PARENT_ERROR, 10.0f, scale,
			threshold));

		// own pass + parent pass = NOT on the cut (the parent represents
		// it) — the B3a placeholder's degenerate shape: error 0
		// everywhere means only the coarsest level draws
		EXPECT_FALSE(zircon_meshlet_cull_is_on_cut(0.0f, 0.0f, 10.0f,
			scale, threshold));

		// own fail (a real simplifier's error past the threshold at
		// this distance) = NOT on the cut — the children must draw
		EXPECT_FALSE(zircon_meshlet_cull_is_on_cut(1.0f,
			ZIRCON_DEF_MESHLET_CULL_NO_PARENT_ERROR, 10.0f, scale,
			threshold));

		// ...but the same error closer passes (the distance scaling)
		EXPECT_TRUE(zircon_meshlet_cull_is_on_cut(1.0f,
			ZIRCON_DEF_MESHLET_CULL_NO_PARENT_ERROR, 1000.0f, scale,
			threshold));
	}

	// the indirect command's byte layout: the 20-byte NRI
	// DrawIndexedDesc / D3D12_DRAW_INDEXED_ARGUMENTS field order, pinned
	// against a written byte stream
	TEST(Zircon_NriMeshlet, IndirectCommandLayoutPins)
	{
		zircon_meshlet_cull_layout::indirect_command_t command{};
		command.m_index_count = 372u;
		command.m_instance_count = 1u;
		command.m_start_index = 108u;
		command.m_base_vertex = -7; // the int32 lane, negative legal
		command.m_start_instance = 42u;

		kotek::uint8_t bytes[20]{};
		std::memcpy(bytes, &command, sizeof(command));

		// the five u32 lanes, little-endian
		const kotek::uint32_t expected[5] = {372u, 1u, 108u,
			static_cast<kotek::uint32_t>(-7), 42u};

		for (int lane = 0; lane < 5; ++lane)
		{
			kotek::uint32_t value = 0;
			std::memcpy(&value, bytes + lane * 4, 4);
			EXPECT_EQ(value, expected[lane]) << "command lane " << lane;
		}
	}

	// the GPU cluster record's byte layout: the 64-byte table, the w
	// lanes carrying the cull operands
	TEST(Zircon_NriMeshlet, GpuRecordLayoutPins)
	{
		zircon_meshlet_cull_layout::gpu_record_t record{};
		record.m_aabb_min[0] = -1.0f;
		record.m_aabb_min[1] = -2.0f;
		record.m_aabb_min[2] = -3.0f;
		record.m_aabb_min[3] = 2.0f; // the never-cull cone sentinel
		record.m_aabb_max[0] = 1.0f;
		record.m_aabb_max[1] = 2.0f;
		record.m_aabb_max[2] = 3.0f;
		record.m_aabb_max[3] = 0.0f; // the error slot
		record.m_cone_axis[0] = 0.0f;
		record.m_cone_axis[1] = 0.0f;
		record.m_cone_axis[2] = 1.0f;
		record.m_cone_axis[3] =
			ZIRCON_DEF_MESHLET_CULL_NO_PARENT_ERROR; // no parent
		record.m_index_count = 36u;
		record.m_base_vertex = 72u;
		record.m_start_index = 72u;
		record.m_reserved = 0u;

		kotek::uint8_t bytes[64]{};
		std::memcpy(bytes, &record, sizeof(record));

		// the float lanes
		float value = 0.0f;
		std::memcpy(&value, bytes + 0, 4);
		EXPECT_FLOAT_EQ(value, -1.0f); // aabb_min.x
		std::memcpy(&value, bytes + 12, 4);
		EXPECT_FLOAT_EQ(value, 2.0f); // cone_cutoff in the w lane
		std::memcpy(&value, bytes + 28, 4);
		EXPECT_FLOAT_EQ(value, 0.0f); // error_metric in the w lane
		std::memcpy(&value, bytes + 44, 4);
		EXPECT_FLOAT_EQ(value,
			ZIRCON_DEF_MESHLET_CULL_NO_PARENT_ERROR); // parent error

		// the draw offsets
		kotek::uint32_t word = 0;
		std::memcpy(&word, bytes + 48, 4);
		EXPECT_EQ(word, 36u);
		std::memcpy(&word, bytes + 52, 4);
		EXPECT_EQ(word, 72u);
		std::memcpy(&word, bytes + 56, 4);
		EXPECT_EQ(word, 72u);
		std::memcpy(&word, bytes + 60, 4);
		EXPECT_EQ(word, 0u);
	}

	// the CPU mirror's full cull+compact: a six-cluster fixture with a
	// known camera, every predicate outcome represented, the exact
	// visible set + the compacted commands byte-pinned
	TEST(Zircon_NriMeshlet, CpuMirrorCompactPins)
	{
		using record_t = zircon_meshlet_cull_layout::gpu_record_t;

		// the identity-clip view: inside = -1<=x,y<=1, 0<=z<=1; the
		// camera sits at (0,0,-5) looking +z
		zircon_meshlet_cull_view_t view{};
		const float identity_vp[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			1.0f};
		zircon_meshlet_cull_extract_frustum_planes(identity_vp,
			view.m_planes);
		view.m_camera_position[0] = 0.0f;
		view.m_camera_position[1] = 0.0f;
		view.m_camera_position[2] = -5.0f;
		view.m_proj_scale = 540.0f;
		view.m_error_threshold =
			ZIRCON_DEF_NRI_MESHLET_LOD_ERROR_THRESHOLD;

		record_t records[6]{};

		// cluster 0: on the cut (a root), inside, never-cull cone ->
		// VISIBLE
		records[0].m_aabb_min[0] = -0.5f;
		records[0].m_aabb_min[1] = -0.5f;
		records[0].m_aabb_min[2] = 0.2f;
		records[0].m_aabb_min[3] = 2.0f; // the never-cull sentinel
		records[0].m_aabb_max[0] = 0.5f;
		records[0].m_aabb_max[1] = 0.5f;
		records[0].m_aabb_max[2] = 0.8f;
		records[0].m_aabb_max[3] = 0.0f; // own error 0
		records[0].m_cone_axis[3] =
			ZIRCON_DEF_MESHLET_CULL_NO_PARENT_ERROR;
		records[0].m_index_count = 36u;
		records[0].m_base_vertex = 0u;
		records[0].m_start_index = 0u;

		// cluster 1: on the cut but OUTSIDE the left plane -> culled
		records[1] = records[0];
		records[1].m_aabb_min[0] = 2.0f;
		records[1].m_aabb_max[0] = 3.0f;

		// cluster 2: on the cut, inside, but the cone backfaces the
		// camera (the flat quad's normals face +z, the cutoff admits
		// nothing) -> culled
		records[2] = records[0];
		records[2].m_aabb_min[3] = 0.0f;      // cutoff sin(0) = 0
		records[2].m_cone_axis[2] = 1.0f;     // axis +z
		records[2].m_cone_axis[3] =
			ZIRCON_DEF_MESHLET_CULL_NO_PARENT_ERROR;

		// cluster 3: NOT on the cut — the parent's error passes (the
		// parent draws it instead) -> culled
		records[3] = records[0];
		records[3].m_cone_axis[3] = 0.0f; // the parent's error

		// cluster 4: on the cut, inside, never-cull, its own slice
		// offsets -> VISIBLE
		records[4] = records[0];
		records[4].m_aabb_min[2] = 0.1f;
		records[4].m_aabb_max[2] = 0.4f;
		records[4].m_index_count = 9u;
		records[4].m_base_vertex = 36u;
		records[4].m_start_index = 36u;

		// cluster 5: on the cut but behind the near plane -> culled
		records[5] = records[0];
		records[5].m_aabb_min[2] = -3.0f;
		records[5].m_aabb_max[2] = -2.0f;

		zircon_meshlet_cull_layout::indirect_command_t commands[6]{};

		const kotek::uint32_t visible_count =
			zircon_meshlet_cull_compact(records, 6u, view, commands,
				6u);

		// exactly clusters 0 and 4 survive, in table order
		ASSERT_EQ(visible_count, 2u);

		EXPECT_EQ(commands[0].m_index_count, 36u);
		EXPECT_EQ(commands[0].m_instance_count, 1u);
		EXPECT_EQ(commands[0].m_start_index, 0u);
		EXPECT_EQ(commands[0].m_base_vertex, 0);
		EXPECT_EQ(commands[0].m_start_instance, 0u); // the cluster index

		EXPECT_EQ(commands[1].m_index_count, 9u);
		EXPECT_EQ(commands[1].m_instance_count, 1u);
		EXPECT_EQ(commands[1].m_start_index, 36u);
		EXPECT_EQ(commands[1].m_base_vertex, 36);
		EXPECT_EQ(commands[1].m_start_instance, 4u);

		// the single-cluster classify: the distance rides out
		float distance = 0.0f;
		EXPECT_TRUE(zircon_meshlet_cull_classify_cluster(
			records[0], view, distance));
		EXPECT_NEAR(distance, 5.5f, 0.001f); // |z = 0.5 - (-5)|
	}

	// the classic/nanite toggle's persisted key (task Z24 B3c): the
	// default is "classic", a written "nanite" survives the real
	// game_config.json roundtrip, an absent key keeps the default — the
	// Z22 byte backup/restore pattern (the test never drifts the user's
	// settings)
	TEST(Zircon_NriMeshlet, RenderGeometryPathConfigRoundtrips)
	{
		meshlet_nri_test_env* p_env = new meshlet_nri_test_env{};
		p_env->initialize();

		ktk_filesystem_path path_to_file;
		p_env->filesystem.Make_Path(
			path_to_file, kotek::core::eFolderIndex::kFolderIndex_DataUser);
		path_to_file /= kZirconConfig_FileName;

		if (p_env->filesystem.Is_Exists(path_to_file) == false)
		{
			p_env->shutdown();
			delete p_env;
			GTEST_SKIP() << "game_config.json is absent — the roundtrip "
							"needs the real file to preserve";
		}

		kotek::array_t<unsigned char, 2048> backup{};
		kotek::ktk::size_t backup_size = backup.size();
		unsigned char* p_backup_data = backup.data();

		ASSERT_TRUE(p_env->filesystem.Read_File(
			path_to_file, p_backup_data, backup_size));
		ASSERT_LT(backup_size, backup.size());

		// the ctor default
		{
			zircon_config config_default;
			EXPECT_STREQ(config_default.get_render_geometry_path(),
				kZirconConfig_RenderGeometryPathClassic);
		}

		// a written "nanite" persists
		{
			zircon_config config_write;
			config_write.set_render_geometry_path(
				kZirconConfig_RenderGeometryPathNanite);
			config_write.serialize(&p_env->filesystem);

			zircon_config config_read;
			config_read.deserialize(&p_env->filesystem);

			EXPECT_STREQ(config_read.get_render_geometry_path(),
				kZirconConfig_RenderGeometryPathNanite);
		}

		// "classic" written back -> the read value is "classic" even
		// though the in-memory value below differs (proves the key is
		// actually read)
		{
			zircon_config config_write;
			config_write.set_render_geometry_path(
				kZirconConfig_RenderGeometryPathClassic);
			config_write.serialize(&p_env->filesystem);

			zircon_config config_read;
			config_read.set_render_geometry_path(
				kZirconConfig_RenderGeometryPathNanite);
			config_read.deserialize(&p_env->filesystem);

			EXPECT_STREQ(config_read.get_render_geometry_path(),
				kZirconConfig_RenderGeometryPathClassic);
		}

		// restore the user's bytes
		EXPECT_TRUE(p_env->filesystem.Write_File(path_to_file,
			reinterpret_cast<const char*>(backup.data()), backup_size));

		p_env->shutdown();
		delete p_env;
	}
} // namespace

		#endif
	#endif
#endif
