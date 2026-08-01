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
		#include "../../render/bgfx/passes/no_streaming/zircon_render_graph_pass_editor_gizmo_own.h"

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

using zircon_pass_editor_gizmo_own =
	no_streaming::zircon_render_graph_pass_editor_gizmo_own_bgfx;

// functional proofs for task Z3 P2e (editor own-gizmo pass). The pass's
// picking and drag math is pure statics on float arrays (the analytic
// R7 design — no GPU picking), so the whole interaction model is pinned
// headless here: handle picking with its center > plane > axis priority,
// the drag-delta application per mode with snapping on/off, and the
// click-select sphere pick against a test world. The drag-END command
// issuance through the real history lives in
// zircon_unit_tests_command_history.cpp
// (CommandHistory_GizmoDragEndEditCommand).

TEST(Zircon_Editor, RenderPassEditorGizmoOwnSnapAndScreenSize)
{
	// snap_value rounds to the nearest step multiple; a non-positive
	// step passes the value through
	EXPECT_FLOAT_EQ(
		zircon_pass_editor_gizmo_own::snap_value(0.3f, 0.25f), 0.25f);
	EXPECT_FLOAT_EQ(
		zircon_pass_editor_gizmo_own::snap_value(2.0f, 0.25f), 2.0f);
	EXPECT_FLOAT_EQ(
		zircon_pass_editor_gizmo_own::snap_value(-0.3f, 0.25f), -0.25f);
	EXPECT_FLOAT_EQ(
		zircon_pass_editor_gizmo_own::snap_value(0.3f, 0.0f), 0.3f);
	EXPECT_FLOAT_EQ(zircon_pass_editor_gizmo_own::snap_value(20.0f, 15.0f),
		15.0f);

	// the constant-screen-size factor: the gizmo at twice the distance
	// must scale exactly twice (the on-screen pixel extent is invariant)
	const float camera_near[3] = {4.0f, 3.0f, 4.0f};
	const float camera_far[3] = {8.0f, 6.0f, 8.0f};
	const float origin[3] = {0.0f, 0.0f, 0.0f};

	// 60-degree fov: projection[5] = 1/tan(30 deg)
	constexpr float _kProj60 = 1.7320508f;

	const float scale_near =
		zircon_pass_editor_gizmo_own::compute_gizmo_scale(
			camera_near, origin, _kProj60, 1080.0f);
	const float scale_far =
		zircon_pass_editor_gizmo_own::compute_gizmo_scale(
			camera_far, origin, _kProj60, 1080.0f);

	EXPECT_GT(scale_near, 0.0f);
	EXPECT_NEAR(scale_far / scale_near, 2.0f, 0.0001f);

	// degenerate inputs stay finite (the window can be minimized)
	const float scale_guarded =
		zircon_pass_editor_gizmo_own::compute_gizmo_scale(
			camera_near, origin, 0.0f, 0.0f);

	EXPECT_TRUE(std::isfinite(scale_guarded));
	EXPECT_GT(scale_guarded, 0.0f);
}

TEST(Zircon_Editor, RenderPassEditorGizmoOwnPickHandles)
{
	const float gizmo_origin[3] = {0.0f, 0.0f, 0.0f};
	constexpr float _kScale = 1.0f;

	// straight at the X arrow's shaft from below
	{
		const float ray_origin[3] = {0.5f, -3.0f, 0.0f};
		const float ray_direction[3] = {0.0f, 1.0f, 0.0f};

		float ray_length = 0.0f;

		EXPECT_EQ(zircon_pass_editor_gizmo_own::pick_handle(ray_origin,
					  ray_direction,
					  no_streaming::eZirconRenderPassGizmoMode::kTranslate,
					  gizmo_origin, _kScale, &ray_length),
			0);
		EXPECT_NEAR(ray_length, 3.0f, 0.0001f);
	}

	// past the arrow tip: nothing under the ray
	{
		const float ray_origin[3] = {2.0f, -3.0f, 0.0f};
		const float ray_direction[3] = {0.0f, 1.0f, 0.0f};

		EXPECT_EQ(zircon_pass_editor_gizmo_own::pick_handle(ray_origin,
					  ray_direction,
					  no_streaming::eZirconRenderPassGizmoMode::kTranslate,
					  gizmo_origin, _kScale),
			-1);
	}

	// the center handle beats the axis handle even when the axis's
	// intersection is nearer... the class priority is absolute
	// (center > plane > axis): a ray down the X axis at a small offset
	// clips BOTH the shaft and the center sphere — the center (handle 6
	// of the translate set) must win
	{
		const float ray_origin[3] = {-3.0f, 0.05f, 0.0f};
		const float ray_direction[3] = {1.0f, 0.0f, 0.0f};

		EXPECT_EQ(zircon_pass_editor_gizmo_own::pick_handle(ray_origin,
					  ray_direction,
					  no_streaming::eZirconRenderPassGizmoMode::kTranslate,
					  gizmo_origin, _kScale),
			6);
	}

	// the XY plane quad from straight above (handle 3)
	{
		const float ray_origin[3] = {0.5f, 0.5f, 5.0f};
		const float ray_direction[3] = {0.0f, 0.0f, -1.0f};

		EXPECT_EQ(zircon_pass_editor_gizmo_own::pick_handle(ray_origin,
					  ray_direction,
					  no_streaming::eZirconRenderPassGizmoMode::kTranslate,
					  gizmo_origin, _kScale),
			3);
	}

	// the quad's hole (inside the quad's minimum) hits nothing in
	// translate mode... the ray misses the axes too (it is parallel to
	// the Z shaft and too far from X/Y)
	{
		const float ray_origin[3] = {0.2f, 0.2f, 5.0f};
		const float ray_direction[3] = {0.0f, 0.0f, -1.0f};

		EXPECT_EQ(zircon_pass_editor_gizmo_own::pick_handle(ray_origin,
					  ray_direction,
					  no_streaming::eZirconRenderPassGizmoMode::kTranslate,
					  gizmo_origin, _kScale),
			-1);
	}

	// rotate mode: the X ring is the YZ-plane band — a ray meeting the
	// plane at unit distance from the origin hits handle 7, one meeting
	// it inside the ring's hole hits nothing
	{
		const float ray_origin_hit[3] = {3.0f, 1.0f, 0.0f};
		const float ray_origin_miss[3] = {3.0f, 0.5f, 0.0f};
		const float ray_direction[3] = {-1.0f, 0.0f, 0.0f};

		EXPECT_EQ(zircon_pass_editor_gizmo_own::pick_handle(ray_origin_hit,
					  ray_direction,
					  no_streaming::eZirconRenderPassGizmoMode::kRotate,
					  gizmo_origin, _kScale),
			7);

		EXPECT_EQ(zircon_pass_editor_gizmo_own::pick_handle(ray_origin_miss,
					  ray_direction,
					  no_streaming::eZirconRenderPassGizmoMode::kRotate,
					  gizmo_origin, _kScale),
			-1);
	}

	// scale mode: the center cube (handle 13) and an axis shaft+cube
	// (handle 10 for X)
	{
		const float ray_origin_center[3] = {0.0f, 0.0f, 5.0f};
		const float ray_direction[3] = {0.0f, 0.0f, -1.0f};

		EXPECT_EQ(zircon_pass_editor_gizmo_own::pick_handle(
					  ray_origin_center, ray_direction,
					  no_streaming::eZirconRenderPassGizmoMode::kScale,
					  gizmo_origin, _kScale),
			13);

		const float ray_origin_axis[3] = {0.5f, -3.0f, 0.0f};
		const float ray_direction_axis[3] = {0.0f, 1.0f, 0.0f};

		EXPECT_EQ(zircon_pass_editor_gizmo_own::pick_handle(
					  ray_origin_axis, ray_direction_axis,
					  no_streaming::eZirconRenderPassGizmoMode::kScale,
					  gizmo_origin, _kScale),
			10);
	}

	// the mouse-ray mapping itself: the screen-center pixel of the
	// default orbit must produce the same origin-pointing ray the grid
	// test pins through compute_world_ray
	{
		float view[16];
		float projection[16];

		zircon_pass_editor_grid::build_default_orbit_view_projection(
			view, projection, 4.0f / 3.0f, false);

		float inverse_view_projection[16];

		zircon_pass_editor_grid::compose_inverse_view_projection(
			view, projection, inverse_view_projection);

		float ray_origin[3];
		float ray_direction[3];

		ASSERT_TRUE(zircon_pass_editor_gizmo_own::compute_mouse_ray(
			inverse_view_projection, 400.0f, 300.0f, 800.0f, 600.0f,
			ray_origin, ray_direction));

		// (-4,-3,-4) normalized from the orbit eye
		constexpr float _kSqrt41 = 6.40312423743284869f;

		EXPECT_NEAR(ray_direction[0], -4.0f / _kSqrt41, 0.0001f);
		EXPECT_NEAR(ray_direction[1], -3.0f / _kSqrt41, 0.0001f);
		EXPECT_NEAR(ray_direction[2], -4.0f / _kSqrt41, 0.0001f);
	}
}

TEST(Zircon_Editor, RenderPassEditorGizmoOwnDragTranslate)
{
	const float gizmo_origin[3] = {0.0f, 0.0f, 0.0f};
	const float camera_position[3] = {4.0f, 3.0f, 4.0f};
	const float start_position[3] = {10.0f, 20.0f, 30.0f};
	const float start_scale[3] = {1.0f, 1.0f, 1.0f};
	const float start_rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	// axis drag (handle 0 = X arrow): the mouse ray slides its closest
	// point along the axis; +2 on the axis must move ONLY x
	{
		no_streaming::zircon_render_pass_gizmo_drag_context_t context{};

		const float ray_start_origin[3] = {0.5f, -3.0f, 0.0f};
		const float ray_direction[3] = {0.0f, 1.0f, 0.0f};

		zircon_pass_editor_gizmo_own::begin_drag(context,
			&zircon_pass_editor_gizmo_own::get_handles()[0],
			ray_start_origin, ray_direction, camera_position,
			gizmo_origin, 1.0f, start_position, start_scale,
			start_rotation);

		const float ray_now_origin[3] = {2.5f, -3.0f, 0.0f};

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_now_origin, ray_direction, false);

		EXPECT_NEAR(context.m_result_position[0], 12.0f, 0.0001f);
		EXPECT_NEAR(context.m_result_position[1], 20.0f, 0.0001f);
		EXPECT_NEAR(context.m_result_position[2], 30.0f, 0.0001f);
		EXPECT_NEAR(context.m_delta[0], 2.0f, 0.0001f);
		EXPECT_NEAR(context.m_delta[1], 0.0f, 0.0001f);
		EXPECT_NEAR(context.m_delta[2], 0.0f, 0.0001f);

		// the scale/rotation results pass the start state through
		// untouched
		EXPECT_NEAR(context.m_result_scale[0], 1.0f, 0.0001f);
		EXPECT_NEAR(context.m_result_rotation[3], 1.0f, 0.0001f);

		// snapping on: a 2.3 drag lands on the 0.25 grid
		const float ray_snap_origin[3] = {2.8f, -3.0f, 0.0f};

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_snap_origin, ray_direction, true);

		EXPECT_NEAR(context.m_result_position[0], 12.25f, 0.0001f);
		EXPECT_NEAR(context.m_delta[0], 2.25f, 0.0001f);
	}

	// plane drag (handle 3 = XY quad): both spanned components move, the
	// normal component stays
	{
		no_streaming::zircon_render_pass_gizmo_drag_context_t context{};

		const float ray_start_origin[3] = {0.5f, 0.5f, 5.0f};
		const float ray_direction[3] = {0.0f, 0.0f, -1.0f};

		zircon_pass_editor_gizmo_own::begin_drag(context,
			&zircon_pass_editor_gizmo_own::get_handles()[3],
			ray_start_origin, ray_direction, camera_position,
			gizmo_origin, 1.0f, start_position, start_scale,
			start_rotation);

		const float ray_now_origin[3] = {1.6f, 0.8f, 5.0f};

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_now_origin, ray_direction, false);

		EXPECT_NEAR(context.m_result_position[0], 11.1f, 0.0001f);
		EXPECT_NEAR(context.m_result_position[1], 20.3f, 0.0001f);
		EXPECT_NEAR(context.m_result_position[2], 30.0f, 0.0001f);

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_now_origin, ray_direction, true);

		EXPECT_NEAR(context.m_result_position[0], 11.0f, 0.0001f);
		EXPECT_NEAR(context.m_result_position[1], 20.25f, 0.0001f);
		EXPECT_NEAR(context.m_result_position[2], 30.0f, 0.0001f);
	}
}

TEST(Zircon_Editor, RenderPassEditorGizmoOwnDragRotate)
{
	const float gizmo_origin[3] = {0.0f, 0.0f, 0.0f};
	const float camera_position[3] = {4.0f, 3.0f, 4.0f};
	const float start_position[3] = {0.0f, 0.0f, 0.0f};
	const float start_scale[3] = {1.0f, 1.0f, 1.0f};

	// ring Z (handle 9): grabbing the ring at +X and moving the grab
	// point to +Y is a +90-degree rotation about +Z (right-hand rule)
	{
		no_streaming::zircon_render_pass_gizmo_drag_context_t context{};

		const float ray_start_origin[3] = {1.0f, 0.0f, 5.0f};
		const float ray_now_origin[3] = {0.0f, 1.0f, 5.0f};
		const float ray_direction[3] = {0.0f, 0.0f, -1.0f};
		const float start_rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};

		zircon_pass_editor_gizmo_own::begin_drag(context,
			&zircon_pass_editor_gizmo_own::get_handles()[9],
			ray_start_origin, ray_direction, camera_position,
			gizmo_origin, 1.0f, start_position, start_scale,
			start_rotation);

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_now_origin, ray_direction, false);

		constexpr float _kHalfSqrt2 = 0.70710678f;

		EXPECT_NEAR(context.m_result_rotation[0], 0.0f, 0.0001f);
		EXPECT_NEAR(context.m_result_rotation[1], 0.0f, 0.0001f);
		EXPECT_NEAR(context.m_result_rotation[2], _kHalfSqrt2, 0.0001f);
		EXPECT_NEAR(context.m_result_rotation[3], _kHalfSqrt2, 0.0001f);
		EXPECT_NEAR(context.m_delta[2], 90.0f, 0.001f);

		// 20 degrees snaps down to 15 on the 15-degree grid
		const float ray_snap_origin[3] = {0.9396926f, 0.3420201f, 5.0f};

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_snap_origin, ray_direction, true);

		EXPECT_NEAR(context.m_delta[2], 15.0f, 0.001f);
		EXPECT_NEAR(context.m_result_rotation[2], 0.13052619f, 0.0001f);
		EXPECT_NEAR(context.m_result_rotation[3], 0.99144486f, 0.0001f);
	}

	// composition order: a +90-degree Z drag on a start rotation of +90
	// about X must give the Hamilton product qd * start = (0.5, 0.5,
	// 0.5, 0.5) — the drag applies AFTER the start orientation
	{
		no_streaming::zircon_render_pass_gizmo_drag_context_t context{};

		const float ray_start_origin[3] = {1.0f, 0.0f, 5.0f};
		const float ray_now_origin[3] = {0.0f, 1.0f, 5.0f};
		const float ray_direction[3] = {0.0f, 0.0f, -1.0f};
		const float start_rotation[4] = {0.70710678f, 0.0f, 0.0f,
			0.70710678f};

		zircon_pass_editor_gizmo_own::begin_drag(context,
			&zircon_pass_editor_gizmo_own::get_handles()[9],
			ray_start_origin, ray_direction, camera_position,
			gizmo_origin, 1.0f, start_position, start_scale,
			start_rotation);

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_now_origin, ray_direction, false);

		EXPECT_NEAR(context.m_result_rotation[0], 0.5f, 0.0001f);
		EXPECT_NEAR(context.m_result_rotation[1], 0.5f, 0.0001f);
		EXPECT_NEAR(context.m_result_rotation[2], 0.5f, 0.0001f);
		EXPECT_NEAR(context.m_result_rotation[3], 0.5f, 0.0001f);
	}
}

TEST(Zircon_Editor, RenderPassEditorGizmoOwnDragScale)
{
	const float gizmo_origin[3] = {0.0f, 0.0f, 0.0f};
	const float camera_position[3] = {4.0f, 3.0f, 4.0f};
	const float start_position[3] = {0.0f, 0.0f, 0.0f};
	const float start_scale[3] = {1.0f, 1.0f, 1.0f};
	const float start_rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	// axis scale (handle 10 = X shaft+cube): only the dragged axis's
	// scale component moves; the minimum clamp holds
	{
		no_streaming::zircon_render_pass_gizmo_drag_context_t context{};

		const float ray_start_origin[3] = {1.0f, -3.0f, 0.0f};
		const float ray_direction[3] = {0.0f, 1.0f, 0.0f};

		zircon_pass_editor_gizmo_own::begin_drag(context,
			&zircon_pass_editor_gizmo_own::get_handles()[10],
			ray_start_origin, ray_direction, camera_position,
			gizmo_origin, 1.0f, start_position, start_scale,
			start_rotation);

		const float ray_now_origin[3] = {1.6f, -3.0f, 0.0f};

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_now_origin, ray_direction, false);

		EXPECT_NEAR(context.m_result_scale[0], 1.6f, 0.0001f);
		EXPECT_NEAR(context.m_result_scale[1], 1.0f, 0.0001f);
		EXPECT_NEAR(context.m_result_scale[2], 1.0f, 0.0001f);

		// snapping on the 0.1 grid: 0.64 -> 0.6
		const float ray_snap_origin[3] = {1.64f, -3.0f, 0.0f};

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_snap_origin, ray_direction, true);

		EXPECT_NEAR(context.m_result_scale[0], 1.6f, 0.0001f);

		// dragging past zero clamps at the minimum scale
		const float ray_clamp_origin[3] = {-2.0f, -3.0f, 0.0f};

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_clamp_origin, ray_direction, false);

		EXPECT_NEAR(context.m_result_scale[0],
			zircon_DEF_RENDER_PASS_GIZMO_SCALE_MIN, 0.0001f);
	}

	// center scale (handle 13): the ray-to-gizmo distance ratio scales
	// every component uniformly
	{
		no_streaming::zircon_render_pass_gizmo_drag_context_t context{};

		const float ray_start_origin[3] = {0.0f, 3.0f, 0.0f};
		const float ray_direction[3] = {0.0f, 0.0f, -1.0f};

		zircon_pass_editor_gizmo_own::begin_drag(context,
			&zircon_pass_editor_gizmo_own::get_handles()[13],
			ray_start_origin, ray_direction, camera_position,
			gizmo_origin, 1.0f, start_position, start_scale,
			start_rotation);

		const float ray_now_origin[3] = {0.0f, 6.0f, 0.0f};

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_now_origin, ray_direction, false);

		EXPECT_NEAR(context.m_result_scale[0], 2.0f, 0.0001f);
		EXPECT_NEAR(context.m_result_scale[1], 2.0f, 0.0001f);
		EXPECT_NEAR(context.m_result_scale[2], 2.0f, 0.0001f);

		// snapping: a 0.5333... delta rounds to 0.5 on the 0.1 grid
		const float ray_snap_origin[3] = {0.0f, 4.6f, 0.0f};

		zircon_pass_editor_gizmo_own::apply_drag(
			context, ray_snap_origin, ray_direction, true);

		EXPECT_NEAR(context.m_result_scale[0], 1.5f, 0.0001f);
		EXPECT_NEAR(context.m_result_scale[1], 1.5f, 0.0001f);
		EXPECT_NEAR(context.m_result_scale[2], 1.5f, 0.0001f);
	}
}

TEST(Zircon_Editor, RenderPassEditorGizmoOwnClickSelect)
{
	// same heap-allocated headless world the collect-draw-items test
	// uses (the console alone is ~1 MB of stack)
	zircon_test_model_static_env& env = *new zircon_test_model_static_env();

	env.factory.Initialize(&env.engine_config, &env.console, &env.input);

	env.world.initialize("zircon_p2e_test_world", &env.engine_config,
		&env.console, &env.input, &env.factory,
		ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT);

	zircon_ecs_context_t* p_context = env.world.get_ecs_context();

	// entity A: bare transform at (10,0,0) — the fixed 0.5 m fallback
	// sphere
	kotek::entity_t entity_a = env.factory.create_entity(p_context);

	env.factory.create_component(p_context, entity_a,
		eZirconComponentType::kzircon_component_transform);

	zircon_component_transform* p_transform_a =
		static_cast<zircon_component_transform*>(
			env.factory.get_component_by_enum(p_context, entity_a,
				eZirconComponentType::kzircon_component_transform));

	ASSERT_NE(p_transform_a, nullptr);

	p_transform_a->set_position(kotek::math::vec3f_t(10.0f, 0.0f, 0.0f));

	// entity B: transform at (10,5,0) with a bounding sphere of radius
	// 1 — the component's sphere replaces the fallback
	kotek::entity_t entity_b = env.factory.create_entity(p_context);

	env.factory.create_component(p_context, entity_b,
		eZirconComponentType::kzircon_component_transform);
	env.factory.create_component(p_context, entity_b,
		eZirconComponentType::kzircon_component_bounding_sphere);

	zircon_component_transform* p_transform_b =
		static_cast<zircon_component_transform*>(
			env.factory.get_component_by_enum(p_context, entity_b,
				eZirconComponentType::kzircon_component_transform));

	zircon_component_bounding_sphere* p_sphere_b =
		static_cast<zircon_component_bounding_sphere*>(
			env.factory.get_component_by_enum(p_context, entity_b,
				eZirconComponentType::kzircon_component_bounding_sphere));

	ASSERT_NE(p_transform_b, nullptr);
	ASSERT_NE(p_sphere_b, nullptr);

	p_transform_b->set_position(kotek::math::vec3f_t(10.0f, 5.0f, 0.0f));
	p_sphere_b->set_radius(1.0f);
	p_sphere_b->set_enabled(true);

	kotek::entity_t picked{kotek::ktk::kInvalidECSEntity};
	const float ray_direction[3] = {0.0f, 0.0f, -1.0f};

	// a ray straight at A picks A (and not B further along the ray —
	// nearest wins... here B is beside the ray, out of reach)
	{
		const float ray_origin[3] = {10.0f, 0.0f, 10.0f};

		ASSERT_TRUE(zircon_pass_editor_gizmo_own::pick_entity(&env.factory,
			p_context, env.world.get_entity_count_max_limit(), ray_origin,
			ray_direction, &picked));

		EXPECT_EQ(picked.id, entity_a.id);
	}

	// a ray 0.8 m off B's center misses A entirely and picks B through
	// its bounding sphere (the fallback 0.5 would be too small... the
	// radius comes from the component)
	{
		const float ray_origin[3] = {10.0f, 4.2f, 10.0f};

		ASSERT_TRUE(zircon_pass_editor_gizmo_own::pick_entity(&env.factory,
			p_context, env.world.get_entity_count_max_limit(), ray_origin,
			ray_direction, &picked));

		EXPECT_EQ(picked.id, entity_b.id);
	}

	// 0.6 m off A's center: outside the fallback 0.5 m sphere, outside
	// B's — a clean miss (the caller deselects)
	{
		const float ray_origin[3] = {10.0f, 0.6f, 10.0f};

		EXPECT_FALSE(zircon_pass_editor_gizmo_own::pick_entity(
			&env.factory, p_context,
			env.world.get_entity_count_max_limit(), ray_origin,
			ray_direction, &picked));
	}

	env.world.shutdown(&env.factory);
	env.factory.Shutdown();

	delete &env;
}

		#endif
	#endif
#endif
