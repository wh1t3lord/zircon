#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include "../../core/zircon_config.h"
		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_sdk_camera.h"
		#include "../../ecs/zircon_component_sdk_input.h"
		#include "../../ecs/zircon_component_transform.h"
		#include "../../editor/session/zircon_editor_sdk_camera.h"
		#include "../../world/zircon_world.h"

		#include <kotek.core.console/include/kotek_console.h>

		#ifndef ZIRCON_DEF_UNIT_TEST_PLAY
			#define ZIRCON_DEF_UNIT_TEST_PLAY 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_PLAY == 1

namespace
{
	struct zircon_test_play_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;
		kotek::core::ktkConsole console;
		kotek::core::ktkInput input;
		zircon_config engine_config;
		zircon_factory source_factory;
		zircon_factory destination_factory;
		zircon_world source_world;
		zircon_world destination_world;

		void initialize(void)
		{
			this->filesystem.Initialize(&this->framework_config);
			this->input.Initialize(
				kotek::core::eInputPlatformBackend::kPlatformBackend_GLFW3);
			this->source_factory.Initialize(
				&this->engine_config, &this->console, &this->input);
			this->destination_factory.Initialize(
				&this->engine_config, &this->console, &this->input);

			this->source_world.initialize("zircon_play_source",
				&this->engine_config, &this->console, &this->input,
				&this->source_factory, 256);
			this->destination_world.initialize("zircon_play_destination",
				&this->engine_config, &this->console, &this->input,
				&this->destination_factory, 256);
		}

		void shutdown(void)
		{
			this->destination_world.shutdown(&this->destination_factory);
			this->source_world.shutdown(&this->source_factory);
			this->destination_factory.Shutdown();
			this->source_factory.Shutdown();
			this->input.Shutdown();
			this->filesystem.Shutdown();
		}

		kotek::uint32_t entity_count(
			zircon_factory& factory, zircon_world& world)
		{
			kotek::uint32_t result = 0;

			for (kotek::uint32_t raw_id = 0;
				 raw_id < world.get_entity_count_max_limit(); ++raw_id)
			{
				if (factory.is_valid_entity(
						world.get_ecs_context(), kotek::entity_t{raw_id}))
				{
					++result;
				}
			}

			return result;
		}

		kotek::entity_t first_entity(
			zircon_factory& factory, zircon_world& world)
		{
			for (kotek::uint32_t raw_id = 0;
				 raw_id < world.get_entity_count_max_limit(); ++raw_id)
			{
				const kotek::entity_t id{raw_id};

				if (factory.is_valid_entity(world.get_ecs_context(), id))
					return id;
			}

			return kotek::entity_t{kotek::ktk::kInvalidECSEntity};
		}
	};
} // namespace

TEST(Zircon_Editor, PlayWorldBakeReplacesGameWorldAndRebindsComponents)
{
	zircon_test_play_env& env = *new zircon_test_play_env();
	env.initialize();

	const kotek::entity_t source_id = zircon_editor_ensure_sdk_bootstrap_entity(
		&env.source_factory, env.source_world.get_ecs_context(),
		env.source_world.get_entity_count_max_limit());
	ASSERT_TRUE(env.source_factory.is_valid_entity(
		env.source_world.get_ecs_context(), source_id));

	// A stale game entity proves every Play starts from the current editor
	// scene instead of accumulating previous simulation worlds.
	const kotek::entity_t stale_id = env.destination_factory.create_entity(
		env.destination_world.get_ecs_context());

	ASSERT_TRUE(zircon_game_manager::compile_world_for_simulation(
		&env.source_world, &env.destination_world));

	EXPECT_EQ(env.entity_count(env.source_factory, env.source_world), 1u);
	EXPECT_EQ(
		env.entity_count(env.destination_factory, env.destination_world), 1u);
	EXPECT_FALSE(env.destination_factory.is_valid_entity(
		env.destination_world.get_ecs_context(), stale_id));

	kotek::entity_t destination_id =
		env.first_entity(env.destination_factory, env.destination_world);
	ASSERT_TRUE(env.destination_factory.is_valid_entity(
		env.destination_world.get_ecs_context(), destination_id));

	EXPECT_TRUE(env.destination_factory.has_component(
		env.destination_world.get_ecs_context(), destination_id,
		eZirconComponentType::kzircon_component_transform));
	EXPECT_TRUE(env.destination_factory.has_component(
		env.destination_world.get_ecs_context(), destination_id,
		eZirconComponentType::kzircon_component_sdk_camera));
	EXPECT_TRUE(env.destination_factory.has_component(
		env.destination_world.get_ecs_context(), destination_id,
		eZirconComponentType::kzircon_component_sdk_input));

	// The bake crosses the json seam, so the non-serialized input manager
	// must be rebound just like it is for undo/redo and scene restoration.
	auto* p_destination_input = static_cast<zircon_component_sdk_input*>(
		env.destination_factory.get_component_by_enum(
			env.destination_world.get_ecs_context(), destination_id,
			eZirconComponentType::kzircon_component_sdk_input));
	ASSERT_NE(p_destination_input, nullptr);
	EXPECT_FALSE(p_destination_input->get_input().is_key_holding(
		kotek::core::eInputAllKeys::kCM_KEY_RIGHT));

	// Editing the editor scene and pressing Play again replaces (never
	// duplicates) the previous game state and carries the new values.
	auto* p_source_transform = static_cast<zircon_component_transform*>(
		env.source_factory.get_component_by_enum(
			env.source_world.get_ecs_context(), source_id,
			eZirconComponentType::kzircon_component_transform));
	ASSERT_NE(p_source_transform, nullptr);
	p_source_transform->set_position(kotek::ktk::math::vec3f_t(9.0f, 8.0f, 7.0f));

	ASSERT_TRUE(zircon_game_manager::compile_world_for_simulation(
		&env.source_world, &env.destination_world));
	ASSERT_EQ(
		env.entity_count(env.destination_factory, env.destination_world), 1u);

	destination_id =
		env.first_entity(env.destination_factory, env.destination_world);
	auto* p_destination_transform = static_cast<zircon_component_transform*>(
		env.destination_factory.get_component_by_enum(
			env.destination_world.get_ecs_context(), destination_id,
			eZirconComponentType::kzircon_component_transform));
	ASSERT_NE(p_destination_transform, nullptr);

	const kotek::math::vec3f_t& position =
		p_destination_transform->get_position();
	EXPECT_FLOAT_EQ(position.x(), 9.0f);
	EXPECT_FLOAT_EQ(position.y(), 8.0f);
	EXPECT_FLOAT_EQ(position.z(), 7.0f);

	// The production editor/game sessions share one world: that path is a
	// successful zero-copy bake and must never clear the source scene.
	EXPECT_TRUE(zircon_game_manager::compile_world_for_simulation(
		&env.source_world, &env.source_world));
	EXPECT_EQ(env.entity_count(env.source_factory, env.source_world), 1u);

	env.shutdown();
	delete &env;
}

		#endif
	#endif
#endif
