#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cfloat>
		#include <cmath>
		#include <cstring>

		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_light.h"
		#include "../../ecs/zircon_component_transform.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_light_buffer.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_shadow_cascade.h"
		#include "../../world/zircon_world.h"

		#ifndef ZIRCON_DEF_UNIT_TEST_SHADOW
			#define ZIRCON_DEF_UNIT_TEST_SHADOW 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_SHADOW == 1

// functional proofs for task Z24 B5c (the CSM sun shadows): the
// shadow-caster flag's component/json/collection path + the first-wins
// resolution, the practical split scheme (the lambda blend pinned), the
// perspective near/far extraction (incl. the graceful rejection of the
// GLM-convention camera-component projection — the documented v1 limit:
// only the bx-convention (row-vector, [0,1] depth) camera feeds the
// cascades this phase), the slice-corner fit on a known camera, the
// texel snap, the world->light->texel transform, the cascade selection
// by view depth, the 3x3 PCF kernel mirror and the no-caster factor-1.0
// contract (the pre-B5c output byte-identical). The GPU side (the depth
// array render + the FS sampling) is boot evidence — the B1/B5b
// discipline. Tier: lightweight (rule 8a).

namespace
{
	// the headless ecs environment (the light/terrain fixtures' shape):
	// a real factory and world, no imgui, no window. Heap per the fixture
	// rule (the console alone is ~1 MB of stack)
	struct zircon_shadow_ecs_env
	{
		kotek::core::ktkConsole console;
		kotek::core::ktkInput input;
		zircon_config engine_config;
		zircon_factory factory;
		zircon_world world;

		void initialize(void)
		{
			this->factory.Initialize(
				&this->engine_config, &this->console, &this->input);

			this->world.initialize("zircon_b5c_test_world",
				&this->engine_config, &this->console, &this->input,
				&this->factory, 65536);
		}

		void shutdown(void)
		{
			this->world.shutdown(&this->factory);
			this->factory.Shutdown();
		}
	};

	// the hand-built standard row-vector perspective with [0,1] depth
	// (the bx::mtxProj shape — the pass's default orbit builds it): fov
	// 90, aspect 1, the given near/far. m[0] = m[5] = 1 (tan(45) = 1),
	// m[10] = f/(f-n), m[11] = 1 (the w producer), m[14] = -n*f/(f-n),
	// m[15] = 0
	void shadow_test_perspective(float near_z, float far_z,
		float* p_out_projection_16)
	{
		std::memset(p_out_projection_16, 0, 16 * sizeof(float));

		p_out_projection_16[0] = 1.0f;
		p_out_projection_16[5] = 1.0f;
		p_out_projection_16[10] = far_z / (far_z - near_z);
		p_out_projection_16[11] = 1.0f;
		p_out_projection_16[14] = -near_z * far_z / (far_z - near_z);
	}

	void shadow_test_identity_matrix(float* p_out_16)
	{
		std::memset(p_out_16, 0, 16 * sizeof(float));
		p_out_16[0] = 1.0f;
		p_out_16[5] = 1.0f;
		p_out_16[10] = 1.0f;
		p_out_16[15] = 1.0f;
	}

	// one light entity carrying a configured light component (the
	// collection fixture's shorthand)
	void shadow_test_add_light(zircon_shadow_ecs_env& env,
		eZirconLightType type, bool casts_shadow)
	{
		kotek::entity_t entity =
			env.factory.create_entity(env.world.get_ecs_context());

		ASSERT_TRUE(env.factory.create_component(
			env.world.get_ecs_context(), entity,
			eZirconComponentType::kzircon_component_light));

		zircon_component_light* p_light =
			static_cast<zircon_component_light*>(
				env.factory.get_component_by_enum(
					env.world.get_ecs_context(), entity,
					eZirconComponentType::kzircon_component_light));

		ASSERT_TRUE(p_light != nullptr);

		p_light->set_light_type(type);
		p_light->set_shadow_caster(casts_shadow);
	}
} // namespace

// (a) the component's caster flag: the default is ON (the sun's job), the
// setter round-trips through json, and a json document WITHOUT the flags
// key (content written before the flag existed) keeps the default
TEST(Zircon_Game, ShadowComponentCasterFlagJson)
{
	zircon_component_light fresh;

	EXPECT_TRUE(fresh.is_shadow_caster());

	fresh.set_shadow_caster(false);

	EXPECT_FALSE(fresh.is_shadow_caster());

	// the json roundtrip carries it
	{
		zircon_component_light configured;
		configured.set_light_type(eZirconLightType::kDirectional);
		configured.set_shadow_caster(false);

		kotek::ktk::json::value serialized =
			kotek::ktk::json::value_from(configured);

		zircon_component_light restored =
			kotek::ktk::json::value_to<zircon_component_light>(serialized);

		EXPECT_FALSE(restored.is_shadow_caster());
		EXPECT_TRUE(restored.get_light_type() ==
			eZirconLightType::kDirectional);
	}

	// the absent key keeps the default (the B5b-era content contract)
	{
		zircon_component_light configured;
		configured.set_shadow_caster(false);

		kotek::ktk::json::value serialized =
			kotek::ktk::json::value_from(configured);

		serialized.as_object().erase(
			ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_FLAGS);

		zircon_component_light restored =
			kotek::ktk::json::value_to<zircon_component_light>(serialized);

		EXPECT_TRUE(restored.is_shadow_caster());
	}
}

// (b) the caster resolution over the collected table: only a DIRECTIONAL
// with the flag qualifies; the first wins; none -> kNoShadowCaster (the
// factor-1.0 frame)
TEST(Zircon_Game, ShadowCasterResolution)
{
	zircon_render_light_buffer lights;

	const float color[3] = {1.0f, 1.0f, 1.0f};
	const float dir[3] = {0.0f, -1.0f, 0.0f};

	// the empty table: no caster
	EXPECT_EQ(lights.find_shadow_caster(),
		zircon_render_light_buffer::kNoShadowCaster);

	// a point light WITH the flag never qualifies (the B5c contract: the
	// sun only — spot/point atlases are the deferred phase)
	lights.append_light(eZirconLightType::kPoint, nullptr, nullptr, color,
		1.0f, 10.0f, 0.0f, 0.0f,
		zircon_render_light_buffer::kLightFlagCastsShadow);

	EXPECT_EQ(lights.find_shadow_caster(),
		zircon_render_light_buffer::kNoShadowCaster);

	// a directional WITHOUT the flag never qualifies
	lights.clear();
	lights.append_light(eZirconLightType::kDirectional, nullptr, dir, color,
		1.0f, 0.0f, 0.0f, 0.0f, 0);

	EXPECT_EQ(lights.find_shadow_caster(),
		zircon_render_light_buffer::kNoShadowCaster);

	// one qualifying directional -> its table index
	lights.clear();
	lights.append_light(eZirconLightType::kPoint, nullptr, nullptr, color,
		1.0f, 10.0f, 0.0f, 0.0f, 0);
	lights.append_light(eZirconLightType::kDirectional, nullptr, dir, color,
		1.0f, 0.0f, 0.0f, 0.0f,
		zircon_render_light_buffer::kLightFlagCastsShadow);

	EXPECT_EQ(lights.find_shadow_caster(), 1u);

	// two qualifying directionals -> the FIRST wins (the multi-caster
	// note is a log, not a state change)
	lights.clear();
	lights.append_light(eZirconLightType::kDirectional, nullptr, dir, color,
		1.0f, 0.0f, 0.0f, 0.0f,
		zircon_render_light_buffer::kLightFlagCastsShadow);
	lights.append_light(eZirconLightType::kDirectional, nullptr, dir, color,
		1.0f, 0.0f, 0.0f, 0.0f,
		zircon_render_light_buffer::kLightFlagCastsShadow);

	EXPECT_EQ(lights.find_shadow_caster(), 0u);
}

// (c) the collection maps the component's flag into the table's narrow
// flags stream (the B5b pre-wire gone live), through a REAL world
TEST(Zircon_Game, ShadowCollectionMapsCasterFlag)
{
	zircon_shadow_ecs_env& env = *new zircon_shadow_ecs_env();
	env.initialize();

	// a directional caster, a directional NON-caster, a point caster
	// (ignored by the resolution)
	shadow_test_add_light(
		env, eZirconLightType::kDirectional, true);
	shadow_test_add_light(
		env, eZirconLightType::kDirectional, false);
	shadow_test_add_light(env, eZirconLightType::kPoint, true);

	zircon_render_light_buffer lights;

	ASSERT_EQ(lights.collect_from_world(&env.factory,
				  env.world.get_ecs_context(),
				  env.world.get_entity_count_max_limit()),
		3u);

	const kotek::uint8_t* p_flags = lights.get_flags();

	EXPECT_TRUE(
		(p_flags[0] & zircon_render_light_buffer::kLightFlagCastsShadow) !=
		0);
	EXPECT_TRUE(
		(p_flags[1] & zircon_render_light_buffer::kLightFlagCastsShadow) ==
		0);
	EXPECT_TRUE(
		(p_flags[2] & zircon_render_light_buffer::kLightFlagCastsShadow) !=
		0);

	// the resolution picks the FIRST directional caster (index 0), the
	// point's flag notwithstanding
	EXPECT_EQ(lights.find_shadow_caster(), 0u);

	env.shutdown();
	delete &env;
}

// (d) the practical split scheme: the log/uniform blend pinned on a known
// near/far with the house lambda, plus the two pure endpoints and the
// single-cascade degenerate
TEST(Zircon_Game, ShadowSplitDistances)
{
	// near 0.5, far 100, count 4, lambda 0.5:
	//   log_i     = 0.5 * 200^(i/4)
	//   uniform_i = 0.5 + 99.5 * i/4
	//   split_i   = (log_i + uniform_i) / 2
	float splits[5];

	zircon_render_shadow_cascade::compute_split_distances(
		0.5f, 100.0f, 4, 0.5f, splits);

	EXPECT_FLOAT_EQ(splits[0], 0.5f);
	EXPECT_FLOAT_EQ(splits[4], 100.0f);

	const float expected_log_1 = 0.5f * std::pow(200.0f, 0.25f);
	const float expected_uniform_1 = 0.5f + 99.5f * 0.25f;
	const float expected_log_2 = 0.5f * std::pow(200.0f, 0.5f);
	const float expected_uniform_2 = 0.5f + 99.5f * 0.5f;
	const float expected_log_3 = 0.5f * std::pow(200.0f, 0.75f);
	const float expected_uniform_3 = 0.5f + 99.5f * 0.75f;

	EXPECT_NEAR(splits[1],
		(expected_log_1 + expected_uniform_1) * 0.5f, 1e-4f);
	EXPECT_NEAR(splits[2],
		(expected_log_2 + expected_uniform_2) * 0.5f, 1e-4f);
	EXPECT_NEAR(splits[3],
		(expected_log_3 + expected_uniform_3) * 0.5f, 1e-4f);

	// lambda 0 = the uniform scheme, lambda 1 = the log scheme
	float splits_uniform[5];
	float splits_log[5];

	zircon_render_shadow_cascade::compute_split_distances(
		0.5f, 100.0f, 4, 0.0f, splits_uniform);
	zircon_render_shadow_cascade::compute_split_distances(
		0.5f, 100.0f, 4, 1.0f, splits_log);

	EXPECT_NEAR(splits_uniform[1], expected_uniform_1, 1e-4f);
	EXPECT_NEAR(splits_log[1], expected_log_1, 1e-4f);

	// the single-cascade degenerate: [near, far] exactly
	float splits_single[2];

	zircon_render_shadow_cascade::compute_split_distances(
		0.5f, 100.0f, 1, 0.5f, splits_single);

	EXPECT_FLOAT_EQ(splits_single[0], 0.5f);
	EXPECT_FLOAT_EQ(splits_single[1], 100.0f);
}

// (e) the near/far extraction: the bx-convention perspective recovers its
// planes; the GLM-convention camera-component projection (the w producer
// at m[11] = -1, the [-1,1] depth range), an ortho and a degenerate
// matrix are REJECTED (the frame falls back, loud once — the documented
// v1 limit)
TEST(Zircon_Game, ShadowPerspectiveNearFarExtraction)
{
	float projection[16];

	shadow_test_perspective(1.0f, 100.0f, projection);

	float near_z = 0.0f;
	float far_z = 0.0f;

	ASSERT_TRUE(
		zircon_render_shadow_cascade::extract_perspective_near_far(
			projection, near_z, far_z));

	EXPECT_FLOAT_EQ(near_z, 1.0f);
	// far = e/(1-d) subtracts two near-equal floats (d = f/(f-n) ~ 1.01
	// here) — the catastrophic cancellation is inherent to the f32
	// formula: 100.00048 vs 100.0 (5 ppm, immaterial to the split math)
	EXPECT_NEAR(far_z, 100.0f, 1e-2f);

	// the GLM-convention perspective (column-vector, RH, [-1,1] depth)
	// read as bytes: m[11] = -1, m[15] = 0 -> rejected
	{
		float glm_projection[16] = {0};

		glm_projection[0] = 1.0f;
		glm_projection[5] = 1.0f;
		glm_projection[10] = -(100.0f + 1.0f) / (100.0f - 1.0f);
		glm_projection[11] = -1.0f;
		glm_projection[14] = -2.0f * 100.0f * 1.0f / (100.0f - 1.0f);

		EXPECT_FALSE(
			zircon_render_shadow_cascade::extract_perspective_near_far(
				glm_projection, near_z, far_z));
	}

	// an ortho (m[11] = 0, m[15] = 1) -> rejected
	{
		float ortho[16];

		shadow_test_identity_matrix(ortho);

		EXPECT_FALSE(
			zircon_render_shadow_cascade::extract_perspective_near_far(
				ortho, near_z, far_z));
	}

	// the zero matrix -> rejected
	{
		float zero[16] = {0};

		EXPECT_FALSE(
			zircon_render_shadow_cascade::extract_perspective_near_far(
				zero, near_z, far_z));
	}
}

// (f) the full cascade computation on a known camera: the identity view
// (eye at the origin, looking down +z), fov 90 / aspect 1 / near 1 / far
// 100, the sun straight down; count 2 at the 100-texel map. Every step is
// hand-derived (see the expectations) — the slice corners, the light
// space, the snapped windows, the VPs' corner mappings
TEST(Zircon_Game, ShadowCascadeComputationKnownCamera)
{
	float view[16];
	float projection[16];

	shadow_test_identity_matrix(view);
	shadow_test_perspective(1.0f, 100.0f, projection);

	// the sun travels straight down
	const float sun_dir[3] = {0.0f, -1.0f, 0.0f};

	zircon_shadow_cascade_set_t cascades;

	ASSERT_TRUE(zircon_render_shadow_cascade::compute_cascades(view,
		projection, sun_dir, 100 /* the map */, 2 /* the cascades */,
		0.5f /* lambda */, 50.0f /* the caster margin */, cascades));

	ASSERT_EQ(cascades.m_cascade_count, 2);

	// the splits: [1, 30.25, 100] (log 10, uniform 50.5, blended); the far
	// boundary carries the extraction's cancellation error (the test (e)
	// note: ~5 ppm)
	EXPECT_NEAR(cascades.m_split_far[0], 30.25f, 1e-2f);
	EXPECT_NEAR(cascades.m_split_far[1], 100.0f, 1e-2f);

	// the light view is the (x, z, -y) permutation (the straight-down
	// sun's frame, hand-derived) and is shared by both cascades
	for (int cascade = 0; cascade < 2; ++cascade)
	{
		const float* p_view = cascades.m_cascade_view[cascade];

		EXPECT_FLOAT_EQ(p_view[0], 1.0f); // right = +x
		EXPECT_FLOAT_EQ(p_view[4], 0.0f);
		EXPECT_FLOAT_EQ(p_view[8], 0.0f);
		EXPECT_FLOAT_EQ(p_view[1], 0.0f); // up = +z
		EXPECT_FLOAT_EQ(p_view[5], 0.0f);
		EXPECT_FLOAT_EQ(p_view[9], 1.0f);
		EXPECT_FLOAT_EQ(p_view[2], 0.0f); // forward = -y
		EXPECT_FLOAT_EQ(p_view[6], -1.0f);
		EXPECT_FLOAT_EQ(p_view[10], 0.0f);
		// the rotation-only contract: the translation row is zero (the
		// world-anchored texel grid)
		EXPECT_FLOAT_EQ(p_view[12], 0.0f);
		EXPECT_FLOAT_EQ(p_view[13], 0.0f);
		EXPECT_FLOAT_EQ(p_view[14], 0.0f);
	}

	// cascade 0: the slice [1, 30.25] -> the light-space window x
	// [-30.25, 30.25] (exact texel multiples at the 100 map: UNSNAPPED),
	// y snapped to [0.8775, 30.42] (hand-derived: texel 0.2925, floor
	// 3.4188 -> 3, ceil 103.4188 -> 104), z [-80.25, 30.25] (the 50 m
	// caster margin). The far-side corner (30.25, 30.25, 30.25) maps to
	// light space (30.25, 30.25, -30.25) -> ndc (1, ~0.988489, 50/110.5)
	{
		const float point[4] = {30.25f, 30.25f, 30.25f, 1.0f};
		float clip[4];

		zircon_render_shadow_cascade::transform_point_raw(
			cascades.m_cascade_view_proj[0], point, clip);

		EXPECT_NEAR(clip[0] / clip[3], 1.0f, 1e-3f);
		EXPECT_NEAR(clip[1] / clip[3],
			(60.5f - 31.2975f) / 29.5425f, 1e-3f);
		EXPECT_NEAR(clip[2] / clip[3], 50.0f / 110.5f, 1e-3f);
		EXPECT_FLOAT_EQ(clip[3], 1.0f);
	}

	// cascade 1: the slice [30.25, 100] -> x [-100, 100] (unsnapped), z
	// [-150, 100]; the far corner (100, 100, 100) -> light space
	// (100, 100, -100) -> ndc (1, ~0.987504, 0.2)
	{
		const float point[4] = {100.0f, 100.0f, 100.0f, 1.0f};
		float clip[4];

		zircon_render_shadow_cascade::transform_point_raw(
			cascades.m_cascade_view_proj[1], point, clip);

		EXPECT_NEAR(clip[0] / clip[3], 1.0f, 1e-3f);
		EXPECT_NEAR(clip[2] / clip[3], 0.2f, 1e-3f);
	}

	// the rejection contract: a zero light direction, an ortho camera
	// and an over-cap cascade count all answer false (the caller falls
	// back, never a fault)
	{
		const float zero_dir[3] = {0.0f, 0.0f, 0.0f};

		EXPECT_FALSE(zircon_render_shadow_cascade::compute_cascades(view,
			projection, zero_dir, 100, 2, 0.5f, 50.0f, cascades));

		float ortho[16];

		shadow_test_identity_matrix(ortho);

		EXPECT_FALSE(zircon_render_shadow_cascade::compute_cascades(view,
			ortho, sun_dir, 100, 2, 0.5f, 50.0f, cascades));

		EXPECT_FALSE(zircon_render_shadow_cascade::compute_cascades(view,
			projection, sun_dir, 100,
			ZIRCON_DEF_SHADOW_CASCADE_COUNT + 1, 0.5f, 50.0f, cascades));
	}
}

// (g) the texel snap: min floors / max ceils to the current texel grid
// (coverage preserved), exact-multiple windows stay put, a degenerate
// axis widens first
TEST(Zircon_Game, ShadowTexelSnapping)
{
	// window [1.3, 7.7] at the 10-texel map: texel 0.64 -> [2*0.64,
	// 13*0.64] = [1.28, 8.32]
	{
		float min_x = 1.3f;
		float max_x = 7.7f;
		float min_y = -2.0f;
		float max_y = 2.0f;

		zircon_render_shadow_cascade::snap_ortho_to_texels(
			min_x, max_x, min_y, max_y, 10);

		EXPECT_FLOAT_EQ(min_x, 1.28f);
		EXPECT_FLOAT_EQ(max_x, 8.32f);
		// the y window is exactly 10 texels: the snap never moves it
		EXPECT_FLOAT_EQ(min_y, -2.0f);
		EXPECT_FLOAT_EQ(max_y, 2.0f);
	}

	// the degenerate axis widens to one world unit, then snaps
	{
		float min_x = 3.0f;
		float max_x = 3.0f;
		float min_y = 0.0f;
		float max_y = 1.0f;

		zircon_render_shadow_cascade::snap_ortho_to_texels(
			min_x, max_x, min_y, max_y, 10);

		EXPECT_FLOAT_EQ(min_x, 2.5f);
		EXPECT_FLOAT_EQ(max_x, 3.5f);
	}
}

// (h) the small matrix kit: multiply matches the hand-computed product,
// transform_point_raw is the documented clip formula, invert round-trips
// a known VP and rejects the singular matrix
TEST(Zircon_Game, ShadowMatrixKit)
{
	// A = the straight-down light view (the (x, z, -y) permutation), B =
	// the ortho [-1,1]x[-2,2]x[0,10]: P = A*B is hand-derived
	float light_view[16];

	const float straight_down[3] = {0.0f, -1.0f, 0.0f};

	ASSERT_TRUE(zircon_render_shadow_cascade::build_light_view(
		straight_down, light_view));

	float ortho[16];

	zircon_render_shadow_cascade::build_ortho_projection(
		-1.0f, 1.0f, -2.0f, 2.0f, 0.0f, 10.0f, ortho);

	float product[16];

	zircon_render_shadow_cascade::multiply_4x4(light_view, ortho, product);

	// P rows: B row0; -B row2 x0.1... (the hand derivation): row0 =
	// (1,0,0,0), row1 = (0,0,-0.1,0), row2 = (0,0.5,0,0), row3 = (0,0,0,1)
	const float expected[16] = {1.0f, 0.0f, 0.0f, 0.0f, //
		0.0f, 0.0f, -0.1f, 0.0f,                        //
		0.0f, 0.5f, 0.0f, 0.0f,                         //
		0.0f, 0.0f, 0.0f, 1.0f};

	for (int element = 0; element < 16; ++element)
	{
		EXPECT_FLOAT_EQ(product[element], expected[element]);
	}

	// the transform through the product: (1,2,3) -> light (1,3,-2) ->
	// ortho (1, 1.5, -0.2)
	{
		const float point[4] = {1.0f, 2.0f, 3.0f, 1.0f};
		float clip[4];

		zircon_render_shadow_cascade::transform_point_raw(
			product, point, clip);

		EXPECT_FLOAT_EQ(clip[0], 1.0f);
		EXPECT_FLOAT_EQ(clip[1], 1.5f);
		EXPECT_FLOAT_EQ(clip[2], -0.2f);
		EXPECT_FLOAT_EQ(clip[3], 1.0f);
	}

	// the inverse: VP * VP^-1 = identity on the known camera; the
	// singular matrix answers false
	{
		float view[16];
		float projection[16];

		shadow_test_identity_matrix(view);
		shadow_test_perspective(1.0f, 100.0f, projection);

		float view_projection[16];

		zircon_render_shadow_cascade::multiply_4x4(
			view, projection, view_projection);

		float inverse[16];

		ASSERT_TRUE(
			zircon_render_shadow_cascade::invert_4x4(view_projection, inverse));

		float roundtrip[16];

		zircon_render_shadow_cascade::multiply_4x4(
			view_projection, inverse, roundtrip);

		for (int row = 0; row < 4; ++row)
		{
			for (int column = 0; column < 4; ++column)
			{
				EXPECT_NEAR(roundtrip[row * 4 + column],
					row == column ? 1.0f : 0.0f, 1e-3f);
			}
		}

		float zero[16] = {0};
		float inverse_out[16];

		EXPECT_FALSE(
			zircon_render_shadow_cascade::invert_4x4(zero, inverse_out));
	}

	// the camera frame: the identity view sits at the origin looking down
	// +z; a translation-only view at (5,6,7) recovers its eye (the last
	// row holds -dot(eye, basis))
	{
		float identity[16];
		shadow_test_identity_matrix(identity);

		float eye[3];
		float forward[3];

		zircon_render_shadow_cascade::extract_camera_frame(
			identity, eye, forward);

		EXPECT_FLOAT_EQ(eye[0], 0.0f);
		EXPECT_FLOAT_EQ(eye[1], 0.0f);
		EXPECT_FLOAT_EQ(eye[2], 0.0f);
		EXPECT_FLOAT_EQ(forward[2], 1.0f);

		float translated[16];
		shadow_test_identity_matrix(translated);
		translated[12] = -5.0f;
		translated[13] = -6.0f;
		translated[14] = -7.0f;

		zircon_render_shadow_cascade::extract_camera_frame(
			translated, eye, forward);

		EXPECT_FLOAT_EQ(eye[0], 5.0f);
		EXPECT_FLOAT_EQ(eye[1], 6.0f);
		EXPECT_FLOAT_EQ(eye[2], 7.0f);
	}
}

// (i) the frustum corners through the inverse VP: the known camera's
// near corners are (+-1, +-1, 1), the far corners (+-100, +-100, 100) —
// the fov-90/aspect-1 geometry by hand
TEST(Zircon_Game, ShadowFrustumCorners)
{
	float view[16];
	float projection[16];

	shadow_test_identity_matrix(view);
	shadow_test_perspective(1.0f, 100.0f, projection);

	float corners[8 * 3];

	ASSERT_TRUE(zircon_render_shadow_cascade::compute_frustum_corners(
		view, projection, corners));

	// the documented order: near (z = 0 ndc) at (x,y) = (-1,-1), (1,-1),
	// (1,1), (-1,1), then the same four at the far plane
	const float expected_x[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
	const float expected_y[4] = {-1.0f, -1.0f, 1.0f, 1.0f};

	for (int corner = 0; corner < 8; ++corner)
	{
		const float plane = corner < 4 ? 1.0f : 100.0f;

		EXPECT_NEAR(corners[corner * 3 + 0], expected_x[corner & 3] * plane,
			plane * 1e-3f);
		EXPECT_NEAR(corners[corner * 3 + 1], expected_y[corner & 3] * plane,
			plane * 1e-3f);
		EXPECT_NEAR(corners[corner * 3 + 2], plane, plane * 1e-3f);
	}
}

// (j) the world->light->texel transform: the identity-box ortho pins the
// uv/texel mapping incl. the pixel-rows-down y flip; a point behind the
// light eye answers false
TEST(Zircon_Game, ShadowWorldToLightTexel)
{
	float ortho[16];

	zircon_render_shadow_cascade::build_ortho_projection(
		-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, ortho);

	float texel[2];
	float depth = 0.0f;

	// (0.5, -0.5, 0.25) -> ndc (0.5, -0.5, 0.25) -> uv (0.75, 0.75) ->
	// texel (12, 12) at the 16-texel map
	const float point_a[3] = {0.5f, -0.5f, 0.25f};

	ASSERT_TRUE(zircon_render_shadow_cascade::world_to_light_texel(ortho,
		point_a, 16, texel, depth));

	EXPECT_FLOAT_EQ(texel[0], 12.0f);
	EXPECT_FLOAT_EQ(texel[1], 12.0f);
	EXPECT_FLOAT_EQ(depth, 0.25f);

	// the y flip: ndc y = +1 is texture row 0
	const float point_b[3] = {0.0f, 1.0f, 0.5f};

	ASSERT_TRUE(zircon_render_shadow_cascade::world_to_light_texel(ortho,
		point_b, 16, texel, depth));

	EXPECT_FLOAT_EQ(texel[1], 0.0f);

	// a point behind the light eye of a perspective VP: w <= 0 -> false
	float view[16];
	float projection[16];

	shadow_test_identity_matrix(view);
	shadow_test_perspective(1.0f, 100.0f, projection);

	float view_projection[16];

	zircon_render_shadow_cascade::multiply_4x4(
		view, projection, view_projection);

	const float point_behind[3] = {0.0f, 0.0f, -1.0f};

	EXPECT_FALSE(zircon_render_shadow_cascade::world_to_light_texel(
		view_projection, point_behind, 16, texel, depth));
}

// (k) the cascade selection by view depth (the FS's per-fragment branch):
// the first cascade whose far boundary is >= view_z; boundaries are
// inclusive-low; past the last = cascade_count (the factor-1.0 answer)
TEST(Zircon_Game, ShadowCascadeSelectionByViewDepth)
{
	const float split_far[4] = {10.0f, 30.0f, 60.0f, 100.0f};

	EXPECT_EQ(zircon_render_shadow_cascade::select_cascade(5.0f, split_far, 4),
		0);
	EXPECT_EQ(
		zircon_render_shadow_cascade::select_cascade(10.0f, split_far, 4),
		0);
	EXPECT_EQ(
		zircon_render_shadow_cascade::select_cascade(10.01f, split_far, 4),
		1);
	EXPECT_EQ(
		zircon_render_shadow_cascade::select_cascade(30.0f, split_far, 4),
		1);
	EXPECT_EQ(
		zircon_render_shadow_cascade::select_cascade(59.999f, split_far, 4),
		2);
	EXPECT_EQ(
		zircon_render_shadow_cascade::select_cascade(100.0f, split_far, 4),
		3);
	// past the last cascade: out of range (the FS lights the fragment)
	EXPECT_EQ(
		zircon_render_shadow_cascade::select_cascade(100.01f, split_far, 4),
		4);
}

// (l) the 3x3 PCF kernel mirror: a known depth layer + a known receiver
// -> the exact lit average, the edge clamp's tap weights, the all-lit
// (exactly 1.0) and all-shadowed (exactly 0.0) extremes and the bias's
// direction
TEST(Zircon_Game, ShadowPcfKernelMirror)
{
	// the 4x4 layer: all 0.5 except texel (0,0) = 0.1
	float layer[16];

	for (int texel = 0; texel < 16; ++texel)
		layer[texel] = 0.5f;

	layer[0] = 0.1f;

	// receiver 0.3, bias 0.01: receiver - bias = 0.29 — lit where the
	// stored 0.5 >= 0.29, shadowed at (0,0) (0.1 < 0.29)
	//
	// texel (1.5, 1.5): the taps cover (0..2, 0..2) — only (0,0) is
	// dark -> 8/9
	EXPECT_FLOAT_EQ(
		zircon_render_shadow_cascade::sample_shadow_pcf_3x3(
			layer, 4, 1.5f, 1.5f, 0.3f, 0.01f),
		8.0f / 9.0f);

	// texel (0.4, 0.4): the base (0,0) with the edge clamp — the tap
	// indices are columns {0,0,1} x rows {0,0,1}, so (0,0) is hit FOUR
	// times, (1,0) and (0,1) twice, (1,1) once -> 4 dark, 5 lit -> 5/9
	EXPECT_FLOAT_EQ(
		zircon_render_shadow_cascade::sample_shadow_pcf_3x3(
			layer, 4, 0.4f, 0.4f, 0.3f, 0.01f),
		5.0f / 9.0f);

	// the extremes: an all-lit receiver (0.1 - 0.01 <= the darkest texel
	// 0.1) -> EXACTLY 1.0 (the byte-identical factor), an all-shadowed
	// one (0.9 - 0.01 > the lightest 0.5) -> exactly 0.0
	EXPECT_FLOAT_EQ(
		zircon_render_shadow_cascade::sample_shadow_pcf_3x3(
			layer, 4, 1.5f, 1.5f, 0.1f, 0.01f),
		1.0f);
	EXPECT_FLOAT_EQ(
		zircon_render_shadow_cascade::sample_shadow_pcf_3x3(
			layer, 4, 1.5f, 1.5f, 0.9f, 0.01f),
		0.0f);

	// the bias's direction: a receiver exactly bias-deep behind the
	// stored depth stays lit (receiver - bias == stored)
	EXPECT_FLOAT_EQ(
		zircon_render_shadow_cascade::compare_shadow_depth(0.5f, 0.51f, 0.01f),
		1.0f);
	EXPECT_FLOAT_EQ(
		zircon_render_shadow_cascade::compare_shadow_depth(
			0.5f, 0.51f + 1e-4f, 0.01f),
		0.0f);
}

// (m) the no-caster fallback contract: no caster in the world -> the
// resolution answers kNoShadowCaster (the pass then drives the rig
// inactive, meta = 0, the FS's factor branch never runs — the pre-B5c
// output byte-identical); the math half: the factor-1.0 multiply is
// bit-exact
TEST(Zircon_Game, ShadowNoCasterFallbackContract)
{
	zircon_shadow_ecs_env& env = *new zircon_shadow_ecs_env();
	env.initialize();

	// lights but no caster: a point and a non-casting directional
	shadow_test_add_light(env, eZirconLightType::kPoint, true);
	shadow_test_add_light(env, eZirconLightType::kDirectional, false);

	zircon_render_light_buffer lights;

	ASSERT_EQ(lights.collect_from_world(&env.factory,
				  env.world.get_ecs_context(),
				  env.world.get_entity_count_max_limit()),
		2u);

	EXPECT_EQ(lights.find_shadow_caster(),
		zircon_render_light_buffer::kNoShadowCaster);

	// the byte-identical claim's arithmetic half: factor 1.0 leaves any
	// finite value bit-exact
	const float samples[5] = {0.0f, 0.25f, 1.0f, 3.14159f, 1e30f};

	for (float sample : samples)
	{
		const float shadowed = sample * 1.0f;

		EXPECT_TRUE(std::memcmp(&shadowed, &sample, sizeof(float)) == 0);
	}

	env.shutdown();
	delete &env;
}

		#endif // ZIRCON_DEF_UNIT_TEST_SHADOW
	#endif // KOTEK_DEBUG
#endif // KOTEK_USE_TESTS_RUNTIME
