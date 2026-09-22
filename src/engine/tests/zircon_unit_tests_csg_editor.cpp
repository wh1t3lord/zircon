#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cmath>
		#include <cstring>

		#include "../../core/zircon_config.h"
		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_csg_primitive.h"
		#include "../../ecs/zircon_component_csg.h"
		#include "../../ecs/zircon_csg_evaluate.h"
		#include "../../world/zircon_world.h"
		#include "../../editor/commands/zircon_command_history.h"
		#include "../../editor/commands/zircon_command_csg_create_primitive.h"
		#include "../../editor/commands/zircon_command_csg_delete_primitive.h"
		#include "../../editor/commands/zircon_command_csg_edit_primitive.h"
		#include "../../editor/session/zircon_session_editor.h"
		#include "../../editor/session/zircon_session_editor_manager.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_csg_editor_pool.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_pass_factory.h"

		#include <kotek.core.console/include/kotek_console.h>

		#ifndef ZIRCON_DEF_UNIT_TEST_CSG_EDITOR
			#define ZIRCON_DEF_UNIT_TEST_CSG_EDITOR 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_CSG_EDITOR == 1

// functional proofs for task Z25 A2 (the editor CSG integration):
// the three journaled primitive commands through the REAL command
// history (execute -> state + dirty flag; undo -> pre-state + dirty;
// redo -> post-state + dirty; the journal delta roundtrips), the
// dirty-watch + debounce scheduler on the INJECTED clock (input quiet
// fires one rebuild after 150 ms, a re-edit inside the window resets
// the timer, the journaled-batch boundary fires immediately, an edit
// during flight coalesces into one trailing rebuild), the real worker
// handoff, the one-draw-call dynamic pool (range bookkeeping, the
// byte-pin of untouched compounds, the degenerate-hole remove, the
// amortized defrag, the overflow guards) and the pass registration in
// the generated factory. Tier: lightweight (rule 8a — every fixture
// is a tiny compound; no sleeping anywhere — the clock is a counter,
// the worker handoff waits on the completion condvar with a bound).

namespace
{
	// the injected debounce clock (a counter the tests advance)
	struct zircon_test_csg_clock
	{
		kotek::uint32_t m_now;
	};

	kotek::uint32_t zircon_test_csg_clock_now(void* p_owner) noexcept
	{
		return static_cast<zircon_test_csg_clock*>(p_owner)->m_now;
	}

	/// @brief \~english the headless editor environment (the
	/// command-history suite's fixture shape): a real filesystem,
	/// factory, world, session editor manager with one session, its
	/// command history and its rebuild scheduler
	struct zircon_test_csg_editor_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;
		kotek::core::ktkMainManager main_manager;
		kotek::core::ktkConsole console;
		kotek::core::ktkInput input;
		zircon_config engine_config;
		zircon_factory factory;
		zircon_world world;
		zircon_session_editor_manager session_manager;

		void initialize(const char* p_streaming_folder_name)
		{
			this->filesystem.Initialize(&this->framework_config);

			this->main_manager.Set_FileSystem(&this->filesystem);
			this->main_manager.Set_FrameworkConfig(
				&this->framework_config);

			this->factory.Initialize(
				&this->engine_config, &this->console, &this->input);

			this->world.initialize(
				"zircon_z25_a2_test_world",
				&this->engine_config,
				&this->console,
				&this->input,
				&this->factory,
				65536);

			this->session_manager.initialize(
				&this->engine_config, &this->main_manager);

			kotek::uint8_t session_id =
				this->session_manager.create_session();

			this->session_manager.set_current_session_id(session_id);

			zircon_session_editor* p_session =
				this->session_manager.get_session(session_id);

			p_session->initialize(
				"zircon_z25_a2_test_session",
				session_id,
				&this->world,
				&this->session_manager,
				&this->main_manager,
				&this->console,
				&this->filesystem,
				&this->engine_config,
				p_streaming_folder_name);
		}

		void shutdown(void)
		{
			this->session_manager.shutdown();
			this->world.shutdown(&this->factory);
			this->factory.Shutdown();
			this->filesystem.Shutdown();
		}

		zircon_session_editor* session(void)
		{
			return this->session_manager.get_session(
				this->session_manager.get_current_session_id());
		}

		zircon_editor_command_history* history(void)
		{
			return this->session()->get_command_history();
		}

		zircon_ecs_context_t* ecs_context(void)
		{
			return this->world.get_ecs_context();
		}
	};

	void zircon_test_csg_remove_streaming_folder(
		kotek::core::ktkIFileSystem* p_filesystem,
		const char* p_folder_name)
	{
		ktk_filesystem_path path;

		p_filesystem->Make_Path(
			path,
			kotek::core::eFolderIndex::
				kFolderIndex_DataUser_SDK_Scenes);

		path /= p_folder_name;

		if (p_filesystem->Is_Exists(path))
		{
			std::filesystem::remove_all(
				std::filesystem::path(
					reinterpret_cast<const char*>(
						path.u8string().data())));
		}
	}

	// a fresh journal per test (the history suite's discipline)
	void zircon_test_csg_reset_journal(const char* p_folder_name)
	{
		auto* p_config = new kotek::core::ktkFrameworkConfig();
		auto* p_filesystem = new kotek::core::ktkFileSystem();

		p_filesystem->Initialize(p_config);

		zircon_test_csg_remove_streaming_folder(
			p_filesystem, p_folder_name);

		p_filesystem->Shutdown();

		delete p_filesystem;
		delete p_config;
	}

	/// @brief \~english creates a bare compound entity (fixture
	/// setup, NOT journaled — the tests journal only the primitive
	/// commands)
	kotek::entity_t zircon_test_csg_make_compound(
		zircon_test_csg_editor_env& env)
	{
		kotek::entity_t compound =
			env.factory.create_entity(env.ecs_context());

		EXPECT_TRUE(env.factory.create_component(env.ecs_context(),
			compound, eZirconComponentType::kzircon_component_csg));

		return compound;
	}

	zircon_component_csg* zircon_test_csg_compound(
		zircon_test_csg_editor_env& env, kotek::entity_t compound)
	{
		return static_cast<zircon_component_csg*>(
			env.factory.get_component_by_enum(env.ecs_context(),
				compound,
				eZirconComponentType::kzircon_component_csg));
	}

	zircon_component_csg_primitive* zircon_test_csg_primitive(
		zircon_test_csg_editor_env& env, kotek::entity_t primitive)
	{
		return static_cast<zircon_component_csg_primitive*>(
			env.factory.get_component_by_enum(env.ecs_context(),
				primitive,
				eZirconComponentType::kzircon_component_csg_primitive));
	}

	// the unit-box creation parameters
	const double _k_box_dimensions[3] = {1.0, 1.0, 1.0};
	const double _k_box_position[3] = {0.0, 0.0, 0.0};
	const double _k_identity_rotation[4] = {0.0, 0.0, 0.0, 1.0};

	/// @brief \~english creates a bare primitive entity and
	/// registers it in the compound (fixture setup, NOT journaled) —
	/// a compound with members evaluates to a real mesh
	kotek::entity_t zircon_test_csg_add_box_primitive(
		zircon_test_csg_editor_env& env, kotek::entity_t compound)
	{
		kotek::entity_t primitive =
			env.factory.create_entity(env.ecs_context());

		EXPECT_TRUE(env.factory.create_component(env.ecs_context(),
			primitive,
			eZirconComponentType::kzircon_component_csg_primitive));

		zircon_component_csg_primitive* p_primitive =
			zircon_test_csg_primitive(env, primitive);

		using traits_t =
			zircon_csg_scalar_traits<zircon_csg_scalar_t>;

		p_primitive->set_primitive_type(
			eZirconCsgPrimitiveType::kBox);
		p_primitive->set_dimension(
			0, traits_t::from_double(_k_box_dimensions[0]));
		p_primitive->set_dimension(
			1, traits_t::from_double(_k_box_dimensions[1]));
		p_primitive->set_dimension(
			2, traits_t::from_double(_k_box_dimensions[2]));
		p_primitive->set_position(
			traits_t::from_double(_k_box_position[0]),
			traits_t::from_double(_k_box_position[1]),
			traits_t::from_double(_k_box_position[2]));
		p_primitive->set_rotation(
			traits_t::from_double(_k_identity_rotation[0]),
			traits_t::from_double(_k_identity_rotation[1]),
			traits_t::from_double(_k_identity_rotation[2]),
			traits_t::from_double(_k_identity_rotation[3]));

		zircon_component_csg* p_compound =
			zircon_test_csg_compound(env, compound);

		if (p_compound)
			p_compound->add_primitive_entity(primitive);

		return primitive;
	}

	/// @brief \~english places a create-primitive command into the
	/// history pool and executes it (the gizmo commit's pattern)
	kotek::entity_t zircon_test_csg_execute_create(
		zircon_test_csg_editor_env& env, kotek::entity_t compound)
	{
		zircon_editor_command_history* p_history = env.history();

		unsigned char* p_memory =
			p_history->allocate_memory_for_command(
				sizeof(zircon_command_csg_create_primitive),
				"zircon_command_csg_create_primitive");

		EXPECT_NE(p_memory, nullptr);

		auto* p_command =
			new (p_memory) zircon_command_csg_create_primitive(
				&env.session_manager, &env.factory, compound,
				eZirconCsgPrimitiveType::kBox, _k_box_dimensions,
				_k_box_position, _k_identity_rotation,
				0, // additive
				0);

		p_history->ExecuteCommand(p_command);

		return p_command->GetEntityID();
	}

	// a synthetic mesh for the pool proofs: up to four triangles,
	// four welded positions, distinct byte patterns pin the writes
	// (the triangle count is selectable per test — the arrays always
	// cover the maximum)
	struct zircon_test_pool_mesh
	{
		static constexpr kotek::uint32_t kMaxTriangles = 4;

		float m_positions[4 * 3];
		float m_normals[kMaxTriangles * 3];
		kotek::uint32_t m_indices[kMaxTriangles * 3];
		kotek::uint32_t m_position_count = 4;
		kotek::uint32_t m_triangle_count = 2;

		void fill(float base)
		{
			for (int i = 0; i < 12; ++i)
				this->m_positions[i] = base + 0.01f * i;
			for (kotek::uint32_t i = 0;
				 i < kMaxTriangles * 3; ++i)
				this->m_normals[i] = base + 1.0f + 0.01f * i;
			for (kotek::uint32_t i = 0;
				 i < kMaxTriangles * 3; ++i)
				this->m_indices[i] =
					static_cast<kotek::uint32_t>(i % 4);
		}
	};
} // namespace

// ------------------------------------------------------------------
// the journaled primitive commands through the real history
// ------------------------------------------------------------------

TEST(Zircon_Editor, CsgEditorCommandCreatePrimitiveUndoRedo)
{
	constexpr const char* _k_folder = "z25_a2_create";

	zircon_test_csg_reset_journal(_k_folder);

	zircon_test_csg_editor_env& env =
		*new zircon_test_csg_editor_env();
	env.initialize(_k_folder);

	const kotek::entity_t compound =
		zircon_test_csg_make_compound(env);

	zircon_component_csg* p_compound =
		zircon_test_csg_compound(env, compound);

	ASSERT_NE(p_compound, nullptr);
	EXPECT_EQ(p_compound->get_primitive_count(), 0);
	EXPECT_EQ(p_compound->is_dirty(), 0);

	// execute: the entity exists, carries the primitive, is
	// registered, the compound is dirty (the rebuild trigger)
	const kotek::entity_t primitive =
		zircon_test_csg_execute_create(env, compound);

	ASSERT_TRUE(env.factory.is_valid_entity(
		env.ecs_context(), primitive));
	ASSERT_NE(zircon_test_csg_primitive(env, primitive), nullptr);
	EXPECT_EQ(p_compound->get_primitive_count(), 1);
	EXPECT_TRUE(p_compound->has_primitive_entity(primitive));
	EXPECT_NE(p_compound->is_dirty(), 0); // nonzero = dirty (a generation counter since A2)

	// undo: everything returns to the pre-state, the compound is
	// dirty again (the rebuild keys on dirtiness, not the kind)
	env.history()->Undo();

	EXPECT_FALSE(env.factory.is_valid_entity(
		env.ecs_context(), primitive));
	EXPECT_EQ(p_compound->get_primitive_count(), 0);
	EXPECT_NE(p_compound->is_dirty(), 0); // nonzero = dirty (a generation counter since A2)

	// redo
	env.history()->Redo();

	const kotek::entity_t reincarnated =
		p_compound->get_primitive_entities()[0];

	EXPECT_TRUE(env.factory.is_valid_entity(
		env.ecs_context(), reincarnated));
	EXPECT_EQ(p_compound->get_primitive_count(), 1);
	EXPECT_NE(p_compound->is_dirty(), 0); // nonzero = dirty (a generation counter since A2)

	env.shutdown();

	delete &env;
}

TEST(Zircon_Editor, CsgEditorCommandDeletePrimitiveUndoRedo)
{
	constexpr const char* _k_folder = "z25_a2_delete";

	zircon_test_csg_reset_journal(_k_folder);

	zircon_test_csg_editor_env& env =
		*new zircon_test_csg_editor_env();
	env.initialize(_k_folder);

	const kotek::entity_t compound =
		zircon_test_csg_make_compound(env);

	zircon_component_csg* p_compound =
		zircon_test_csg_compound(env, compound);

	const kotek::entity_t primitive =
		zircon_test_csg_execute_create(env, compound);

	ASSERT_EQ(p_compound->get_primitive_count(), 1);

	// the delete command (capture -> unregister -> destroy)
	{
		zircon_editor_command_history* p_history = env.history();

		unsigned char* p_memory =
			p_history->allocate_memory_for_command(
				sizeof(zircon_command_csg_delete_primitive),
				"zircon_command_csg_delete_primitive");

		ASSERT_NE(p_memory, nullptr);

		auto* p_command =
			new (p_memory) zircon_command_csg_delete_primitive(
				&env.session_manager, &env.factory, compound,
				primitive);

		p_history->ExecuteCommand(p_command);
	}

	EXPECT_FALSE(env.factory.is_valid_entity(
		env.ecs_context(), primitive));
	EXPECT_EQ(p_compound->get_primitive_count(), 0);
	EXPECT_NE(p_compound->is_dirty(), 0); // nonzero = dirty (a generation counter since A2)

	// undo: the primitive comes back (a fresh incarnation), is
	// re-registered, the compound is dirty
	env.history()->Undo();

	EXPECT_EQ(p_compound->get_primitive_count(), 1);

	const kotek::entity_t restored =
		p_compound->get_primitive_entities()[0];

	EXPECT_TRUE(env.factory.is_valid_entity(
		env.ecs_context(), restored));
	ASSERT_NE(zircon_test_csg_primitive(env, restored), nullptr);
	EXPECT_NE(p_compound->is_dirty(), 0); // nonzero = dirty (a generation counter since A2)

	// redo
	env.history()->Redo();

	EXPECT_EQ(p_compound->get_primitive_count(), 0);
	EXPECT_NE(p_compound->is_dirty(), 0); // nonzero = dirty (a generation counter since A2)

	env.shutdown();

	delete &env;
}

TEST(Zircon_Editor, CsgEditorCommandEditPrimitiveUndoRedo)
{
	constexpr const char* _k_folder = "z25_a2_edit";

	zircon_test_csg_reset_journal(_k_folder);

	zircon_test_csg_editor_env& env =
		*new zircon_test_csg_editor_env();
	env.initialize(_k_folder);

	const kotek::entity_t compound =
		zircon_test_csg_make_compound(env);

	zircon_component_csg* p_compound =
		zircon_test_csg_compound(env, compound);

	const kotek::entity_t primitive =
		zircon_test_csg_execute_create(env, compound);

	zircon_component_csg_primitive* p_primitive =
		zircon_test_csg_primitive(env, primitive);

	ASSERT_NE(p_primitive, nullptr);

	using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_t>;

	const double dimension_before =
		traits_t::to_double(p_primitive->get_dimension(0));

	// the after-state: the same component with a stretched X
	// (built through a temp component — the proven
	// serialize/deserialize seam, no in-place json mutation)
	kotek::ktk::json::value state_after =
		zircon_serialize_component(p_primitive);

	zircon_component_csg_primitive edited{};
	env.factory.DeserializeComponent(&edited, state_after);
	edited.set_dimension(0, traits_t::from_double(2.5));
	state_after = zircon_serialize_component(&edited);

	{
		zircon_editor_command_history* p_history = env.history();

		unsigned char* p_memory =
			p_history->allocate_memory_for_command(
				sizeof(zircon_command_csg_edit_primitive),
				"zircon_command_csg_edit_primitive");

		ASSERT_NE(p_memory, nullptr);

		auto* p_command =
			new (p_memory) zircon_command_csg_edit_primitive(
				&env.session_manager, &env.factory, compound,
				primitive, state_after);

		p_history->ExecuteCommand(p_command);
	}

	EXPECT_NEAR(traits_t::to_double(p_primitive->get_dimension(0)),
		2.5, 1e-4);
	EXPECT_NE(p_compound->is_dirty(), 0); // nonzero = dirty (a generation counter since A2)

	// undo restores the exact prior value and marks the compound
	env.history()->Undo();

	EXPECT_NEAR(traits_t::to_double(p_primitive->get_dimension(0)),
		dimension_before, 1e-6);
	EXPECT_NE(p_compound->is_dirty(), 0); // nonzero = dirty (a generation counter since A2)

	// redo
	env.history()->Redo();

	EXPECT_NEAR(traits_t::to_double(p_primitive->get_dimension(0)),
		2.5, 1e-4);

	env.shutdown();

	delete &env;
}

TEST(Zircon_Editor, CsgEditorCommandJournalDeltaRoundTrips)
{
	// the journal reconstruction path: a delta serialized after a
	// real execute must deserialize into a blank command that
	// re-executes to the same observable state
	constexpr const char* _k_folder = "z25_a2_delta";

	zircon_test_csg_reset_journal(_k_folder);

	zircon_test_csg_editor_env& env =
		*new zircon_test_csg_editor_env();
	env.initialize(_k_folder);

	const kotek::entity_t compound =
		zircon_test_csg_make_compound(env);

	zircon_component_csg* p_compound =
		zircon_test_csg_compound(env, compound);

	// --- create: execute, serialize, reconstruct, execute again
	zircon_editor_command_history* p_history = env.history();

	unsigned char* p_create_memory =
		p_history->allocate_memory_for_command(
			sizeof(zircon_command_csg_create_primitive),
			"zircon_command_csg_create_primitive");

	ASSERT_NE(p_create_memory, nullptr);

	auto* p_create_command =
		new (p_create_memory) zircon_command_csg_create_primitive(
			&env.session_manager, &env.factory, compound,
			eZirconCsgPrimitiveType::kBox, _k_box_dimensions,
			_k_box_position, _k_identity_rotation, 0, 0);

	p_history->ExecuteCommand(p_create_command);

	const kotek::entity_t primitive = p_create_command->GetEntityID();

	unsigned char delta_buffer[4096]{};
	kotek::size_t delta_size = 0;

	{
		zircon_command_delta_writer writer(
			delta_buffer, sizeof(delta_buffer));

		ASSERT_TRUE(p_create_command->Serialize_Delta(writer));

		delta_size = writer.get_offset();
	}

	const kotek::uint32_t member_count_before =
		p_compound->get_primitive_count();

	{
		zircon_command_csg_create_primitive reconstructed(
			&env.session_manager, &env.factory);

		zircon_command_delta_reader reader(
			delta_buffer, delta_size);

		ASSERT_TRUE(reconstructed.Deserialize_Delta(reader));

		env.history()->ExecuteCommand(&reconstructed);

		// the reconstructed command created its own entity and
		// registered it: the member list grew by exactly one
		EXPECT_EQ(p_compound->get_primitive_count(),
			member_count_before + 1);
	}

	// --- edit: the same roundtrip on the second primitive
	const kotek::entity_t second =
		p_compound->get_primitive_entities()[1];

	zircon_component_csg_primitive* p_second =
		zircon_test_csg_primitive(env, second);

	kotek::ktk::json::value edit_after =
		zircon_serialize_component(p_second);

	{
		using traits_t =
			zircon_csg_scalar_traits<zircon_csg_scalar_t>;

		zircon_component_csg_primitive edited{};
		env.factory.DeserializeComponent(&edited, edit_after);
		edited.set_dimension(1, traits_t::from_double(3.0));
		edit_after = zircon_serialize_component(&edited);
	}

	{
		zircon_editor_command_history* p_history = env.history();

		unsigned char* p_memory =
			p_history->allocate_memory_for_command(
				sizeof(zircon_command_csg_edit_primitive),
				"zircon_command_csg_edit_primitive");

		ASSERT_NE(p_memory, nullptr);

		auto* p_command =
			new (p_memory) zircon_command_csg_edit_primitive(
				&env.session_manager, &env.factory, compound, second,
				edit_after);

		p_history->ExecuteCommand(p_command);

		zircon_command_delta_writer writer(
			delta_buffer, sizeof(delta_buffer));

		ASSERT_TRUE(p_command->Serialize_Delta(writer));

		delta_size = writer.get_offset();
	}

	{
		zircon_command_csg_edit_primitive reconstructed(
			&env.session_manager, &env.factory);

		zircon_command_delta_reader reader(
			delta_buffer, delta_size);

		ASSERT_TRUE(reconstructed.Deserialize_Delta(reader));

		// undo the original edit, then let the reconstructed
		// command re-apply it: the observable state must land on the
		// same value
		env.history()->Undo();

		env.history()->ExecuteCommand(&reconstructed);

		using traits_t =
			zircon_csg_scalar_traits<zircon_csg_scalar_t>;

		EXPECT_NEAR(
			traits_t::to_double(p_second->get_dimension(1)), 3.0,
			1e-4);
	}

	// --- delete: roundtrip removes the registered primitive
	{
		zircon_editor_command_history* p_history = env.history();

		unsigned char* p_memory =
			p_history->allocate_memory_for_command(
				sizeof(zircon_command_csg_delete_primitive),
				"zircon_command_csg_delete_primitive");

		ASSERT_NE(p_memory, nullptr);

		auto* p_command =
			new (p_memory) zircon_command_csg_delete_primitive(
				&env.session_manager, &env.factory, compound,
				primitive);

		p_history->ExecuteCommand(p_command);

		zircon_command_delta_writer writer(
			delta_buffer, sizeof(delta_buffer));

		ASSERT_TRUE(p_command->Serialize_Delta(writer));

		delta_size = writer.get_offset();
	}

	{
		zircon_command_csg_delete_primitive reconstructed(
			&env.session_manager, &env.factory);

		zircon_command_delta_reader reader(
			delta_buffer, delta_size);

		ASSERT_TRUE(reconstructed.Deserialize_Delta(reader));

		// undo the original delete (the primitive returns as a new
		// incarnation), then let the reconstructed delete remove it —
		// the history's replay path re-targets the recorded id
		// through the reincarnation chain (execute_node's
		// SetEntityID contract), mirrored here explicitly
		env.history()->Undo();

		reconstructed.SetEntityID(
			env.history()->get_live_entity_id(primitive));

		env.history()->ExecuteCommand(&reconstructed);

		EXPECT_EQ(p_compound->get_primitive_count(), 1);
	}

	env.shutdown();

	delete &env;
}

// ------------------------------------------------------------------
// the dirty-watch + debounce scheduler on the injected clock
// ------------------------------------------------------------------

namespace
{
	// a scheduler fixture: the injected clock + the synchronous
	// execution mode (no thread — the full schedule -> evaluate ->
	// complete path inline)
	struct zircon_test_scheduler_fixture
	{
		zircon_test_csg_clock clock;
		zircon_csg_editor_rebuild_scheduler scheduler;

		zircon_test_scheduler_fixture(void)
		{
			this->clock.m_now = 0;
			this->scheduler.set_synchronous_execution(true);
			this->scheduler.initialize(&zircon_test_csg_clock_now,
				&this->clock);
		}

		~zircon_test_scheduler_fixture(void)
		{
			this->scheduler.shutdown();
		}
	};

	constexpr kotek::uint64_t _k_epoch_a = 1000;
	constexpr kotek::uint64_t _k_epoch_b = 2000;
} // namespace

TEST(Zircon_Editor, CsgEditorSchedulerDebouncesOnInputQuiet)
{
	zircon_test_csg_editor_env& env =
		*new zircon_test_csg_editor_env();
	env.initialize("z25_a2_sched_quiet");

	const kotek::entity_t compound =
		zircon_test_csg_make_compound(env);
	(void)zircon_test_csg_add_box_primitive(env, compound);

	zircon_component_csg* p_compound =
		zircon_test_csg_compound(env, compound);

	zircon_test_scheduler_fixture fixture;

	p_compound->mark_dirty();

	// the first update only baselines the history epoch — no
	// boundary, the compound enters the watch table
	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_pending_count(), 1);
	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 0);

	// 149 ms of quiet: still waiting
	fixture.clock.m_now = 149;

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_pending_count(), 1);
	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 0);

	// 150 ms of quiet: exactly ONE rebuild
	fixture.clock.m_now = 150;

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 1);
	EXPECT_EQ(fixture.scheduler.get_completed_count(), 1);
	EXPECT_EQ(p_compound->is_dirty(), 0); // cleared at dispatch

	// the completed result names the compound and carries the unit
	// box (12 triangles)
	zircon_csg_rebuild_result_view_t view{};

	ASSERT_TRUE(fixture.scheduler.pop_completed(view));
	EXPECT_EQ(view.m_compound_id, compound.id);
	EXPECT_EQ(view.m_status,
		static_cast<kotek::uint32_t>(
			eZirconCsgEvaluationStatus::kSuccess));
	EXPECT_EQ(view.m_triangle_count, 12);

	fixture.scheduler.return_completed(view.m_compound_id);

	// no further rebuilds without new edits
	fixture.clock.m_now = 5000;

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 1);

	env.shutdown();

	delete &env;
}

TEST(Zircon_Editor, CsgEditorSchedulerReeditResetsTheTimer)
{
	zircon_test_csg_editor_env& env =
		*new zircon_test_csg_editor_env();
	env.initialize("z25_a2_sched_reedit");

	const kotek::entity_t compound =
		zircon_test_csg_make_compound(env);
	(void)zircon_test_csg_add_box_primitive(env, compound);

	zircon_component_csg* p_compound =
		zircon_test_csg_compound(env, compound);

	zircon_test_scheduler_fixture fixture;

	p_compound->mark_dirty();

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	// a second edit 100 ms into the window resets the timer
	fixture.clock.m_now = 100;
	p_compound->mark_dirty();

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 0);

	// 249 ms from the FIRST edit but 149 from the second: still
	// waiting
	fixture.clock.m_now = 249;

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 0);

	// 250 ms from the first, 150 from the second: fires once
	fixture.clock.m_now = 250;

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 1);
	EXPECT_EQ(fixture.scheduler.get_completed_count(), 1);

	env.shutdown();

	delete &env;
}

TEST(Zircon_Editor, CsgEditorSchedulerCommandBoundaryFiresImmediately)
{
	zircon_test_csg_editor_env& env =
		*new zircon_test_csg_editor_env();
	env.initialize("z25_a2_sched_boundary");

	const kotek::entity_t compound =
		zircon_test_csg_make_compound(env);
	(void)zircon_test_csg_add_box_primitive(env, compound);

	zircon_component_csg* p_compound =
		zircon_test_csg_compound(env, compound);

	zircon_test_scheduler_fixture fixture;

	p_compound->mark_dirty();

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 0);

	// the journaled batch completed (the epoch moved) — the rebuild
	// fires WITHOUT waiting the debounce out
	fixture.clock.m_now = 5;

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_b);

	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 1);
	EXPECT_EQ(fixture.scheduler.get_completed_count(), 1);

	env.shutdown();

	delete &env;
}

TEST(Zircon_Editor, CsgEditorSchedulerRedirtyCoalescesOneTrailingRebuild)
{
	zircon_test_csg_editor_env& env =
		*new zircon_test_csg_editor_env();
	env.initialize("z25_a2_sched_redirty");

	const kotek::entity_t compound =
		zircon_test_csg_make_compound(env);
	(void)zircon_test_csg_add_box_primitive(env, compound);

	zircon_component_csg* p_compound =
		zircon_test_csg_compound(env, compound);

	zircon_test_scheduler_fixture fixture;

	p_compound->mark_dirty();

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	// no boundary yet — the timer path
	fixture.clock.m_now = 200;

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 1);

	// an edit WHILE the result sits unconsumed in the slot: the
	// tracked slot is dispatched — the edit becomes the re-dirty
	// flag, NOT a second dispatch
	p_compound->mark_dirty();

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 1);

	// consuming the result re-arms the debounce on the injected
	// clock; one trailing rebuild follows the quiet window
	zircon_csg_rebuild_result_view_t view{};

	ASSERT_TRUE(fixture.scheduler.pop_completed(view));
	fixture.scheduler.return_completed(view.m_compound_id);

	fixture.clock.m_now = 200 + ZIRCON_DEF_CSG_EDITOR_REBUILD_DEBOUNCE_MS - 1;

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 1);

	fixture.clock.m_now = 200 + ZIRCON_DEF_CSG_EDITOR_REBUILD_DEBOUNCE_MS;

	fixture.scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);

	EXPECT_EQ(fixture.scheduler.get_scheduled_count(), 2);
	EXPECT_EQ(fixture.scheduler.get_completed_count(), 2);

	env.shutdown();

	delete &env;
}

TEST(Zircon_Editor, CsgEditorSchedulerWorkerHandoff)
{
	// the REAL thread: schedule -> worker evaluates -> the
	// completion condvar wakes the consumer (no sleeping — a bounded
	// wait on the slot)
	zircon_test_csg_editor_env& env =
		*new zircon_test_csg_editor_env();
	env.initialize("z25_a2_sched_worker");

	const kotek::entity_t compound =
		zircon_test_csg_make_compound(env);
	(void)zircon_test_csg_add_box_primitive(env, compound);

	zircon_component_csg* p_compound =
		zircon_test_csg_compound(env, compound);

	zircon_csg_editor_rebuild_scheduler& scheduler =
		*new zircon_csg_editor_rebuild_scheduler();

	scheduler.initialize();

	EXPECT_TRUE(scheduler.is_worker_running());

	p_compound->mark_dirty();

	scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_a);
	scheduler.update(&env.factory, env.ecs_context(), 65536,
		_k_epoch_b);

	zircon_csg_rebuild_result_view_t view{};

	ASSERT_TRUE(scheduler.wait_pop_completed(view, 5000));
	EXPECT_EQ(view.m_compound_id, compound.id);
	EXPECT_EQ(view.m_status,
		static_cast<kotek::uint32_t>(
			eZirconCsgEvaluationStatus::kSuccess));

	scheduler.return_completed(view.m_compound_id);

	scheduler.shutdown();

	delete &scheduler;

	env.shutdown();

	delete &env;
}

TEST(Zircon_Editor, CsgEditorSessionDrivesTheScheduler)
{
	// the end-to-end through the session: a journaled command marks
	// the compound dirty; the session's per-frame update observes
	// the history movement (the batch boundary) and dispatches; the
	// session's worker completes the rebuild
	constexpr const char* _k_folder = "z25_a2_session";

	zircon_test_csg_reset_journal(_k_folder);

	zircon_test_csg_editor_env& env =
		*new zircon_test_csg_editor_env();
	env.initialize(_k_folder);

	zircon_session_editor* p_session = env.session();

	ASSERT_NE(p_session, nullptr);
	ASSERT_NE(p_session->get_csg_scheduler(), nullptr);
	EXPECT_TRUE(p_session->get_csg_scheduler()->is_worker_running());

	const kotek::entity_t compound =
		zircon_test_csg_make_compound(env);

	zircon_component_csg* p_compound =
		zircon_test_csg_compound(env, compound);

	p_compound->mark_dirty();

	// frame 1: the scheduler baselines the history epoch
	p_session->update();

	EXPECT_EQ(p_session->get_csg_scheduler()->get_scheduled_count(),
		0);

	// a journaled command completes (the epoch moves) and marks the
	// compound dirty
	zircon_test_csg_execute_create(env, compound);

	// frame 2: the batch boundary dispatches the rebuild
	p_session->update();

	zircon_csg_rebuild_result_view_t view{};

	ASSERT_TRUE(p_session->get_csg_scheduler()->wait_pop_completed(
		view, 5000));

	EXPECT_EQ(view.m_compound_id, compound.id);
	EXPECT_EQ(view.m_status,
		static_cast<kotek::uint32_t>(
			eZirconCsgEvaluationStatus::kSuccess));

	// one additive unit box evaluates to exactly 12 triangles
	EXPECT_EQ(view.m_triangle_count, 12);

	p_session->get_csg_scheduler()->return_completed(
		view.m_compound_id);

	env.shutdown();

	// (the session manager owns the session — the pointer dangles
	// past shutdown; the worker's join is the scheduler's own
	// shutdown contract, covered by the fixture tests and the
	// dtor's not-initialized assert)

	delete &env;
}

// ------------------------------------------------------------------
// the one-draw-call dynamic pool (bgfx-free, headless)
// ------------------------------------------------------------------

TEST(Zircon_Editor, CsgEditorPoolRangesAndDirtySpans)
{
	zircon_render_csg_editor_pool& pool =
		*new zircon_render_csg_editor_pool();

	pool.initialize();

	zircon_test_pool_mesh mesh{};
	mesh.fill(0.0f);

	EXPECT_TRUE(pool.upsert_compound_mesh(42, mesh.m_positions,
		mesh.m_position_count, mesh.m_normals, mesh.m_indices,
		mesh.m_triangle_count));

	EXPECT_EQ(pool.get_live_compound_count(), 1);
	EXPECT_EQ(pool.get_vertex_high_water(), 6);
	EXPECT_EQ(pool.get_index_high_water(), 6);

	kotek::uint32_t v_offset, v_count, i_offset, i_count;
	pool.get_record_ranges(0, v_offset, v_count, i_offset, i_count);

	EXPECT_EQ(v_offset, 0);
	EXPECT_EQ(v_count, 6);
	EXPECT_EQ(i_offset, 0);
	EXPECT_EQ(i_count, 6);

	// the pending spans cover both pools; the take clears them
	zircon_csg_editor_pool_dirty_span_t spans[4]{};

	EXPECT_EQ(pool.take_dirty_spans(spans, 4), 2);
	EXPECT_EQ(pool.take_dirty_spans(spans, 4), 0);

	delete &pool;
}

TEST(Zircon_Editor, CsgEditorPoolRangeUpdateBytePin)
{
	zircon_render_csg_editor_pool& pool =
		*new zircon_render_csg_editor_pool();

	pool.initialize();

	zircon_test_pool_mesh mesh_a{};
	mesh_a.fill(0.0f);

	zircon_test_pool_mesh mesh_b{};
	mesh_b.fill(7.0f);

	// B is a 3-triangle mesh — a different range size than A
	mesh_b.m_triangle_count = 3;

	EXPECT_TRUE(pool.upsert_compound_mesh(1, mesh_a.m_positions,
		mesh_a.m_position_count, mesh_a.m_normals, mesh_a.m_indices,
		mesh_a.m_triangle_count));

	// pin A's bytes (vertex + index spans)
	zircon_model_static_vertex_t pinned_vertices[6]{};
	kotek::uint32_t pinned_indices[6]{};

	kotek::uint32_t v_offset, v_count, i_offset, i_count;
	pool.get_record_ranges(0, v_offset, v_count, i_offset, i_count);

	std::memcpy(pinned_vertices,
		pool.get_vertex_shadow() + v_offset,
		v_count * sizeof(zircon_model_static_vertex_t));
	std::memcpy(pinned_indices, pool.get_index_shadow() + i_offset,
		i_count * sizeof(kotek::uint32_t));

	// B's rebuild leaves A's range bytes untouched
	EXPECT_TRUE(pool.upsert_compound_mesh(2, mesh_b.m_positions,
		mesh_b.m_position_count, mesh_b.m_normals, mesh_b.m_indices,
		mesh_b.m_triangle_count));

	EXPECT_EQ(std::memcmp(pinned_vertices,
				  pool.get_vertex_shadow() + v_offset,
				  v_count * sizeof(zircon_model_static_vertex_t)),
		0);
	EXPECT_EQ(std::memcmp(pinned_indices,
				  pool.get_index_shadow() + i_offset,
				  i_count * sizeof(kotek::uint32_t)),
		0);

	// B's spans do not overlap A's
	kotek::uint32_t b_v_offset, b_v_count, b_i_offset, b_i_count;
	pool.get_record_ranges(1, b_v_offset, b_v_count, b_i_offset,
		b_i_count);

	EXPECT_GE(b_v_offset, v_offset + v_count);
	EXPECT_GE(b_i_offset, i_offset + i_count);

	delete &pool;
}

TEST(Zircon_Editor, CsgEditorPoolRemoveDegeneratesTheHole)
{
	zircon_render_csg_editor_pool& pool =
		*new zircon_render_csg_editor_pool();

	pool.initialize();

	zircon_test_pool_mesh mesh_a{};
	mesh_a.fill(0.0f);
	zircon_test_pool_mesh mesh_b{};
	mesh_b.fill(7.0f);

	EXPECT_TRUE(pool.upsert_compound_mesh(1, mesh_a.m_positions,
		mesh_a.m_position_count, mesh_a.m_normals, mesh_a.m_indices,
		mesh_a.m_triangle_count));
	EXPECT_TRUE(pool.upsert_compound_mesh(2, mesh_b.m_positions,
		mesh_b.m_position_count, mesh_b.m_normals, mesh_b.m_indices,
		mesh_b.m_triangle_count));

	const kotek::uint32_t index_high_water =
		pool.get_index_high_water();

	EXPECT_TRUE(pool.remove_compound(1));

	EXPECT_EQ(pool.get_live_compound_count(), 1);

	// A's freed index span now reads as degenerate zeros (the single
	// draw skips nothing — the hole rasterizes nothing)
	kotek::uint32_t b_v_offset, b_v_count, b_i_offset, b_i_count;
	pool.get_record_ranges(1, b_v_offset, b_v_count, b_i_offset,
		b_i_count);

	EXPECT_EQ(b_i_offset, 6);

	for (kotek::uint32_t i = 0; i < 6; ++i)
		EXPECT_EQ(pool.get_index_shadow()[i], 0u);

	// B's range is intact
	EXPECT_EQ(pool.get_index_shadow()[6], 6u);

	// the high water survives the removal (B still occupies the
	// tail); removing the last compound resets it
	EXPECT_EQ(pool.get_index_high_water(), index_high_water);

	EXPECT_TRUE(pool.remove_compound(2));

	EXPECT_EQ(pool.get_live_compound_count(), 0);
	EXPECT_EQ(pool.get_index_high_water(), 0);
	EXPECT_EQ(pool.get_vertex_high_water(), 0);

	delete &pool;
}

TEST(Zircon_Editor, CsgEditorPoolDefragCompactsAndReuploads)
{
	zircon_render_csg_editor_pool& pool =
		*new zircon_render_csg_editor_pool();

	pool.initialize();

	// fill the pool to the cap so NO tail hole exists: 32 chunks of
	// 6144 vertices (2048 triangles each) tile the 196608-vertex
	// pool exactly. Removing two NON-ADJACENT chunks leaves two
	// 6144 holes separated by live data — a 9216-vertex request
	// fits the TOTAL free (12288) but no single hole: the failed-fit
	// trigger compacts once and the allocation retries
	constexpr kotek::uint32_t kChunkTriangles = 2048;
	constexpr kotek::uint32_t kChunkVertices = kChunkTriangles * 3;
	constexpr kotek::uint32_t kFillCompounds =
		zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_VERTICES / kChunkVertices;
	static_assert(kFillCompounds * kChunkVertices ==
			zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_VERTICES,
		"the fixture chunks must tile the pool exactly");

	struct big_mesh_t
	{
		// sized for the largest fixture (the 9216-vertex E request)
		enum : kotek::uint32_t
		{
			kMaxVertices = 9216
		};

		float m_positions[4 * 3];
		float m_normals[kMaxVertices * 3];
		kotek::uint32_t m_indices[kMaxVertices];

		void fill(float base)
		{
			for (int i = 0; i < 12; ++i)
				this->m_positions[i] = base + 0.01f * i;
			for (kotek::uint32_t i = 0; i < kMaxVertices * 3;
				 ++i)
				this->m_normals[i] = base + 1.0f + 0.01f * i;
			for (kotek::uint32_t i = 0; i < kMaxVertices;
				 ++i)
				this->m_indices[i] =
					static_cast<kotek::uint32_t>(i % 4);
		}
	};

	big_mesh_t& mesh = *new big_mesh_t();
	mesh.fill(0.0f);

	for (kotek::uint32_t id = 1; id <= kFillCompounds; ++id)
	{
		EXPECT_TRUE(pool.upsert_compound_mesh(id, mesh.m_positions,
			4, mesh.m_normals, mesh.m_indices, kChunkTriangles));
	}

	EXPECT_EQ(pool.get_vertex_allocator().get_free_total(), 0);

	// remove two non-adjacent chunks (the first and the third)
	EXPECT_TRUE(pool.remove_compound(1));
	EXPECT_TRUE(pool.remove_compound(3));

	EXPECT_EQ(pool.get_vertex_allocator().get_free_range_count(), 2);
	EXPECT_EQ(pool.get_vertex_allocator().get_free_total(),
		kChunkVertices * 2);

	// E needs 9216 contiguous vertices — the defrag path
	big_mesh_t& mesh_e = *new big_mesh_t();
	mesh_e.fill(5.0f);

	constexpr kotek::uint32_t kETriangles = 3072;
	constexpr kotek::uint32_t kEVertices = kETriangles * 3;

	EXPECT_TRUE(pool.upsert_compound_mesh(kFillCompounds + 1,
		mesh_e.m_positions, 4, mesh_e.m_normals, mesh_e.m_indices,
		kETriangles));

	// after the compaction every live range tiles [0, used_end)
	// contiguously (no gaps) and the free list is one tail range
	EXPECT_EQ(pool.get_live_compound_count(), kFillCompounds - 1);
	EXPECT_EQ(pool.get_vertex_allocator().get_free_range_count(), 1);
	EXPECT_EQ(pool.get_index_allocator().get_free_range_count(), 1);

	kotek::uint32_t coverage = 0;

	for (kotek::uint32_t slot = 0; slot < pool.get_record_count();
		 ++slot)
	{
		if (pool.is_compound_live(slot) == false)
			continue;

		kotek::uint32_t v_offset, v_count, i_offset, i_count;
		pool.get_record_ranges(slot, v_offset, v_count, i_offset,
			i_count);

		EXPECT_EQ(v_offset, coverage);

		coverage += v_count;
	}

	EXPECT_EQ(coverage,
		(kFillCompounds - 2) * kChunkVertices + kEVertices);

	// the defrag degraded the upload to the full-range spans
	zircon_csg_editor_pool_dirty_span_t spans[4]{};

	EXPECT_EQ(pool.take_dirty_spans(spans, 4), 2);
	EXPECT_EQ(spans[0].m_offset, 0);
	EXPECT_EQ(spans[0].m_count, pool.get_vertex_high_water());
	EXPECT_EQ(spans[1].m_offset, 0);
	EXPECT_EQ(spans[1].m_count, pool.get_index_high_water());

	delete &mesh_e;
	delete &mesh;
	delete &pool;
}

TEST(Zircon_Editor, CsgEditorPoolOverflowIsLoudAndGraceful)
{
	zircon_render_csg_editor_pool& pool =
		*new zircon_render_csg_editor_pool();

	pool.initialize();

	// one triangle beyond the triangle cap: 65537 triangles need
	// 196611 vertices — past the pool cap
	EXPECT_FALSE(pool.upsert_compound_mesh(1, nullptr, 0, nullptr,
		nullptr, ZIRCON_DEF_CSG_MAX_TRIANGLES_PER_EVALUATION + 1));

	EXPECT_EQ(pool.get_live_compound_count(), 0);
	EXPECT_EQ(pool.get_vertex_high_water(), 0);

	// unknown removals are loud no-ops
	EXPECT_FALSE(pool.remove_compound(777));

	delete &pool;
}

// ------------------------------------------------------------------
// the pass registration (the generated factory picks the class up)
// ------------------------------------------------------------------

TEST(Zircon_Game, CsgEditorPassRegisteredInTheFactory)
{
	const char* const kExpectedName =
		"no_streaming::zircon_render_graph_pass_editor_csg_bgfx";

	bool is_found = false;

	for (unsigned i = 0;
		 i < zircon_render_editor_passes_registry_count; ++i)
	{
		if (std::strcmp(zircon_render_editor_passes_registry[i],
				kExpectedName) == 0)
		{
			is_found = true;
			break;
		}
	}

	EXPECT_TRUE(is_found);

	// the factory creates the pass by name (the executor's path)
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass =
		zircon_render_pass_factory::create(kExpectedName);

	EXPECT_NE(p_pass, nullptr);

	delete p_pass;

	// the registry grew by exactly this pass (7 = 6 + editor_csg)
	EXPECT_EQ(zircon_render_editor_passes_registry_count, 7u);
}

		#endif
	#endif
#endif
