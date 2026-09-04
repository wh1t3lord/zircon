#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include "../../core/zircon_cancel_arbiter.h"
		#include "../../core/zircon_config.h"
		#include "../../editor/session/zircon_session_editor.h"
		#include "../../editor/session/zircon_session_editor_manager.h"
		#include "../../editor/ui/zircon_editor_ui_state.h"
		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_transform.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_graph_pass_editor_gizmo_own.h"
		#include "../../world/zircon_world.h"

		#ifndef ZIRCON_DEF_UNIT_TEST_CANCEL_ARBITER
			#define ZIRCON_DEF_UNIT_TEST_CANCEL_ARBITER 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_CANCEL_ARBITER == 1

// functional proofs for task Z19 (ESC/cancel arbitration): the arbiter's
// mechanics (priority order, inactive skipped, exactly one dismissal per
// event, false-when-idle, the capacity guard) are driven against
// synthetic owners — the probes are the same fnptr+void* seam the real
// consumers use; the session-registered consumers are then driven
// through a REAL headless editor session (selection dismissal, gizmo
// drag beating selection by priority), and the gizmo pass's
// cancel_drag_edit is pinned as the drag-cancel half of the Z6 drag
// fixtures (state restored, the journal untouched)

using zircon_pass_editor_gizmo_own =
	no_streaming::zircon_render_graph_pass_editor_gizmo_own_bgfx;

namespace
{
	/// @brief \~english one synthetic consumer owner: the probes below
	/// are the test's controlled version of the session's consumers —
	/// activeness is a plain flag and dismiss counts its calls, so the
	/// suite observes exactly what the arbiter polled and fired
	struct zircon_test_cancel_owner
	{
		bool m_is_active;
		bool m_dismiss_result;
		kotek::uint32_t m_dismiss_calls;
	};

	bool zircon_test_cancel_is_active(void* p_owner)
	{
		return static_cast<zircon_test_cancel_owner*>(p_owner)
			->m_is_active;
	}

	bool zircon_test_cancel_dismiss(void* p_owner)
	{
		zircon_test_cancel_owner* p_typed_owner =
			static_cast<zircon_test_cancel_owner*>(p_owner);

		++p_typed_owner->m_dismiss_calls;
		return p_typed_owner->m_dismiss_result;
	}

	zircon_cancel_consumer_t zircon_test_cancel_make_consumer(
		zircon_test_cancel_owner* p_owner, kotek::uint8_t priority,
		const char* p_debug_name)
	{
		zircon_cancel_consumer_t consumer{};

		consumer.pfn_is_active = &zircon_test_cancel_is_active;
		consumer.pfn_dismiss = &zircon_test_cancel_dismiss;
		consumer.p_owner = p_owner;
		consumer.priority = priority;
		consumer.p_debug_name = p_debug_name;

		return consumer;
	}

	/// @brief \~english the headless editor environment (the same shape
	/// as the Z6 history fixture — real filesystem, factory, world and
	/// one editor session): the session's initialize registers the REAL
	/// default consumers, so the suite drives the exact registration
	/// the boot uses. The main manager has no imgui wrapper, which is
	/// the point: the imgui-state consumers must probe inactive
	/// headless instead of crashing
	struct zircon_test_cancel_env
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

			this->world.initialize("zircon_z19_test_world",
				&this->engine_config, &this->console, &this->input,
				&this->factory, 65536);

			this->session_manager.initialize(
				&this->engine_config, &this->main_manager);

			kotek::uint8_t session_id =
				this->session_manager.create_session();

			this->session_manager.set_current_session_id(session_id);

			this->session()->initialize("zircon_z19_test_session",
				session_id, &this->world, &this->session_manager,
				&this->main_manager, &this->console, &this->filesystem,
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

		zircon_ecs_context_t* ecs_context(void)
		{
			return this->world.get_ecs_context();
		}
	};

	void zircon_test_cancel_remove_streaming_folder(
		kotek::core::ktkIFileSystem* p_filesystem,
		const char* p_folder_name)
	{
		ktk_filesystem_path path;

		p_filesystem->Make_Path(path,
			kotek::core::eFolderIndex::kFolderIndex_DataUser_SDK_Scenes);

		path /= p_folder_name;

		if (p_filesystem->Is_Exists(path))
		{
			std::filesystem::remove_all(std::filesystem::path(
				reinterpret_cast<const char*>(path.u8string().data())));
		}
	}
} // namespace

TEST(Zircon_Core, CancelArbiter_PriorityOrderOnInsert)
{
	zircon_cancel_arbiter arbiter;

	// registered OUT of priority order — the registry keeps itself
	// sorted on insert, so the poll order is 10 -> 30 -> 50 regardless
	zircon_test_cancel_owner owner_low{true, true, 0};
	zircon_test_cancel_owner owner_high{true, true, 0};
	zircon_test_cancel_owner owner_mid{true, true, 0};

	EXPECT_TRUE(arbiter.register_consumer(
		zircon_test_cancel_make_consumer(&owner_low, 50, "low")));
	EXPECT_TRUE(arbiter.register_consumer(
		zircon_test_cancel_make_consumer(&owner_high, 10, "high")));
	EXPECT_TRUE(arbiter.register_consumer(
		zircon_test_cancel_make_consumer(&owner_mid, 30, "mid")));

	EXPECT_EQ(arbiter.get_consumer_count(), 3);

	const char* p_consumed_name = nullptr;

	// the lowest priority number fires first, not the first registered
	EXPECT_TRUE(arbiter.handle_cancel(&p_consumed_name));
	EXPECT_STREQ(p_consumed_name, "high");
	EXPECT_EQ(owner_high.m_dismiss_calls, 1);
	EXPECT_EQ(owner_mid.m_dismiss_calls, 0);
	EXPECT_EQ(owner_low.m_dismiss_calls, 0);

	// with the winner gone the next priority steps up
	owner_high.m_is_active = false;

	EXPECT_TRUE(arbiter.handle_cancel(&p_consumed_name));
	EXPECT_STREQ(p_consumed_name, "mid");
	EXPECT_EQ(owner_mid.m_dismiss_calls, 1);
	EXPECT_EQ(owner_low.m_dismiss_calls, 0);

	owner_mid.m_is_active = false;

	EXPECT_TRUE(arbiter.handle_cancel(&p_consumed_name));
	EXPECT_STREQ(p_consumed_name, "low");
	EXPECT_EQ(owner_low.m_dismiss_calls, 1);
}

TEST(Zircon_Core, CancelArbiter_InactiveConsumersSkipped)
{
	zircon_cancel_arbiter arbiter;

	// the higher-priority consumer is inactive: the arbiter must poll
	// past it, never dismiss it
	zircon_test_cancel_owner owner_inactive{false, true, 0};
	zircon_test_cancel_owner owner_active{true, true, 0};

	EXPECT_TRUE(arbiter.register_consumer(zircon_test_cancel_make_consumer(
		&owner_inactive, 10, "inactive")));
	EXPECT_TRUE(arbiter.register_consumer(zircon_test_cancel_make_consumer(
		&owner_active, 20, "active")));

	const char* p_consumed_name = nullptr;

	EXPECT_TRUE(arbiter.handle_cancel(&p_consumed_name));
	EXPECT_STREQ(p_consumed_name, "active");
	EXPECT_EQ(owner_inactive.m_dismiss_calls, 0);
	EXPECT_EQ(owner_active.m_dismiss_calls, 1);
}

TEST(Zircon_Core, CancelArbiter_ExactlyOneDismissalPerEvent)
{
	zircon_cancel_arbiter arbiter;

	// two live consumers at once: ONE event dismisses exactly ONE of
	// them — ESC never falls through to the next consumer in the same
	// event
	zircon_test_cancel_owner owner_first{true, true, 0};
	zircon_test_cancel_owner owner_second{true, true, 0};

	EXPECT_TRUE(arbiter.register_consumer(zircon_test_cancel_make_consumer(
		&owner_first, 10, "first")));
	EXPECT_TRUE(arbiter.register_consumer(zircon_test_cancel_make_consumer(
		&owner_second, 20, "second")));

	EXPECT_TRUE(arbiter.handle_cancel());
	EXPECT_EQ(owner_first.m_dismiss_calls, 1);
	EXPECT_EQ(owner_second.m_dismiss_calls, 0);

	// a dismissed consumer's state ENDS with the dismissal (a real one
	// stops reporting active — the selection is cleared, the popup
	// closed): the NEXT event then reaches the consumer below it, and
	// still dismisses exactly one
	owner_first.m_is_active = false;

	EXPECT_TRUE(arbiter.handle_cancel());
	EXPECT_EQ(owner_first.m_dismiss_calls, 1);
	EXPECT_EQ(owner_second.m_dismiss_calls, 1);
}

TEST(Zircon_Core, CancelArbiter_FalseWhenIdle)
{
	zircon_cancel_arbiter arbiter;

	const char* p_consumed_name = "sentinel";

	// an empty registry consumes nothing and leaves the out-name alone
	EXPECT_FALSE(arbiter.handle_cancel(&p_consumed_name));
	EXPECT_EQ(p_consumed_name, nullptr);

	// registered but inactive is idle too
	zircon_test_cancel_owner owner{false, true, 0};

	EXPECT_TRUE(arbiter.register_consumer(
		zircon_test_cancel_make_consumer(&owner, 10, "idle")));

	EXPECT_FALSE(arbiter.handle_cancel(&p_consumed_name));
	EXPECT_EQ(p_consumed_name, nullptr);
	EXPECT_EQ(owner.m_dismiss_calls, 0);
}

TEST(Zircon_Core, CancelArbiter_CapacityGuard)
{
	zircon_cancel_arbiter arbiter;

	zircon_test_cancel_owner owners
		[ZIRCON_DEF_CANCEL_ARBITER_MAX_CONSUMERS + 1]{};

	// exactly the named capacity registers — every slot
	for (kotek::uint8_t index = 0;
	     index < ZIRCON_DEF_CANCEL_ARBITER_MAX_CONSUMERS; ++index)
	{
		EXPECT_TRUE(
			arbiter.register_consumer(zircon_test_cancel_make_consumer(
				&owners[index], index, "slot")));
	}

	EXPECT_EQ(arbiter.get_consumer_count(),
		ZIRCON_DEF_CANCEL_ARBITER_MAX_CONSUMERS);

	// the overflow consumer is rejected loudly but NOT fatally (the
	// guard logs an error and returns false — an assert would kill the
	// whole boot the suite runs inside)
	EXPECT_FALSE(
		arbiter.register_consumer(zircon_test_cancel_make_consumer(
			&owners[ZIRCON_DEF_CANCEL_ARBITER_MAX_CONSUMERS], 200,
			"overflow")));

	EXPECT_EQ(arbiter.get_consumer_count(),
		ZIRCON_DEF_CANCEL_ARBITER_MAX_CONSUMERS);

	// and the registry keeps working after the rejected insert
	owners[0].m_is_active = true;
	owners[0].m_dismiss_result = true;

	const char* p_consumed_name = nullptr;

	EXPECT_TRUE(arbiter.handle_cancel(&p_consumed_name));
	EXPECT_STREQ(p_consumed_name, "slot");
	EXPECT_EQ(owners[0].m_dismiss_calls, 1);
}

TEST(Zircon_Core, CancelArbiter_ClearDropsRegistrations)
{
	zircon_cancel_arbiter arbiter;

	zircon_test_cancel_owner owner{true, true, 0};

	EXPECT_TRUE(arbiter.register_consumer(
		zircon_test_cancel_make_consumer(&owner, 10, "dropped")));
	EXPECT_EQ(arbiter.get_consumer_count(), 1);

	// session shutdown clears the registry — the owners die with the
	// session, so a stale entry would probe freed state
	arbiter.clear();

	EXPECT_EQ(arbiter.get_consumer_count(), 0);
	EXPECT_FALSE(arbiter.handle_cancel());
	EXPECT_EQ(owner.m_dismiss_calls, 0);
}

// the REAL session-registered consumers (initialize registers them —
// nothing is re-created test-side): ESC with a selection dismisses the
// selection through the arbiter; with a gizmo drag published AND a
// selection the drag wins by priority and the selection survives the
// same event. The imgui-state consumers (popup, text input) probe
// inactive headless — the main manager has no wrapper here, which is
// itself part of the proof
TEST(Zircon_Editor, CancelArbiter_SessionConsumersSelectionAndDrag)
{
	constexpr const char* _k_test_folder = "z19_session_consumers_test";

	// fresh journal per run (the session's history opens one)
	{
		auto* p_config = new kotek::core::ktkFrameworkConfig();
		auto* p_filesystem = new kotek::core::ktkFileSystem();

		p_filesystem->Initialize(p_config);

		zircon_test_cancel_remove_streaming_folder(
			p_filesystem, _k_test_folder);

		p_filesystem->Shutdown();

		delete p_filesystem;
		delete p_config;
	}

	// heap allocated like every history fixture (the console alone is
	// ~1 MB of stack)
	zircon_test_cancel_env& env = *new zircon_test_cancel_env();
	env.initialize(_k_test_folder);

	zircon_session_editor* p_session = env.session();

	ASSERT_NE(p_session, nullptr);

	zircon_editor_ui_state* p_ui_state = p_session->get_ui_state();
	zircon_cancel_arbiter* p_arbiter = p_session->get_cancel_arbiter();

	ASSERT_NE(p_ui_state, nullptr);
	ASSERT_NE(p_arbiter, nullptr);

	// the default set registered at initialize: gizmo drag, popup,
	// text input, selection (the drag-drop slot stays reserved)
	EXPECT_EQ(p_arbiter->get_consumer_count(), 4);

	kotek::entity_t entity =
		env.factory.create_entity(env.ecs_context());

	p_ui_state->set_selected_entity(entity);

	const char* p_consumed_name = nullptr;

	// ESC 1: the selection is the only active consumer
	EXPECT_TRUE(p_arbiter->handle_cancel(&p_consumed_name));
	EXPECT_STREQ(p_consumed_name, "entity_selection");
	EXPECT_TRUE(p_ui_state->get_selected_entity() ==
		kotek::ktk::kInvalidECSEntity);

	// ESC 2: nothing active — the frame just continues
	p_consumed_name = nullptr;
	EXPECT_FALSE(p_arbiter->handle_cancel(&p_consumed_name));
	EXPECT_EQ(p_consumed_name, nullptr);

	// a gizmo drag published through the overlay POD (what the pass
	// writes every frame mid-drag) PLUS a fresh selection: the drag
	// outranks the selection and the same event must NOT fall through
	p_ui_state->get_gizmo_overlay_state().m_is_drag_active = true;
	p_ui_state->set_selected_entity(entity);

	EXPECT_TRUE(p_arbiter->handle_cancel(&p_consumed_name));
	EXPECT_STREQ(p_consumed_name, "gizmo_drag");
	EXPECT_TRUE(
		p_ui_state->get_gizmo_overlay_state().m_cancel_drag_requested);
	EXPECT_TRUE(p_ui_state->get_selected_entity() == entity);

	// the pass side would now honor the request (the next test pins
	// that half); simulate the drag ending — then the selection is the
	// top consumer again
	p_ui_state->get_gizmo_overlay_state().m_is_drag_active = false;
	p_ui_state->get_gizmo_overlay_state().m_cancel_drag_requested =
		false;

	EXPECT_TRUE(p_arbiter->handle_cancel(&p_consumed_name));
	EXPECT_STREQ(p_consumed_name, "entity_selection");
	EXPECT_TRUE(p_ui_state->get_selected_entity() ==
		kotek::ktk::kInvalidECSEntity);

	env.shutdown();
	delete &env;
}

// the drag-CANCEL half of the Z6 drag fixtures: mid-drag the component
// holds the live preview; ESC arbitration calls the pass's
// cancel_drag_edit, which restores the drag-START state and journals
// NOTHING — the exact counterpart of the commit_drag_edit contract the
// P2e test pins (state restored, the recorded-command count unmoved)
TEST(Zircon_Editor, CancelArbiter_GizmoDragCancelRestoresWithoutJournal)
{
	constexpr const char* _k_test_folder = "z19_gizmo_cancel_test";

	// fresh journal per run
	{
		auto* p_config = new kotek::core::ktkFrameworkConfig();
		auto* p_filesystem = new kotek::core::ktkFileSystem();

		p_filesystem->Initialize(p_config);

		zircon_test_cancel_remove_streaming_folder(
			p_filesystem, _k_test_folder);

		p_filesystem->Shutdown();

		delete p_filesystem;
		delete p_config;
	}

	zircon_test_cancel_env& env = *new zircon_test_cancel_env();
	env.initialize(_k_test_folder);

	zircon_editor_command_history* p_history =
		env.session()->get_command_history();

	ASSERT_NE(p_history, nullptr);
	ASSERT_EQ(p_history->get_total_recorded_commands(), 0);

	// unjournaled scene setup: one entity with a transform
	kotek::entity_t entity =
		env.factory.create_entity(env.ecs_context());

	env.factory.create_component(env.ecs_context(), entity,
		eZirconComponentType::kzircon_component_transform);

	zircon_component_transform* p_transform =
		static_cast<zircon_component_transform*>(
			env.factory.get_component_by_enum(env.ecs_context(), entity,
				eZirconComponentType::kzircon_component_transform));

	ASSERT_NE(p_transform, nullptr);

	const float half_sqrt2 = 0.70710678118654752f;

	p_transform->set_position(kotek::math::vec3f_t(1.0f, 2.0f, 3.0f));
	p_transform->set_scale(kotek::math::vec3f_t(1.0f, 1.0f, 1.0f));
	p_transform->set_rotation(
		kotek::math::quatf_t(0.0f, 0.0f, 0.0f, 1.0f));

	// the pass's drag flow up to the ESC: the start state captured at
	// mouse-down (the drag context's start capture)...
	const float start_position[3] = {1.0f, 2.0f, 3.0f};
	const float start_scale[3] = {1.0f, 1.0f, 1.0f};
	const float start_rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	// ...the live preview writes the dragged state straight into the
	// component (a translate AND a rotate, so the restore is proven on
	// every channel)...
	p_transform->set_position(kotek::math::vec3f_t(4.0f, 2.0f, 3.0f));
	p_transform->set_scale(kotek::math::vec3f_t(2.0f, 2.0f, 2.0f));
	p_transform->set_rotation(
		kotek::math::quatf_t(0.0f, half_sqrt2, 0.0f, half_sqrt2));

	// ...and the ESC arbitration fires the pass's cancel path — the
	// exact static the pass calls from OnUpdate when
	// m_cancel_drag_requested is set
	ASSERT_TRUE(zircon_pass_editor_gizmo_own::cancel_drag_edit(
		&env.factory, env.ecs_context(), entity, start_position,
		start_scale, start_rotation));

	// the journal saw NOTHING of the aborted drag
	EXPECT_EQ(p_history->get_total_recorded_commands(), 0);

	// the component is back at the drag-START state on every channel
	p_transform = static_cast<zircon_component_transform*>(
		env.factory.get_component_by_enum(env.ecs_context(), entity,
			eZirconComponentType::kzircon_component_transform));

	ASSERT_NE(p_transform, nullptr);

	EXPECT_FLOAT_EQ(p_transform->get_position().x(), 1.0f);
	EXPECT_FLOAT_EQ(p_transform->get_position().y(), 2.0f);
	EXPECT_FLOAT_EQ(p_transform->get_position().z(), 3.0f);
	EXPECT_FLOAT_EQ(p_transform->get_scale().x(), 1.0f);
	EXPECT_FLOAT_EQ(p_transform->get_rotation().y(), 0.0f);
	EXPECT_FLOAT_EQ(p_transform->get_rotation().w(), 1.0f);

	env.shutdown();
	delete &env;
}

		#endif
	#endif
#endif
