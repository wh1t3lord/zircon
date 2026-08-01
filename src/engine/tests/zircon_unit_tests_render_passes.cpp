#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cmath>

		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_geometry.h"
		#include "../../ecs/zircon_component_transform.h"
		#include "../../world/zircon_world.h"
		#include "../../core/zircon_config.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_graph_pass_model_static.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_graph_pass_editor_grid.h"

		#ifndef ZIRCON_DEF_UNIT_TEST_RENDER_PASSES
			#define ZIRCON_DEF_UNIT_TEST_RENDER_PASSES 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_RENDER_PASSES == 1

// functional proofs for task Z3 P2b (game static-model pass): the
// fallback cube fixture is the pass's mesh source and the future editor
// "add primitive" — its shape is pinned here; collect_draw_items is the
// pass's per-frame world query, driven against a test-only world (a
// matching cube entity must produce exactly one draw item with the
// entity's composed model matrix; anything else is a clean no-op)

using zircon_pass_model_static =
	no_streaming::zircon_render_graph_pass_model_static_bgfx;

TEST(Zircon_Game, RenderPassModelStaticCubeMeshFixture)
{
	zircon_model_static_vertex_t
		vertices[zircon_pass_model_static::kCubeVertexCount];
	kotek::uint16_t
		indices[zircon_pass_model_static::kCubeIndexCount];

	zircon_pass_model_static::build_cube_mesh(vertices, indices);

	static_assert(zircon_pass_model_static::kCubeVertexCount == 24,
		"the fallback cube is 6 faces x 4 vertices");
	static_assert(zircon_pass_model_static::kCubeIndexCount == 36,
		"the fallback cube is 12 triangles");

	// AABB of the unit cube centered at the origin
	float aabb_min[3] = {0.0f, 0.0f, 0.0f};
	float aabb_max[3] = {0.0f, 0.0f, 0.0f};

	for (kotek::uint8_t vertex_index = 0;
		 vertex_index < zircon_pass_model_static::kCubeVertexCount;
		 ++vertex_index)
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			const float coordinate =
				vertices[vertex_index].m_position[axis];

			if (vertex_index == 0 ||
				coordinate < aabb_min[axis])
			{
				aabb_min[axis] = coordinate;
			}

			if (vertex_index == 0 ||
				coordinate > aabb_max[axis])
			{
				aabb_max[axis] = coordinate;
			}
		}

		// every vertex carries an opaque per-face color
		EXPECT_EQ(vertices[vertex_index].m_color_abgr & 0xff000000u,
			0xff000000u);
	}

	// every vertex carries its face's unit axis normal (face f of 6,
	// 4 vertices each; the pass fixture order is +X,-X,+Y,-Y,+Z,-Z)
	for (kotek::uint8_t vertex_index = 0;
		 vertex_index < zircon_pass_model_static::kCubeVertexCount;
		 ++vertex_index)
	{
		const int axis = (vertex_index / 4) / 2;
		const float sign = ((vertex_index / 4) % 2 == 0) ? 1.0f : -1.0f;

		for (int component = 0; component < 3; ++component)
		{
			EXPECT_FLOAT_EQ(
				vertices[vertex_index].m_normal[component],
				component == axis ? sign : 0.0f);
		}
	}

	for (int axis = 0; axis < 3; ++axis)
	{
		EXPECT_FLOAT_EQ(aabb_min[axis], -1.0f);
		EXPECT_FLOAT_EQ(aabb_max[axis], 1.0f);
	}

	// indices stay inside the vertex range and cover full quads
	for (kotek::uint8_t index_offset = 0;
		 index_offset < zircon_pass_model_static::kCubeIndexCount;
		 ++index_offset)
	{
		EXPECT_LT(indices[index_offset],
			zircon_pass_model_static::kCubeVertexCount);
	}
}

namespace
{
	// headless world environment for the collect test: heap allocated on
	// purpose — ktkConsole alone is about a megabyte (see the note in
	// zircon_unit_tests_game.cpp) and a stack fixture overflows the 1 Mb
	// default thread stack
	struct zircon_test_model_static_env
	{
		kotek::core::ktkConsole console;
		kotek::core::ktkInput input;
		zircon_config engine_config;
		zircon_factory factory;
		zircon_world world;
	};
} // namespace

TEST(Zircon_Game, RenderPassModelStaticCollectDrawItems)
{
	zircon_test_model_static_env& env = *new zircon_test_model_static_env();

	env.factory.Initialize(&env.engine_config, &env.console, &env.input);

	env.world.initialize("zircon_p2b_test_world", &env.engine_config,
		&env.console, &env.input, &env.factory,
		ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT);

	zircon_ecs_context_t* p_context = env.world.get_ecs_context();

	// an empty world is a clean no-op
	zircon_render_pass_model_static_draw_item_t
		draw_items[zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT];

	kotek::uint32_t draw_count =
		zircon_pass_model_static::collect_draw_items(&env.factory,
			p_context, env.world.get_entity_count_max_limit(), draw_items,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT);

	EXPECT_EQ(draw_count, 0u);

	// one drawable cube entity: translation + non-uniform scale,
	// identity rotation
	kotek::entity_t entity_cube = env.factory.create_entity(p_context);

	env.factory.create_component(p_context, entity_cube,
		eZirconComponentType::kzircon_component_transform);
	env.factory.create_component(p_context, entity_cube,
		eZirconComponentType::kzircon_component_geometry);

	zircon_component_transform* p_transform =
		static_cast<zircon_component_transform*>(
			env.factory.get_component_by_enum(p_context, entity_cube,
				eZirconComponentType::kzircon_component_transform));

	zircon_component_geometry* p_geometry =
		static_cast<zircon_component_geometry*>(
			env.factory.get_component_by_enum(p_context, entity_cube,
				eZirconComponentType::kzircon_component_geometry));

	ASSERT_NE(p_transform, nullptr);
	ASSERT_NE(p_geometry, nullptr);

	p_transform->set_position(kotek::math::vec3f_t(1.0f, 2.0f, 3.0f));
	p_transform->set_scale(kotek::math::vec3f_t(2.0f, 3.0f, 4.0f));
	p_transform->set_rotation(
		kotek::math::quatf_t(0.0f, 0.0f, 0.0f, 1.0f));
	p_geometry->set_geometry_type(
		kotek::core::eStaticGeometryType::kBox);

	// a non-drawable companion: transform but a geometry kind the pass
	// has no mesh source for yet (kUnknown)
	kotek::entity_t entity_other = env.factory.create_entity(p_context);

	env.factory.create_component(p_context, entity_other,
		eZirconComponentType::kzircon_component_transform);
	env.factory.create_component(p_context, entity_other,
		eZirconComponentType::kzircon_component_geometry);

	draw_count = zircon_pass_model_static::collect_draw_items(
		&env.factory, p_context, env.world.get_entity_count_max_limit(),
		draw_items, zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT);

	ASSERT_EQ(draw_count, 1u);

	// column-major model matrix: scale folds into the rotation columns,
	// translation lives at [12..14]
	const float* p_model = draw_items[0].m_model_matrix;

	EXPECT_FLOAT_EQ(p_model[0], 2.0f);
	EXPECT_FLOAT_EQ(p_model[1], 0.0f);
	EXPECT_FLOAT_EQ(p_model[2], 0.0f);
	EXPECT_FLOAT_EQ(p_model[3], 0.0f);
	EXPECT_FLOAT_EQ(p_model[4], 0.0f);
	EXPECT_FLOAT_EQ(p_model[5], 3.0f);
	EXPECT_FLOAT_EQ(p_model[6], 0.0f);
	EXPECT_FLOAT_EQ(p_model[7], 0.0f);
	EXPECT_FLOAT_EQ(p_model[8], 0.0f);
	EXPECT_FLOAT_EQ(p_model[9], 0.0f);
	EXPECT_FLOAT_EQ(p_model[10], 4.0f);
	EXPECT_FLOAT_EQ(p_model[11], 0.0f);
	EXPECT_FLOAT_EQ(p_model[12], 1.0f);
	EXPECT_FLOAT_EQ(p_model[13], 2.0f);
	EXPECT_FLOAT_EQ(p_model[14], 3.0f);
	EXPECT_FLOAT_EQ(p_model[15], 1.0f);

	// rotation composes (bx quaternion convention, matching bx::mtxLookAt/
	// mtxProj): 90 degrees about Z maps +X to +Y in bx's row-vector
	// reading — stored column-major that is m[1] == -1 and m[4] == +1
	const float half_sqrt2 = 0.70710678118654752f;

	p_transform->set_position(kotek::math::vec3f_t(0.0f, 0.0f, 0.0f));
	p_transform->set_scale(kotek::math::vec3f_t(1.0f, 1.0f, 1.0f));
	p_transform->set_rotation(
		kotek::math::quatf_t(0.0f, 0.0f, half_sqrt2, half_sqrt2));

	draw_count = zircon_pass_model_static::collect_draw_items(
		&env.factory, p_context, env.world.get_entity_count_max_limit(),
		draw_items, zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT);

	ASSERT_EQ(draw_count, 1u);

	EXPECT_NEAR(p_model[0], 0.0f, 0.0001f);
	EXPECT_NEAR(p_model[1], -1.0f, 0.0001f);
	EXPECT_NEAR(p_model[4], 1.0f, 0.0001f);
	EXPECT_NEAR(p_model[5], 0.0f, 0.0001f);
	EXPECT_NEAR(p_model[10], 1.0f, 0.0001f);
	EXPECT_NEAR(p_model[15], 1.0f, 0.0001f);

	// disabling visibility removes the draw
	p_geometry->set_visible(false);

	draw_count = zircon_pass_model_static::collect_draw_items(
		&env.factory, p_context, env.world.get_entity_count_max_limit(),
		draw_items, zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT);

	EXPECT_EQ(draw_count, 0u);

	env.world.shutdown(&env.factory);
	env.factory.Shutdown();

	delete &env;
}

using zircon_pass_editor_grid =
	no_streaming::zircon_render_graph_pass_editor_grid_bgfx;

// functional proof for task Z3 P2d (editor grid pass): the pass's
// fragment shader reconstructs the world ray through a pixel from the
// inverse view-projection and intersects it with the y=0 grid plane; the
// C++ statics mirror that math one-to-one (grid.fs.slang carries the same
// formulas) and are pinned here against the pass's documented fallback —
// the default orbit eye (4,3,4) looks at the origin, so the screen-center
// ray must hit the grid plane exactly at the origin
TEST(Zircon_Editor, RenderPassEditorGridRayMath)
{
	constexpr float _kAspect = 4.0f / 3.0f;

	float view[16];
	float projection[16];

	// no bgfx in the unit tests: the homogeneous-depth flag only selects
	// the projection's z mapping and the ray math is convention-agnostic
	// (two arbitrary NDC depths stay on the same view ray)
	zircon_pass_editor_grid::build_default_orbit_view_projection(
		view, projection, _kAspect, false);

	// the camera position extracted from the rigid view matrix must be
	// the orbit eye
	float camera_position[3];

	zircon_pass_editor_grid::compute_camera_position(
		view, camera_position);

	EXPECT_NEAR(camera_position[0], 4.0f, 0.0001f);
	EXPECT_NEAR(camera_position[1], 3.0f, 0.0001f);
	EXPECT_NEAR(camera_position[2], 4.0f, 0.0001f);

	float inverse_view_projection[16];

	zircon_pass_editor_grid::compose_inverse_view_projection(
		view, projection, inverse_view_projection);

	float point_near[3];
	float point_far[3];

	ASSERT_TRUE(zircon_pass_editor_grid::compute_world_ray(
		inverse_view_projection, 0.0f, 0.0f, point_near, point_far));

	// the screen-center ray of an orbit looking at the origin must point
	// exactly at the origin: direction = (target - eye) / |target - eye|
	// with eye (4,3,4) and target (0,0,0)
	float ray_direction[3] = {point_far[0] - point_near[0],
		point_far[1] - point_near[1], point_far[2] - point_near[2]};

	const float direction_length = std::sqrt(
		ray_direction[0] * ray_direction[0] +
		ray_direction[1] * ray_direction[1] +
		ray_direction[2] * ray_direction[2]);

	ASSERT_GT(direction_length, 0.0f);

	ray_direction[0] /= direction_length;
	ray_direction[1] /= direction_length;
	ray_direction[2] /= direction_length;

	// (-4,-3,-4) normalized, |(-4,-3,-4)| = sqrt(41)
	constexpr float _kSqrt41 = 6.40312423743284869f;

	EXPECT_NEAR(ray_direction[0], -4.0f / _kSqrt41, 0.0001f);
	EXPECT_NEAR(ray_direction[1], -3.0f / _kSqrt41, 0.0001f);
	EXPECT_NEAR(ray_direction[2], -4.0f / _kSqrt41, 0.0001f);

	// ...and the ray meets the y=0 grid plane exactly at the orbit's
	// target (the origin)
	float hit[3];

	ASSERT_TRUE(zircon_pass_editor_grid::intersect_xz_plane(
		camera_position, ray_direction, hit));

	EXPECT_NEAR(hit[0], 0.0f, 0.001f);
	EXPECT_NEAR(hit[1], 0.0f, 0.001f);
	EXPECT_NEAR(hit[2], 0.0f, 0.001f);

	// the plane intersection's contract on its own: a horizontal ray is
	// parallel (no hit), a ray away from the plane never reaches it
	// (behind the camera), a straight-down ray hits under the camera
	const float ray_horizontal[3] = {0.70710678f, 0.0f, 0.70710678f};
	const float ray_up[3] = {0.0f, 1.0f, 0.0f};
	const float ray_down[3] = {0.0f, -1.0f, 0.0f};

	EXPECT_FALSE(zircon_pass_editor_grid::intersect_xz_plane(
		camera_position, ray_horizontal, hit));

	EXPECT_FALSE(zircon_pass_editor_grid::intersect_xz_plane(
		camera_position, ray_up, hit));

	ASSERT_TRUE(zircon_pass_editor_grid::intersect_xz_plane(
		camera_position, ray_down, hit));

	EXPECT_NEAR(hit[0], 4.0f, 0.0001f);
	EXPECT_NEAR(hit[1], 0.0f, 0.0001f);
	EXPECT_NEAR(hit[2], 4.0f, 0.0001f);

	// a camera BELOW the plane still sees it when looking up (the grid
	// must not vanish when the editor camera dips under the floor)
	const float camera_below[3] = {1.0f, -2.0f, 1.0f};

	ASSERT_TRUE(zircon_pass_editor_grid::intersect_xz_plane(
		camera_below, ray_up, hit));

	EXPECT_NEAR(hit[0], 1.0f, 0.0001f);
	EXPECT_NEAR(hit[1], 0.0f, 0.0001f);
	EXPECT_NEAR(hit[2], 1.0f, 0.0001f);
}

		#endif
	#endif
#endif
