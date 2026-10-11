#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cmath>
		#include <cstring>
		#include <filesystem>

		#include "../../core/zircon_config.h"
		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_terrain_static.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_terrain_chunks.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_graph_pass_model_static_gpu_driven.h"
		#include "../../world/zircon_world.h"

		#ifndef ZIRCON_DEF_UNIT_TEST_TERRAIN
			#define ZIRCON_DEF_UNIT_TEST_TERRAIN 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_TERRAIN == 1

// functional proofs for task Z24 B5a (the terrain foundation): the
// component's descriptor json roundtrip + ECS registration, the
// heightfield intake through the real dispatcher (a known 4x4 u8
// fixture -> the exact chunk grid, positions/normals/AABBs pinned per
// cell; a missing/oversized/mismatched heightmap -> loud-graceful
// absence), the no-cracks seam property (adjacent chunks share edge
// vertices bit-exactly), the B1 chunk-pool cull integration against the
// known camera, and the u16-LE leg. Headless — the GPU side is boot
// evidence. Tier: lightweight (rule 8a — the fixtures are 4x4/5x5
// heightfields, the pool is the heap-allocated fixture shape).

using zircon_gpu_driven_pass =
	no_streaming::zircon_render_graph_pass_model_static_gpu_driven_bgfx;

namespace
{
	// the headless filesystem environment (the texture-bcn fixture's
	// shape): a real filesystem behind a real framework config — the
	// boot dispatcher, no engine session needed
	struct zircon_terrain_test_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;

		void initialize(void)
		{
			this->filesystem.Initialize(&this->framework_config);
		}

		void shutdown(void) { this->filesystem.Shutdown(); }
	};

	// the headless ecs environment (the camera-sdk fixture's shape): a
	// real factory and world, no imgui, no window. Heap per the fixture
	// rule (the console alone is ~1 MB of stack)
	struct zircon_terrain_ecs_env
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

			this->world.initialize("zircon_b5a_test_world",
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

	kotek::static_path_t terrain_make_tests_path(
		kotek::core::ktkIFileSystem* p_fs, const char* p_name)
	{
		kotek::static_path_t out;
		p_fs->Make_Path(
			out, kotek::core::eFolderIndex::kFolderIndex_DataUser_Tests);
		out /= p_name;
		return out;
	}

	void terrain_remove_file_quiet(const kotek::static_path_t& path)
	{
		std::error_code ec;
		std::filesystem::remove(std::filesystem::path(path.c_str()), ec);
	}

	// the known 4x4 u8 fixture: a ramp along +x (sample = x * 85 ->
	// heights 0, 2/3, 4/3, 2 m at scale 2), constant along z — the
	// normals are then uniform (-2,3,0)/sqrt(13) everywhere and every
	// value is pinned by hand
	void terrain_write_fixture_4x4_u8(
		kotek::core::ktkIFileSystem* p_fs, const char* p_name)
	{
		kotek::uint8_t samples[16];

		for (kotek::uint32_t y = 0; y < 4; ++y)
		{
			for (kotek::uint32_t x = 0; x < 4; ++x)
			{
				samples[y * 4 + x] = static_cast<kotek::uint8_t>(x * 85);
			}
		}

		kotek::static_path_t path = terrain_make_tests_path(p_fs, p_name);

		ASSERT_TRUE(p_fs->Write_File(path, samples, sizeof(samples)));
	}

	// the test camera: eye (0,0,-10) -> origin, 60-degree fov, square
	// aspect, near 0.1 far 100 — the chunk-pool suite's camera, so the
	// culled/visible expectations reuse its proven planes
	void terrain_build_test_planes(float* p_out_planes_6x4)
	{
		float view[16];
		float projection[16];

		bx::mtxLookAt(view, bx::Vec3(0.0f, 0.0f, -10.0f),
			bx::Vec3(0.0f, 0.0f, 0.0f), bx::Vec3(0.0f, 1.0f, 0.0f));

		bx::mtxProj(projection, 60.0f, 1.0f, 0.1f, 100.0f,
			bgfx::getCaps() ? bgfx::getCaps()->homogeneousDepth : true);

		float view_projection[16];
		bx::mtxMul(view_projection, view, projection);

		zircon_render_chunk_pool::extract_frustum_planes(
			view_projection, p_out_planes_6x4);
	}

	// loads + builds the 4x4 ramp fixture into the pool; returns the
	// chunk count (0 = the build failed — the asserting test fails)
	kotek::uint32_t terrain_build_4x4_u8_into_pool(
		kotek::core::ktkIFileSystem* p_fs, zircon_render_chunk_pool& pool,
		const char* p_name, const float* p_model_matrix_or_null)
	{
		// heap like the pool fixture (128 KB of file scratch)
		kotek::uint8_t* p_file_scratch = new kotek::uint8_t[
			zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES];

		zircon_terrain_heightfield_view_t heightfield;

		const bool loaded = zircon_terrain_load_heightfield(p_fs,
			terrain_make_tests_path(p_fs, p_name),
			eZirconTerrainHeightmapFormat::kGray8, 4, 4, p_file_scratch,
			zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES, heightfield);

		kotek::uint32_t built = 0;

		if (loaded)
		{
			zircon_model_static_vertex_t
				vertex_scratch[zircon_DEF_TERRAIN_CHUNK_MAX_VERTICES];
			kotek::uint16_t
				index_scratch[zircon_DEF_TERRAIN_CHUNK_MAX_INDICES];

			built = zircon_terrain_build_chunks(pool, heightfield, 3.0f,
				3.0f, 2.0f, 0, p_model_matrix_or_null, vertex_scratch,
				index_scratch);
		}

		delete[] p_file_scratch;

		return built;
	}
} // namespace

// (a) the descriptor's json roundtrip: non-default values survive
// value_from/value_to field-for-field (the scene-file contract — the
// component serializes the descriptor, never the heightfield bytes)
TEST(Zircon_Game, TerrainStaticComponentJsonRoundTrip)
{
	zircon_component_terrain_static terrain;

	terrain.set_enabled(false);
	terrain.set_heightmap_format(eZirconTerrainHeightmapFormat::kGray16LE);
	terrain.set_resolution(129, 65);
	terrain.set_world_size(200.0f, 100.0f);
	terrain.set_height_scale(25.5f);
	terrain.set_heightmap_path("terrain/heightmaps/island.r16");

	kotek::ktk::json::value serialized =
		kotek::ktk::json::value_from(terrain);

	zircon_component_terrain_static restored =
		kotek::ktk::json::value_to<zircon_component_terrain_static>(
			serialized);

	EXPECT_FALSE(restored.is_enabled());
	EXPECT_TRUE(restored.get_heightmap_format() ==
		eZirconTerrainHeightmapFormat::kGray16LE);
	EXPECT_EQ(restored.get_resolution_x(), 129);
	EXPECT_EQ(restored.get_resolution_y(), 65);
	EXPECT_FLOAT_EQ(restored.get_world_size_x(), 200.0f);
	EXPECT_FLOAT_EQ(restored.get_world_size_z(), 100.0f);
	EXPECT_FLOAT_EQ(restored.get_height_scale(), 25.5f);
	EXPECT_STREQ(
		restored.get_heightmap_path(), "terrain/heightmaps/island.r16");

	// the defaults of a fresh component: unconfigured and inert
	zircon_component_terrain_static fresh;

	EXPECT_TRUE(fresh.is_enabled());
	EXPECT_EQ(fresh.get_resolution_x(), 0);
	EXPECT_EQ(fresh.get_resolution_y(), 0);
	EXPECT_TRUE(fresh.get_heightmap_format() ==
		eZirconTerrainHeightmapFormat::kGray8);
	EXPECT_EQ(fresh.get_heightmap_path()[0], '\0');
}

// (b) the ECS registration: the codegen'd enum <-> name translation and
// the factory create/has path on a real entity, including the
// scene-load shape (create through the serialized descriptor)
TEST(Zircon_Game, TerrainStaticComponentRegisteredInFactory)
{
	zircon_terrain_ecs_env& env = *new zircon_terrain_ecs_env();
	env.initialize();

	zircon_ecs_context_t* p_context = env.world.get_ecs_context();

	// the inspector-combo contract: the name lookup returns the
	// registered component name
	EXPECT_STREQ(
		env.factory.get_component_name_by_enum(
			eZirconComponentType::kzircon_component_terrain_static),
		"zircon_component_terrain_static");

	EXPECT_TRUE(env.factory.get_component_enum_by_name(
					"zircon_component_terrain_static") ==
		eZirconComponentType::kzircon_component_terrain_static);

	kotek::entity_t entity = env.factory.create_entity(p_context);

	// the plain create/has path
	ASSERT_TRUE(env.factory.create_component(p_context, entity,
		eZirconComponentType::kzircon_component_terrain_static));

	EXPECT_TRUE(env.factory.has_component(p_context, entity,
		eZirconComponentType::kzircon_component_terrain_static));

	zircon_component_terrain_static* p_terrain =
		static_cast<zircon_component_terrain_static*>(
			env.factory.get_component_by_enum(p_context, entity,
				eZirconComponentType::kzircon_component_terrain_static));

	ASSERT_TRUE(p_terrain != nullptr);
	EXPECT_EQ(p_terrain->get_component_type(),
		static_cast<kotek::uint8_t>(
			eZirconComponentType::kzircon_component_terrain_static));

	// the scene-load shape: a configured descriptor round-trips through
	// the factory's json create
	{
		zircon_component_terrain_static configured;
		configured.set_resolution(17, 33);
		configured.set_world_size(64.0f, 128.0f);
		configured.set_height_scale(10.0f);
		configured.set_heightmap_path("terrain/heightmaps/hill.r8");

		kotek::ktk::json::value serialized =
			kotek::ktk::json::value_from(configured);

		kotek::entity_t second = env.factory.create_entity(p_context);

		ASSERT_TRUE(env.factory.create_component(p_context, second,
			eZirconComponentType::kzircon_component_terrain_static,
			serialized));

		const zircon_component_terrain_static* p_loaded =
			static_cast<const zircon_component_terrain_static*>(
				env.factory.get_component_by_enum(p_context, second,
					eZirconComponentType::
						kzircon_component_terrain_static));

		ASSERT_TRUE(p_loaded != nullptr);
		EXPECT_EQ(p_loaded->get_resolution_x(), 17);
		EXPECT_EQ(p_loaded->get_resolution_y(), 33);
		EXPECT_FLOAT_EQ(p_loaded->get_world_size_x(), 64.0f);
		EXPECT_FLOAT_EQ(p_loaded->get_world_size_z(), 128.0f);
		EXPECT_FLOAT_EQ(p_loaded->get_height_scale(), 10.0f);
		EXPECT_STREQ(
			p_loaded->get_heightmap_path(), "terrain/heightmaps/hill.r8");
	}

	env.factory.destroy_entity(p_context, entity);

	env.shutdown();
	delete &env;
}

// (c) the heightfield load + the exact chunk grid: the 4x4 ramp fixture
// (3x3 quads -> the lattice clamps the 8-cell define to 3 cells per
// side -> 9 chunks of 1 quad each), positions/normals/AABBs/winding
// pinned per cell
TEST(Zircon_Game, TerrainHeightfieldLoadAndChunkBuild)
{
	zircon_terrain_test_env& env = *new zircon_terrain_test_env();
	env.initialize();

	terrain_write_fixture_4x4_u8(&env.filesystem, "b5a_ramp_4x4.r8");

	// the cell-layout pins the fixture stands on
	EXPECT_EQ(zircon_terrain_cells_per_side(4), 3);
	EXPECT_EQ(zircon_terrain_chunk_count_total(4, 4), 3u * 3u);

	{
		kotek::uint16_t begin = 0xffff;
		kotek::uint16_t count = 0;

		zircon_terrain_cell_quad_range(3, 3, 0, begin, count);
		EXPECT_EQ(begin, 0);
		EXPECT_EQ(count, 1);
		zircon_terrain_cell_quad_range(3, 3, 2, begin, count);
		EXPECT_EQ(begin, 2);
		EXPECT_EQ(count, 1);

		// the remainder front-loads: 11 quads over 8 cells = 2,2,2,1,...
		zircon_terrain_cell_quad_range(11, 8, 0, begin, count);
		EXPECT_EQ(begin, 0);
		EXPECT_EQ(count, 2);
		zircon_terrain_cell_quad_range(11, 8, 3, begin, count);
		EXPECT_EQ(begin, 6);
		EXPECT_EQ(count, 1);
		zircon_terrain_cell_quad_range(11, 8, 7, begin, count);
		EXPECT_EQ(begin, 10);
		EXPECT_EQ(count, 1);
	}

	// the pool is ~2 MB of shadows/tables — heap per the fixture rule
	zircon_render_chunk_pool& pool = *new zircon_render_chunk_pool();

	const kotek::uint32_t built = terrain_build_4x4_u8_into_pool(
		&env.filesystem, pool, "b5a_ramp_4x4.r8", nullptr);

	ASSERT_EQ(built, 9u);
	EXPECT_EQ(pool.get_chunk_slot_count(), 9u);
	EXPECT_EQ(pool.get_live_chunk_count(), 9u);
	// 9 chunks x 4 vertices / 6 indices
	EXPECT_EQ(pool.get_vertex_high_water(), 36u);
	EXPECT_EQ(pool.get_index_high_water(), 54u);

	// chunk 0 = cell (0,0): texels (0..1, 0..1), the ramp corner.
	// Positions: x in {-1.5, -0.5}, z in {-1.5, -0.5}, heights 0 (x=0)
	// and 85/255*2 (x=1); the AABB wraps them tightly
	const zircon_chunk_bounds_t* p_bounds = pool.get_bounds();
	const zircon_chunk_ranges_t* p_ranges = pool.get_ranges();

	const float ramp_height = 85.0f / 255.0f * 2.0f;

	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_min[0], -1.5f);
	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_min[1], 0.0f);
	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_min[2], -1.5f);
	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_max[0], -0.5f);
	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_max[1], ramp_height);
	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_max[2], -0.5f);

	EXPECT_EQ(p_ranges[0].m_vertex_offset, 0u);
	EXPECT_EQ(p_ranges[0].m_vertex_count, 4u);
	EXPECT_EQ(p_ranges[0].m_index_offset, 0u);
	EXPECT_EQ(p_ranges[0].m_index_count, 6u);

	// the shadow's four vertices: (local x,z) -> the lattice texel
	const zircon_model_static_vertex_t* p_shadow =
		pool.get_vertex_shadow();

	// v(0,0): x -1.5, z -1.5, height 0 (texel 0,0)
	EXPECT_FLOAT_EQ(p_shadow[0].m_position[0], -1.5f);
	EXPECT_FLOAT_EQ(p_shadow[0].m_position[1], 0.0f);
	EXPECT_FLOAT_EQ(p_shadow[0].m_position[2], -1.5f);
	// v(1,0): the ramp rises along +x
	EXPECT_FLOAT_EQ(p_shadow[1].m_position[0], -0.5f);
	EXPECT_FLOAT_EQ(p_shadow[1].m_position[1], ramp_height);
	EXPECT_FLOAT_EQ(p_shadow[1].m_position[2], -1.5f);
	// v(0,1) / v(1,1): constant along z
	EXPECT_FLOAT_EQ(p_shadow[2].m_position[0], -1.5f);
	EXPECT_FLOAT_EQ(p_shadow[2].m_position[1], 0.0f);
	EXPECT_FLOAT_EQ(p_shadow[2].m_position[2], -0.5f);
	EXPECT_FLOAT_EQ(p_shadow[3].m_position[0], -0.5f);
	EXPECT_FLOAT_EQ(p_shadow[3].m_position[1], ramp_height);
	EXPECT_FLOAT_EQ(p_shadow[3].m_position[2], -0.5f);

	// the ramp normal is uniform: dH/dx = (2/3)/1 m, dH/dz = 0 ->
	// n = (-2/3, 1, 0)/sqrt(13/9) = (-2, 3, 0)/sqrt(13)
	const float normal_scale = 1.0f / std::sqrt(13.0f);

	for (kotek::uint32_t vertex_index = 0; vertex_index < 4;
		 ++vertex_index)
	{
		EXPECT_NEAR(p_shadow[vertex_index].m_normal[0],
			-2.0f * normal_scale, 1e-6f);
		EXPECT_NEAR(p_shadow[vertex_index].m_normal[1],
			3.0f * normal_scale, 1e-6f);
		EXPECT_NEAR(p_shadow[vertex_index].m_normal[2], 0.0f, 1e-6f);
		EXPECT_EQ(p_shadow[vertex_index].m_color_abgr, 0xffffffffu);
	}

	// the up-facing winding: (v00,v01,v10) + (v10,v01,v11)
	const kotek::uint16_t* p_indices = pool.get_index_shadow();

	EXPECT_EQ(p_indices[0], 0u);
	EXPECT_EQ(p_indices[1], 2u);
	EXPECT_EQ(p_indices[2], 1u);
	EXPECT_EQ(p_indices[3], 1u);
	EXPECT_EQ(p_indices[4], 2u);
	EXPECT_EQ(p_indices[5], 3u);

	// the last chunk (cell (2,2)) reaches the far lattice edge
	EXPECT_FLOAT_EQ(p_bounds[8].m_aabb_max[0], 1.5f);
	EXPECT_FLOAT_EQ(p_bounds[8].m_aabb_max[2], 1.5f);
	EXPECT_FLOAT_EQ(p_bounds[8].m_aabb_max[1], 2.0f); // texel x=3: 255/255*2

	terrain_remove_file_quiet(
		terrain_make_tests_path(&env.filesystem, "b5a_ramp_4x4.r8"));

	delete &pool;

	env.shutdown();
	delete &env;
}

// (d) the graceful rejections: missing file, size mismatch, over-cap /
// degenerate resolutions, unknown format, null/unbuilt build inputs —
// every one loud, none a fault
TEST(Zircon_Game, TerrainHeightfieldGracefulRejections)
{
	zircon_terrain_test_env& env = *new zircon_terrain_test_env();
	env.initialize();

	kotek::uint8_t* p_file_scratch = new kotek::uint8_t[
		zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES];
	zircon_terrain_heightfield_view_t heightfield;

	// a missing heightmap: false + the designed loud-absence (no fault)
	EXPECT_FALSE(zircon_terrain_load_heightfield(&env.filesystem,
		terrain_make_tests_path(
			&env.filesystem, "b5a_definitely_missing.r8"),
		eZirconTerrainHeightmapFormat::kGray8, 4, 4, p_file_scratch,
		zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES, heightfield));
	EXPECT_TRUE(heightfield.m_p_samples == nullptr);

	// a size mismatch IS the headerless format's integrity check
	{
		const kotek::uint8_t junk[15] = {};

		kotek::static_path_t wrong_size_path =
			terrain_make_tests_path(&env.filesystem, "b5a_wrong_size.r8");

		ASSERT_TRUE(
			env.filesystem.Write_File(wrong_size_path, junk, sizeof(junk)));

		EXPECT_FALSE(zircon_terrain_load_heightfield(&env.filesystem,
			wrong_size_path, eZirconTerrainHeightmapFormat::kGray8, 4, 4,
			p_file_scratch, zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES,
			heightfield));

		terrain_remove_file_quiet(wrong_size_path);
	}

	// the capacity guard: over the resolution cap, rejected before any
	// read (no file needed)
	EXPECT_FALSE(zircon_terrain_load_heightfield(&env.filesystem,
		terrain_make_tests_path(&env.filesystem, "b5a_never_read.r8"),
		eZirconTerrainHeightmapFormat::kGray8,
		static_cast<kotek::uint16_t>(zircon_DEF_TERRAIN_MAX_RESOLUTION + 1),
		4, p_file_scratch, zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES,
		heightfield));

	// degenerate resolutions / unknown format: rejected before any read
	EXPECT_FALSE(zircon_terrain_load_heightfield(&env.filesystem,
		terrain_make_tests_path(&env.filesystem, "b5a_never_read.r8"),
		eZirconTerrainHeightmapFormat::kGray8, 1, 4, p_file_scratch,
		zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES, heightfield));
	EXPECT_FALSE(zircon_terrain_load_heightfield(&env.filesystem,
		terrain_make_tests_path(&env.filesystem, "b5a_never_read.r8"),
		static_cast<eZirconTerrainHeightmapFormat>(7), 4, 4, p_file_scratch,
		zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES, heightfield));

	// the caller's too-small scratch is a caller error (the required
	// size is logged), still graceful
	EXPECT_FALSE(zircon_terrain_load_heightfield(&env.filesystem,
		terrain_make_tests_path(&env.filesystem, "b5a_never_read.r8"),
		eZirconTerrainHeightmapFormat::kGray8, 4, 4, p_file_scratch, 8,
		heightfield));

	// the build's own guards: an unloaded (null) view and a degenerate
	// world size build nothing
	{
		zircon_render_chunk_pool& pool = *new zircon_render_chunk_pool();

		zircon_model_static_vertex_t
			vertex_scratch[zircon_DEF_TERRAIN_CHUNK_MAX_VERTICES];
		kotek::uint16_t
			index_scratch[zircon_DEF_TERRAIN_CHUNK_MAX_INDICES];

		EXPECT_EQ(zircon_terrain_build_chunks(pool, heightfield, 3.0f,
					  3.0f, 2.0f, 0, nullptr, vertex_scratch,
					  index_scratch),
			0u);

		terrain_write_fixture_4x4_u8(&env.filesystem, "b5a_ramp_4x4.r8");

		ASSERT_TRUE(zircon_terrain_load_heightfield(&env.filesystem,
			terrain_make_tests_path(&env.filesystem, "b5a_ramp_4x4.r8"),
			eZirconTerrainHeightmapFormat::kGray8, 4, 4, p_file_scratch,
			zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES, heightfield));

		EXPECT_EQ(zircon_terrain_build_chunks(pool, heightfield, 0.0f,
					  3.0f, 2.0f, 0, nullptr, vertex_scratch,
					  index_scratch),
			0u);
		EXPECT_EQ(pool.get_chunk_slot_count(), 0u);

		terrain_remove_file_quiet(
			terrain_make_tests_path(&env.filesystem, "b5a_ramp_4x4.r8"));

		delete &pool;
	}

	delete[] p_file_scratch;

	env.shutdown();
	delete &env;
}

// (e) the no-cracks seam property THROUGH THE BUILT POOL: every lattice
// vertex shared by 2+ chunks (a 5x5 heightfield = 4x4 quads -> 4x4
// cells -> 16 chunks of one quad) is computed bit-identically in every
// owning chunk — positions AND normals (the global-stencil rule)
TEST(Zircon_Game, TerrainChunkSeamsSharedVertices)
{
	zircon_terrain_test_env& env = *new zircon_terrain_test_env();
	env.initialize();

	// a deterministic non-planar 5x5 field (a center hill — normals vary
	// per vertex, so a per-chunk stencil would diverge at the seams)
	{
		kotek::uint8_t samples[25];

		for (kotek::uint32_t y = 0; y < 5; ++y)
		{
			for (kotek::uint32_t x = 0; x < 5; ++x)
			{
				const kotek::uint32_t dx = x < 2 ? 2 - x : x - 2;
				const kotek::uint32_t dy = y < 2 ? 2 - y : y - 2;
				samples[y * 5 + x] = static_cast<kotek::uint8_t>(
					255 - (dx + dy) * 60);
			}
		}

		kotek::static_path_t path =
			terrain_make_tests_path(&env.filesystem, "b5a_hill_5x5.r8");

		ASSERT_TRUE(env.filesystem.Write_File(path, samples, sizeof(samples)));
	}

	zircon_render_chunk_pool& pool = *new zircon_render_chunk_pool();

	kotek::uint8_t* p_file_scratch = new kotek::uint8_t[
		zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES];
	zircon_terrain_heightfield_view_t heightfield;

	ASSERT_TRUE(zircon_terrain_load_heightfield(&env.filesystem,
		terrain_make_tests_path(&env.filesystem, "b5a_hill_5x5.r8"),
		eZirconTerrainHeightmapFormat::kGray8, 5, 5, p_file_scratch,
		zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES, heightfield));

	zircon_model_static_vertex_t
		vertex_scratch[zircon_DEF_TERRAIN_CHUNK_MAX_VERTICES];
	kotek::uint16_t
		index_scratch[zircon_DEF_TERRAIN_CHUNK_MAX_INDICES];

	ASSERT_EQ(zircon_terrain_build_chunks(pool, heightfield, 4.0f, 4.0f,
				  3.0f, 0, nullptr, vertex_scratch, index_scratch),
		16u);

	// walk every chunk's vertices back to their global texels: the first
	// occurrence is the reference, every later occurrence must be
	// BIT-identical (memcmp — the seam property is exact, not near)
	const zircon_chunk_ranges_t* p_ranges = pool.get_ranges();
	const zircon_model_static_vertex_t* p_shadow =
		pool.get_vertex_shadow();

	bool texel_seen[25] = {};
	float reference_positions[25][3] = {};
	float reference_normals[25][3] = {};

	for (kotek::uint32_t chunk = 0; chunk < 16; ++chunk)
	{
		const kotek::uint16_t cell_x =
			static_cast<kotek::uint16_t>(chunk % 4);
		const kotek::uint16_t cell_y =
			static_cast<kotek::uint16_t>(chunk / 4);

		for (kotek::uint32_t local = 0; local < 4; ++local)
		{
			const kotek::uint16_t texel_x = static_cast<kotek::uint16_t>(
				cell_x + static_cast<kotek::uint16_t>(local % 2));
			const kotek::uint16_t texel_y = static_cast<kotek::uint16_t>(
				cell_y + static_cast<kotek::uint16_t>(local / 2));
			const kotek::uint32_t texel = texel_y * 5 + texel_x;

			const zircon_model_static_vertex_t& vertex =
				p_shadow[p_ranges[chunk].m_vertex_offset + local];

			if (texel_seen[texel])
			{
				EXPECT_TRUE(std::memcmp(vertex.m_position,
								reference_positions[texel],
								sizeof(float) * 3) == 0)
					<< "position seam mismatch at texel " << texel;
				EXPECT_TRUE(std::memcmp(vertex.m_normal,
								reference_normals[texel],
								sizeof(float) * 3) == 0)
					<< "normal seam mismatch at texel " << texel;
			}
			else
			{
				texel_seen[texel] = true;
				std::memcpy(reference_positions[texel], vertex.m_position,
					sizeof(float) * 3);
				std::memcpy(reference_normals[texel], vertex.m_normal,
					sizeof(float) * 3);
			}
		}
	}

	// every texel of the 5x5 lattice was covered exactly once as a
	// reference (the chunks tile the lattice completely)
	for (kotek::uint32_t texel = 0; texel < 25; ++texel)
	{
		EXPECT_TRUE(texel_seen[texel]);
	}

	// every interior texel (0<x<4, 0<y<4) is shared by exactly 4 chunks,
	// the edge ones by 2 — the pool holds 16*4 = 64 vertices for the 25
	// unique texels (the documented duplication)
	EXPECT_EQ(pool.get_vertex_high_water(), 64u);

	terrain_remove_file_quiet(
		terrain_make_tests_path(&env.filesystem, "b5a_hill_5x5.r8"));

	delete[] p_file_scratch;

	delete &pool;

	env.shutdown();
	delete &env;
}

// (f) the B1 pool integration under the known camera: the terrain at
// the origin is fully visible, a translated terrain culls away, and the
// far camera culls everything — the terrain is chunk content like
// everything else
TEST(Zircon_Game, TerrainChunkPoolCullIntegration)
{
	zircon_terrain_test_env& env = *new zircon_terrain_test_env();
	env.initialize();

	terrain_write_fixture_4x4_u8(&env.filesystem, "b5a_ramp_4x4.r8");

	zircon_render_chunk_pool& pool = *new zircon_render_chunk_pool();

	ASSERT_EQ(terrain_build_4x4_u8_into_pool(
				  &env.filesystem, pool, "b5a_ramp_4x4.r8", nullptr),
		9u);

	float planes[24];
	terrain_build_test_planes(planes);

	kotek::uint32_t visible_ids[zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS];

	// the terrain spans [-1.5, 1.5] x [0, 2] x [-1.5, 1.5] around the
	// origin — the look-at point: every chunk visible
	EXPECT_EQ(pool.cull_chunks_against_frustum(planes, visible_ids,
				  zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS),
		9u);

	// a second terrain translated a kilometer down +x: the transform
	// bakes at registration and the whole terrain culls out
	{
		const float position[3] = {1000.0f, 0.0f, 0.0f};
		const float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		const float scale[3] = {1.0f, 1.0f, 1.0f};

		float model[16];

		zircon_gpu_driven_pass::compose_model_matrix(
			position, rotation, scale, model);

		ASSERT_EQ(terrain_build_4x4_u8_into_pool(&env.filesystem, pool,
					  "b5a_ramp_4x4.r8", model),
			9u);

		// the baked AABB moved with the transform (the second terrain's
		// chunk 9 = cell (0,0)): x [998.5, 999.5]
		const zircon_chunk_bounds_t* p_bounds = pool.get_bounds();

		EXPECT_FLOAT_EQ(p_bounds[9].m_aabb_min[0], 998.5f);
		EXPECT_FLOAT_EQ(p_bounds[9].m_aabb_max[0], 999.5f);

		kotek::uint32_t visible_after =
			pool.cull_chunks_against_frustum(planes, visible_ids,
				zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS);

		EXPECT_EQ(visible_after, 9u);

		for (kotek::uint32_t i = 0; i < visible_after; ++i)
		{
			EXPECT_LT(visible_ids[i], 9u);
		}
	}

	// the far camera (1000 m back, 100 m far plane) culls everything
	{
		float view[16];
		float projection[16];

		bx::mtxLookAt(view, bx::Vec3(0.0f, 0.0f, -1000.0f),
			bx::Vec3(0.0f, 0.0f, 0.0f), bx::Vec3(0.0f, 1.0f, 0.0f));
		bx::mtxProj(projection, 60.0f, 1.0f, 0.1f, 100.0f,
			bgfx::getCaps() ? bgfx::getCaps()->homogeneousDepth : true);

		float far_view_projection[16];
		bx::mtxMul(far_view_projection, view, projection);

		float far_planes[24];
		zircon_render_chunk_pool::extract_frustum_planes(
			far_view_projection, far_planes);

		EXPECT_EQ(pool.cull_chunks_against_frustum(far_planes, visible_ids,
					  zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS),
			0u);
	}

	terrain_remove_file_quiet(
		terrain_make_tests_path(&env.filesystem, "b5a_ramp_4x4.r8"));

	delete &pool;

	env.shutdown();
	delete &env;
}

// (g) the u16-LE leg: the explicit little-endian decode, the /65535
// mapping, and a built chunk grid identical in shape to the u8 leg
TEST(Zircon_Game, TerrainHeightmapU16LE)
{
	zircon_terrain_test_env& env = *new zircon_terrain_test_env();
	env.initialize();

	// 4x4 u16-LE: a ramp along +x of 0, 21845, 43690, 65535 (thirds of
	// the full range — heights 0, s/3, 2s/3, s)
	{
		kotek::uint8_t bytes[32];

		for (kotek::uint32_t y = 0; y < 4; ++y)
		{
			for (kotek::uint32_t x = 0; x < 4; ++x)
			{
				const kotek::uint16_t sample =
					static_cast<kotek::uint16_t>(x * 21845);
				bytes[(y * 4 + x) * 2] =
					static_cast<kotek::uint8_t>(sample & 0xff);
				bytes[(y * 4 + x) * 2 + 1] =
					static_cast<kotek::uint8_t>(sample >> 8);
			}
		}

		kotek::static_path_t path =
			terrain_make_tests_path(&env.filesystem, "b5a_ramp_4x4.r16");

		ASSERT_TRUE(env.filesystem.Write_File(path, bytes, sizeof(bytes)));
	}

	kotek::uint8_t* p_file_scratch = new kotek::uint8_t[
		zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES];
	zircon_terrain_heightfield_view_t heightfield;

	ASSERT_TRUE(zircon_terrain_load_heightfield(&env.filesystem,
		terrain_make_tests_path(&env.filesystem, "b5a_ramp_4x4.r16"),
		eZirconTerrainHeightmapFormat::kGray16LE, 4, 4, p_file_scratch,
		zircon_DEF_TERRAIN_HEIGHTFIELD_MAX_BYTES, heightfield));

	// the decode pins: LE assembly + the /65535 mapping
	EXPECT_FLOAT_EQ(zircon_terrain_sample_height_meters(
						heightfield, 0, 0, 10.0f),
		0.0f);
	EXPECT_FLOAT_EQ(zircon_terrain_sample_height_meters(
						heightfield, 3, 2, 10.0f),
		10.0f); // 65535/65535 * 10
	EXPECT_NEAR(zircon_terrain_sample_height_meters(
					heightfield, 1, 0, 10.0f),
		21845.0f / 65535.0f * 10.0f, 1e-4f);

	zircon_render_chunk_pool& pool = *new zircon_render_chunk_pool();

	zircon_model_static_vertex_t
		vertex_scratch[zircon_DEF_TERRAIN_CHUNK_MAX_VERTICES];
	kotek::uint16_t
		index_scratch[zircon_DEF_TERRAIN_CHUNK_MAX_INDICES];

	ASSERT_EQ(zircon_terrain_build_chunks(pool, heightfield, 3.0f, 3.0f,
				  10.0f, 0, nullptr, vertex_scratch, index_scratch),
		9u);

	// chunk 0's corner heights through the pool shadow: 0 and 21845/65535*10
	const zircon_model_static_vertex_t* p_shadow =
		pool.get_vertex_shadow();

	EXPECT_FLOAT_EQ(p_shadow[0].m_position[1], 0.0f);
	EXPECT_NEAR(p_shadow[1].m_position[1], 21845.0f / 65535.0f * 10.0f,
		1e-4f);

	// the ramp reaches exactly the height scale at the far edge
	const zircon_chunk_bounds_t* p_bounds = pool.get_bounds();
	EXPECT_FLOAT_EQ(p_bounds[8].m_aabb_max[1], 10.0f);

	terrain_remove_file_quiet(
		terrain_make_tests_path(&env.filesystem, "b5a_ramp_4x4.r16"));

	delete[] p_file_scratch;

	delete &pool;

	env.shutdown();
	delete &env;
}

		#endif
	#endif
#endif
