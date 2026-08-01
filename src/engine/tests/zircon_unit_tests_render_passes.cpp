#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_geometry.h"
		#include "../../ecs/zircon_component_transform.h"
		#include "../../world/zircon_world.h"
		#include "../../core/zircon_config.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_graph_pass_model_static.h"

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

		#endif
	#endif
#endif
