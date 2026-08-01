#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <algorithm>
		#include <random>

		#include "../../editor/commands/zircon_command_history.h"
		#include "../../editor/commands/zircon_command_create_entity.h"
		#include "../../editor/commands/zircon_command_delete_entity.h"
		#include "../../editor/commands/zircon_command_add_component_to_entity.h"
		#include "../../editor/commands/zircon_command_delete_component_from_entity.h"
		#include "../../editor/commands/zircon_command_edit_component_state.h"
		#include "../../editor/session/zircon_session_editor.h"
		#include "../../editor/session/zircon_session_editor_manager.h"
		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_transform.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_graph_pass_editor_gizmo_own.h"
		#include "../../world/zircon_world.h"
		#include "../../core/zircon_config.h"

		#ifndef ZIRCON_DEF_UNIT_TEST_COMMAND_HISTORY
			#define ZIRCON_DEF_UNIT_TEST_COMMAND_HISTORY 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_COMMAND_HISTORY == 1

// functional proof for task Z6 (full-retention undo/redo): the suite
// drives ~100k randomized commands across all built-in command types
// through zircon_editor_command_history and verifies the manager's
// promises: (a) every command is recorded, (b) undoing everything
// restores the initial world state, (c) redoing everything replays
// to the exact same final state, (d) journal + snapshots stay within
// a stated compression budget

namespace
{
	constexpr kotek::uint32_t _k_test_command_count = 100000;
	constexpr kotek::uint32_t _k_test_max_alive_entities = 200;

	// components that are cheap to create headless and have a full
	// json roundtrip through the generated fields
	constexpr eZirconComponentType _k_component_pool[] = {
		eZirconComponentType::kzircon_component_transform,
		eZirconComponentType::kzircon_component_geometry,
		eZirconComponentType::kzircon_component_actor,
		eZirconComponentType::kzircon_component_frustum,
		eZirconComponentType::kzircon_component_bounding_sphere,
		eZirconComponentType::kzircon_component_camera,
		eZirconComponentType::kzircon_component_ui_surface,
		eZirconComponentType::kzircon_component_sdk_scene_name};

	constexpr kotek::uint32_t _k_component_pool_size =
		sizeof(_k_component_pool) / sizeof(_k_component_pool[0]);

	struct zircon_test_entity_model
	{
		kotek::uint32_t m_recorded_id;
		kotek::uint32_t m_live_id;
		kotek::uint32_t m_components_mask;
		bool m_alive;
	};

	/// @brief \~english one executed operation in the test's own
	/// log; the log mirrors the history's preferred path (its redo
	/// tail is truncated on branch, exactly what the history's
	/// preferred child does)
	struct zircon_test_op_record
	{
		// 1 = create entity, 2 = delete entity, 3 = add component,
		// 4 = delete component, 5 = edit component state
		kotek::uint32_t m_type;
		/// @brief \~english index into the entity model vector
		kotek::uint32_t m_model_index;
		kotek::uint32_t m_component_index;
	};

	kotek::uint64_t zircon_test_fnv1a(
		const void* p_data, kotek::size_t size,
		kotek::uint64_t hash = 14695981039346656037ULL
	) noexcept
	{
		const unsigned char* p_bytes =
			reinterpret_cast<const unsigned char*>(p_data);

		for (kotek::size_t i = 0; i < size; ++i)
		{
			hash ^= p_bytes[i];
			hash *= 1099511628211ULL;
		}

		return hash;
	}

	/// @brief \~english canonical (entity-id agnostic) world
	/// serialization: per entity an ordered list of
	/// "type:json_state" entries without the entity id, entities
	/// sorted by content; two worlds with the same logical state
	/// produce the same string even when their entity ids differ
	void zircon_test_serialize_world(
		zircon_factory* p_factory,
		zircon_ecs_context_t* p_context,
		kotek::uint32_t entity_watermark,
		kotek::hybrid_vector_t<char, 4096>& output
	)
	{
		output.clear();

		kotek::hybrid_vector_t<kotek::entity_t, 256> entities;
		entities.resize(entity_watermark + 16);

		const kotek::uint32_t entity_count =
			p_factory->get_all_entities(
				p_context,
				entity_watermark + 16,
				entities.data(),
				static_cast<kotek::uint32_t>(entities.size())
			);

		entities.resize(entity_count);

		kotek::hybrid_vector_t<char, 4096> flat;
		kotek::hybrid_vector_t<
			kotek::pair_t<kotek::uint32_t, kotek::uint32_t>,
			256>
			spans;

		for (const kotek::entity_t& entity : entities)
		{
			const kotek::uint32_t span_offset =
				static_cast<kotek::uint32_t>(flat.size());

			for (kotek::uint32_t comp_index = 0;
			     comp_index < _k_component_pool_size;
			     ++comp_index)
			{
				const eZirconComponentType component_type =
					_k_component_pool[comp_index];

				if (p_factory->has_component(
						p_context, entity, component_type
					) == false)
				{
					continue;
				}

				zircon_component_interface* p_component =
					p_factory->get_component_by_enum(
						p_context, entity, component_type
					);

				if (p_component == nullptr)
					continue;

				kotek::ktk::json::value serialized_state =
					zircon_serialize_component(p_component);

				auto state_string =
					kotek::ktk::json::serialize(
						serialized_state
					);

				const kotek::uint32_t type_as_int =
					static_cast<kotek::uint32_t>(
						component_type
					);

				flat.push_back('C');
				flat.push_back(
					static_cast<char>('0' + (type_as_int % 10))
				);
				flat.push_back(
					static_cast<char>(
						'0' + ((type_as_int / 10) % 10)
					)
				);
				flat.push_back(':');
				flat.insert(
					flat.end(),
					state_string.data(),
					state_string.data() +
						state_string.size()
				);
				flat.push_back(';');
			}

			const kotek::uint32_t span_size =
				static_cast<kotek::uint32_t>(flat.size()) -
				span_offset;

			spans.push_back({span_offset, span_size});
		}

		std::sort(
			spans.begin(),
			spans.end(),
			[&flat](
				const kotek::pair_t<
					kotek::uint32_t,
					kotek::uint32_t>& left,
				const kotek::pair_t<
					kotek::uint32_t,
					kotek::uint32_t>& right
			)
			{
				const kotek::uint32_t compare_size =
					left.second < right.second
					? left.second
					: right.second;

				const int comparison = memcmp(
					flat.data() + left.first,
					flat.data() + right.first,
					compare_size
				);

				if (comparison != 0)
					return comparison < 0;

				return left.second < right.second;
			}
		);

		output.push_back('W');
		output.push_back('1');

		for (const auto& span : spans)
		{
			output.insert(
				output.end(),
				flat.data() + span.first,
				flat.data() + span.first + span.second
			);
		}
	}

	kotek::uint64_t zircon_test_world_hash(
		zircon_factory* p_factory,
		zircon_ecs_context_t* p_context,
		kotek::uint32_t entity_watermark,
		kotek::hybrid_vector_t<char, 4096>& scratch
	)
	{
		zircon_test_serialize_world(
			p_factory, p_context, entity_watermark, scratch
		);

		return zircon_test_fnv1a(scratch.data(), scratch.size());
	}

	/// @brief \~english byte comparison for hybrid byte vectors
	/// (the hybrid vector has no operator==)
	bool zircon_test_byte_vectors_equal(
		const kotek::hybrid_vector_t<char, 4096>& left,
		const kotek::hybrid_vector_t<char, 4096>& right
	)
	{
		if (left.size() != right.size())
			return false;

		if (left.empty())
			return true;

		return memcmp(left.data(), right.data(), left.size()) == 0;
	}

	/// @brief \~english headless editor environment: real
	/// filesystem, factory, world, session editor manager with one
	/// session and its command history
	struct zircon_test_history_env
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

		void initialize(
			const char* p_streaming_folder_name,
			kotek::uint32_t entity_count_max_limit = 65536
		)
		{
			this->filesystem.Initialize(&this->framework_config);

			this->main_manager.Set_FileSystem(&this->filesystem);
			this->main_manager.Set_FrameworkConfig(
				&this->framework_config
			);

			this->factory.Initialize(
				&this->engine_config, &this->console, &this->input
			);

			this->world.initialize(
				"zircon_z6_test_world",
				&this->engine_config,
				&this->console,
				&this->input,
				&this->factory,
				entity_count_max_limit
			);

			this->session_manager.initialize(
				&this->engine_config, &this->main_manager
			);

			kotek::uint8_t session_id =
				this->session_manager.create_session();

			this->session_manager.set_current_session_id(
				session_id
			);

			zircon_session_editor* p_session =
				this->session_manager.get_session(session_id);

			p_session->initialize(
				"zircon_z6_test_session",
				session_id,
				&this->world,
				&this->session_manager,
				&this->main_manager,
				&this->console,
				&this->filesystem,
				p_streaming_folder_name
			);
		}

		void shutdown(void)
		{
			this->session_manager.shutdown();
			this->world.shutdown(&this->factory);
			this->factory.Shutdown();
			this->filesystem.Shutdown();
		}

		zircon_editor_command_history* history(void)
		{
			return this->session_manager
				.get_session(
					this->session_manager
						.get_current_session_id()
				)
				->get_command_history();
		}

		zircon_ecs_context_t* ecs_context(void)
		{
			return this->world.get_ecs_context();
		}
	};

	void zircon_test_remove_streaming_folder(
		kotek::core::ktkIFileSystem* p_filesystem,
		const char* p_folder_name
	)
	{
		ktk_filesystem_path path;

		p_filesystem->Make_Path(
			path,
			kotek::core::eFolderIndex::
				kFolderIndex_DataUser_SDK_Scenes
		);

		path /= p_folder_name;

		if (p_filesystem->Is_Exists(path))
		{
			std::filesystem::remove_all(
				std::filesystem::path(
					reinterpret_cast<const char*>(
						path.u8string().data()
					)
				)
			);
		}
	}
} // namespace

TEST(Zircon_Editor, CommandHistory_Stress_100k_Commands)
{
	constexpr const char* _k_test_folder = "z6_stress_test";

	// fresh journal per run
	{
		auto* p_config = new kotek::core::ktkFrameworkConfig();
		auto* p_filesystem = new kotek::core::ktkFileSystem();

		p_filesystem->Initialize(p_config);

		zircon_test_remove_streaming_folder(
			p_filesystem, _k_test_folder
		);

		p_filesystem->Shutdown();

		delete p_filesystem;
		delete p_config;
	}

	// NOTE: the fixture is heap allocated on purpose: ktkConsole is
	// about a megabyte by itself (see the note in
	// zircon_unit_tests_game.cpp) and a stack allocated fixture
	// overflows the 1 Mb default thread stack
	zircon_test_history_env& env = *new zircon_test_history_env();
	env.initialize(_k_test_folder);

	zircon_editor_command_history* p_history = env.history();

	ASSERT_NE(p_history, nullptr);
	ASSERT_TRUE(p_history->get_total_recorded_commands() == 0);

	kotek::hybrid_vector_t<char, 4096> world_state_origin;
	kotek::hybrid_vector_t<char, 4096> world_state_final;
	kotek::hybrid_vector_t<char, 4096> world_state_restored;

	kotek::uint32_t entity_watermark = 16;

	zircon_test_serialize_world(
		&env.factory,
		env.ecs_context(),
		entity_watermark,
		world_state_origin
	);

	const kotek::uint64_t origin_hash = zircon_test_fnv1a(
		world_state_origin.data(), world_state_origin.size()
	);

	kotek::hybrid_vector_t<zircon_test_entity_model, 256> model;
	kotek::hybrid_vector_t<zircon_test_op_record, 1024> model_log;
	kotek::uint32_t model_cursor = 0;

	/// @brief \~english indices of alive entities inside model,
	/// kept small by _k_test_max_alive_entities so picks are O(n)
	/// over a tiny vector
	kotek::hybrid_vector_t<kotek::uint32_t, 256> alive_indices;

	auto model_remove_from_alive =
		[&alive_indices](kotek::uint32_t model_index)
	{
		for (kotek::uint32_t i = 0; i < alive_indices.size(); ++i)
		{
			if (alive_indices[i] == model_index)
			{
				alive_indices[i] = alive_indices.back();
				alive_indices.pop_back();
				return;
			}
		}
	};

	kotek::uint64_t executed_count = 0;

	std::mt19937_64 rng(0x26'07'23);

	auto pick_alive_entity =
		[&alive_indices, &rng]() -> kotek::uint32_t
	{
		if (alive_indices.empty())
			return zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID;

		return alive_indices[rng() % alive_indices.size()];
	};

	auto count_alive = [&alive_indices]() -> kotek::uint32_t
	{
		return static_cast<kotek::uint32_t>(alive_indices.size());
	};

	auto issue_create = [&]()
	{
		unsigned char* p_memory =
			p_history->allocate_memory_for_command(
				sizeof(zircon_command_create_entity),
				"zircon_command_create_entity"
			);

		zircon_command_create_entity* p_command =
			new (p_memory) zircon_command_create_entity(
				&env.session_manager, &env.factory
			);

		p_history->ExecuteCommand(p_command);

		const kotek::uint32_t new_id =
			static_cast<kotek::uint32_t>(
				p_command->GetEntityID().id
			);

		if (new_id > entity_watermark)
			entity_watermark = new_id;

		const kotek::uint32_t model_index =
			static_cast<kotek::uint32_t>(model.size());

		model.push_back({new_id, new_id, 0, true});
		alive_indices.push_back(model_index);

		if (model_cursor < model_log.size())
		{
			model_log.resize(model_cursor);
		}

		model_log.push_back({1, model_index, 0});
		++model_cursor;
		++executed_count;
	};

	auto issue_delete = [&](kotek::uint32_t model_index)
	{
		zircon_test_entity_model& entry = model[model_index];

		fprintf(
			stderr,
			"[z6 op]: delete entity recorded %u live %u\n",
			entry.m_recorded_id, entry.m_live_id
		);

		unsigned char* p_memory =
			p_history->allocate_memory_for_command(
				sizeof(zircon_command_delete_entity),
				"zircon_command_delete_entity"
			);

		zircon_command_delete_entity* p_command =
			new (p_memory) zircon_command_delete_entity(
				&env.session_manager,
				&env.factory,
				kotek::entity_t{entry.m_live_id}
			);

		p_history->ExecuteCommand(p_command);

		entry.m_alive = false;
		model_remove_from_alive(model_index);

		if (model_cursor < model_log.size())
		{
			model_log.resize(model_cursor);
		}

		model_log.push_back({2, model_index, 0});
		++model_cursor;
		++executed_count;
	};

	auto issue_add_component = [&](kotek::uint32_t model_index)
	{
		zircon_test_entity_model& entry = model[model_index];

		kotek::uint32_t component_index =
			zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID;

		{
			kotek::hybrid_vector_t<kotek::uint32_t, 8> missing;

			for (kotek::uint32_t i = 0;
			     i < _k_component_pool_size;
			     ++i)
			{
				if ((entry.m_components_mask & (1u << i)) == 0)
				{
					missing.push_back(i);
				}
			}

			if (missing.empty())
				return;

			component_index = missing[rng() % missing.size()];
		}

		const char* p_component_name =
			zircon_translate_component_type_enum_to_string(
				_k_component_pool[component_index]
			);

		fprintf(
			stderr,
			"[z6 op]: add component %u to recorded %u live %u\n",
			component_index, entry.m_recorded_id, entry.m_live_id
		);

		unsigned char* p_memory =
			p_history->allocate_memory_for_command(
				sizeof(zircon_command_add_component_to_entity),
				"zircon_command_add_component_to_entity"
			);

		zircon_command_add_component_to_entity* p_command =
			new (p_memory)
				zircon_command_add_component_to_entity(
					&env.session_manager,
					kotek::entity_t{entry.m_live_id},
					p_component_name
				);

		p_history->ExecuteCommand(p_command);

		entry.m_components_mask |= (1u << component_index);

		if (model_cursor < model_log.size())
		{
			model_log.resize(model_cursor);
		}

		model_log.push_back({3, model_index, component_index});
		++model_cursor;
		++executed_count;
	};

	auto issue_delete_component = [&](kotek::uint32_t model_index)
	{
		zircon_test_entity_model& entry = model[model_index];

		kotek::uint32_t component_index =
			zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID;

		{
			kotek::hybrid_vector_t<kotek::uint32_t, 8> present;

			for (kotek::uint32_t i = 0;
			     i < _k_component_pool_size;
			     ++i)
			{
				if (entry.m_components_mask & (1u << i))
				{
					present.push_back(i);
				}
			}

			if (present.empty())
				return;

			component_index = present[rng() % present.size()];
		}

		fprintf(
			stderr,
			"[z6 op]: delete component %u from recorded %u live "
			"%u\n",
			component_index, entry.m_recorded_id, entry.m_live_id
		);

		const char* p_component_name =
			zircon_translate_component_type_enum_to_string(
				_k_component_pool[component_index]
			);

		unsigned char* p_memory =
			p_history->allocate_memory_for_command(
				sizeof(
					zircon_command_delete_component_from_entity
				),
				"zircon_command_delete_component_from_entity"
			);

		zircon_command_delete_component_from_entity* p_command =
			new (p_memory)
				zircon_command_delete_component_from_entity(
					&env.session_manager,
					&env.factory,
					kotek::entity_t{entry.m_live_id},
					p_component_name
				);

		p_history->ExecuteCommand(p_command);

		entry.m_components_mask &= ~(1u << component_index);

		if (model_cursor < model_log.size())
		{
			model_log.resize(model_cursor);
		}

		model_log.push_back({4, model_index, component_index});
		++model_cursor;
		++executed_count;
	};

		// a component mutation must go through the journal like any
		// other command: the redo/restore replay reproduces the
		// world from the journal only, a direct in-place edit
		// would diverge (the "same final state" promise, task Z6)
	auto issue_edit_component = [&](kotek::uint32_t model_index)
	{
		zircon_test_entity_model& entry = model[model_index];

		// the transform is component pool index 0
		if ((entry.m_components_mask & 1u) == 0)
			return;

		zircon_component_transform* p_transform =
			static_cast<zircon_component_transform*>(
				env.factory.get_component_by_enum(
					env.ecs_context(),
					kotek::entity_t{entry.m_live_id},
					eZirconComponentType::
						kzircon_component_transform
				)
			);

		if (p_transform == nullptr)
			return;

		const kotek::uint32_t salt =
			static_cast<kotek::uint32_t>(rng());

		// integer valued floats keep the json text
		// deterministic across serialize/deserialize
		const kotek::math::vec3f_t previous_position =
			p_transform->get_position();

		p_transform->set_position(
			kotek::math::vec3f_t{
				static_cast<float>(salt % 97),
				static_cast<float>((salt / 7) % 89),
				static_cast<float>((salt / 13) % 83)}
		);

		kotek::ktk::json::value state_after =
			zircon_serialize_component(p_transform);

		p_transform->set_position(previous_position);

		fprintf(
			stderr,
			"[z6 op]: edit transform of recorded %u live %u\n",
			entry.m_recorded_id, entry.m_live_id
		);

		unsigned char* p_memory =
			p_history->allocate_memory_for_command(
				sizeof(zircon_command_edit_component_state),
				"zircon_command_edit_component_state"
			);

		zircon_command_edit_component_state* p_command =
			new (p_memory) zircon_command_edit_component_state(
				&env.session_manager,
				&env.factory,
				kotek::entity_t{entry.m_live_id},
				zircon_translate_component_type_enum_to_string(
					eZirconComponentType::
						kzircon_component_transform
				),
				state_after
			);

		p_history->ExecuteCommand(p_command);

		if (model_cursor < model_log.size())
		{
			model_log.resize(model_cursor);
		}

		model_log.push_back({5, model_index, 0});
		++model_cursor;
		++executed_count;
	};

	auto issue_undo = [&]()
	{
		const kotek::uint32_t cursor_before =
			p_history->get_cursor_node_id();
		p_history->Undo();

		const kotek::uint32_t cursor_after =
			p_history->get_cursor_node_id();

		if (cursor_after == cursor_before)
			return;

		if (model_cursor == 0)
		{
			FAIL() << "model log is empty but undo moved the "
			        << "cursor";
			return;
		}

		const zircon_test_op_record& op =
			model_log[model_cursor - 1];

		zircon_test_entity_model* p_entry =
			&model[op.m_model_index];

		switch (op.m_type)
		{
		case 1:
		{
			p_entry->m_alive = false;
			model_remove_from_alive(op.m_model_index);
			break;
		}
		case 2:
		{
			p_entry->m_alive = true;
			p_entry->m_live_id = static_cast<kotek::uint32_t>(
				p_history
					->get_live_entity_id(
						kotek::entity_t{p_entry->m_recorded_id}
					)
					.id
			);
			if (p_entry->m_live_id > entity_watermark)
				entity_watermark = p_entry->m_live_id;
			alive_indices.push_back(op.m_model_index);
			break;
		}
		case 3:
		{
			p_entry->m_components_mask &=
				~(1u << op.m_component_index);
			break;
		}
		case 4:
		{
			p_entry->m_components_mask |=
				(1u << op.m_component_index);
			break;
		}
		case 5:
		{
			// an edit changes no structure the model tracks
			break;
		}
		default:
		{
			FAIL() << "unknown op in the model log";
			break;
		}
		}

		--model_cursor;
	};

	auto issue_redo = [&]()
	{
		const kotek::uint32_t cursor_before =
			p_history->get_cursor_node_id();

		p_history->Redo();

		const kotek::uint32_t cursor_after =
			p_history->get_cursor_node_id();

		if (cursor_after == cursor_before)
			return;

		if (model_cursor >= model_log.size())
		{
			FAIL() << "model log is exhausted but redo moved the "
			       << "cursor";
			return;
		}

		const zircon_test_op_record& op =
			model_log[model_cursor];

		zircon_test_entity_model* p_entry =
			&model[op.m_model_index];

		switch (op.m_type)
		{
		case 1:
		{
			p_entry->m_alive = true;
			p_entry->m_live_id = static_cast<kotek::uint32_t>(
				p_history
					->get_live_entity_id(
						kotek::entity_t{p_entry->m_recorded_id}
					)
					.id
			);
			if (p_entry->m_live_id > entity_watermark)
				entity_watermark = p_entry->m_live_id;
			alive_indices.push_back(op.m_model_index);
			break;
		}
		case 2:
		{
			p_entry->m_alive = false;
			model_remove_from_alive(op.m_model_index);
			break;
		}
		case 3:
		{
			p_entry->m_components_mask |=
				(1u << op.m_component_index);
			break;
		}
		case 4:
		{
			p_entry->m_components_mask &=
				~(1u << op.m_component_index);
			break;
		}
		case 5:
		{
			// an edit changes no structure the model tracks
			break;
		}
		default:
		{
			FAIL() << "unknown op in the model log";
			break;
		}
		}

		++model_cursor;
	};

	for (kotek::uint32_t iteration = 0;
	     iteration < _k_test_command_count;
	     ++iteration)
	{
		const kotek::uint32_t roll =
			static_cast<kotek::uint32_t>(rng() % 100);

		fprintf(
			stderr, "[z6 op]: %u roll %u cursor %u\n", iteration,
			roll, p_history->get_cursor_node_id()
		);

		if (roll < 20)
		{
			if (count_alive() < _k_test_max_alive_entities)
			{
				issue_create();
			}
			else
			{
				const kotek::uint32_t target =
					pick_alive_entity();

				if (target !=
				    zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID)
				{
					issue_delete(target);
				}
				else
				{
					issue_undo();
				}
			}
		}
		else if (roll < 30)
		{
			const kotek::uint32_t target = pick_alive_entity();

			if (target !=
			    zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID)
			{
				issue_delete(target);
			}
			else
			{
				issue_create();
			}
		}
		else if (roll < 50)
		{
			const kotek::uint32_t target = pick_alive_entity();

			if (target !=
			    zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID)
			{
				issue_add_component(target);
			}
			else
			{
				issue_create();
			}
		}
		else if (roll < 65)
		{
			const kotek::uint32_t target = pick_alive_entity();

			if (target !=
			    zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID)
			{
				issue_edit_component(target);
			}
			else
			{
				issue_create();
			}
		}
		else if (roll < 75)
		{
			const kotek::uint32_t target = pick_alive_entity();

			if (target !=
			    zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID)
			{
				issue_delete_component(target);
			}
			else
			{
				issue_create();
			}
		}
		else if (roll < 90)
		{
			issue_undo();
		}
		else
		{
			issue_redo();
		}
	}

	// (a) every executed command must be recorded
	EXPECT_EQ(
		executed_count,
		p_history->get_total_recorded_commands()
	);

	const kotek::uint32_t final_node_id =
		p_history->get_cursor_node_id();

	// the history's watermark tracks every entity id it has ever
	// observed (pico_ecs never recycles ids, re-executions mint
	// fresh ones), resync the scan range from it
	if (p_history->get_entity_watermark() > entity_watermark)
	{
		entity_watermark = p_history->get_entity_watermark();
	}

	zircon_test_serialize_world(
		&env.factory,
		env.ecs_context(),
		entity_watermark,
		world_state_final
	);

	const kotek::uint64_t final_hash = zircon_test_fnv1a(
		world_state_final.data(), world_state_final.size()
	);

	// (b) full undo to the origin restores the initial state
	kotek::uint64_t undone_steps = 0;

	while (p_history->get_cursor_node_id() != 0)
	{
		p_history->Undo();
		++undone_steps;
	}

	KOTEK_MESSAGE(
		"[z6 stress]: {} commands executed, {} undo steps to "
		"the origin",
		executed_count,
		undone_steps
	);

	// undoing delete-commands minted fresh entity ids too, resync
	// the scan range so nothing alive can be missed
	if (p_history->get_entity_watermark() > entity_watermark)
	{
		entity_watermark = p_history->get_entity_watermark();
	}

	zircon_test_serialize_world(
		&env.factory,
		env.ecs_context(),
		entity_watermark,
		world_state_restored
	);

	const kotek::uint64_t undone_hash = zircon_test_fnv1a(
		world_state_restored.data(),
		world_state_restored.size()
	);

	EXPECT_EQ(origin_hash, undone_hash);
	EXPECT_TRUE(zircon_test_byte_vectors_equal(
		world_state_origin, world_state_restored
	));

	// (c) full redo replays to the exact same final state
	while (p_history->get_cursor_node_id() != final_node_id)
	{
		p_history->Redo();
	}

	// redo minted fresh entity ids again, resync the scan range
	if (p_history->get_entity_watermark() > entity_watermark)
	{
		entity_watermark = p_history->get_entity_watermark();
	}

	zircon_test_serialize_world(
		&env.factory,
		env.ecs_context(),
		entity_watermark,
		world_state_restored
	);

	const kotek::uint64_t redone_hash = zircon_test_fnv1a(
		world_state_restored.data(),
		world_state_restored.size()
	);

	// divergence diagnostics: dump both canonical world states so
	// the differing component json can be inspected offline (must
	// run BEFORE the EXPECTs: run_unit_tests sets gtest
	// break_on_failure, the first failed EXPECT aborts the
	// process)
	const bool is_redone_equal = zircon_test_byte_vectors_equal(
		world_state_final, world_state_restored
	);

	if (is_redone_equal == false)
	{
		const kotek::size_t min_size =
			world_state_final.size() < world_state_restored.size()
			? world_state_final.size()
			: world_state_restored.size();

		kotek::size_t first_diff = min_size;

		for (kotek::size_t k = 0; k < min_size; ++k)
		{
			if (world_state_final[k] != world_state_restored[k])
			{
				first_diff = k;
				break;
			}
		}

		fprintf(
			stderr,
			"[z6 stress]: redo divergence, final size %zu, "
			"redone size %zu, first diff at byte %zu\n",
			world_state_final.size(),
			world_state_restored.size(), first_diff
		);

		const kotek::size_t dump_from =
			first_diff > 200 ? first_diff - 200 : 0;

		int final_left = static_cast<int>(
			world_state_final.size() - dump_from
		);
		int redone_left = static_cast<int>(
			world_state_restored.size() - dump_from
		);

		if (final_left > 400)
			final_left = 400;
		if (redone_left > 400)
			redone_left = 400;

		fprintf(
			stderr, "[z6 stress]: final:  %.*s\n", final_left,
			world_state_final.data() + dump_from
		);
		fprintf(
			stderr, "[z6 stress]: redone: %.*s\n", redone_left,
			world_state_restored.data() + dump_from
		);
	}

	EXPECT_EQ(final_hash, redone_hash);
	EXPECT_TRUE(is_redone_equal);

	// (d) disk budget: the journal must compress to at least half
	// of its raw size (zstd on repetitive command payloads), and
	// the total disk usage must stay under an absolute cap
	const kotek::uint64_t journal_disk_size =
		p_history->get_journal_file_size();
	const kotek::uint64_t snapshot_disk_size =
		p_history->get_snapshot_file_size();
	const kotek::uint64_t journal_raw_size =
		p_history->get_journal_raw_entry_bytes();

	KOTEK_MESSAGE(
		"[z6 stress]: journal raw {} bytes, journal disk {} "
		"bytes, snapshots disk {} bytes, snapshots count {}",
		journal_raw_size,
		journal_disk_size,
		snapshot_disk_size,
		p_history->get_total_snapshot_count()
	);

	EXPECT_LE(journal_disk_size * 2, journal_raw_size + 4096)
		<< "journal compression must be at least 2x";

	constexpr kotek::uint64_t _k_disk_budget_bytes =
		64ULL * 1024ULL * 1024ULL;

	EXPECT_LE(
		journal_disk_size + snapshot_disk_size,
		_k_disk_budget_bytes
	);

	env.shutdown();
	delete &env;

	// retention across sessions: reopening the same journal must
	// restore the whole recorded history
	{
		zircon_test_history_env& env_reopen =
			*new zircon_test_history_env();
		env_reopen.initialize(_k_test_folder);

		zircon_editor_command_history* p_reopened_history =
			env_reopen.history();

		ASSERT_NE(p_reopened_history, nullptr);

		EXPECT_EQ(
			executed_count,
			p_reopened_history->get_total_recorded_commands()
		);

		env_reopen.shutdown();
		delete &env_reopen;
	}
}

TEST(Zircon_Editor, CommandHistory_Restore_Node_From_Snapshots)
{
	constexpr const char* _k_test_folder = "z6_restore_test";

	fprintf(stderr, "[z6]: restore test phase 1 (wipe)\n");

	{
		auto* p_config = new kotek::core::ktkFrameworkConfig();
		auto* p_filesystem = new kotek::core::ktkFileSystem();

		p_filesystem->Initialize(p_config);

		zircon_test_remove_streaming_folder(
			p_filesystem, _k_test_folder
		);

		p_filesystem->Shutdown();

		delete p_filesystem;
		delete p_config;
	}

	fprintf(stderr, "[z6]: restore test phase 2 (env init)\n");

	zircon_test_history_env& env = *new zircon_test_history_env();
	env.initialize(_k_test_folder);

	fprintf(stderr, "[z6]: restore test phase 3 (commands)\n");

	zircon_editor_command_history* p_history = env.history();

	ASSERT_NE(p_history, nullptr);

	// dense snapshots for the spot checks
	p_history->set_snapshot_interval(64);

	kotek::hybrid_vector_t<char, 4096> world_state_scratch;

	kotek::uint32_t entity_watermark = 16;

	kotek::hybrid_vector_t<
		kotek::pair_t<kotek::uint32_t, kotek::uint64_t>,
		1024>
		node_hashes;

	// the world state text at node 64 (the first snapshot node)
	// for diffing against the restored state
	kotek::hybrid_vector_t<char, 4096> world_state_at_64;

	auto record_current_hash = [&]()
	{
		const kotek::uint64_t hash = zircon_test_world_hash(
			&env.factory,
			env.ecs_context(),
			entity_watermark,
			world_state_scratch
		);

		node_hashes.push_back(
			{p_history->get_cursor_node_id(), hash}
		);

		if (p_history->get_cursor_node_id() == 64)
		{
			world_state_at_64.assign(
				world_state_scratch.begin(),
				world_state_scratch.end()
			);
		}
	};

	record_current_hash();

	// linear history: creates + component edits, no undo
	// interleaving
	constexpr kotek::uint32_t _k_linear_command_count = 700;

	kotek::hybrid_vector_t<kotek::uint32_t, 256> alive_entities;

	std::mt19937_64 rng(0xC0FFEE);

	for (kotek::uint32_t i = 0; i < _k_linear_command_count; ++i)
	{
		const kotek::uint32_t roll =
			static_cast<kotek::uint32_t>(rng() % 100);

		if (roll < 40 || alive_entities.empty())
		{
			unsigned char* p_memory =
				p_history->allocate_memory_for_command(
					sizeof(zircon_command_create_entity),
					"zircon_command_create_entity"
				);

			zircon_command_create_entity* p_command =
				new (p_memory) zircon_command_create_entity(
					&env.session_manager, &env.factory
				);

			p_history->ExecuteCommand(p_command);

			const kotek::uint32_t new_id =
				static_cast<kotek::uint32_t>(
					p_command->GetEntityID().id
				);

			if (new_id > entity_watermark)
				entity_watermark = new_id;

			alive_entities.push_back(new_id);
		}
		else
		{
			const kotek::uint32_t live_id =
				alive_entities[rng() % alive_entities.size()];

			const kotek::uint32_t component_index =
				static_cast<kotek::uint32_t>(
					rng() % _k_component_pool_size
				);

			const eZirconComponentType component_type =
				_k_component_pool[component_index];

			if (env.factory.has_component(
					env.ecs_context(),
					kotek::entity_t{live_id},
					component_type
				))
			{
				// mutate then delete the component so the
				// delta carries content
				if (component_type ==
				    eZirconComponentType::
					    kzircon_component_transform)
				{
					zircon_component_transform* p_transform =
						static_cast<
							zircon_component_transform*>(
							env.factory
								.get_component_by_enum(
									env.ecs_context(),
									kotek::entity_t{live_id},
									component_type
								)
						);

					if (p_transform)
					{
						p_transform->set_position(
							kotek::math::vec3f_t{
								static_cast<float>(i % 50),
								static_cast<float>(
									(i / 3) % 40
								),
								static_cast<float>(
									(i / 7) % 30
								)}
						);
					}
				}

				unsigned char* p_memory =
					p_history->allocate_memory_for_command(
						sizeof(
							zircon_command_delete_component_from_entity
						),
						"zircon_command_delete_component_from_"
						"entity"
					);

				zircon_command_delete_component_from_entity*
					p_command = new (p_memory)
						zircon_command_delete_component_from_entity(
							&env.session_manager,
							&env.factory,
							kotek::entity_t{live_id},
							zircon_translate_component_type_enum_to_string(
								component_type
							)
						);

				p_history->ExecuteCommand(p_command);
			}
			else
			{
				unsigned char* p_memory =
					p_history->allocate_memory_for_command(
						sizeof(
							zircon_command_add_component_to_entity
						),
						"zircon_command_add_component_to_entity"
					);

				zircon_command_add_component_to_entity*
					p_command = new (p_memory)
						zircon_command_add_component_to_entity(
							&env.session_manager,
							kotek::entity_t{live_id},
							zircon_translate_component_type_enum_to_string(
								component_type
							)
						);

				p_history->ExecuteCommand(p_command);
			}
		}

		record_current_hash();
	}

	ASSERT_EQ(
		node_hashes.size(),
		static_cast<kotek::size_t>(_k_linear_command_count) + 1
	);

	fprintf(stderr, "[z6]: restore test phase 4 (restore probes)\n");

	// deterministic bisect: restoring the snapshot node itself must
	// be exact, then step forward to find the first divergence
	for (kotek::uint32_t node_id = 64; node_id <= 256; ++node_id)
	{
		if (p_history->restore_node(node_id) == false)
		{
			fprintf(
				stderr, "[z6]: restore_node(%u) failed\n", node_id
			);
			break;
		}

		if (p_history->get_entity_watermark() >
		    entity_watermark)
		{
			entity_watermark =
				p_history->get_entity_watermark();
		}

		const kotek::uint64_t restored_hash =
			zircon_test_world_hash(
				&env.factory,
				env.ecs_context(),
				entity_watermark,
				world_state_scratch
			);

		if (restored_hash != node_hashes[node_id].second)
		{
			fprintf(
				stderr,
				"[z6]: FIRST DIVERGENCE at node %u (recorded "
				"%llu vs restored %llu)\n",
				node_id, node_hashes[node_id].second,
				restored_hash
			);

			if (node_id == 64)
			{
				const kotek::size_t min_size =
					world_state_at_64.size() <
							world_state_scratch.size()
					? world_state_at_64.size()
					: world_state_scratch.size();

				kotek::size_t first_diff = min_size;

				for (kotek::size_t k = 0; k < min_size; ++k)
				{
					if (world_state_at_64[k] !=
					    world_state_scratch[k])
					{
						first_diff = k;
						break;
					}
				}

				fprintf(
					stderr,
					"[z6]: recorded size %zu, restored size "
					"%zu, first diff at byte %zu\n",
					world_state_at_64.size(),
					world_state_scratch.size(), first_diff
				);

				const kotek::size_t dump_from =
					first_diff > 40 ? first_diff - 40 : 0;

				fprintf(
					stderr, "[z6]: recorded: %.*s\n",
					120,
					world_state_at_64.data() + dump_from
				);
				fprintf(
					stderr, "[z6]: restored: %.*s\n",
					120,
					world_state_scratch.data() + dump_from
				);
			}

			break;
		}
	}

	// snapshots must exist for a 700 command history with
	// interval 64
	EXPECT_GE(p_history->get_total_snapshot_count(), 5);

	// spot check: restoring arbitrary nodes must reproduce the
	// world state captured at that node (snapshot + journal
	// replay path)
	for (kotek::uint32_t probe = 0; probe < 64; ++probe)
	{
		const kotek::uint32_t node_id = static_cast<
			kotek::uint32_t>(rng() % node_hashes.size());

		fprintf(
			stderr, "[z6]: probe %u restore_node(%u)\n", probe,
			node_id
		);

		try
		{
			ASSERT_TRUE(p_history->restore_node(node_id));

			// restore creates entities with fresh ids, resync the
			// scan watermark
			if (p_history->get_entity_watermark() >
			    entity_watermark)
			{
				entity_watermark =
					p_history->get_entity_watermark();
			}

			const kotek::uint64_t restored_hash =
				zircon_test_world_hash(
					&env.factory,
					env.ecs_context(),
					entity_watermark,
					world_state_scratch
				);

			EXPECT_EQ(node_hashes[node_id].second, restored_hash)
				<< "restore_node(" << node_id
				<< ") produced a different world state";
		}
		catch (const std::exception& exception)
		{
			FAIL() << "restore_node(" << node_id
			       << ") threw: " << exception.what();
		}
	}

	// restoring the last node must equal the final state
	ASSERT_TRUE(p_history->restore_node(
		static_cast<kotek::uint32_t>(
			p_history->get_total_recorded_commands()
		)
	));

	if (p_history->get_entity_watermark() > entity_watermark)
	{
		entity_watermark = p_history->get_entity_watermark();
	}

	const kotek::uint64_t restored_final_hash =
		zircon_test_world_hash(
			&env.factory,
			env.ecs_context(),
			entity_watermark,
			world_state_scratch
		);

	EXPECT_EQ(
		node_hashes.back().second, restored_final_hash
	);

	fprintf(stderr, "[z6]: restore test phase 5 (shutdown)\n");

	env.shutdown();
	delete &env;
}

using zircon_pass_editor_gizmo_own =
	no_streaming::zircon_render_graph_pass_editor_gizmo_own_bgfx;

// functional proof for task Z3 P2e's drag-END contract: the gizmo's
// per-frame live preview mutates the transform component directly, and
// the mouse-release commit issues exactly ONE
// zircon_command_edit_component_state through the real history whose
// before-state is the drag-START state (not the preview) — so undo
// restores the pre-drag transform and redo replays the drag's result
TEST(Zircon_Editor, CommandHistory_GizmoDragEndEditCommand)
{
	constexpr const char* _k_test_folder = "z6_gizmo_drag_end_test";

	// fresh journal per run
	{
		auto* p_config = new kotek::core::ktkFrameworkConfig();
		auto* p_filesystem = new kotek::core::ktkFileSystem();

		p_filesystem->Initialize(p_config);

		zircon_test_remove_streaming_folder(
			p_filesystem, _k_test_folder
		);

		p_filesystem->Shutdown();

		delete p_filesystem;
		delete p_config;
	}

	// heap allocated like every history fixture (the console alone is
	// ~1 MB of stack)
	zircon_test_history_env& env = *new zircon_test_history_env();
	env.initialize(_k_test_folder);

	zircon_editor_command_history* p_history = env.history();

	ASSERT_NE(p_history, nullptr);
	ASSERT_EQ(p_history->get_total_recorded_commands(), 0);

	// unjournaled scene setup: one entity with a transform (the test
	// tracks the gizmo's edit command only)
	kotek::entity_t entity =
		env.factory.create_entity(env.ecs_context());

	env.factory.create_component(env.ecs_context(), entity,
		eZirconComponentType::kzircon_component_transform);

	zircon_component_transform* p_transform =
		static_cast<zircon_component_transform*>(
			env.factory.get_component_by_enum(env.ecs_context(), entity,
				eZirconComponentType::kzircon_component_transform));

	ASSERT_NE(p_transform, nullptr);

	p_transform->set_position(kotek::math::vec3f_t(1.0f, 2.0f, 3.0f));
	p_transform->set_scale(kotek::math::vec3f_t(1.0f, 1.0f, 1.0f));
	p_transform->set_rotation(
		kotek::math::quatf_t(0.0f, 0.0f, 0.0f, 1.0f));

	// the pass's drag flow: the start state is captured at mouse-down
	// (these PODs are the drag context's start capture)...
	const float start_position[3] = {1.0f, 2.0f, 3.0f};
	const float start_scale[3] = {1.0f, 1.0f, 1.0f};
	const float start_rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	// ...the live preview writes the dragged state straight into the
	// component (an X-axis drag of +3)...
	p_transform->set_position(kotek::math::vec3f_t(4.0f, 2.0f, 3.0f));

	// ...and mouse release commits through the pass's static — the
	// exact call the pass makes from OnUpdate
	ASSERT_TRUE(zircon_pass_editor_gizmo_own::commit_drag_edit(
		&env.session_manager, &env.factory, p_history,
		env.ecs_context(), entity, start_position, start_scale,
		start_rotation));

	// one journaled command; the world holds the dragged state
	EXPECT_EQ(p_history->get_total_recorded_commands(), 1);

	p_transform = static_cast<zircon_component_transform*>(
		env.factory.get_component_by_enum(env.ecs_context(), entity,
			eZirconComponentType::kzircon_component_transform));

	ASSERT_NE(p_transform, nullptr);

	EXPECT_FLOAT_EQ(p_transform->get_position().x(), 4.0f);
	EXPECT_FLOAT_EQ(p_transform->get_position().y(), 2.0f);
	EXPECT_FLOAT_EQ(p_transform->get_position().z(), 3.0f);

	// undo restores the drag-START state (the command's before is not
	// the preview that sat in the component at commit time)
	p_history->Undo();

	p_transform = static_cast<zircon_component_transform*>(
		env.factory.get_component_by_enum(env.ecs_context(), entity,
			eZirconComponentType::kzircon_component_transform));

	ASSERT_NE(p_transform, nullptr);

	EXPECT_FLOAT_EQ(p_transform->get_position().x(), 1.0f);
	EXPECT_FLOAT_EQ(p_transform->get_position().y(), 2.0f);
	EXPECT_FLOAT_EQ(p_transform->get_position().z(), 3.0f);

	// redo replays the drag's result
	p_history->Redo();

	p_transform = static_cast<zircon_component_transform*>(
		env.factory.get_component_by_enum(env.ecs_context(), entity,
			eZirconComponentType::kzircon_component_transform));

	ASSERT_NE(p_transform, nullptr);

	EXPECT_FLOAT_EQ(p_transform->get_position().x(), 4.0f);
	EXPECT_FLOAT_EQ(p_transform->get_position().y(), 2.0f);
	EXPECT_FLOAT_EQ(p_transform->get_position().z(), 3.0f);

	env.shutdown();
	delete &env;
}

		#endif

void zircon_register_unit_tests_command_history() {}

	#endif
#endif
