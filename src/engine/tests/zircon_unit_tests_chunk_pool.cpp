#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cmath>
		#include <cstring>

		#include "../../render/bgfx/passes/no_streaming/zircon_render_chunk_pool.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_graph_pass_model_static_gpu_driven.h"

		#ifndef ZIRCON_DEF_UNIT_TEST_RENDER_PASSES
			#define ZIRCON_DEF_UNIT_TEST_RENDER_PASSES 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_RENDER_PASSES == 1

// functional proofs for task Z24 B1 (the GPU-driven classic baseline):
// (a) the pool range allocator (fits, free-list reuse, coalescing,
//     defrag, overflow) as pure statics/members;
// (b) the frustum classification mirror (the C++ twin of the compute
//     shader's zircon_is_aabb_visible — keep the two in sync);
// (c) chunk registration + the synthetic fixture under a known camera
//     (the headless half of the A/B proof — the GPU half is the boot's
//     readback evidence line, see the pass header).

using zircon_gpu_driven_pass =
	no_streaming::zircon_render_graph_pass_model_static_gpu_driven_bgfx;
using zircon_pass_model_static =
	no_streaming::zircon_render_graph_pass_model_static_bgfx;

namespace
{
	// the test camera: eye (0,0,-10) -> origin, 60-degree fov, square
	// aspect, near 0.1 far 100 — composed with bx exactly the way the
	// pass composes its default orbit, so the tested planes are the
	// drawn frame's planes
	void build_test_view_projection(float* p_out_view_projection_16)
	{
		float view[16];
		float projection[16];

		bx::mtxLookAt(view, bx::Vec3(0.0f, 0.0f, -10.0f),
			bx::Vec3(0.0f, 0.0f, 0.0f), bx::Vec3(0.0f, 1.0f, 0.0f));

		bx::mtxProj(projection, 60.0f, 1.0f, 0.1f, 100.0f,
			bgfx::getCaps() ? bgfx::getCaps()->homogeneousDepth : true);

		bx::mtxMul(p_out_view_projection_16, view, projection);
	}

	// extracts the test camera's planes into caller storage (6x4)
	void build_test_planes(float* p_out_planes_6x4)
	{
		float view_projection[16];
		build_test_view_projection(view_projection);

		zircon_render_chunk_pool::extract_frustum_planes(
			view_projection, p_out_planes_6x4);
	}

	bool is_box_visible(const float* p_planes_6x4, float min_x, float min_y,
		float min_z, float max_x, float max_y, float max_z)
	{
		const float aabb_min[3] = {min_x, min_y, min_z};
		const float aabb_max[3] = {max_x, max_y, max_z};

		return zircon_render_chunk_pool::test_aabb_against_frustum(
			p_planes_6x4, aabb_min, aabb_max);
	}
} // namespace

// (a) the allocator: first-fit fits, free-list reuse with sorted insert
// and both-neighbour coalescing, and the exhausted-pool failure
TEST(Zircon_Game, ChunkPoolAllocatorOps)
{
	zircon_pool_range_allocator allocator;

	allocator.initialize(64);

	EXPECT_EQ(allocator.get_free_total(), 64u);
	EXPECT_EQ(allocator.get_free_range_count(), 1u);

	kotek::uint32_t offset = 0xffffffffu;

	// first-fit donates from the front of the first fitting range:
	// allocated [0,10) [10,30) [30,45), free [45,64)
	ASSERT_TRUE(allocator.allocate(10, offset));
	EXPECT_EQ(offset, 0u);
	ASSERT_TRUE(allocator.allocate(20, offset));
	EXPECT_EQ(offset, 10u);
	ASSERT_TRUE(allocator.allocate(15, offset));
	EXPECT_EQ(offset, 30u);

	EXPECT_EQ(allocator.get_free_total(), 19u);

	// too big for the remaining single range — clean failure, nothing
	// mutated
	EXPECT_FALSE(allocator.allocate(40, offset));
	EXPECT_EQ(allocator.get_free_total(), 19u);
	EXPECT_EQ(allocator.get_free_range_count(), 1u);

	// freeing the middle range splits the free list into two
	ASSERT_TRUE(allocator.free(10, 20));
	EXPECT_EQ(allocator.get_free_range_count(), 2u);

	// first-fit takes the [10,30) hole (not the [45,64) tail) and splits
	// it — allocated [0,10) [10,25) [30,45), free [25,30) [45,64)
	ASSERT_TRUE(allocator.allocate(15, offset));
	EXPECT_EQ(offset, 10u);

	// freeing [0,10) inserts sorted (no adjacency) — three free ranges
	ASSERT_TRUE(allocator.free(0, 10));
	EXPECT_EQ(allocator.get_free_range_count(), 3u);

	// freeing [10,25) coalesces with BOTH neighbours -> [0,30)
	ASSERT_TRUE(allocator.free(10, 15));
	EXPECT_EQ(allocator.get_free_range_count(), 2u);

	// freeing [30,45) coalesces everything back into the single full
	// range
	ASSERT_TRUE(allocator.free(30, 15));
	EXPECT_EQ(allocator.get_free_range_count(), 1u);
	EXPECT_EQ(allocator.get_free_total(), 64u);

	// a full-capacity allocation succeeds after the coalesce
	ASSERT_TRUE(allocator.allocate(64, offset));
	EXPECT_EQ(offset, 0u);
	EXPECT_EQ(allocator.get_free_total(), 0u);

	// (a range escaping the pool is a programmer error guarded by
	// KOTEK_ASSERT in free()/allocate() — not a runtime path a gtest can
	// exercise)
}

// (a2) defragmentation: the relocation plan moves the live segments down
// and the post-defrag layout accepts the fit that failed before
TEST(Zircon_Game, ChunkPoolAllocatorDefrag)
{
	zircon_pool_range_allocator allocator;

	allocator.initialize(32);

	kotek::uint32_t offset_a = 0;
	kotek::uint32_t offset_b = 0;
	kotek::uint32_t offset_c = 0;
	kotek::uint32_t offset_d = 0;

	ASSERT_TRUE(allocator.allocate(8, offset_a));
	ASSERT_TRUE(allocator.allocate(8, offset_b));
	ASSERT_TRUE(allocator.allocate(8, offset_c));
	ASSERT_TRUE(allocator.allocate(8, offset_d));

	// punch two holes: live = [0,8) + [16,8), free = [8,8) + [24,8)
	ASSERT_TRUE(allocator.free(offset_b, 8));
	ASSERT_TRUE(allocator.free(offset_d, 8));

	EXPECT_EQ(allocator.get_free_total(), 16u);
	EXPECT_EQ(allocator.get_free_range_count(), 2u);

	// a 12-element fit fails fragmented though 16 are free in total
	kotek::uint32_t offset = 0;
	EXPECT_FALSE(allocator.allocate(12, offset));

	zircon_pool_range_allocator::relocation_t relocations[4];

	kotek::uint32_t new_used_end = 0;

	const kotek::uint32_t relocation_count =
		allocator.compute_defrag_relocations(
			relocations, 4, new_used_end);

	// exactly one live segment moves: [16,8) -> [8,8)
	ASSERT_EQ(relocation_count, 1u);
	EXPECT_EQ(relocations[0].m_old_offset, 16u);
	EXPECT_EQ(relocations[0].m_new_offset, 8u);
	EXPECT_EQ(relocations[0].m_count, 8u);
	EXPECT_EQ(new_used_end, 16u);

	allocator.apply_defrag_layout(new_used_end);

	EXPECT_EQ(allocator.get_free_range_count(), 1u);
	EXPECT_EQ(allocator.get_free_total(), 16u);

	// the fit that failed before now succeeds in the tail range
	ASSERT_TRUE(allocator.allocate(12, offset));
	EXPECT_EQ(offset, 16u);
}

// (b0) the plane extraction itself: the identity view-projection yields
// the canonical unit-cube planes, normalized
TEST(Zircon_Game, ChunkPoolFrustumPlaneExtraction)
{
	const float identity[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	float planes[24] = {};

	zircon_render_chunk_pool::extract_frustum_planes(identity, planes);

	// left = c3+c0 = (1,0,0,1); right = c3-c0 = (-1,0,0,1);
	// bottom = c3+c1 = (0,1,0,1); top = c3-c1 = (0,-1,0,1);
	// near = c2 = (0,0,1,0); far = c3-c2 = (0,0,-1,1)
	const float expected[24] = {1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f};

	for (int element = 0; element < 24; ++element)
	{
		EXPECT_FLOAT_EQ(planes[element], expected[element]);
	}

	// the identity planes classify the canonical cases: the unit cube
	// is in, a point past x=1 is out (the right plane)
	EXPECT_TRUE(is_box_visible(planes, -0.5f, -0.5f, -0.5f, 0.5f, 0.5f,
		0.5f));
	EXPECT_FALSE(is_box_visible(planes, 2.0f, 0.0f, 0.0f, 2.0f, 0.0f,
		0.0f));
}

// (b) the classification mirror against the test camera: fully in,
// fully out per plane, straddling, degenerate AABB, camera inside
TEST(Zircon_Game, ChunkPoolFrustumClassification)
{
	float planes[24];
	build_test_planes(planes);

	// every plane comes out normalized (unit xyz length)
	for (int plane_index = 0; plane_index < 6; ++plane_index)
	{
		const float* p_plane = planes + plane_index * 4;
		const float length = std::sqrt(p_plane[0] * p_plane[0] +
			p_plane[1] * p_plane[1] + p_plane[2] * p_plane[2]);

		EXPECT_NEAR(length, 1.0f, 1e-4f);
	}

	// fully in: the unit cube at the look-at point
	EXPECT_TRUE(is_box_visible(planes, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
		1.0f));

	// fully out, one case per plane: left/right (x), bottom/top (y),
	// near (behind the eye), far (past the far plane)
	EXPECT_FALSE(is_box_visible(planes, -22.0f, -1.0f, -1.0f, -20.0f,
		1.0f, 1.0f));
	EXPECT_FALSE(is_box_visible(planes, 20.0f, -1.0f, -1.0f, 22.0f, 1.0f,
		1.0f));
	EXPECT_FALSE(is_box_visible(planes, -1.0f, -22.0f, -1.0f, 1.0f,
		-20.0f, 1.0f));
	EXPECT_FALSE(is_box_visible(planes, -1.0f, 20.0f, -1.0f, 1.0f, 22.0f,
		1.0f));
	EXPECT_FALSE(is_box_visible(planes, -1.0f, -1.0f, -22.0f, 1.0f, 1.0f,
		-20.0f));
	EXPECT_FALSE(is_box_visible(planes, -1.0f, -1.0f, 200.0f, 1.0f, 1.0f,
		202.0f));

	// straddling: half in half out across the right plane — the
	// positive-vertex test must keep it visible
	EXPECT_TRUE(is_box_visible(planes, 5.0f, -1.0f, -1.0f, 7.0f, 1.0f,
		1.0f));

	// degenerate AABB (a point): inside the frustum -> visible, outside
	// -> culled
	EXPECT_TRUE(is_box_visible(planes, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f));
	EXPECT_FALSE(is_box_visible(planes, 100.0f, 0.0f, 0.0f, 100.0f, 0.0f,
		0.0f));

	// the camera inside the AABB: every p-vertex is inside, visible
	EXPECT_TRUE(is_box_visible(planes, -100.0f, -100.0f, -100.0f, 100.0f,
		100.0f, 100.0f));
}

// (c) registration + the synthetic fixture under the known camera:
// distinct ids/AABBs, the expected visible set, removal dropping a
// chunk from the cull, clear resetting — plus the bake-correctness pin
// against compose_model_matrix
TEST(Zircon_Game, ChunkPoolRegisterAndCull)
{
	// heap-allocated on purpose — the pool is ~2 MB of shadows/tables
	// (see the fixture note in zircon_unit_tests_game.cpp)
	zircon_render_chunk_pool& pool = *new zircon_render_chunk_pool();

	constexpr kotek::uint16_t _kGridSide = 2;

	const kotek::uint32_t registered =
		zircon_gpu_driven_pass::build_synthetic_grid(pool, _kGridSide);

	EXPECT_EQ(registered, 8u);
	EXPECT_EQ(pool.get_chunk_slot_count(), 8u);
	EXPECT_EQ(pool.get_live_chunk_count(), 8u);

	// chunk 0 sits at the (-2,-2,-2) grid corner: the unit cube's AABB
	// wraps it exactly
	const zircon_chunk_bounds_t* p_bounds = pool.get_bounds();

	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_min[0], -3.0f);
	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_min[1], -3.0f);
	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_min[2], -3.0f);
	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_max[0], -1.0f);
	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_max[1], -1.0f);
	EXPECT_FLOAT_EQ(p_bounds[0].m_aabb_max[2], -1.0f);

	// the live flag rides the spare lane as the bit pattern the shader
	// tests
	kotek::uint32_t live_bits = 0;
	std::memcpy(&live_bits, &p_bounds[0].m_aabb_min[3], sizeof(float));
	EXPECT_EQ(live_bits, 1u);

	// every grid chunk is visible from the test camera (the grid wraps
	// the look-at point)
	float planes[24];
	build_test_planes(planes);

	kotek::uint32_t visible_ids
		[zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS];

	kotek::uint32_t visible_count = pool.cull_chunks_against_frustum(
		planes, visible_ids, zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS);

	EXPECT_EQ(visible_count, 8u);

	// a camera 1000 m back with a 100 m far plane culls everything
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

	// removal drops the chunk from the cull; the stale second remove is
	// a loud no-op
	ASSERT_TRUE(pool.remove_chunk(3));
	EXPECT_EQ(pool.get_live_chunk_count(), 7u);
	EXPECT_FALSE(pool.is_chunk_live(3));
	EXPECT_FALSE(pool.remove_chunk(3));

	visible_count = pool.cull_chunks_against_frustum(
		planes, visible_ids, zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS);

	ASSERT_EQ(visible_count, 7u);

	for (kotek::uint32_t visible_index = 0; visible_index < visible_count;
		 ++visible_index)
	{
		EXPECT_NE(visible_ids[visible_index], 3u);
	}

	// the removed slot recycles into the next registration
	{
		zircon_model_static_vertex_t
			cube_vertices
				[zircon_pass_model_static::
						kCubeVertexCount];
		kotek::uint16_t
			cube_indices
				[zircon_pass_model_static::
						kCubeIndexCount];

		zircon_pass_model_static::build_cube_mesh(
			cube_vertices, cube_indices);

		kotek::uint32_t recycled_id =
			zircon_render_chunk_pool::kInvalidChunkId;

		ASSERT_TRUE(pool.register_chunk(cube_vertices,
			zircon_pass_model_static::kCubeVertexCount,
			cube_indices,
			zircon_pass_model_static::kCubeIndexCount,
			nullptr, 0, recycled_id));

		EXPECT_EQ(recycled_id, 3u);
		EXPECT_EQ(pool.get_live_chunk_count(), 8u);
	}

	// the bake: a translated+scaled cube lands in the pool at the baked
	// spot with the exact AABB (compose_model_matrix pins the matrix)
	{
		zircon_model_static_vertex_t
			cube_vertices
				[zircon_pass_model_static::
						kCubeVertexCount];
		kotek::uint16_t
			cube_indices
				[zircon_pass_model_static::
						kCubeIndexCount];

		zircon_pass_model_static::build_cube_mesh(
			cube_vertices, cube_indices);

		const float position[3] = {10.0f, 0.0f, 0.0f};
		const float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		const float scale[3] = {2.0f, 2.0f, 2.0f};

		float model[16];

		zircon_gpu_driven_pass::compose_model_matrix(
			position, rotation, scale, model);

		kotek::uint32_t baked_id =
			zircon_render_chunk_pool::kInvalidChunkId;

		ASSERT_TRUE(pool.register_chunk(cube_vertices,
			zircon_pass_model_static::kCubeVertexCount,
			cube_indices,
			zircon_pass_model_static::kCubeIndexCount,
			model, 0, baked_id));

		const zircon_chunk_bounds_t& baked_bounds =
			pool.get_bounds()[baked_id];

		EXPECT_FLOAT_EQ(baked_bounds.m_aabb_min[0], 8.0f);
		EXPECT_FLOAT_EQ(baked_bounds.m_aabb_max[0], 12.0f);
		EXPECT_FLOAT_EQ(baked_bounds.m_aabb_min[1], -2.0f);
		EXPECT_FLOAT_EQ(baked_bounds.m_aabb_max[1], 2.0f);

		// the shadow holds the baked (world-space) positions
		const zircon_chunk_ranges_t& baked_ranges =
			pool.get_ranges()[baked_id];

		const zircon_model_static_vertex_t* p_shadow =
			pool.get_vertex_shadow();

		// the cube fixture's vertex 0 is (1,-1,-1) model-space -> baked
		// (12,-2,-2)
		EXPECT_FLOAT_EQ(
			p_shadow[baked_ranges.m_vertex_offset].m_position[0], 12.0f);
		EXPECT_FLOAT_EQ(
			p_shadow[baked_ranges.m_vertex_offset].m_position[1], -2.0f);
		EXPECT_FLOAT_EQ(
			p_shadow[baked_ranges.m_vertex_offset].m_position[2], -2.0f);

		EXPECT_TRUE(pool.is_pools_dirty());
		EXPECT_TRUE(pool.is_tables_dirty());
	}

	pool.clear();

	EXPECT_EQ(pool.get_chunk_slot_count(), 0u);
	EXPECT_EQ(pool.get_live_chunk_count(), 0u);
	EXPECT_EQ(pool.get_vertex_high_water(), 0u);

	delete &pool;
}

// (d) overflow is loud, never silent: the chunk table cap rejects the
// 1025th chunk and a vertex-pool overflow rejects the registration —
// both with the invalid id and a false return
TEST(Zircon_Game, ChunkPoolOverflowLoud)
{
	zircon_render_chunk_pool& pool = *new zircon_render_chunk_pool();

	zircon_model_static_vertex_t
		cube_vertices
			[zircon_pass_model_static::kCubeVertexCount];
	kotek::uint16_t
		cube_indices
			[zircon_pass_model_static::kCubeIndexCount];

	zircon_pass_model_static::build_cube_mesh(
		cube_vertices, cube_indices);

	// fill the chunk table (1024 cubes = 24576 vertices — the pools
	// stay far from their caps, so only the table bound is exercised)
	for (kotek::uint32_t chunk_index = 0;
		 chunk_index < zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS;
		 ++chunk_index)
	{
		kotek::uint32_t chunk_id =
			zircon_render_chunk_pool::kInvalidChunkId;

		ASSERT_TRUE(pool.register_chunk(cube_vertices,
			zircon_pass_model_static::kCubeVertexCount,
			cube_indices,
			zircon_pass_model_static::kCubeIndexCount,
			nullptr, 0, chunk_id));

		EXPECT_EQ(chunk_id, chunk_index);
	}

	// the 1025th: the table is full
	kotek::uint32_t overflow_id = 0;

	EXPECT_FALSE(pool.register_chunk(cube_vertices,
		zircon_pass_model_static::kCubeVertexCount,
		cube_indices,
		zircon_pass_model_static::kCubeIndexCount,
		nullptr, 0, overflow_id));

	EXPECT_EQ(overflow_id, zircon_render_chunk_pool::kInvalidChunkId);
	EXPECT_EQ(pool.get_live_chunk_count(),
		static_cast<kotek::uint32_t>(
			zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS));

	pool.clear();

	// the vertex pool: one 40000-vertex chunk fits, the second one does
	// not (25536 free < 40000, and defragmentation cannot help — the
	// failed fit is rolled back cleanly)
	constexpr kotek::uint32_t _kBigVertexCount = 40000;
	constexpr kotek::uint32_t _kBigIndexCount = 40000;

	zircon_model_static_vertex_t* p_big_vertices =
		new zircon_model_static_vertex_t[_kBigVertexCount];
	kotek::uint16_t* p_big_indices = new kotek::uint16_t[_kBigIndexCount];

	for (kotek::uint32_t vertex_index = 0;
		 vertex_index < _kBigVertexCount; ++vertex_index)
	{
		p_big_vertices[vertex_index].m_position[0] = 0.0f;
		p_big_vertices[vertex_index].m_position[1] = 0.0f;
		p_big_vertices[vertex_index].m_position[2] = 0.0f;
		p_big_vertices[vertex_index].m_normal[0] = 0.0f;
		p_big_vertices[vertex_index].m_normal[1] = 1.0f;
		p_big_vertices[vertex_index].m_normal[2] = 0.0f;
		p_big_vertices[vertex_index].m_color_abgr = 0xffffffffu;
	}

	for (kotek::uint32_t index_index = 0; index_index < _kBigIndexCount;
		 ++index_index)
	{
		p_big_indices[index_index] = static_cast<kotek::uint16_t>(
			index_index % _kBigVertexCount);
	}

	kotek::uint32_t big_id = zircon_render_chunk_pool::kInvalidChunkId;

	ASSERT_TRUE(pool.register_chunk(p_big_vertices, _kBigVertexCount,
		p_big_indices, _kBigIndexCount, nullptr, 0, big_id));

	EXPECT_FALSE(pool.register_chunk(p_big_vertices, _kBigVertexCount,
		p_big_indices, _kBigIndexCount, nullptr, 0, big_id));

	EXPECT_EQ(big_id, zircon_render_chunk_pool::kInvalidChunkId);
	EXPECT_EQ(pool.get_live_chunk_count(), 1u);
	// the rolled-back half-fit: only the first chunk's elements are used
	EXPECT_EQ(pool.get_vertex_high_water(), _kBigVertexCount);

	delete[] p_big_indices;
	delete[] p_big_vertices;

	delete &pool;
}

		#endif
	#endif
#endif
