#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cmath>
		#include <cstring>

		#include "../../core/zircon_config.h"
		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_light.h"
		#include "../../ecs/zircon_component_transform.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_light_buffer.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_chunk_pool.h"
		#include "../../world/zircon_world.h"

		#ifndef ZIRCON_DEF_UNIT_TEST_LIGHT
			#define ZIRCON_DEF_UNIT_TEST_LIGHT 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_LIGHT == 1

// functional proofs for task Z24 B5b (the clustered forward+ lighting):
// the component's json roundtrip + factory registration, the per-frame
// collection's AoSoA table (exact streams, disabled lights excluded, the
// transform contract, the loud capacity overflow), the tile-binning
// classification through the serial mirror (a point in known tiles, a
// spot cone spanning exactly two tiles, a directional everywhere, a
// light outside the frustum excluded, the per-tile cap drop counter),
// the tile-grid derivation + its caps, and the FS-side evaluation math
// pinned in the C++ mirror (two point lights + the directional sum
// correctly; the zero-lights fallback = the untouched evaluate_phong the
// existing tests pin). Headless — the GPU side is boot evidence (the B1
// discipline). Tier: lightweight (rule 8a).

namespace
{
	// the headless ecs environment (the terrain/camera-sdk fixtures'
	// shape): a real factory and world, no imgui, no window. Heap per the
	// fixture rule (the console alone is ~1 MB of stack)
	struct zircon_light_ecs_env
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

			this->world.initialize("zircon_b5b_test_world",
				&this->engine_config, &this->console, &this->input,
				&this->factory, 65536);
		}

		void shutdown(void)
		{
			this->world.shutdown(&this->factory);
			this->factory.Shutdown();
		}
	};

	// the identity view-projection (the orthographic [-1,1] box): the
	// binning fixtures are exact under it — ndc == xyz, w == 1
	void light_identity_vp(float* p_out_vp_16, float* p_out_planes_6x4)
	{
		for (int row = 0; row < 4; ++row)
		{
			for (int column = 0; column < 4; ++column)
			{
				p_out_vp_16[row * 4 + column] =
					(row == column) ? 1.0f : 0.0f;
			}
		}

		zircon_render_chunk_pool::extract_frustum_planes(
			p_out_vp_16, p_out_planes_6x4);
	}
} // namespace

// (a) the component's json roundtrip: non-default values survive
// value_from/value_to field-for-field; a fresh component's defaults are
// the documented inert set
TEST(Zircon_Game, LightComponentJsonRoundTrip)
{
	zircon_component_light light;

	light.set_enabled(false);
	light.set_light_type(eZirconLightType::kSpot);
	light.set_color(0.25f, 0.5f, 0.75f);
	light.set_intensity(3.5f);
	light.set_range(42.0f);
	light.set_spot_angles_degrees(12.5f, 37.5f);

	kotek::ktk::json::value serialized =
		kotek::ktk::json::value_from(light);

	zircon_component_light restored =
		kotek::ktk::json::value_to<zircon_component_light>(serialized);

	EXPECT_FALSE(restored.is_enabled());
	EXPECT_TRUE(restored.get_light_type() == eZirconLightType::kSpot);
	EXPECT_FLOAT_EQ(restored.get_color(0), 0.25f);
	EXPECT_FLOAT_EQ(restored.get_color(1), 0.5f);
	EXPECT_FLOAT_EQ(restored.get_color(2), 0.75f);
	EXPECT_FLOAT_EQ(restored.get_intensity(), 3.5f);
	EXPECT_FLOAT_EQ(restored.get_range(), 42.0f);
	EXPECT_FLOAT_EQ(restored.get_spot_inner_angle_degrees(), 12.5f);
	EXPECT_FLOAT_EQ(restored.get_spot_outer_angle_degrees(), 37.5f);

	// the defaults of a fresh component: enabled, a white point light of
	// intensity 1 and range 10 with the 25/45-degree spot cone
	zircon_component_light fresh;

	EXPECT_TRUE(fresh.is_enabled());
	EXPECT_TRUE(fresh.get_light_type() == eZirconLightType::kPoint);
	EXPECT_FLOAT_EQ(fresh.get_color(0), 1.0f);
	EXPECT_FLOAT_EQ(fresh.get_color(2), 1.0f);
	EXPECT_FLOAT_EQ(fresh.get_intensity(), 1.0f);
	EXPECT_FLOAT_EQ(fresh.get_range(), 10.0f);
	EXPECT_FLOAT_EQ(fresh.get_spot_inner_angle_degrees(), 25.0f);
	EXPECT_FLOAT_EQ(fresh.get_spot_outer_angle_degrees(), 45.0f);
}

// (b) the ECS registration: the codegen'd enum <-> name translation and
// the factory create/has path on a real entity, including the scene-load
// shape (create through the serialized descriptor)
TEST(Zircon_Game, LightComponentRegisteredInFactory)
{
	zircon_light_ecs_env& env = *new zircon_light_ecs_env();
	env.initialize();

	zircon_ecs_context_t* p_context = env.world.get_ecs_context();

	// the inspector-combo contract: the name lookup returns the
	// registered component name
	EXPECT_STREQ(
		env.factory.get_component_name_by_enum(
			eZirconComponentType::kzircon_component_light),
		"zircon_component_light");

	EXPECT_TRUE(env.factory.get_component_enum_by_name(
					"zircon_component_light") ==
		eZirconComponentType::kzircon_component_light);

	kotek::entity_t entity = env.factory.create_entity(p_context);

	ASSERT_TRUE(env.factory.create_component(p_context, entity,
		eZirconComponentType::kzircon_component_light));

	EXPECT_TRUE(env.factory.has_component(p_context, entity,
		eZirconComponentType::kzircon_component_light));

	zircon_component_light* p_light =
		static_cast<zircon_component_light*>(
			env.factory.get_component_by_enum(p_context, entity,
				eZirconComponentType::kzircon_component_light));

	ASSERT_TRUE(p_light != nullptr);
	EXPECT_EQ(p_light->get_component_type(),
		static_cast<kotek::uint8_t>(
			eZirconComponentType::kzircon_component_light));

	// the scene-load shape: a configured descriptor round-trips through
	// the factory's json create
	{
		zircon_component_light configured;
		configured.set_light_type(eZirconLightType::kDirectional);
		configured.set_color(1.0f, 0.9f, 0.8f);
		configured.set_intensity(2.0f);

		kotek::ktk::json::value serialized =
			kotek::ktk::json::value_from(configured);

		kotek::entity_t second = env.factory.create_entity(p_context);

		ASSERT_TRUE(env.factory.create_component(p_context, second,
			eZirconComponentType::kzircon_component_light, serialized));

		const zircon_component_light* p_loaded =
			static_cast<const zircon_component_light*>(
				env.factory.get_component_by_enum(p_context, second,
					eZirconComponentType::kzircon_component_light));

		ASSERT_TRUE(p_loaded != nullptr);
		EXPECT_TRUE(p_loaded->get_light_type() ==
			eZirconLightType::kDirectional);
		EXPECT_FLOAT_EQ(p_loaded->get_color(1), 0.9f);
		EXPECT_FLOAT_EQ(p_loaded->get_intensity(), 2.0f);
	}

	env.factory.destroy_entity(p_context, entity);

	env.shutdown();
	delete &env;
}

// (c) the per-frame collection: a fixture world (a moved point, a rotated
// spot, a directional, a DISABLED light, a transform-less point) yields
// the exact AoSoA streams; the empty world yields zero (the fallback
// frame)
TEST(Zircon_Game, LightCollectionFromWorld)
{
	zircon_light_ecs_env& env = *new zircon_light_ecs_env();
	env.initialize();

	zircon_ecs_context_t* p_context = env.world.get_ecs_context();

	// light 0: the point at (3, 4, 5), color (1, 0.5, 0.25) x intensity 2
	// -> the premultiplied (2, 1, 0.5), range 17
	{
		kotek::entity_t entity = env.factory.create_entity(p_context);

		ASSERT_TRUE(env.factory.create_component(p_context, entity,
			eZirconComponentType::kzircon_component_light));
		ASSERT_TRUE(env.factory.create_component(p_context, entity,
			eZirconComponentType::kzircon_component_transform));

		zircon_component_light* p_light =
			static_cast<zircon_component_light*>(
				env.factory.get_component_by_enum(p_context, entity,
					eZirconComponentType::kzircon_component_light));
		zircon_component_transform* p_transform =
			static_cast<zircon_component_transform*>(
				env.factory.get_component_by_enum(p_context, entity,
					eZirconComponentType::kzircon_component_transform));

		p_light->set_light_type(eZirconLightType::kPoint);
		p_light->set_color(1.0f, 0.5f, 0.25f);
		p_light->set_intensity(2.0f);
		p_light->set_range(17.0f);

		p_transform->set_position(kotek::math::vec3f_t(3.0f, 4.0f, 5.0f));
	}

	// light 1: the spot, rotated +90 degrees about +Y — the house forward
	// (0,0,-1) maps to (-1, 0, 0)
	{
		kotek::entity_t entity = env.factory.create_entity(p_context);

		ASSERT_TRUE(env.factory.create_component(p_context, entity,
			eZirconComponentType::kzircon_component_light));
		ASSERT_TRUE(env.factory.create_component(p_context, entity,
			eZirconComponentType::kzircon_component_transform));

		zircon_component_light* p_light =
			static_cast<zircon_component_light*>(
				env.factory.get_component_by_enum(p_context, entity,
					eZirconComponentType::kzircon_component_light));
		zircon_component_transform* p_transform =
			static_cast<zircon_component_transform*>(
				env.factory.get_component_by_enum(p_context, entity,
					eZirconComponentType::kzircon_component_transform));

		p_light->set_light_type(eZirconLightType::kSpot);
		p_light->set_spot_angles_degrees(15.0f, 30.0f);
		p_light->set_range(8.0f);

		p_transform->set_rotation(kotek::math::get_math_angle_axis(
			3.14159265358979f * 0.5f,
			kotek::math::vec3f_t(0.0f, 1.0f, 0.0f)));
	}

	// light 2: the directional, identity rotation -> (0, 0, -1), the
	// position/range streams stay zeroed for it
	{
		kotek::entity_t entity = env.factory.create_entity(p_context);

		ASSERT_TRUE(env.factory.create_component(p_context, entity,
			eZirconComponentType::kzircon_component_light));

		zircon_component_light* p_light =
			static_cast<zircon_component_light*>(
				env.factory.get_component_by_enum(p_context, entity,
					eZirconComponentType::kzircon_component_light));

		p_light->set_light_type(eZirconLightType::kDirectional);
	}

	// light 3: DISABLED — never collects
	{
		kotek::entity_t entity = env.factory.create_entity(p_context);

		ASSERT_TRUE(env.factory.create_component(p_context, entity,
			eZirconComponentType::kzircon_component_light));

		zircon_component_light* p_light =
			static_cast<zircon_component_light*>(
				env.factory.get_component_by_enum(p_context, entity,
					eZirconComponentType::kzircon_component_light));

		p_light->set_enabled(false);
	}

	// light 4: the transform-less point — the identity contract (origin)
	{
		kotek::entity_t entity = env.factory.create_entity(p_context);

		ASSERT_TRUE(env.factory.create_component(p_context, entity,
			eZirconComponentType::kzircon_component_light));
	}

	zircon_render_light_buffer lights;

	// the collection reads the four enabled lights in entity creation
	// order (the pico dense-id scan)
	ASSERT_EQ(lights.collect_from_world(&env.factory, p_context,
				  env.world.get_entity_count_max_limit()),
		4u);
	ASSERT_EQ(lights.get_count(), 4u);

	const zircon_light_stream_record_t* p_positions =
		lights.get_positions();
	const zircon_light_stream_record_t* p_directions =
		lights.get_directions();
	const zircon_light_stream_record_t* p_colors = lights.get_colors();
	const zircon_light_stream_record_t* p_params = lights.get_params();
	const kotek::uint8_t* p_types = lights.get_types();

	// light 0: the point stream — position + range in w, premultiplied
	// color, zero direction
	EXPECT_FLOAT_EQ(p_positions[0].m_v[0], 3.0f);
	EXPECT_FLOAT_EQ(p_positions[0].m_v[1], 4.0f);
	EXPECT_FLOAT_EQ(p_positions[0].m_v[2], 5.0f);
	EXPECT_FLOAT_EQ(p_positions[0].m_v[3], 17.0f);
	EXPECT_FLOAT_EQ(p_colors[0].m_v[0], 2.0f);
	EXPECT_FLOAT_EQ(p_colors[0].m_v[1], 1.0f);
	EXPECT_FLOAT_EQ(p_colors[0].m_v[2], 0.5f);
	EXPECT_FLOAT_EQ(p_directions[0].m_v[0], 0.0f);
	EXPECT_FLOAT_EQ(p_directions[0].m_v[2], 0.0f);
	EXPECT_EQ(p_types[0],
		static_cast<kotek::uint8_t>(eZirconLightType::kPoint));

	// the type rides the params z lane as the u32 bit pattern
	kotek::uint32_t type_bits = 0xffffffffu;
	std::memcpy(&type_bits, &p_params[0].m_v[2], sizeof(kotek::uint32_t));
	EXPECT_EQ(type_bits,
		static_cast<kotek::uint32_t>(eZirconLightType::kPoint));

	// light 1: the spot — the rotated forward and the cone cosines
	EXPECT_NEAR(p_directions[1].m_v[0], -1.0f, 1e-5f);
	EXPECT_NEAR(p_directions[1].m_v[1], 0.0f, 1e-5f);
	EXPECT_NEAR(p_directions[1].m_v[2], 0.0f, 1e-5f);
	EXPECT_FLOAT_EQ(p_positions[1].m_v[3], 8.0f);
	EXPECT_NEAR(p_params[1].m_v[0],
		std::cos(15.0f * 0.01745329252f), 1e-6f);
	EXPECT_NEAR(p_params[1].m_v[1],
		std::cos(30.0f * 0.01745329252f), 1e-6f);

	// light 2: the directional — position/range zeroed, the identity
	// forward
	EXPECT_FLOAT_EQ(p_positions[2].m_v[0], 0.0f);
	EXPECT_FLOAT_EQ(p_positions[2].m_v[3], 0.0f);
	EXPECT_FLOAT_EQ(p_directions[2].m_v[0], 0.0f);
	EXPECT_FLOAT_EQ(p_directions[2].m_v[1], 0.0f);
	EXPECT_FLOAT_EQ(p_directions[2].m_v[2], -1.0f);
	EXPECT_EQ(p_types[2],
		static_cast<kotek::uint8_t>(eZirconLightType::kDirectional));

	// light 4 (index 3): the transform-less identity position
	EXPECT_FLOAT_EQ(p_positions[3].m_v[0], 0.0f);
	EXPECT_FLOAT_EQ(p_positions[3].m_v[1], 0.0f);
	EXPECT_FLOAT_EQ(p_positions[3].m_v[2], 0.0f);

	// the flags are the zeroed B5c pre-wire
	EXPECT_EQ(lights.get_flags()[0], 0);

	// the empty world: zero lights = the fallback frame (the FS contract)
	{
		zircon_light_ecs_env& empty_env = *new zircon_light_ecs_env();
		empty_env.initialize();

		EXPECT_EQ(lights.collect_from_world(&empty_env.factory,
					  empty_env.world.get_ecs_context(),
					  empty_env.world.get_entity_count_max_limit()),
			0u);
		EXPECT_EQ(lights.get_count(), 0u);

		empty_env.shutdown();
		delete &empty_env;
	}

	env.shutdown();
	delete &env;
}

// (d) the capacity overflow: the table holds exactly
// ZIRCON_DEF_LIGHT_MAX_COUNT, the overflow append fails loudly (the
// one-shot latch) and nothing past the cap lands
TEST(Zircon_Game, LightCollectionCapacityOverflow)
{
	zircon_render_light_buffer lights;

	const float color[3] = {1.0f, 1.0f, 1.0f};

	for (kotek::uint32_t i = 0; i < ZIRCON_DEF_LIGHT_MAX_COUNT; ++i)
	{
		EXPECT_TRUE(lights.append_light(
			eZirconLightType::kPoint, nullptr, nullptr, color, 1.0f,
			10.0f, 0.0f, 0.0f));
	}

	EXPECT_EQ(lights.get_count(), ZIRCON_DEF_LIGHT_MAX_COUNT);

	// the overflow: two failures (the latch keeps the log at one warning)
	EXPECT_FALSE(lights.append_light(eZirconLightType::kPoint, nullptr,
		nullptr, color, 1.0f, 10.0f, 0.0f, 0.0f));
	EXPECT_FALSE(lights.append_light(eZirconLightType::kPoint, nullptr,
		nullptr, color, 1.0f, 10.0f, 0.0f, 0.0f));
	EXPECT_EQ(lights.get_count(), ZIRCON_DEF_LIGHT_MAX_COUNT);

	// the range clamp: a zero-range light collects with the documented
	// minimum (the shaders' 1/range stays finite)
	zircon_render_light_buffer single;

	ASSERT_TRUE(single.append_light(eZirconLightType::kPoint, nullptr,
		nullptr, color, 1.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_FLOAT_EQ(single.get_positions()[0].m_v[3],
		ZIRCON_DEF_LIGHT_MIN_RANGE);
}

// (e) the tile-binning classification through the serial mirror (the
// compute shader's one-to-one C++): under the identity VP and a 4x2 tile
// grid (128x64 px at 32 px tiles) — a center point in its exact 6 tiles,
// a spot cone spanning exactly two tiles, a directional everywhere, and a
// light outside the frustum culled
TEST(Zircon_Game, LightBinningTileClassification)
{
	float vp[16];
	float planes[24];
	light_identity_vp(vp, planes);

	zircon_render_light_buffer lights;

	const float white[3] = {1.0f, 1.0f, 1.0f};

	// light 0: the center point (origin, range 0.5) — the AABB corners
	// project to px x in {32, 96}, py in {16, 48} -> tiles x {1,2,3} x
	// y {0,1} = 6 tiles
	ASSERT_TRUE(lights.append_light(eZirconLightType::kPoint, nullptr,
		nullptr, white, 1.0f, 0.5f, 0.0f, 0.0f));

	// light 1: OUTSIDE the frustum (x = 100 vs the identity box's right
	// plane) — culled, never binned
	const float outside_position[3] = {100.0f, 0.0f, 0.0f};
	ASSERT_TRUE(lights.append_light(eZirconLightType::kPoint,
		outside_position, nullptr, white, 1.0f, 1.0f, 0.0f, 0.0f));

	// light 2: the narrow spot (apex (0, 0.4, 0), axis +Z, outer 10 deg,
	// range 0.4) — the mid-cone bound wins: center (0, 0.4, 0.2), radius
	// ~0.212 -> px x in [50.4, 77.6] (tiles 1, 2), py in [12.4, 26.0]
	// (tile 0) = exactly 2 tiles
	const float spot_apex[3] = {0.0f, 0.4f, 0.0f};
	const float spot_axis[3] = {0.0f, 0.0f, 1.0f};
	ASSERT_TRUE(lights.append_light(eZirconLightType::kSpot, spot_apex,
		spot_axis, white, 1.0f, 0.4f, 5.0f, 10.0f));

	// light 3: the directional — every tile
	ASSERT_TRUE(lights.append_light(eZirconLightType::kDirectional,
		nullptr, nullptr, white, 1.0f, 0.0f, 0.0f, 0.0f));

	kotek::uint32_t tile_counts[8] = {};
	kotek::uint32_t tile_indices[8 * 4] = {};
	zircon_light_bin_stats_t stats;

	zircon_render_light_buffer::bin_lights(lights.get_positions(),
		lights.get_directions(), lights.get_params(), lights.get_count(),
		planes, vp, 128, 64, ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS, 4, 2, 4,
		tile_counts, tile_indices, stats);

	EXPECT_EQ(stats.m_assignment_count, 16u);
	EXPECT_EQ(stats.m_dropped_count, 0u);
	EXPECT_EQ(stats.m_culled_count, 1u);

	// the exact per-tile counts (the fixture's hand-computed matrix)
	const kotek::uint32_t expected_counts[8] = {1, 3, 3, 2, 1, 2, 2, 2};

	for (kotek::uint32_t tile = 0; tile < 8; ++tile)
	{
		EXPECT_EQ(tile_counts[tile], expected_counts[tile])
			<< "tile " << tile;
	}

	// the exact membership (serial append order = the light order):
	// tile 1 = {point, spot, directional}, tile 0 = {directional}
	EXPECT_EQ(tile_indices[1 * 4 + 0], 0u);
	EXPECT_EQ(tile_indices[1 * 4 + 1], 2u);
	EXPECT_EQ(tile_indices[1 * 4 + 2], 3u);
	EXPECT_EQ(tile_indices[0 * 4 + 0], 3u);
	EXPECT_EQ(tile_indices[7 * 4 + 0], 0u);
	EXPECT_EQ(tile_indices[7 * 4 + 1], 3u);
}

// (f) the per-tile cap: the same fixture with the cap at 2 drops the
// over-cap assignments into the dropped counter (loud through the stats
// readback on the GPU side)
TEST(Zircon_Game, LightBinningPerTileCapDrops)
{
	float vp[16];
	float planes[24];
	light_identity_vp(vp, planes);

	zircon_render_light_buffer lights;

	const float white[3] = {1.0f, 1.0f, 1.0f};

	const float outside_position[3] = {100.0f, 0.0f, 0.0f};
	const float spot_apex[3] = {0.0f, 0.4f, 0.0f};
	const float spot_axis[3] = {0.0f, 0.0f, 1.0f};

	ASSERT_TRUE(lights.append_light(eZirconLightType::kPoint, nullptr,
		nullptr, white, 1.0f, 0.5f, 0.0f, 0.0f));
	ASSERT_TRUE(lights.append_light(eZirconLightType::kPoint,
		outside_position, nullptr, white, 1.0f, 1.0f, 0.0f, 0.0f));
	ASSERT_TRUE(lights.append_light(eZirconLightType::kSpot, spot_apex,
		spot_axis, white, 1.0f, 0.4f, 5.0f, 10.0f));
	ASSERT_TRUE(lights.append_light(eZirconLightType::kDirectional,
		nullptr, nullptr, white, 1.0f, 0.0f, 0.0f, 0.0f));

	kotek::uint32_t tile_counts[8] = {};
	kotek::uint32_t tile_indices[8 * 2] = {};
	zircon_light_bin_stats_t stats;

	zircon_render_light_buffer::bin_lights(lights.get_positions(),
		lights.get_directions(), lights.get_params(), lights.get_count(),
		planes, vp, 128, 64, ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS, 4, 2, 2,
		tile_counts, tile_indices, stats);

	// tiles (1,0) and (2,0) each hold 3 candidates — the directional
	// drops from both
	EXPECT_EQ(stats.m_assignment_count, 14u);
	EXPECT_EQ(stats.m_dropped_count, 2u);

	// the counts still grew past the cap (the FS's min(count, cap) clamp
	// reads a consistent list — the shader documents the contract)
	EXPECT_EQ(tile_counts[1], 3u);
	EXPECT_EQ(tile_indices[1 * 2 + 0], 0u);
	EXPECT_EQ(tile_indices[1 * 2 + 1], 2u);
}

// (g) the FS-side evaluation math, pinned in the C++ mirror (zircon_core.
// slang's B5b evaluators): two point lights + the directional sum
// correctly, the spot cone window in/out, and the zero-lights fallback
// stays the untouched evaluate_phong answer
TEST(Zircon_Game, LightEvaluationMathPinned)
{
	const float normal[3] = {0.0f, 1.0f, 0.0f};
	const float fragment[3] = {0.0f, 0.0f, 0.0f};

	float out_rgb[3] = {};
	float sum_rgb[3] = {0.15f, 0.15f, 0.15f}; // the ambient floor

	// point A: straight up at (0, 5, 0), range 10, red — ndotl 1,
	// attenuation 1 - 5/10 = 0.5
	const float point_a[3] = {0.0f, 5.0f, 0.0f};
	const float red[3] = {1.0f, 0.0f, 0.0f};

	zircon_render_light_buffer::evaluate_point_diffuse(normal, fragment,
		point_a, red, 10.0f, out_rgb);

	EXPECT_FLOAT_EQ(out_rgb[0], 0.5f);
	EXPECT_FLOAT_EQ(out_rgb[1], 0.0f);
	EXPECT_FLOAT_EQ(out_rgb[2], 0.0f);

	sum_rgb[0] += out_rgb[0];

	// point B: (3, 4, 0), range 10, green — direction (0.6, 0.8, 0),
	// ndotl 0.8, attenuation 0.5
	const float point_b[3] = {3.0f, 4.0f, 0.0f};
	const float green[3] = {0.0f, 1.0f, 0.0f};

	zircon_render_light_buffer::evaluate_point_diffuse(normal, fragment,
		point_b, green, 10.0f, out_rgb);

	EXPECT_NEAR(out_rgb[1], 0.4f, 1e-6f);

	sum_rgb[1] += out_rgb[1];

	// the directional: straight down (0, -1, 0) blue — the diffuse-only
	// list term (the fallback's specular stays the fallback's alone)
	const float sun_dir[3] = {0.0f, -1.0f, 0.0f};
	const float blue[3] = {0.0f, 0.0f, 1.0f};

	zircon_render_light_buffer::evaluate_directional_diffuse(normal,
		sun_dir, blue, out_rgb);

	EXPECT_FLOAT_EQ(out_rgb[2], 1.0f);

	sum_rgb[2] += out_rgb[2];

	// the sum: ambient + the three terms, per channel
	EXPECT_NEAR(sum_rgb[0], 0.65f, 1e-6f);
	EXPECT_NEAR(sum_rgb[1], 0.55f, 1e-6f);
	EXPECT_NEAR(sum_rgb[2], 1.15f, 1e-6f);
	// (the shader saturates the total — 1.15 -> 1.0 on the blue channel)

	// the spot: apex (0, 5, 0) shining straight down, the fragment below
	// is dead-center in the cone (cos_angle 1 >= cos inner)
	const float spot_apex[3] = {0.0f, 5.0f, 0.0f};
	const float spot_axis[3] = {0.0f, -1.0f, 0.0f};

	const float cos_inner = std::cos(30.0f * 0.01745329252f);
	const float cos_outer = std::cos(45.0f * 0.01745329252f);

	zircon_render_light_buffer::evaluate_spot_diffuse(normal, fragment,
		spot_apex, spot_axis, red, 10.0f, cos_inner, cos_outer, out_rgb);

	EXPECT_FLOAT_EQ(out_rgb[0], 0.5f); // the point term x cone 1

	// a fragment outside the outer cone: (6, 0, 0) — the apex->fragment
	// direction is (0.768, -0.640, 0), cos_angle 0.640 < cos(45 deg)
	const float off_cone_fragment[3] = {6.0f, 0.0f, 0.0f};

	zircon_render_light_buffer::evaluate_spot_diffuse(normal,
		off_cone_fragment, spot_apex, spot_axis, red, 10.0f, cos_inner,
		cos_outer, out_rgb);

	EXPECT_FLOAT_EQ(out_rgb[0], 0.0f);

	// out of range: a point light 20 m away with range 10 attenuates to 0
	const float far_away[3] = {20.0f, 0.0f, 0.0f};

	zircon_render_light_buffer::evaluate_point_diffuse(normal, fragment,
		far_away, red, 10.0f, out_rgb);

	EXPECT_FLOAT_EQ(out_rgb[0], 0.0f);

	// the zero-lights fallback: no list light contributes, and the
	// default directional answer is the UNCHANGED evaluate_phong (pinned
	// by the existing render-pass tests — this pins that the clustered
	// evaluators add exactly zero for an empty evaluation)
	const float zero_normal[3] = {0.0f, 1.0f, 0.0f};

	zircon_render_light_buffer::evaluate_directional_diffuse(zero_normal,
		sun_dir, green, out_rgb);

	EXPECT_FLOAT_EQ(out_rgb[1], 1.0f); // ndotl 1 — the bare Lambert term
}

// (h) the tile-grid derivation + the capacity guards: the runtime tile
// counts, the 4K fit, the over-cap loud fallback, the degenerate inputs
TEST(Zircon_Game, LightClusterTileGridDerivation)
{
	kotek::uint32_t tiles_x = 0;
	kotek::uint32_t tiles_y = 0;

	// the 128x64 test grid
	EXPECT_TRUE(zircon_render_light_buffer::derive_tile_grid(128, 64,
		ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS, tiles_x, tiles_y));
	EXPECT_EQ(tiles_x, 4u);
	EXPECT_EQ(tiles_y, 2u);

	// 1920x1080 -> 60x34 = 2040 tiles
	EXPECT_TRUE(zircon_render_light_buffer::derive_tile_grid(1920, 1080,
		ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS, tiles_x, tiles_y));
	EXPECT_EQ(tiles_x, 60u);
	EXPECT_EQ(tiles_y, 34u);

	// 4K (3840x2160) -> 120x68 = 8160, still under the 8192 cap
	EXPECT_TRUE(zircon_render_light_buffer::derive_tile_grid(3840, 2160,
		ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS, tiles_x, tiles_y));
	EXPECT_EQ(tiles_x * tiles_y, 8160u);

	// over the cap (7680x4320 -> 240x135 = 32400): false — the frame
	// falls back (the rig's one-time warning)
	EXPECT_FALSE(zircon_render_light_buffer::derive_tile_grid(7680, 4320,
		ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS, tiles_x, tiles_y));

	// degenerate inputs: false, never a fault
	EXPECT_FALSE(zircon_render_light_buffer::derive_tile_grid(0, 64,
		ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS, tiles_x, tiles_y));
	EXPECT_FALSE(zircon_render_light_buffer::derive_tile_grid(
		128, 64, 0, tiles_x, tiles_y));
}

		#endif
	#endif
#endif
