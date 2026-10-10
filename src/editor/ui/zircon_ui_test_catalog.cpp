#include "zircon_ui_test_catalog.h"

#include "../../core/zircon_config.h"
#include "../../core/zircon_editor_enums.h"
#include "../../ecs/zircon_factory.h"
#include "../../render/bgfx/zircon_renderer.h"
#include "../../world/zircon_world.h"
#include "../commands/zircon_command_history.h"
#include "../session/zircon_session_editor.h"
#include "zircon_editor_ui_state.h"

#include <kotek.core.filesystem/include/kotek_filesystem_helpers.h>

#include <cstring>

// the UI-test catalog (task Z17): one constexpr step table per UI test +
// the probe functions the tables assert through. Everything here is
// namespace-scope constexpr POD (the Z18 rule-1a exemption) or free
// functions — zero runtime allocation, zero statics.
//
// FRAME CADENCE (the offsets below assume it — see the harness header):
// a kClickWidget at offset F injects move+press into the io queue
// (processed at NewFrame F+1), the auto-release injects at F+2
// (processed at F+3) and the widget fires during frame F+3's window
// draw; a widget that Executes a console command mutates the world in
// that same draw, a widget that Pushes one (the menu items) is flushed
// at frame F+4's loop top. Assertions sit at F+6. A kKeyPress at F is
// seen by the cancel adapter at F+1; assertions at F+3. A
// pass-structure edit (add/remove/move) rebuilds the editor render
// graph at the top of the NEXT frame — the imgui context is recreated
// there, so the first widget after a structural click is reached
// through a kWaitWidget (the registry re-fills on the rebuild frame's
// draw).
//
// SCRATCH IS PER-TEST (zeroed at each test's start) — a test that needs
// the created entity's id re-captures it (probe_capture_max_entity_id).

namespace
{
	// the wrapper's backend-callback seam speaks the window backend's
	// key codes — GLFW on the default window library (GLFW_KEY_ESCAPE)
	constexpr kotek::int32_t _kKeyCode_Escape = 256;

	// the editor pass names the structural tests drive (the full
	// registered spellings from the generated zircon_render_pass_factory.h
	// — a rename breaks these waits loudly, by design)
	constexpr const char* _kPassEditorGrid =
		"no_streaming::zircon_render_graph_pass_editor_grid_bgfx";
	constexpr const char* _kPassEditorGizmoOwn =
		"no_streaming::zircon_render_graph_pass_editor_gizmo_own_bgfx";
	constexpr const char* _kPassEditorGizmoImguizmo =
		"no_streaming::zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx";

	// the tracked widget labels the render-passes window reports (the
	// window sprintfs "<verb>:<session>:<full pass name>")
	constexpr const char* _kLabelEnabledEditorGrid =
		"enabled:editor:no_streaming::zircon_render_graph_pass_editor_grid_"
		"bgfx";
	constexpr const char* _kLabelEnabledEditorGizmoOwn =
		"enabled:editor:no_streaming::zircon_render_graph_pass_editor_gizmo_"
		"own_bgfx";
	constexpr const char* _kLabelAddEditorGizmoImguizmo =
		"add:editor:no_streaming::zircon_render_graph_pass_editor_gizmo_"
		"imguizmo_bgfx";
	constexpr const char* _kLabelRemoveEditorGizmoImguizmo =
		"remove:editor:no_streaming::zircon_render_graph_pass_editor_gizmo_"
		"imguizmo_bgfx";
	constexpr const char* _kLabelMoveDownEditorGrid =
		"move_down:editor:no_streaming::zircon_render_graph_pass_editor_"
		"grid_bgfx";
	constexpr const char* _kLabelMoveUpEditorGrid =
		"move_up:editor:no_streaming::zircon_render_graph_pass_editor_grid_"
		"bgfx";

	// the top bar's menu labels
	constexpr const char* _kTopBarMenuView =
		"View##ZirconImGuiSDK_MainBar_View";
	constexpr const char* _kTopBarMenuFile = "File";
	constexpr const char* _kTopBarMenuEdit = "Edit";
	constexpr const char* _kTopBarMenuShowWindowsAll = "Show windows All";

	// ---- constexpr step builders (keep the tables readable) ----

	constexpr zircon_ui_test_step_t st_click(kotek::uint16_t frame_offset,
		const char* p_window, const char* p_label, const char* p_debug)
	{
		return {frame_offset, eZirconUiTestStepAction::kClickWidget,
			ZIRCON_DEF_UI_TEST_STEP_FLAG_NONE, p_window, p_label, nullptr,
			p_debug, {0, 0, 0}};
	}

	/// the label is a "%u" format over scratch[scratch_slot] (the entity
	/// row labels "row:<id>")
	constexpr zircon_ui_test_step_t st_click_fmt(
		kotek::uint16_t frame_offset, const char* p_window,
		const char* p_label_format, kotek::int32_t scratch_slot,
		const char* p_debug)
	{
		return {frame_offset, eZirconUiTestStepAction::kClickWidget,
			ZIRCON_DEF_UI_TEST_STEP_FLAG_LABEL_FORMAT_SCRATCH, p_window,
			p_label_format, nullptr, p_debug, {0, scratch_slot, 0}};
	}

	constexpr zircon_ui_test_step_t st_wait(kotek::uint16_t frame_offset,
		const char* p_window, const char* p_label,
		kotek::int32_t timeout_frames, const char* p_debug)
	{
		return {frame_offset, eZirconUiTestStepAction::kWaitWidget,
			ZIRCON_DEF_UI_TEST_STEP_FLAG_NONE, p_window, p_label, nullptr,
			p_debug, {timeout_frames, 0, 0}};
	}

	constexpr zircon_ui_test_step_t st_wait_fmt(
		kotek::uint16_t frame_offset, const char* p_window,
		const char* p_label_format, kotek::int32_t timeout_frames,
		kotek::int32_t scratch_slot, const char* p_debug)
	{
		return {frame_offset, eZirconUiTestStepAction::kWaitWidget,
			ZIRCON_DEF_UI_TEST_STEP_FLAG_LABEL_FORMAT_SCRATCH, p_window,
			p_label_format, nullptr, p_debug,
			{timeout_frames, scratch_slot, 0}};
	}

	constexpr zircon_ui_test_step_t st_hover(kotek::uint16_t frame_offset,
		const char* p_window, const char* p_label, const char* p_debug)
	{
		return {frame_offset, eZirconUiTestStepAction::kMouseMoveToWidget,
			ZIRCON_DEF_UI_TEST_STEP_FLAG_NONE, p_window, p_label, nullptr,
			p_debug, {0, 0, 0}};
	}

	constexpr zircon_ui_test_step_t st_focus(
		kotek::uint16_t frame_offset, const char* p_window_title,
		const char* p_debug)
	{
		return {frame_offset, eZirconUiTestStepAction::kFocusWindow,
			ZIRCON_DEF_UI_TEST_STEP_FLAG_NONE, p_window_title, nullptr,
			nullptr, p_debug, {0, 0, 0}};
	}

	constexpr zircon_ui_test_step_t st_key(kotek::uint16_t frame_offset,
		kotek::int32_t key_code, const char* p_debug)
	{
		return {frame_offset, eZirconUiTestStepAction::kKeyPress,
			ZIRCON_DEF_UI_TEST_STEP_FLAG_NONE, nullptr, nullptr, nullptr,
			p_debug, {key_code, 0, 0}};
	}

	constexpr zircon_ui_test_step_t st_wheel(kotek::uint16_t frame_offset,
		kotek::int32_t wheel_y, const char* p_debug)
	{
		return {frame_offset, eZirconUiTestStepAction::kMouseWheel,
			ZIRCON_DEF_UI_TEST_STEP_FLAG_NONE, nullptr, nullptr, nullptr,
			p_debug, {0, wheel_y, 0}};
	}

	constexpr zircon_ui_test_step_t st_probe(kotek::uint16_t frame_offset,
		zircon_ui_test_probe_pfn_t pfn, const char* p_debug,
		kotek::int32_t p0 = 0, kotek::int32_t p1 = 0, kotek::int32_t p2 = 0)
	{
		return {frame_offset, eZirconUiTestStepAction::kProbe,
			ZIRCON_DEF_UI_TEST_STEP_FLAG_NONE, nullptr, nullptr, pfn,
			p_debug, {p0, p1, p2}};
	}

	/// a probe whose target label carries data (the pass name for the
	/// renderer probes, the substring for the config-file probe)
	constexpr zircon_ui_test_step_t st_probe_l(
		kotek::uint16_t frame_offset, zircon_ui_test_probe_pfn_t pfn,
		const char* p_label, const char* p_debug, kotek::int32_t p0 = 0,
		kotek::int32_t p1 = 0, kotek::int32_t p2 = 0)
	{
		return {frame_offset, eZirconUiTestStepAction::kProbe,
			ZIRCON_DEF_UI_TEST_STEP_FLAG_NONE, nullptr, p_label, pfn,
			p_debug, {p0, p1, p2}};
	}

	// ---- probe helpers ----

	kotek::uint32_t probe_entity_count(const zircon_ui_test_context_t& ctx)
	{
		if (ctx.p_world == nullptr || ctx.p_factory == nullptr ||
			ctx.p_world->get_ecs_context() == nullptr)
		{
			return 0;
		}

		kotek::entity_t ids[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};

		return ctx.p_factory->get_all_entities(
			ctx.p_world->get_ecs_context(),
			ctx.p_world->get_entity_count_max_limit(), ids,
			ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT);
	}

	kotek::uint64_t probe_max_entity_id(const zircon_ui_test_context_t& ctx)
	{
		if (ctx.p_world == nullptr || ctx.p_factory == nullptr ||
			ctx.p_world->get_ecs_context() == nullptr)
		{
			return 0;
		}

		kotek::entity_t ids[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};

		const kotek::uint32_t count = ctx.p_factory->get_all_entities(
			ctx.p_world->get_ecs_context(),
			ctx.p_world->get_entity_count_max_limit(), ids,
			ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT);

		kotek::uint64_t max_id = 0;

		for (kotek::uint32_t i = 0; i < count; ++i)
		{
			if (ids[i].id > max_id)
			{
				max_id = ids[i].id;
			}
		}

		return max_id;
	}

	bool probe_entity_has_component(const zircon_ui_test_context_t& ctx,
		kotek::uint64_t recorded_entity_id, eZirconComponentType component_type)
	{
		if (ctx.p_world == nullptr || ctx.p_factory == nullptr ||
			ctx.p_world->get_ecs_context() == nullptr)
		{
			return false;
		}

		// undo/redo reincarnate entities under fresh ids (task Z6) —
		// resolve the recorded id through the history's chain
		kotek::entity_t live_id = ctx.p_history
			? ctx.p_history->get_live_entity_id(
				  kotek::entity_t{recorded_entity_id})
			: kotek::entity_t{recorded_entity_id};

		return ctx.p_factory->has_component(
			ctx.p_world->get_ecs_context(), live_id, component_type);
	}

	// ---- the probes ----

	bool probe_capture_entity_count(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		ctx.scratch[step.m_params[0]] = probe_entity_count(ctx);
		return true;
	}

	bool probe_assert_entity_count_delta(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		const kotek::int64_t expected =
			static_cast<kotek::int64_t>(ctx.scratch[step.m_params[0]]) +
			step.m_params[1];
		const kotek::uint32_t actual = probe_entity_count(ctx);

		if (actual == static_cast<kotek::uint32_t>(expected))
		{
			return true;
		}

		KOTEK_MESSAGE("[ui-test]:     observed entity count {}, expected "
					  "{} (captured {} + delta {})",
			actual, expected, ctx.scratch[step.m_params[0]],
			step.m_params[1]);
		return false;
	}

	bool probe_capture_journal_total(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		ctx.scratch[step.m_params[0]] = ctx.p_history
			? ctx.p_history->get_total_recorded_commands()
			: 0;
		return true;
	}

	bool probe_assert_journal_delta(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		if (ctx.p_history == nullptr)
		{
			return false;
		}

		const kotek::int64_t expected =
			static_cast<kotek::int64_t>(ctx.scratch[step.m_params[0]]) +
			step.m_params[1];
		const kotek::uint64_t actual =
			ctx.p_history->get_total_recorded_commands();

		if (actual == static_cast<kotek::uint64_t>(expected))
		{
			return true;
		}

		KOTEK_MESSAGE("[ui-test]:     observed journal nodes {}, expected "
					  "{} (captured {} + delta {})",
			actual, expected, ctx.scratch[step.m_params[0]],
			step.m_params[1]);
		return false;
	}

	bool probe_capture_max_entity_id(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		ctx.scratch[step.m_params[0]] = probe_max_entity_id(ctx);
		return true;
	}

	bool probe_assert_selection_invalid(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		(void)step;

		if (ctx.p_ui_state == nullptr)
		{
			return false;
		}

		const bool is_invalid =
			ctx.p_ui_state->get_selected_entity() ==
			kotek::ktk::kInvalidECSEntity;

		if (is_invalid == false)
		{
			KOTEK_MESSAGE("[ui-test]:     selection is entity {}, expected "
						  "no selection",
				static_cast<kotek::uint32_t>(
					ctx.p_ui_state->get_selected_entity().id));
		}

		return is_invalid;
	}

	bool probe_assert_selection_equals_scratch(
		zircon_ui_test_context_t& ctx, const zircon_ui_test_step_t& step)
	{
		if (ctx.p_ui_state == nullptr)
		{
			return false;
		}

		const kotek::uint64_t actual =
			ctx.p_ui_state->get_selected_entity().id;
		const kotek::uint64_t expected = ctx.scratch[step.m_params[0]];

		if (actual == expected)
		{
			return true;
		}

		KOTEK_MESSAGE(
			"[ui-test]:     selection is entity {}, expected {}", actual,
			expected);
		return false;
	}

	bool probe_assert_window_shown(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		if (ctx.p_session == nullptr)
		{
			return false;
		}

		const bool expected = step.m_params[1] != 0;

		for (const auto* p_element : ctx.p_session->get_imgui_ui_elements())
		{
			if (p_element && p_element->Get_ID() == step.m_params[0])
			{
				const bool is_shown = p_element->Is_Shown();

				if (is_shown != expected)
				{
					KOTEK_MESSAGE(
						"[ui-test]:     window id {} shown={}, expected {}",
						step.m_params[0], is_shown, expected);
				}

				return is_shown == expected;
			}
		}

		KOTEK_MESSAGE("[ui-test]:     window id {} is not registered in "
					  "the session's ui elements",
			step.m_params[0]);
		return false;
	}

	bool probe_capture_sdk_feature(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		if (ctx.p_config == nullptr)
		{
			return false;
		}

		ctx.scratch[step.m_params[1]] =
			ctx.p_config->is_feature_enabled(
				static_cast<eZirconSDKFeatures>(step.m_params[0]))
			? 1
			: 0;
		return true;
	}

	bool probe_assert_sdk_feature_equals_scratch(
		zircon_ui_test_context_t& ctx, const zircon_ui_test_step_t& step)
	{
		if (ctx.p_config == nullptr)
		{
			return false;
		}

		const kotek::uint64_t current =
			ctx.p_config->is_feature_enabled(
				static_cast<eZirconSDKFeatures>(step.m_params[0]))
				? 1
				: 0;
		const kotek::uint64_t captured = ctx.scratch[step.m_params[1]];

		if (current != captured)
		{
			KOTEK_MESSAGE("[ui-test]:     sdk feature bit {} is {}, the "
						  "captured value was {}",
				step.m_params[0], current, captured);
		}

		return current == captured;
	}

	bool probe_assert_sdk_feature_flipped_scratch(
		zircon_ui_test_context_t& ctx, const zircon_ui_test_step_t& step)
	{
		if (ctx.p_config == nullptr)
		{
			return false;
		}

		const kotek::uint64_t current =
			ctx.p_config->is_feature_enabled(
				static_cast<eZirconSDKFeatures>(step.m_params[0]))
				? 1
				: 0;
		const kotek::uint64_t captured = ctx.scratch[step.m_params[1]];

		if (current == captured)
		{
			KOTEK_MESSAGE("[ui-test]:     sdk feature bit {} did not flip "
						  "(still {})",
				step.m_params[0], current);
		}

		return current != captured;
	}

	bool probe_assert_popup_open(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		if (ctx.p_imgui_wrapper == nullptr)
		{
			return false;
		}

		const bool expected = step.m_params[0] != 0;
		const bool is_open = ctx.p_imgui_wrapper->IsPopupOpen("",
			ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

		if (is_open != expected)
		{
			KOTEK_MESSAGE(
				"[ui-test]:     popup open={}, expected {}", is_open,
				expected);
		}

		return is_open == expected;
	}

	/// shared renderer reach for the pass-set probes; nullptr (the probe
	/// fails loudly) when the session's graph is absent
	const zircon_render_graph_simplified_bgfx_info_t* probe_graph_info(
		const zircon_ui_test_context_t& ctx, bool is_game_session)
	{
		if (ctx.p_renderer_bgfx == nullptr)
		{
			return nullptr;
		}

		const kotek::uint8_t render_graph_id =
			ctx.p_renderer_bgfx->get_render_graph_id_for_session_kind(
				is_game_session);

		if (render_graph_id >= ctx.p_renderer_bgfx->get_render_graph_count())
		{
			return nullptr;
		}

		return &ctx.p_renderer_bgfx->get_render_graph_info(render_graph_id);
	}

	bool probe_assert_pass_enabled(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		const auto* p_info = probe_graph_info(ctx, step.m_params[0] != 0);

		if (p_info == nullptr)
		{
			KOTEK_MESSAGE("[ui-test]:     no render graph for the pass "
						  "probe ('{}')",
				step.p_target_label);
			return false;
		}

		const bool expected = step.m_params[1] != 0;

		for (kotek::ktk::size_t i = 0; i < p_info->pass_names.size(); ++i)
		{
			if (p_info->pass_names[i] == step.p_target_label)
			{
				const bool is_enabled = p_info->pass_enabled[i];

				if (is_enabled != expected)
				{
					KOTEK_MESSAGE("[ui-test]:     pass '{}' enabled={}, "
								  "expected {}",
						step.p_target_label, is_enabled, expected);
				}

				return is_enabled == expected;
			}
		}

		KOTEK_MESSAGE(
			"[ui-test]:     pass '{}' is not in the session's live set",
			step.p_target_label);
		return false;
	}

	bool probe_assert_pass_in_set(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		const auto* p_info = probe_graph_info(ctx, step.m_params[0] != 0);

		if (p_info == nullptr)
		{
			return false;
		}

		const bool expected = step.m_params[1] != 0;

		bool is_present = false;

		for (const auto& name : p_info->pass_names)
		{
			if (name == step.p_target_label)
			{
				is_present = true;
				break;
			}
		}

		if (is_present != expected)
		{
			KOTEK_MESSAGE(
				"[ui-test]:     pass '{}' in-set={}, expected {}",
				step.p_target_label, is_present, expected);
		}

		return is_present == expected;
	}

	bool probe_assert_pass_index(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		const auto* p_info = probe_graph_info(ctx, step.m_params[0] != 0);

		if (p_info == nullptr)
		{
			return false;
		}

		const auto index =
			static_cast<kotek::ktk::size_t>(step.m_params[1]);

		if (index >= p_info->pass_names.size())
		{
			KOTEK_MESSAGE("[ui-test]:     pass index {} out of the live "
						  "set ({} passes)",
				step.m_params[1], p_info->pass_names.size());
			return false;
		}

		const bool matches =
			p_info->pass_names[index] == step.p_target_label;

		if (matches == false)
		{
			KOTEK_MESSAGE("[ui-test]:     pass at index {} is '{}', "
						  "expected '{}'",
				step.m_params[1], p_info->pass_names[index].c_str(),
				step.p_target_label);
		}

		return matches;
	}

	bool probe_assert_component_on_scratch_entity(
		zircon_ui_test_context_t& ctx, const zircon_ui_test_step_t& step)
	{
		const bool expected = step.m_params[2] != 0;

		const bool has = probe_entity_has_component(ctx,
			ctx.scratch[step.m_params[0]],
			static_cast<eZirconComponentType>(step.m_params[1]));

		if (has != expected)
		{
			KOTEK_MESSAGE("[ui-test]:     entity {} has component {} = {}, "
						  "expected {}",
				ctx.scratch[step.m_params[0]], step.m_params[1], has,
				expected);
		}

		return has == expected;
	}

	bool probe_assert_config_file_contains(zircon_ui_test_context_t& ctx,
		const zircon_ui_test_step_t& step)
	{
		if (ctx.p_main_manager == nullptr ||
			ctx.p_main_manager->GetFileSystem() == nullptr)
		{
			return false;
		}

		ktk_filesystem_path path_to_file;
		kotek::core::path_for(ctx.p_main_manager->GetFileSystem(),
			kotek::core::eFolderIndex::kFolderIndex_DataUser,
			kZirconConfig_FileName, path_to_file);

		kotek::array_t<kotek::uint8_t,
			ZIRCON_DEF_UI_TEST_CONFIG_BACKUP_CAPACITY>
			content{};

		kotek::size_t content_size = 0;

		if (kotek::core::read_file(ctx.p_main_manager->GetFileSystem(),
				path_to_file, content.data(), content.size(),
				content_size) == false)
		{
			KOTEK_MESSAGE(
				"[ui-test]:     could not read '{}' for the assertion",
				kZirconConfig_FileName);
			return false;
		}

		// read_file writes a terminator when the buffer has room (the B0
		// contract) — the file is ~600 B against a 4 KB buffer; clamp the
		// tail regardless so strstr never runs off
		content[content.size() - 1] = 0;

		const bool contains =
			std::strstr(reinterpret_cast<const char*>(content.data()),
				step.p_target_label) != nullptr;

		if (contains == false)
		{
			KOTEK_MESSAGE("[ui-test]:     '{}' does not contain '{}'",
				kZirconConfig_FileName, step.p_target_label);
		}

		return contains;
	}

	bool probe_assert_config_file_matches_backup(
		zircon_ui_test_context_t& ctx, const zircon_ui_test_step_t& step)
	{
		(void)step;

		if (ctx.p_main_manager == nullptr ||
			ctx.p_main_manager->GetFileSystem() == nullptr ||
			ctx.p_config_backup == nullptr || ctx.m_config_backup_size == 0)
		{
			return false;
		}

		ktk_filesystem_path path_to_file;
		kotek::core::path_for(ctx.p_main_manager->GetFileSystem(),
			kotek::core::eFolderIndex::kFolderIndex_DataUser,
			kZirconConfig_FileName, path_to_file);

		kotek::array_t<kotek::uint8_t,
			ZIRCON_DEF_UI_TEST_CONFIG_BACKUP_CAPACITY>
			current{};

		kotek::size_t current_size = 0;

		if (kotek::core::read_file(ctx.p_main_manager->GetFileSystem(),
				path_to_file, current.data(), current.size(),
				current_size) == false)
		{
			return false;
		}

		const bool matches =
			current_size == ctx.m_config_backup_size &&
			std::memcmp(current.data(), ctx.p_config_backup,
				ctx.m_config_backup_size) == 0;

		if (matches == false)
		{
			KOTEK_MESSAGE("[ui-test]:     '{}' drifted from the "
						  "activation-time backup ({} vs {} bytes)",
				kZirconConfig_FileName, current_size,
				ctx.m_config_backup_size);
		}

		return matches;
	}

	bool probe_assert_history_rows_match_pool(
		zircon_ui_test_context_t& ctx, const zircon_ui_test_step_t& step)
	{
		(void)step;

		if (ctx.p_history == nullptr || ctx.p_widgets == nullptr)
		{
			return false;
		}

		kotek::uint32_t pool_live = 0;

		for (const auto* p_command : ctx.p_history->GetCommands())
		{
			if (p_command != nullptr)
			{
				++pool_live;
			}
		}

		const kotek::uint32_t rows =
			ctx.p_widgets->count_fresh_with_prefix(
				zircon_ui_test_window_names::kHistoryCommandLog, "row:");

		if (rows != pool_live)
		{
			KOTEK_MESSAGE("[ui-test]:     the history window shows {} "
						  "rows, the live pool holds {} commands",
				rows, pool_live);
		}

		return rows == pool_live;
	}

	// ---- the step tables ----

	using zircon_ui_test_window_names::kComponentInspector;
	using zircon_ui_test_window_names::kEntityList;
	using zircon_ui_test_window_names::kHistoryCommandLog;
	using zircon_ui_test_window_names::kRenderPasses;
	using zircon_ui_test_window_names::kSettings;
	using zircon_ui_test_window_names::kTopBar;

	// opens the View menu through the real menu-bar click, proves the
	// popup reached imgui, then proves ESC dismisses exactly the popup
	// (the Z19 arbiter's popup consumer) and nothing else
	constexpr zircon_ui_test_step_t _kSteps_menu_view[] = {
		st_click(0, kTopBar, _kTopBarMenuView,
			"click the View menu in the main menu bar"),
		st_wait(4, kTopBar, _kTopBarMenuShowWindowsAll, 0,
			"the View menu popup opened"),
		st_probe(5, &probe_assert_popup_open,
			"the View menu is an open imgui popup", 1),
		st_key(6, _kKeyCode_Escape, "press ESC over the open menu"),
		st_probe(9, &probe_assert_popup_open,
			"ESC closed the menu popup (the arbiter's popup consumer)", 0),
	};

	/// one View > Show windows All > <window> toggle trip: open the menu,
	/// hover the submenu (opens it), click the window's item, assert the
	/// engine-side visibility flag (the menu item Pushes the
	/// ShowWindow/HideWindow console command — flushed at the next loop
	/// top). The trailing ESC + popup probe make each trip self-contained:
	/// a failed trip can leave the menu open, and the next trip's click on
	/// an already-open View would CLOSE it instead (imgui toggles), so the
	/// cleanup is part of the contract
	#define ZIRCON_UI_TEST_STEPS_VIEW_TOGGLE(window_literal, window_id_enum, \
		expected_after)                                                      \
	st_click(0, kTopBar, _kTopBarMenuView, "open the View menu"),           \
		st_wait(4, kTopBar, _kTopBarMenuShowWindowsAll, 0,                  \
			"the View menu popup opened"),                                  \
		st_hover(5, kTopBar, _kTopBarMenuShowWindowsAll,                    \
			"hover 'Show windows All' — the submenu opens"),                \
		st_wait(6, kTopBar, window_literal "##ViewImGui", 120,              \
			"the submenu item for " window_literal " appeared"),            \
		st_click(7, kTopBar, window_literal "##ViewImGui",                  \
			"click the " window_literal " show-toggle"),                    \
		st_probe(13, &probe_assert_window_shown,                            \
			"the window's Is_Shown flipped to " #expected_after,            \
			static_cast<kotek::int32_t>(window_id_enum), expected_after),   \
		st_key(14, _kKeyCode_Escape, "close any leftover popup"),           \
		st_probe(15, &probe_assert_popup_open,                              \
			"no popup is left open", 0)

	constexpr zircon_ui_test_step_t _kSteps_view_show_object_list[] = {
		st_probe(0, &probe_assert_window_shown,
			"the Entity List starts hidden (the ctor default)",
			static_cast<kotek::int32_t>(eZirconWindowIDs::kWindow_SDK_ObjectList),
			0),
		ZIRCON_UI_TEST_STEPS_VIEW_TOGGLE("kWindow_SDK_ObjectList",
			eZirconWindowIDs::kWindow_SDK_ObjectList, 1),
	};

	constexpr zircon_ui_test_step_t _kSteps_view_show_inspector[] = {
		st_probe(0, &probe_assert_window_shown,
			"the Component Inspector starts hidden",
			static_cast<kotek::int32_t>(
				eZirconWindowIDs::kWindow_SDK_ComponentInspector),
			0),
		ZIRCON_UI_TEST_STEPS_VIEW_TOGGLE("kWindow_SDK_ComponentInspector",
			eZirconWindowIDs::kWindow_SDK_ComponentInspector, 1),
	};

	constexpr zircon_ui_test_step_t _kSteps_view_show_history[] = {
		st_probe(0, &probe_assert_window_shown,
			"the History Command Log starts hidden",
			static_cast<kotek::int32_t>(
				eZirconWindowIDs::kWindow_SDK_HistoryCommandLog),
			0),
		ZIRCON_UI_TEST_STEPS_VIEW_TOGGLE("kWindow_SDK_HistoryCommandLog",
			eZirconWindowIDs::kWindow_SDK_HistoryCommandLog, 1),
	};

	constexpr zircon_ui_test_step_t _kSteps_view_show_settings[] = {
		st_probe(0, &probe_assert_window_shown,
			"the Settings window starts hidden",
			static_cast<kotek::int32_t>(eZirconWindowIDs::kWindow_SDK_Settings),
			0),
		ZIRCON_UI_TEST_STEPS_VIEW_TOGGLE(
			"kWindow_SDK_Settings", eZirconWindowIDs::kWindow_SDK_Settings, 1),
	};

	constexpr zircon_ui_test_step_t _kSteps_view_show_log[] = {
		ZIRCON_UI_TEST_STEPS_VIEW_TOGGLE(
			"kWindow_SDK_Log", eZirconWindowIDs::kWindow_SDK_Log, 1),
	};

	constexpr zircon_ui_test_step_t _kSteps_view_show_render_stats[] = {
		ZIRCON_UI_TEST_STEPS_VIEW_TOGGLE(
			"kWindow_SDK_RenderStats", eZirconWindowIDs::kWindow_SDK_RenderStats,
			1),
	};

	constexpr zircon_ui_test_step_t _kSteps_view_show_debug_input[] = {
		ZIRCON_UI_TEST_STEPS_VIEW_TOGGLE(
			"kWindow_SDK_DebugInput", eZirconWindowIDs::kWindow_SDK_DebugInput,
			1),
	};

	constexpr zircon_ui_test_step_t _kSteps_view_show_prefab[] = {
		ZIRCON_UI_TEST_STEPS_VIEW_TOGGLE(
			"kWindow_SDK_Prefab", eZirconWindowIDs::kWindow_SDK_Prefab, 1),
	};

	// the Render Passes window auto-opens on start (the
	// show_pass_manager_on_start wizard) — its toggle test drives the
	// full hide-then-show cycle through the same View menu and leaves it
	// shown for the structural tests
	constexpr zircon_ui_test_step_t _kSteps_view_toggle_render_passes[] = {
		st_probe(0, &probe_assert_window_shown,
			"the Render Passes window auto-opened on start",
			static_cast<kotek::int32_t>(
				eZirconWindowIDs::kWindow_SDK_RenderPasses),
			1),
		ZIRCON_UI_TEST_STEPS_VIEW_TOGGLE("kWindow_SDK_RenderPasses",
			eZirconWindowIDs::kWindow_SDK_RenderPasses, 0),
		ZIRCON_UI_TEST_STEPS_VIEW_TOGGLE("kWindow_SDK_RenderPasses",
			eZirconWindowIDs::kWindow_SDK_RenderPasses, 1),
	};

	constexpr zircon_ui_test_step_t _kSteps_entity_create[] = {
		st_focus(0, kEntityList, "raise the Entity List"),
		st_probe(1, &probe_capture_entity_count,
			"capture the entity count before the click", 0),
		st_probe(1, &probe_capture_journal_total,
			"capture the journal node count before the click", 1),
		st_click(2, kEntityList, "Add", "click 'Add' (create entity)"),
		st_probe(8, &probe_assert_entity_count_delta,
			"the world gained exactly one entity", 0, 1),
		st_probe(8, &probe_assert_journal_delta,
			"the journal gained exactly one node (the create is journaled)",
			1, 1),
		st_probe(8, &probe_capture_max_entity_id,
			"capture the created entity's id", 2),
	};

	constexpr zircon_ui_test_step_t _kSteps_entity_select[] = {
		st_focus(0, kEntityList, "raise the Entity List"),
		st_probe(1, &probe_capture_max_entity_id,
			"re-capture the created entity's id (scratch is per-test)", 2),
		st_wait_fmt(2, kEntityList, "row:%u", 0, 2,
			"the created entity's row is listed"),
		st_click_fmt(3, kEntityList, "row:%u", 2,
			"click the entity's row (select it)"),
		st_probe(9, &probe_assert_selection_equals_scratch,
			"the selection contract: ui_state holds the clicked entity", 2),
	};

	constexpr zircon_ui_test_step_t _kSteps_inspector_add_component[] = {
		st_focus(0, kEntityList, "raise the Entity List"),
		st_probe(1, &probe_capture_max_entity_id,
			"re-capture the created entity's id (the modal test's ESC "
			"cleared the selection — re-select)",
			2),
		st_wait_fmt(2, kEntityList, "row:%u", 0, 2,
			"the entity's row is listed"),
		st_click_fmt(3, kEntityList, "row:%u", 2,
			"re-select the entity"),
		st_probe(9, &probe_assert_selection_equals_scratch,
			"the entity is selected", 2),
		st_focus(10, kComponentInspector, "raise the Component Inspector"),
		st_probe(11, &probe_capture_journal_total,
			"capture the journal node count", 1),
		st_click(12, kComponentInspector, "Add Component",
			"open the 'Add Component' combo"),
		st_wait(16, kComponentInspector,
			"comboitem:zircon_component_geometry", 120,
			"the combo opened with the component list"),
		st_click(17, kComponentInspector,
			"comboitem:zircon_component_geometry",
			"pick 'zircon_component_geometry' from the combo"),
		// the button press must land AFTER the item's Selectable fired
		// and the popup closed — a press while the popup is still open is
		// swallowed by the popup-close (the bring-up lesson)
		st_click(22, kComponentInspector, "Add component",
			"click 'Add component' (the journaled add through the console)"),
		st_probe(28, &probe_assert_component_on_scratch_entity,
			"the selected entity gained the geometry component", 2,
			static_cast<kotek::int32_t>(
				eZirconComponentType::kzircon_component_geometry),
			1),
		st_probe(28, &probe_assert_journal_delta,
			"the journal gained exactly one node", 1, 1),
	};

	// the failed-add modal path: 'transform' is combo item 2 — always
	// visible without scrolling (the combo clips items beyond 8; the
	// clipped ones are tracked-but-unclickable, the deep-item lesson).
	// Its guard needs the entity to have NO geometry/camera/sdk_camera —
	// the entity is fresh at this point (the modal test runs BEFORE the
	// add test), so the modal opens. Then ESC dismisses it (the arbiter's
	// popup consumer, priority 20) while the SELECTION (50) survives, and
	// the second ESC clears the selection
	constexpr zircon_ui_test_step_t _kSteps_inspector_modal_esc[] = {
		st_focus(0, kComponentInspector, "raise the Component Inspector"),
		st_probe(1, &probe_capture_max_entity_id,
			"re-capture the created entity's id", 2),
		st_click(2, kComponentInspector, "Add Component",
			"open the 'Add Component' combo"),
		st_wait(6, kComponentInspector,
			"comboitem:zircon_component_transform", 120,
			"the combo opened"),
		st_click(7, kComponentInspector,
			"comboitem:zircon_component_transform",
			"pick 'zircon_component_transform'"),
		st_click(12, kComponentInspector, "Add component",
			"click 'Add component' — the add is blocked, the modal opens"),
		st_probe(18, &probe_assert_popup_open,
			"the failed-add modal is open", 1),
		st_key(19, _kKeyCode_Escape, "ESC #1 — the modal"),
		st_probe(22, &probe_assert_popup_open, "ESC dismissed the modal", 0),
		st_probe(22, &probe_assert_selection_equals_scratch,
			"the selection survived (the popup beats the selection in the "
			"arbiter's priority order)",
			2),
		st_key(23, _kKeyCode_Escape, "ESC #2 — the selection"),
		st_probe(26, &probe_assert_selection_invalid,
			"the second ESC cleared the entity selection"),
	};

	constexpr zircon_ui_test_step_t _kSteps_inspector_delete_component[] = {
		st_focus(0, kComponentInspector, "raise the Component Inspector"),
		st_probe(1, &probe_capture_max_entity_id,
			"re-capture the created entity's id", 2),
		st_probe(1, &probe_capture_journal_total,
			"capture the journal node count", 1),
		st_click(2, kComponentInspector, "Add Component",
			"open the combo to re-select 'geometry' as the list-box item"),
		st_wait(6, kComponentInspector,
			"comboitem:zircon_component_geometry", 120, "the combo opened"),
		st_click(7, kComponentInspector,
			"comboitem:zircon_component_geometry",
			"pick 'zircon_component_geometry'"),
		st_wait(12, kComponentInspector, "Delete component from list box", 60,
			"the delete button is drawn"),
		st_click(13, kComponentInspector, "Delete component from list box",
			"click 'Delete component from list box'"),
		st_probe(19, &probe_assert_component_on_scratch_entity,
			"the entity lost the geometry component", 2,
			static_cast<kotek::int32_t>(
				eZirconComponentType::kzircon_component_geometry),
			0),
		st_probe(19, &probe_assert_journal_delta,
			"the journal gained exactly one node", 1, 1),
	};

	constexpr zircon_ui_test_step_t _kSteps_entity_delete[] = {
		st_focus(0, kEntityList, "raise the Entity List"),
		st_probe(1, &probe_capture_entity_count, "capture the entity count",
			0),
		st_probe(1, &probe_capture_journal_total,
			"capture the journal node count", 1),
		st_probe(1, &probe_capture_max_entity_id,
			"re-capture the created entity's id", 2),
		st_wait_fmt(2, kEntityList, "row:%u", 0, 2,
			"the entity's row is listed"),
		st_click_fmt(3, kEntityList, "row:%u", 2,
			"re-select the entity (the modal test's ESC cleared the "
			"selection)"),
		st_probe(9, &probe_assert_selection_equals_scratch,
			"the entity is selected", 2),
		st_click(10, kEntityList, "Delete",
			"click 'Delete' (delete the selected entity)"),
		st_probe(16, &probe_assert_entity_count_delta,
			"the world lost exactly one entity", 0, -1),
		st_probe(16, &probe_assert_journal_delta,
			"the journal gained exactly one node", 1, 1),
		st_probe(16, &probe_assert_selection_invalid,
			"the selection was cleared with the entity (Z19's rule)"),
	};

	// Edit > Undo / Edit > Redo through the top bar (Push_Command +
	// flush): undo revives the deleted entity, redo deletes it again, and
	// neither appends to the journal (the cursor moves, the node count
	// stands)
	constexpr zircon_ui_test_step_t _kSteps_edit_undo_redo[] = {
		st_probe(0, &probe_capture_entity_count, "capture the entity count",
			0),
		st_probe(0, &probe_capture_journal_total,
			"capture the journal node count", 1),
		st_click(1, kTopBar, _kTopBarMenuEdit, "open the Edit menu"),
		st_wait(5, kTopBar, "Undo", 0, "the Edit menu popup opened"),
		st_click(6, kTopBar, "Undo", "Edit > Undo"),
		st_probe(13, &probe_assert_entity_count_delta,
			"undo revived the deleted entity", 0, 1),
		st_probe(13, &probe_assert_journal_delta,
			"undo appended nothing (the cursor moved)", 1, 0),
		st_click(14, kTopBar, _kTopBarMenuEdit, "open the Edit menu"),
		st_wait(18, kTopBar, "Redo", 0, "the Edit menu popup opened"),
		st_click(19, kTopBar, "Redo", "Edit > Redo"),
		st_probe(26, &probe_assert_entity_count_delta,
			"redo deleted the entity again", 0, 0),
		st_probe(26, &probe_assert_journal_delta, "redo appended nothing", 1,
			0),
	};

	constexpr zircon_ui_test_step_t _kSteps_render_passes_toggle_grid[] = {
		st_focus(0, kRenderPasses, "raise the Render Passes window"),
		st_probe_l(1, &probe_assert_pass_enabled, _kPassEditorGrid,
			"the grid pass starts enabled (the default editor set)", 0, 1),
		st_click(2, kRenderPasses, _kLabelEnabledEditorGrid,
			"uncheck the grid pass"),
		st_probe_l(8, &probe_assert_pass_enabled, _kPassEditorGrid,
			"the executor's skip flag flipped (instant, no rebuild)", 0, 0),
		st_click(9, kRenderPasses, _kLabelEnabledEditorGrid,
			"check the grid pass again"),
		st_probe_l(15, &probe_assert_pass_enabled, _kPassEditorGrid,
			"the grid pass is enabled again", 0, 1),
	};

	// add the imguizmo gizmo pass (registered, not in the default set) —
	// the frame-boundary rebuild puts it live + enabled, and the gizmo
	// mutual exclusion (P2f) disables the own gizmo; then remove it and
	// re-enable the own gizmo (net-zero for the Save test's byte-compare)
	constexpr zircon_ui_test_step_t _kSteps_render_passes_add_remove[] = {
		st_focus(0, kRenderPasses, "raise the Render Passes window"),
		st_wait(1, kRenderPasses, _kLabelAddEditorGizmoImguizmo, 0,
			"the imguizmo gizmo is registered but not in the set"),
		st_click(2, kRenderPasses, _kLabelAddEditorGizmoImguizmo,
			"add the imguizmo gizmo pass (a rebuild runs at the next frame "
			"boundary)"),
		st_wait(10, kRenderPasses, _kLabelRemoveEditorGizmoImguizmo, 180,
			"the rebuilt live set shows the imguizmo pass"),
		st_probe_l(11, &probe_assert_pass_in_set, _kPassEditorGizmoImguizmo,
			"the imguizmo pass joined the live set", 0, 1),
		st_probe_l(11, &probe_assert_pass_enabled, _kPassEditorGizmoImguizmo,
			"a freshly added pass starts enabled", 0, 1),
		st_probe_l(11, &probe_assert_pass_enabled, _kPassEditorGizmoOwn,
			"the mutual exclusion disabled the own gizmo", 0, 0),
		st_click(12, kRenderPasses, _kLabelRemoveEditorGizmoImguizmo,
			"remove the imguizmo pass"),
		st_wait(20, kRenderPasses, _kLabelAddEditorGizmoImguizmo, 180,
			"the imguizmo pass is back in the available list"),
		st_probe_l(21, &probe_assert_pass_in_set, _kPassEditorGizmoImguizmo,
			"the imguizmo pass left the live set", 0, 0),
		st_wait(22, kRenderPasses, _kLabelEnabledEditorGizmoOwn, 60,
			"the own gizmo's checkbox is back"),
		st_click(23, kRenderPasses, _kLabelEnabledEditorGizmoOwn,
			"re-enable the own gizmo (restore the default set's state)"),
		st_probe_l(29, &probe_assert_pass_enabled, _kPassEditorGizmoOwn,
			"the own gizmo is enabled again", 0, 1),
	};

	constexpr zircon_ui_test_step_t _kSteps_render_passes_move_grid[] = {
		st_focus(0, kRenderPasses, "raise the Render Passes window"),
		st_probe_l(1, &probe_assert_pass_index, _kPassEditorGrid,
			"the grid pass starts at index 1 (present, grid, csg, gizmo, "
			"imgui)",
			0, 1),
		st_click(2, kRenderPasses, _kLabelMoveDownEditorGrid,
			"move the grid pass down (a rebuild runs at the frame "
			"boundary)"),
		st_wait(10, kRenderPasses, _kLabelMoveDownEditorGrid, 180,
			"the rebuilt set redraws"),
		st_probe_l(11, &probe_assert_pass_index, _kPassEditorGrid,
			"the grid pass moved to index 2", 0, 2),
		st_click(12, kRenderPasses, _kLabelMoveUpEditorGrid,
			"move the grid pass back up"),
		st_wait(20, kRenderPasses, _kLabelMoveUpEditorGrid, 180,
			"the rebuilt set redraws"),
		st_probe_l(21, &probe_assert_pass_index, _kPassEditorGrid,
			"the grid pass is back at index 1", 0, 1),
	};

	// Save serializes the live pass sets + the "don't show" checkbox to
	// game_config.json — the file flips to false, then back to true, and
	// ends byte-identical to the activation-time backup (the toggles are
	// net-zero)
	constexpr zircon_ui_test_step_t _kSteps_render_passes_save[] = {
		st_focus(0, kRenderPasses, "raise the Render Passes window"),
		st_click(1, kRenderPasses, "checkbox:dont_show_on_start",
			"toggle 'don't show on start again' (not persisted yet)"),
		st_click(6, kRenderPasses, "button:Save",
			"Save — serializes the config to game_config.json"),
		st_probe_l(12, &probe_assert_config_file_contains,
			"\"show_pass_manager_on_start\":false",
			"the file now carries \"show_pass_manager_on_start\":false"),
		st_click(13, kRenderPasses, "checkbox:dont_show_on_start",
			"toggle 'don't show on start again' back"),
		st_click(18, kRenderPasses, "button:Save", "Save again"),
		st_probe_l(24, &probe_assert_config_file_contains,
			"\"show_pass_manager_on_start\":true",
			"the file carries \"show_pass_manager_on_start\":true again"),
		st_probe(24, &probe_assert_config_file_matches_backup,
			"the config file is byte-identical to the activation-time "
			"backup"),
	};

	// every Settings checkbox toggled twice (flip + restore, each
	// asserted against the captured pre-value); the Features header is
	// collapsed by default (collapsing state is per-process, not
	// persisted), so the test expands it first
	constexpr zircon_ui_test_step_t _kSteps_settings_toggle_features[] = {
		st_focus(0, kSettings, "raise the Settings window"),
		st_wait(1, kSettings, "settings.header.features", 0,
			"the Features header is drawn"),
		st_click(2, kSettings, "settings.header.features",
			"expand the Features header (collapsed by default)"),
		st_wait(8, kSettings,
			"settings.feature.sdk_camera_rotation_quaternion", 0,
			"the checkboxes are drawn"),
		st_probe(9, &probe_capture_sdk_feature,
			"capture the quaternion feature",
			static_cast<kotek::int32_t>(
				eZirconSDKFeatures::kSDK_Feature_SDKCamera_Rotation_Quaternion),
			2),
		st_click(10, kSettings,
			"settings.feature.sdk_camera_rotation_quaternion",
			"toggle the quaternion feature checkbox"),
		st_probe(16, &probe_assert_sdk_feature_flipped_scratch,
			"the config feature flipped with the checkbox",
			static_cast<kotek::int32_t>(
				eZirconSDKFeatures::kSDK_Feature_SDKCamera_Rotation_Quaternion),
			2),
		st_click(17, kSettings,
			"settings.feature.sdk_camera_rotation_quaternion",
			"toggle it back"),
		st_probe(23, &probe_assert_sdk_feature_equals_scratch,
			"the config feature is restored",
			static_cast<kotek::int32_t>(
				eZirconSDKFeatures::kSDK_Feature_SDKCamera_Rotation_Quaternion),
			2),
		st_probe(24, &probe_capture_sdk_feature, "capture the bootstrap feature",
			static_cast<kotek::int32_t>(eZirconSDKFeatures::
					kSDK_Feature_AddSdkCameraInputBootstrap_Automatically),
			2),
		st_click(25, kSettings, "settings.feature.sdk_camera_input_bootstrap",
			"toggle the bootstrap feature checkbox"),
		st_probe(31, &probe_assert_sdk_feature_flipped_scratch,
			"the bootstrap feature flipped",
			static_cast<kotek::int32_t>(eZirconSDKFeatures::
					kSDK_Feature_AddSdkCameraInputBootstrap_Automatically),
			2),
		st_click(32, kSettings, "settings.feature.sdk_camera_input_bootstrap",
			"toggle it back"),
		st_probe(38, &probe_assert_sdk_feature_equals_scratch,
			"the bootstrap feature is restored",
			static_cast<kotek::int32_t>(eZirconSDKFeatures::
					kSDK_Feature_AddSdkCameraInputBootstrap_Automatically),
			2),
		st_probe(39, &probe_capture_sdk_feature, "capture the sbb-quality feature",
			static_cast<kotek::int32_t>(
				eZirconSDKFeatures::kSDK_Feature_SphereBoundingBox_Quality),
			2),
		st_click(40, kSettings,
			"settings.feature.sphere_bounding_box_quality",
			"toggle the sphere-bounding-box-quality checkbox"),
		st_probe(46, &probe_assert_sdk_feature_flipped_scratch,
			"the sbb-quality feature flipped",
			static_cast<kotek::int32_t>(
				eZirconSDKFeatures::kSDK_Feature_SphereBoundingBox_Quality),
			2),
		st_wait(47, kSettings, "settings.feature.sbb_quality", 60,
			"the SBB-quality DragInt appeared with the feature on "
			"(conditional-widget proof; assumes the feature's default is "
			"off — the flip above turned it ON)"),
		st_click(48, kSettings,
			"settings.feature.sphere_bounding_box_quality", "toggle it back"),
		st_probe(54, &probe_assert_sdk_feature_equals_scratch,
			"the sbb-quality feature is restored",
			static_cast<kotek::int32_t>(
				eZirconSDKFeatures::kSDK_Feature_SphereBoundingBox_Quality),
			2),
		st_probe(55, &probe_capture_sdk_feature,
			"capture the add-required-components feature",
			static_cast<kotek::int32_t>(eZirconSDKFeatures::
					kSDK_Feature_AddRequiredComponents_Automatically),
			2),
		st_click(56, kSettings, "settings.feature.add_required_components",
			"toggle the add-required-components checkbox"),
		st_probe(62, &probe_assert_sdk_feature_flipped_scratch,
			"the add-required-components feature flipped",
			static_cast<kotek::int32_t>(eZirconSDKFeatures::
					kSDK_Feature_AddRequiredComponents_Automatically),
			2),
		st_click(63, kSettings, "settings.feature.add_required_components",
			"toggle it back"),
		st_probe(69, &probe_assert_sdk_feature_equals_scratch,
			"the add-required-components feature is restored",
			static_cast<kotek::int32_t>(eZirconSDKFeatures::
					kSDK_Feature_AddRequiredComponents_Automatically),
			2),
	};

	// the window is read-only (clicking a row only re-marks a local
	// selection variable) — the pin is the row COUNT against the live
	// command pool (a window↔engine consistency contract)
	constexpr zircon_ui_test_step_t _kSteps_history_window_rows[] = {
		st_focus(0, kHistoryCommandLog, "raise the History Command Log"),
		st_wait(1, kHistoryCommandLog, "row:0", 0,
			"the window lists the journaled commands"),
		st_probe(2, &probe_assert_history_rows_match_pool,
			"the listed rows match the live command pool"),
	};

	// the File menu opens; the destructive items (Open = a blocking OS
	// dialog, Save = the Z8 session-serialize trap, Exit = the
	// save-scene modal) are deliberately not clicked — the test pins the
	// menu opening and the ESC dismiss
	constexpr zircon_ui_test_step_t _kSteps_file_menu[] = {
		st_click(0, kTopBar, _kTopBarMenuFile, "open the File menu"),
		st_wait(4, kTopBar, "Save", 0, "the File menu popup opened"),
		st_probe(5, &probe_assert_popup_open, "the File menu is open", 1),
		st_key(6, _kKeyCode_Escape, "ESC closes the menu"),
		st_probe(9, &probe_assert_popup_open, "the File menu is closed", 0),
	};

	constexpr zircon_ui_test_case_t _kCatalog[] = {
		{"menu_view_opens_esc_closes", _kSteps_menu_view,
			static_cast<kotek::uint16_t>(
				sizeof(_kSteps_menu_view) / sizeof(zircon_ui_test_step_t)),
			24},
		{"view_show_object_list", _kSteps_view_show_object_list,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_view_show_object_list) /
				sizeof(zircon_ui_test_step_t)),
			20},
		{"view_show_component_inspector", _kSteps_view_show_inspector,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_view_show_inspector) /
				sizeof(zircon_ui_test_step_t)),
			20},
		{"view_show_history_command_log", _kSteps_view_show_history,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_view_show_history) /
				sizeof(zircon_ui_test_step_t)),
			20},
		{"view_show_settings", _kSteps_view_show_settings,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_view_show_settings) /
				sizeof(zircon_ui_test_step_t)),
			20},
		{"view_show_log", _kSteps_view_show_log,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_view_show_log) /
				sizeof(zircon_ui_test_step_t)),
			20},
		{"view_show_render_stats", _kSteps_view_show_render_stats,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_view_show_render_stats) /
				sizeof(zircon_ui_test_step_t)),
			20},
		{"view_show_debug_input", _kSteps_view_show_debug_input,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_view_show_debug_input) /
				sizeof(zircon_ui_test_step_t)),
			20},
		{"view_show_prefab", _kSteps_view_show_prefab,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_view_show_prefab) /
				sizeof(zircon_ui_test_step_t)),
			20},
		{"view_toggle_render_passes", _kSteps_view_toggle_render_passes,
			static_cast<kotek::uint16_t>(
				sizeof(_kSteps_view_toggle_render_passes) /
				sizeof(zircon_ui_test_step_t)),
			34},
		{"entity_create", _kSteps_entity_create,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_entity_create) /
				sizeof(zircon_ui_test_step_t)),
			14},
		{"entity_select_row", _kSteps_entity_select,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_entity_select) /
				sizeof(zircon_ui_test_step_t)),
			15},
		{"inspector_blocked_add_modal_esc", _kSteps_inspector_modal_esc,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_inspector_modal_esc) /
				sizeof(zircon_ui_test_step_t)),
			32},
		{"inspector_add_component", _kSteps_inspector_add_component,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_inspector_add_component) /
				sizeof(zircon_ui_test_step_t)),
			34},
		{"inspector_delete_component", _kSteps_inspector_delete_component,
			static_cast<kotek::uint16_t>(
				sizeof(_kSteps_inspector_delete_component) /
				sizeof(zircon_ui_test_step_t)),
			25},
		{"entity_delete", _kSteps_entity_delete,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_entity_delete) /
				sizeof(zircon_ui_test_step_t)),
			22},
		{"edit_undo_redo", _kSteps_edit_undo_redo,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_edit_undo_redo) /
				sizeof(zircon_ui_test_step_t)),
			32},
		{"render_passes_toggle_grid", _kSteps_render_passes_toggle_grid,
			static_cast<kotek::uint16_t>(
				sizeof(_kSteps_render_passes_toggle_grid) /
				sizeof(zircon_ui_test_step_t)),
			21},
		{"render_passes_add_remove_imguizmo_gizmo_exclusion",
			_kSteps_render_passes_add_remove,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_render_passes_add_remove) /
				sizeof(zircon_ui_test_step_t)),
			35},
		{"render_passes_move_grid", _kSteps_render_passes_move_grid,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_render_passes_move_grid) /
				sizeof(zircon_ui_test_step_t)),
			27},
		{"render_passes_save", _kSteps_render_passes_save,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_render_passes_save) /
				sizeof(zircon_ui_test_step_t)),
			30},
		{"settings_toggle_features", _kSteps_settings_toggle_features,
			static_cast<kotek::uint16_t>(
				sizeof(_kSteps_settings_toggle_features) /
				sizeof(zircon_ui_test_step_t)),
			76},
		{"history_window_rows", _kSteps_history_window_rows,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_history_window_rows) /
				sizeof(zircon_ui_test_step_t)),
			8},
		{"file_menu_opens_esc_closes", _kSteps_file_menu,
			static_cast<kotek::uint16_t>(sizeof(_kSteps_file_menu) /
				sizeof(zircon_ui_test_step_t)),
			15},
	};
} // namespace

const zircon_ui_test_case_t* zircon_ui_test_catalog_get_cases(
	kotek::uint8_t& out_count) noexcept
{
	out_count = static_cast<kotek::uint8_t>(
		sizeof(_kCatalog) / sizeof(zircon_ui_test_case_t));
	return _kCatalog;
}

const zircon_ui_test_case_t* zircon_ui_test_catalog_find(
	const char* p_name) noexcept
{
	if (p_name == nullptr)
		return nullptr;

	for (const auto& test_case : _kCatalog)
	{
		if (std::strcmp(test_case.p_name, p_name) == 0)
		{
			return &test_case;
		}
	}

	return nullptr;
}
