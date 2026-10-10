#include "zircon_ui_test_harness.h"

#include "../../core/zircon_config.h"
#include "../../render/bgfx/zircon_renderer.h"
#include "../../world/zircon_world.h"
#include "../session/zircon_session_editor.h"

#include <kotek.core.filesystem/include/kotek_filesystem_helpers.h>

#include <cstring>

namespace
{
	// the wrapper's backend-callback seam speaks the platform backend's
	// codes — GLFW on the default window library (the win32 backend maps
	// the callback forwards to no-ops today, so a WIN32 window-library
	// build fails the harness's waits loudly, never silently)
	constexpr kotek::int32_t _kGlfwActionRelease = 0;
	constexpr kotek::int32_t _kGlfwActionPress = 1;

	bool zircon_ui_test_strings_equal(
		const char* p_left, const char* p_right) noexcept
	{
		if (p_left == nullptr || p_right == nullptr)
			return false;

		return std::strcmp(p_left, p_right) == 0;
	}
} // namespace

zircon_ui_test_widget_registry::zircon_ui_test_widget_registry(void) :
	m_is_active{}, m_was_overflow_warned{}, m_frame_counter{}
{
}

zircon_ui_test_widget_registry::~zircon_ui_test_widget_registry(void) {}

void zircon_ui_test_widget_registry::set_active(bool is_active) noexcept
{
	this->m_is_active = is_active;
}

bool zircon_ui_test_widget_registry::is_active(void) const noexcept
{
	return this->m_is_active;
}

void zircon_ui_test_widget_registry::set_frame_counter(
	kotek::uint32_t frame_counter) noexcept
{
	this->m_frame_counter = frame_counter;
}

kotek::uint32_t zircon_ui_test_widget_registry::get_frame_counter(
	void) const noexcept
{
	return this->m_frame_counter;
}

void zircon_ui_test_widget_registry::track(const char* p_window_name,
	const char* p_widget_label, float min_x, float min_y, float max_x,
	float max_y) noexcept
{
	if (p_window_name == nullptr || p_widget_label == nullptr)
		return;

	for (auto& widget : this->m_widgets)
	{
		if ((widget.m_window_name == p_window_name) &&
			(widget.m_label == p_widget_label))
		{
			widget.m_prev_min_x = widget.m_min_x;
			widget.m_prev_min_y = widget.m_min_y;
			widget.m_prev_max_x = widget.m_max_x;
			widget.m_prev_max_y = widget.m_max_y;
			widget.m_min_x = min_x;
			widget.m_min_y = min_y;
			widget.m_max_x = max_x;
			widget.m_max_y = max_y;
			widget.m_last_frame_touched = this->m_frame_counter;
			return;
		}
	}

	if (this->m_widgets.size() >= this->m_widgets.capacity())
	{
		if (this->m_was_overflow_warned == false)
		{
			this->m_was_overflow_warned = true;
			KOTEK_MESSAGE_ERROR(
				"[ui-test]: the widget registry is full ({} entries) — "
				"'{}/{}' and later widgets are dropped; raise "
				"ZIRCON_DEF_UI_TEST_WIDGET_REGISTRY_MAX",
				this->m_widgets.capacity(), p_window_name,
				p_widget_label);
		}
		return;
	}

	zircon_ui_test_widget_t widget{};
	widget.m_window_name = p_window_name;
	widget.m_label = p_widget_label;
	widget.m_min_x = min_x;
	widget.m_min_y = min_y;
	widget.m_max_x = max_x;
	widget.m_max_y = max_y;
	// the sentinel: a first-sight entry is NOT stable until its rect
	// survives a second track unchanged (moving popups settle in 1-2
	// frames; a first-frame popup click is the classic miss)
	widget.m_prev_min_x = -1.0f;
	widget.m_prev_min_y = -1.0f;
	widget.m_prev_max_x = -1.0f;
	widget.m_prev_max_y = -1.0f;
	widget.m_last_frame_touched = this->m_frame_counter;
	this->m_widgets.push_back(widget);
}

bool zircon_ui_test_widget_registry::is_fresh(const char* p_window_name,
	const char* p_widget_label) const noexcept
{
	float center_x{};
	float center_y{};

	return this->find_fresh_center(
		p_window_name, p_widget_label, center_x, center_y);
}

bool zircon_ui_test_widget_registry::is_fresh_and_stable(
	const char* p_window_name, const char* p_widget_label) const noexcept
{
	if (p_window_name == nullptr || p_widget_label == nullptr)
		return false;

	for (const auto& widget : this->m_widgets)
	{
		if ((widget.m_window_name == p_window_name) &&
			(widget.m_label == p_widget_label))
		{
			const bool is_fresh =
				(widget.m_last_frame_touched +
					ZIRCON_DEF_UI_TEST_WIDGET_FRESHNESS_FRAMES) >=
				this->m_frame_counter;

			return is_fresh &&
				(widget.m_prev_min_x == widget.m_min_x) &&
				(widget.m_prev_min_y == widget.m_min_y) &&
				(widget.m_prev_max_x == widget.m_max_x) &&
				(widget.m_prev_max_y == widget.m_max_y);
		}
	}

	return false;
}

bool zircon_ui_test_widget_registry::find_fresh_center(
	const char* p_window_name, const char* p_widget_label,
	float& out_center_x, float& out_center_y) const noexcept
{
	if (p_window_name == nullptr || p_widget_label == nullptr)
		return false;

	for (const auto& widget : this->m_widgets)
	{
		if ((widget.m_window_name == p_window_name) &&
			(widget.m_label == p_widget_label))
		{
			if ((widget.m_last_frame_touched +
					ZIRCON_DEF_UI_TEST_WIDGET_FRESHNESS_FRAMES) <
				this->m_frame_counter)
			{
				return false;
			}

			// the click point is LEFT-BIASED, not centered (the 2026-10-07
			// clipped-checkbox lesson): a widget's rect spans its full
			// label, but a narrow window clips the label's tail — the
			// rect's center can then sit in ANOTHER window's area, while
			// a few px from the left edge is always inside the widget's
			// own visible hit region (the checkbox square, the button's
			// left face, the menu row's icon column)
			const float width = widget.m_max_x - widget.m_min_x;
			out_center_x = widget.m_min_x +
				(width > 16.0f ? 8.0f : width * 0.5f);
			out_center_y = (widget.m_min_y + widget.m_max_y) * 0.5f;
			return true;
		}
	}

	return false;
}

kotek::uint32_t zircon_ui_test_widget_registry::count_fresh_with_prefix(
	const char* p_window_name, const char* p_label_prefix) const noexcept
{
	if (p_window_name == nullptr || p_label_prefix == nullptr)
		return 0;

	kotek::uint32_t result = 0;

	const kotek::ktk::size_t prefix_length = std::strlen(p_label_prefix);

	for (const auto& widget : this->m_widgets)
	{
		if ((widget.m_window_name == p_window_name) &&
			(std::strncmp(widget.m_label.c_str(), p_label_prefix,
				 prefix_length) == 0) &&
			((widget.m_last_frame_touched +
				 ZIRCON_DEF_UI_TEST_WIDGET_FRESHNESS_FRAMES) >=
				this->m_frame_counter))
		{
			++result;
		}
	}

	return result;
}

void zircon_ui_test_track_widget(
	kotek::core::ktkIImguiWrapper* p_wrapper_imgui,
	zircon_ui_test_widget_registry* p_registry, const char* p_window_name,
	const char* p_widget_label) noexcept
{
	// the one-branch fast path: no run active = zero instrumentation cost
	if (p_registry == nullptr || p_registry->is_active() == false)
		return;

	if (p_wrapper_imgui == nullptr)
		return;

	// GetItemRect* of the just-drawn widget — through the wrapper so the
	// call runs against the EXE's imgui copy (the real context), never
	// this module's own (the two-imgui-copies rule, zircon §5)
	const ImVec2 rect_min = p_wrapper_imgui->GetItemRectMin();
	const ImVec2 rect_max = p_wrapper_imgui->GetItemRectMax();

	p_registry->track(p_window_name, p_widget_label, rect_min.x,
		rect_min.y, rect_max.x, rect_max.y);
}

zircon_ui_test_harness::zircon_ui_test_harness(void) :
	m_is_active{}, m_is_finished{}, m_was_activated_ok{},
	m_current_test_index{}, m_run_count{}, m_current_step_index{},
	m_frame_counter{}, m_test_frame_counter{}, m_last_action_frame{},
	m_wait_frames{},
	m_warmup_frames_remaining{}, m_tests_failed{},
	m_current_test_failures{}, m_p_native_window_handle{},
	m_p_main_manager{}, p_session_cached{}, m_p_renderer_bgfx_cached{},
	m_context{}, m_config_backup{}, m_config_backup_size{}
{
}

zircon_ui_test_harness::~zircon_ui_test_harness(void) {}

bool zircon_ui_test_harness::is_active(void) const noexcept
{
	return this->m_is_active;
}

bool zircon_ui_test_harness::is_finished(void) const noexcept
{
	return this->m_is_finished;
}

bool zircon_ui_test_harness::was_activated_ok(void) const noexcept
{
	return this->m_was_activated_ok;
}

zircon_ui_test_widget_registry*
zircon_ui_test_harness::get_widget_registry(void) noexcept
{
	return &this->m_widget_registry;
}

void zircon_ui_test_harness::activate(const char* p_test_name,
	kotek::core::ktkMainManager* p_main_manager,
	zircon_session_editor* p_session) noexcept
{
	KOTEK_ASSERT(p_test_name && p_test_name[0] != '\0',
		"--ui_test= needs a value (--ui_test=all or a test name)");
	KOTEK_ASSERT(p_main_manager, "must be valid!");
	KOTEK_ASSERT(p_session, "must be valid!");

	this->m_p_main_manager = p_main_manager;
	this->p_session_cached = p_session;

	kotek::core::ktkIImguiWrapper* p_wrapper =
		p_main_manager->Get_ImguiWrapper();

	KOTEK_ASSERT(p_wrapper,
		"--ui_test requires --editor_imgui (the imgui wrapper exists only "
		"in editor boots) — the UI-press harness drives the editor's imgui "
		"frame");

	zircon_renderer_bgfx* p_renderer_bgfx = nullptr;

	if (p_main_manager->GetGameManager())
	{
		p_renderer_bgfx = dynamic_cast<zircon_renderer_bgfx*>(
			p_main_manager->GetGameManager()->GetRenderer());

		this->m_p_native_window_handle =
			p_main_manager->GetGameManager()->GetWindowHandle();
	}

	KOTEK_ASSERT(p_renderer_bgfx,
		"--ui_test drives the editor's bgfx render graph (the editor imgui "
		"pass lives there) — the active backend has no render graphs");

	// the renderer is stable for the boot — cache it for the per-tick
	// context (the pass-set probes read the graph info through it)
	this->m_p_renderer_bgfx_cached = p_renderer_bgfx;

	// the backend-callback forwards (GLFW on the default window library)
	// dereference the native handle (glfwGetKey in UpdateKeyModifiers) —
	// a handle-less boot can never inject
	KOTEK_ASSERT(this->m_p_native_window_handle,
		"--ui_test needs the active window's native handle for the "
		"backend-callback injection seam");

	// the run list from the catalog
	if (zircon_ui_test_strings_equal(p_test_name, "all"))
	{
		kotek::uint8_t case_count{};
		const zircon_ui_test_case_t* p_cases =
			zircon_ui_test_catalog_get_cases(case_count);

		for (kotek::uint8_t i = 0; i < case_count; ++i)
		{
			if (this->m_run_list.size() >= this->m_run_list.capacity())
			{
				KOTEK_ASSERT(false,
					"the ui-test catalog outgrew "
					"ZIRCON_DEF_UI_TEST_MAX_RUN_TESTS");
				break;
			}

			this->m_run_list.push_back(&p_cases[i]);
		}
	}
	else
	{
		const zircon_ui_test_case_t* p_case =
			zircon_ui_test_catalog_find(p_test_name);

		if (p_case == nullptr)
		{
			kotek::uint8_t case_count{};
			const zircon_ui_test_case_t* p_cases =
				zircon_ui_test_catalog_get_cases(case_count);

			for (kotek::uint8_t i = 0; i < case_count; ++i)
			{
				KOTEK_MESSAGE(
					"[ui-test]: the catalog has test '{}'",
					p_cases[i].p_name);
			}

			KOTEK_ASSERT(false,
				"unknown --ui_test='{}' — the catalog's names are listed "
				"above",
				p_test_name);
			return;
		}

		this->m_run_list.push_back(p_case);
	}

	this->m_run_count =
		static_cast<kotek::uint8_t>(this->m_run_list.size());

	// the config byte backup (the Render Passes "Save" test asserts the
	// serialized file flips and then returns byte-identical — the
	// boot's shutdown serialize rewrites the file from the in-memory
	// config, so net-zero in-memory state + this backup pins the whole
	// roundtrip)
	if (kotek::core::ktkIFileSystem* p_filesystem =
			p_main_manager->GetFileSystem())
	{
		ktk_filesystem_path path_to_file;
		kotek::core::path_for(p_filesystem,
			kotek::core::eFolderIndex::kFolderIndex_DataUser,
			kZirconConfig_FileName, path_to_file);

		kotek::size_t read_size = 0;

		if (kotek::core::read_file(p_filesystem, path_to_file,
				this->m_config_backup.data(), this->m_config_backup.size(),
				read_size))
		{
			this->m_config_backup_size = read_size;
		}
		else
		{
			KOTEK_MESSAGE_WARNING(
				"[ui-test]: could not back up '{}' ({} bytes required) — "
				"the Save test's byte-compare will fail loudly if used",
				kZirconConfig_FileName, read_size);
		}
	}

	this->m_is_active = true;
	this->m_was_activated_ok = true;
	this->m_warmup_frames_remaining = ZIRCON_DEF_UI_TEST_WARMUP_FRAMES;
	this->m_widget_registry.set_active(true);

	KOTEK_MESSAGE("[ui-test]: activated — {} ui test(s) queued",
		this->m_run_count);

	// the frame-budget sanity check: the harness stops the application
	// when the run finishes, so --kotek_frames is a pure safety cap — but
	// a cap BELOW the budget cuts the run short (which then fails at
	// module shutdown); warn early with the estimate
	kotek::uint32_t frames_estimated = ZIRCON_DEF_UI_TEST_WARMUP_FRAMES;

	for (const auto* p_case : this->m_run_list)
	{
		frames_estimated += p_case->m_frame_budget;
	}

	const kotek::uint32_t frames_limit =
		p_main_manager->Get_EngineConfig()->Get_FramesLimit();

	if (frames_limit != 0 && frames_limit < frames_estimated)
	{
		KOTEK_MESSAGE_WARNING(
			"[ui-test]: --kotek_frames={} is below the estimated budget "
			"(~{} frames for {} test(s)) — the run will be cut short and "
			"FAIL at module shutdown; raise the cap",
			frames_limit, frames_estimated, this->m_run_count);
	}
}

void zircon_ui_test_harness::on_frame_pre_draw(
	kotek::core::ktkIImguiWrapper* p_wrapper,
	zircon_session_editor* p_session,
	kotek::core::ktkMainManager* p_main_manager) noexcept
{
	if (this->m_is_active == false || this->m_is_finished)
		return;

	++this->m_frame_counter;
	this->m_widget_registry.set_frame_counter(this->m_frame_counter);

	if (this->m_warmup_frames_remaining > 0)
	{
		--this->m_warmup_frames_remaining;
		return;
	}

	if (p_wrapper == nullptr || p_session == nullptr)
	{
		// a headless/torn-down frame can never silently pass a run
		++this->m_current_test_failures;
		KOTEK_MESSAGE_WARNING(
			"[ui-test]: frame without a wrapper/session — counted as a "
			"failure of the current test");
		this->conclude_test();
		return;
	}

	// the OS-cursor poll disarm (the 2026-10-07 lesson, twice): the GLFW
	// backend's ImGui_ImplGlfw_UpdateMouseData polls the REAL OS cursor
	// every NewFrame while the window is focused AND bd->MouseWindow ==
	// nullptr — its pos event is queued AFTER the harness's, clobbering
	// every injected position. A synthetic cursor-enter marks
	// bd->MouseWindow non-null and the poll is skipped. It must be
	// RE-ISSUED EVERY TICK: a render-graph rebuild (pass add/remove/move)
	// destroys and recreates the imgui backend, so bd->MouseWindow goes
	// back to nullptr and the poll silently re-arms — every click after
	// the rebuild then lands on the physical cursor (the add/remove,
	// move, save and settings failures' common root). It must also run
	// here (the first tick), not in activate(): the imgui backend exists
	// only after the editor graph's first OnCreateResources — activate
	// runs during session creation, before it, and the backend's
	// GetBackendData()==nullptr dereference segfaults there. finish_run
	// restores the poll for tidiness.
	p_wrapper->ImGui_CursorEnterCallback(
		this->m_p_native_window_handle, 1);

	this->inject_pending_due(p_wrapper);

	// the per-tick context the probes read
	this->m_context.p_main_manager = p_main_manager;
	this->m_context.p_imgui_wrapper = p_wrapper;
	this->m_context.p_session = p_session;
	this->m_context.p_history = p_session->get_command_history();
	this->m_context.p_ui_state = p_session->get_ui_state();
	this->m_context.p_world = p_session->get_world();
	this->m_context.p_factory =
		this->m_context.p_world ? this->m_context.p_world->get_factory()
								: nullptr;
	this->m_context.p_config = p_session->get_config();
	this->m_context.p_renderer_bgfx = this->m_p_renderer_bgfx_cached;
	this->m_context.p_widgets = &this->m_widget_registry;
	this->m_context.p_config_backup = this->m_config_backup.data();
	this->m_context.m_config_backup_size = this->m_config_backup_size;

	if (this->m_current_test_index >= this->m_run_count)
	{
		this->finish_run();
		return;
	}

	const zircon_ui_test_case_t* p_test =
		this->m_run_list[this->m_current_test_index];

	++this->m_test_frame_counter;

	bool is_blocked_on_wait = false;

	while (this->m_current_step_index < p_test->m_step_count)
	{
		const zircon_ui_test_step_t& step =
			p_test->p_steps[this->m_current_step_index];

		if (step.m_frame_offset > this->m_test_frame_counter)
		{
			break;
		}

		if (step.m_action == eZirconUiTestStepAction::kWaitWidget)
		{
			// a wait's miss is the NORMAL case (the widget hasn't been
			// drawn yet, or is still settling — the STABILITY contract:
			// the rect must survive two consecutive tracks unchanged, so
			// a click after the wait never aims at a popup's first-frame
			// position)
			char formatted_label[ZIRCON_DEF_UI_TEST_WIDGET_LABEL_MAX_LENGTH]{};

			const char* p_label = step.p_target_label;

			if ((step.m_flags &
					ZIRCON_DEF_UI_TEST_STEP_FLAG_LABEL_FORMAT_SCRATCH) &&
				p_label != nullptr)
			{
				kotek::ktk::sprintf(formatted_label, sizeof(formatted_label),
					p_label,
					static_cast<unsigned>(
						this->m_context.scratch[step.m_params[1]]));
				p_label = formatted_label;
			}

			if (this->m_widget_registry.is_fresh_and_stable(
					step.p_target_window, p_label))
			{
				this->m_wait_frames = 0;
				++this->m_current_step_index;
				continue;
			}

			++this->m_wait_frames;

			const kotek::uint32_t timeout =
				step.m_params[0] > 0
				? static_cast<kotek::uint32_t>(step.m_params[0])
				: ZIRCON_DEF_UI_TEST_WAIT_WIDGET_TIMEOUT_FRAMES;

			if (this->m_wait_frames >= timeout)
			{
				++this->m_current_test_failures;
				KOTEK_MESSAGE("[  FAILED ]   Zircon_UI.{} :: step #{} — "
							  "widget '{}/{}' never appeared ({} frames)",
					p_test->p_name, this->m_current_step_index,
					step.p_target_window ? step.p_target_window : "?",
					step.p_target_label ? step.p_target_label : "?",
					timeout);
				this->m_wait_frames = 0;
				++this->m_current_step_index;
				continue;
			}

			is_blocked_on_wait = true;
			break;
		}

		this->m_wait_frames = 0;

		if (step.m_action == eZirconUiTestStepAction::kProbe &&
			this->m_last_action_frame != 0 &&
			(this->m_test_frame_counter - this->m_last_action_frame) <
				ZIRCON_DEF_UI_TEST_PROBE_MIN_GAP_AFTER_ACTION_FRAMES)
		{
			// the probe's effect window hasn't elapsed yet (the last
			// action was pushed late by a wait) — hold the table here;
			// the probe is retried next frame
			break;
		}

		// click serialization (the 2026-10-07 swallowed-press lesson): a
		// press injected while a previous click is still HELD (its release
		// pending) is swallowed by imgui as a duplicate-down — and the
		// first click's release then fires OFF-ITEM after the second
		// click's move. A press step therefore holds until no release for
		// its button/key is outstanding
		if ((step.m_action == eZirconUiTestStepAction::kClickWidget ||
				step.m_action ==
					eZirconUiTestStepAction::kMouseButtonOnWidget ||
				step.m_action == eZirconUiTestStepAction::kKeyPress) &&
			this->has_pending_release_for(step))
		{
			break;
		}

		this->execute_step(this->m_context, step);
		++this->m_current_step_index;
	}

	if (is_blocked_on_wait == false &&
		this->m_current_step_index >= p_test->m_step_count &&
		this->m_pending_events.empty())
	{
		this->conclude_test();
	}
}

void zircon_ui_test_harness::inject_pending_due(
	kotek::core::ktkIImguiWrapper* p_wrapper) noexcept
{
	kotek::uint32_t write_index = 0;

	const kotek::uint32_t count = static_cast<kotek::uint32_t>(
		this->m_pending_events.size());

	for (kotek::uint32_t i = 0; i < count; ++i)
	{
		const pending_event_t& event = this->m_pending_events[i];

		if (event.m_due_frame <= this->m_frame_counter)
		{
			if (event.m_kind == 0)
			{
				p_wrapper->ImGui_MouseButtonCallback(
					this->m_p_native_window_handle,
					static_cast<int>(event.m_code),
					static_cast<int>(_kGlfwActionRelease), 0);
			}
			else
			{
				p_wrapper->ImGui_KeyCallback(
					this->m_p_native_window_handle,
					static_cast<int>(event.m_code), 0,
					static_cast<int>(_kGlfwActionRelease), 0);
			}
		}
		else
		{
			if (write_index != i)
			{
				this->m_pending_events[write_index] = event;
			}
			++write_index;
		}
	}

	while (this->m_pending_events.size() > write_index)
	{
		this->m_pending_events.pop_back();
	}
}

void zircon_ui_test_harness::execute_step(
	zircon_ui_test_context_t& context,
	const zircon_ui_test_step_t& step) noexcept
{
	kotek::core::ktkIImguiWrapper* p_wrapper = context.p_imgui_wrapper;

	// the probe-gap clock: every step that MUTATES something (queued
	// input, a z-order change) moves the clock — probes hold until the
	// effect window elapsed (see
	// ZIRCON_DEF_UI_TEST_PROBE_MIN_GAP_AFTER_ACTION_FRAMES)
	if (step.m_action != eZirconUiTestStepAction::kNoop &&
		step.m_action != eZirconUiTestStepAction::kProbe &&
		step.m_action != eZirconUiTestStepAction::kWaitWidget)
	{
		this->m_last_action_frame = this->m_test_frame_counter;
	}

	switch (step.m_action)
	{
	case eZirconUiTestStepAction::kNoop:
	{
		break;
	}
	case eZirconUiTestStepAction::kMouseMoveToWidget:
	{
		float center_x{};
		float center_y{};

		if (this->resolve_widget_center(step, center_x, center_y))
		{
			p_wrapper->ImGui_CursorPosCallback(
				this->m_p_native_window_handle,
				static_cast<double>(center_x),
				static_cast<double>(center_y));
		}
		break;
	}
	case eZirconUiTestStepAction::kMouseButtonOnWidget:
	{
		if (step.p_target_label != nullptr)
		{
			float center_x{};
			float center_y{};

			if (this->resolve_widget_center(step, center_x, center_y))
			{
				p_wrapper->ImGui_CursorPosCallback(
					this->m_p_native_window_handle,
					static_cast<double>(center_x),
					static_cast<double>(center_y));
			}
		}

		p_wrapper->ImGui_MouseButtonCallback(
			this->m_p_native_window_handle,
			static_cast<int>(step.m_params[0]),
			step.m_params[1] != 0 ? static_cast<int>(_kGlfwActionPress)
								  : static_cast<int>(_kGlfwActionRelease),
			0);
		break;
	}
	case eZirconUiTestStepAction::kMouseWheel:
	{
		p_wrapper->ImGui_ScrollCallback(this->m_p_native_window_handle,
			static_cast<double>(step.m_params[0]),
			static_cast<double>(step.m_params[1]));
		break;
	}
	case eZirconUiTestStepAction::kClickWidget:
	{
		float center_x{};
		float center_y{};

		if (this->resolve_widget_center(step, center_x, center_y) == false)
		{
			// the failure is already logged by the resolver — skip the
			// click entirely (a blind click would land on whatever is
			// under the stale cursor and confuse the failure's origin)
			break;
		}

		p_wrapper->ImGui_CursorPosCallback(this->m_p_native_window_handle,
			static_cast<double>(center_x), static_cast<double>(center_y));

		p_wrapper->ImGui_MouseButtonCallback(this->m_p_native_window_handle,
			static_cast<int>(step.m_params[0]),
			static_cast<int>(_kGlfwActionPress), 0);

		this->schedule_release(0, step.m_params[0]);
		break;
	}
	case eZirconUiTestStepAction::kKeyPress:
	{
		p_wrapper->ImGui_KeyCallback(this->m_p_native_window_handle,
			static_cast<int>(step.m_params[0]), 0,
			static_cast<int>(_kGlfwActionPress), 0);

		this->schedule_release(1, step.m_params[0]);
		break;
	}
	case eZirconUiTestStepAction::kFocusWindow:
	{
		if (step.p_target_window != nullptr)
		{
			p_wrapper->SetWindowFocus(step.p_target_window);
		}
		break;
	}
	case eZirconUiTestStepAction::kProbe:
	{
		if (step.pfn_probe &&
			step.pfn_probe(context, step) == false)
		{
			++this->m_current_test_failures;
			KOTEK_MESSAGE("[  FAILED ]   Zircon_UI.{} :: step #{} — {}",
				this->m_run_list[this->m_current_test_index]->p_name,
				this->m_current_step_index,
				step.p_debug ? step.p_debug : "<unnamed assertion>");
		}
		break;
	}
	default:
	{
		break;
	}
	}
}

bool zircon_ui_test_harness::find_widget_center(
	const zircon_ui_test_step_t& step, float& out_x, float& out_y) noexcept
{
	char formatted_label[ZIRCON_DEF_UI_TEST_WIDGET_LABEL_MAX_LENGTH]{};

	const char* p_label = step.p_target_label;

	if ((step.m_flags & ZIRCON_DEF_UI_TEST_STEP_FLAG_LABEL_FORMAT_SCRATCH) &&
		p_label != nullptr)
	{
		kotek::ktk::sprintf(formatted_label, sizeof(formatted_label),
			p_label,
			static_cast<unsigned>(
				this->m_context.scratch[step.m_params[1]]));
		p_label = formatted_label;
	}

	return this->m_widget_registry.find_fresh_center(
		step.p_target_window, p_label, out_x, out_y);
}

bool zircon_ui_test_harness::resolve_widget_center(
	const zircon_ui_test_step_t& step, float& out_x, float& out_y) noexcept
{
	if (this->find_widget_center(step, out_x, out_y) == false)
	{
		// a missing widget inside an ACTION step is a failed step (the
		// tables use kWaitWidget to synchronize with popups — a bare
		// miss here means the choreography is wrong)
		++this->m_current_test_failures;
		KOTEK_MESSAGE("[  FAILED ]   Zircon_UI.{} :: step #{} — no fresh "
					  "rect for widget '{}/{}' (is the window open? use "
					  "kWaitWidget to synchronize)",
			this->m_run_list[this->m_current_test_index]->p_name,
			this->m_current_step_index,
			step.p_target_window ? step.p_target_window : "?",
			step.p_target_label ? step.p_target_label : "?");
		return false;
	}

	return true;
}

void zircon_ui_test_harness::schedule_release(
	kotek::uint8_t kind, kotek::int32_t code) noexcept
{
	if (this->m_pending_events.size() >= this->m_pending_events.capacity())
	{
		++this->m_current_test_failures;
		KOTEK_MESSAGE_WARNING(
			"[ui-test]: the pending-release queue is full — a click/key "
			"release was dropped (raise "
			"ZIRCON_DEF_UI_TEST_PENDING_EVENTS_MAX)");
		return;
	}

	pending_event_t event{};
	event.m_due_frame =
		this->m_frame_counter + ZIRCON_DEF_UI_TEST_CLICK_RELEASE_DELAY_FRAMES;
	event.m_kind = kind;
	event.m_code = code;
	this->m_pending_events.push_back(event);
}

bool zircon_ui_test_harness::has_pending_release_for(
	const zircon_ui_test_step_t& step) const noexcept
{
	// the step's target button/key: mouse steps name the button in
	// params[0], key presses the key code
	kotek::uint8_t kind = 0;
	kotek::int32_t code = step.m_params[0];

	if (step.m_action == eZirconUiTestStepAction::kKeyPress)
	{
		kind = 1;
	}

	for (const auto& event : this->m_pending_events)
	{
		if (event.m_kind == kind && event.m_code == code)
		{
			return true;
		}
	}

	return false;
}

void zircon_ui_test_harness::conclude_test(void) noexcept
{
	if (this->m_current_test_index >= this->m_run_count)
	{
		return;
	}

	const zircon_ui_test_case_t* p_test =
		this->m_run_list[this->m_current_test_index];

	if (this->m_current_test_failures == 0)
	{
		KOTEK_MESSAGE("[       OK ] Zircon_UI.{} ({} frames)",
			p_test->p_name, this->m_test_frame_counter);
	}
	else
	{
		KOTEK_MESSAGE("[  FAILED ] Zircon_UI.{} — {} assertion(s)/step(s) "
					  "failed (see the step lines above)",
			p_test->p_name, this->m_current_test_failures);
		++this->m_tests_failed;
	}

	++this->m_current_test_index;
	this->m_current_step_index = 0;
	this->m_test_frame_counter = 0;
	this->m_last_action_frame = 0;
	this->m_wait_frames = 0;
	this->m_current_test_failures = 0;

	for (auto& slot : this->m_context.scratch)
	{
		slot = 0;
	}

	if (this->m_current_test_index >= this->m_run_count)
	{
		this->finish_run();
	}
}

void zircon_ui_test_harness::finish_run(void) noexcept
{
	this->m_is_finished = true;
	this->m_widget_registry.set_active(false);

	// re-arm the backend's OS-cursor poll (the activate-time synthetic
	// cursor-enter disabled it for the run — see activate)
	if (this->m_p_main_manager && this->m_p_main_manager->Get_ImguiWrapper() &&
		this->m_p_native_window_handle)
	{
		this->m_p_main_manager->Get_ImguiWrapper()->ImGui_CursorEnterCallback(
			this->m_p_native_window_handle, 0);
	}

	KOTEK_MESSAGE(
		"[ui-test]: [==========] {} ui test(s) ran", this->m_run_count);

	// the config file must be byte-identical to the activation-time
	// backup (every toggle the tests flipped was flipped back — the
	// roundtrip determinism is itself under test here)
	bool is_config_clean = true;

	if (this->m_config_backup_size > 0)
	{
		kotek::core::ktkIFileSystem* p_filesystem =
			this->m_p_main_manager->GetFileSystem();

		if (p_filesystem)
		{
			ktk_filesystem_path path_to_file;
			kotek::core::path_for(p_filesystem,
				kotek::core::eFolderIndex::kFolderIndex_DataUser,
				kZirconConfig_FileName, path_to_file);

			kotek::array_t<kotek::uint8_t,
				ZIRCON_DEF_UI_TEST_CONFIG_BACKUP_CAPACITY>
				current{};

			kotek::size_t current_size = 0;

			if (kotek::core::read_file(p_filesystem, path_to_file,
					current.data(), current.size(), current_size) == false ||
				current_size != this->m_config_backup_size ||
				std::memcmp(current.data(), this->m_config_backup.data(),
					this->m_config_backup_size) != 0)
			{
				is_config_clean = false;

				KOTEK_MESSAGE_ERROR(
					"[ui-test]: game_config.json drifted across the run "
					"(backup {} bytes vs current {} bytes) — a test did "
					"not restore what it toggled; restoring the backup",
					this->m_config_backup_size, current_size);
			}
		}
	}

	if (is_config_clean == false)
	{
		this->restore_config_file();
	}

	if (m_tests_failed == 0 && is_config_clean)
	{
		KOTEK_MESSAGE(
			"[ui-test]: [  PASSED  ] {} ui test(s)", this->m_run_count);
		KOTEK_MESSAGE(
			"[ui-test]: all UI suites green — requesting application stop "
			"(the run finished; --kotek_frames is a safety cap)");

		this->m_p_main_manager->Get_EngineConfig()->SetApplicationWorking(
			false);
	}
	else
	{
		KOTEK_ASSERT(false,
			"[ui-test]: {} of {} ui test(s) FAILED (config drift: {}) — "
			"see the [  FAILED ] lines above",
			this->m_tests_failed, this->m_run_count, !is_config_clean);
	}
}

void zircon_ui_test_harness::restore_config_file(void) noexcept
{
	if (this->m_config_backup_size == 0 || this->m_p_main_manager == nullptr)
		return;

	kotek::core::ktkIFileSystem* p_filesystem =
		this->m_p_main_manager->GetFileSystem();

	if (p_filesystem == nullptr)
		return;

	ktk_filesystem_path path_to_file;
	kotek::core::path_for(p_filesystem,
		kotek::core::eFolderIndex::kFolderIndex_DataUser,
		kZirconConfig_FileName, path_to_file);

	const bool is_written = p_filesystem->Write_File(path_to_file,
		this->m_config_backup.data(), this->m_config_backup_size);

	KOTEK_MESSAGE(is_written
			? "[ui-test]: game_config.json restored from the backup"
			: "[ui-test]: FAILED to restore game_config.json — restore "
			  "it by hand from git");
}

void zircon_ui_test_harness::on_module_shutdown(void) noexcept
{
	if (this->m_is_active && this->was_activated_ok() &&
		this->m_is_finished == false)
	{
		KOTEK_ASSERT(false,
			"[ui-test]: the --ui_test run never finished (stopped at "
			"test {} of {}, step {}) — the --kotek_frames budget is too "
			"small or a kWaitWidget stalled; an unfinished run is a "
			"FAILED run",
			this->m_current_test_index, this->m_run_count,
			this->m_current_step_index);
	}
}
