#pragma once

// task Z17 (owner directive: "unit tests for everything written with
// ImGui in zircon — test all buttons and behaviors, the UI-user
// interaction, executed in the SAME flow as regular unit tests"): the
// UI-press test harness. Design (the Z17 registry row is the reviewed
// design):
//
// * Event injection at the ImGuiIO seam — NO Win32 synthesis. The
//   harness injects through the wrapper's backend-callback forwards
//   (ktkIImguiWrapper::ImGui_CursorPosCallback / _MouseButtonCallback /
//   _KeyCallback / _ScrollCallback): those run EXE-side (the wrapper and
//   the initialized imgui backend live in kotek.exe), so the events are
//   queued into the REAL context's ImGuiIO with the exe's GImGui — a
//   game.ktk-side direct io.AddMousePosEvent would resolve to game.ktk's
//   own imgui copy whose GImGui is null (the 2026-09-03 two-imgui-copies
//   rule, zircon §5). Injection runs in the editor imgui pass's OnUpdate
//   AFTER NewFrame and BEFORE the windows draw, so an event injected at
//   frame F is processed by the NewFrame of frame F+1 and the widget
//   reacts during frame F+1's draw (the tables' frame offsets account
//   for the queue delay).
// * Deterministic frame stepping: one tick per rendered frame; each UI
//   test is a constexpr step table {frame_offset, action, target,
//   probe}; the harness advances the table as the frames tick.
// * Assertions on ENGINE CONTRACTS, never pixels: the probes read the
//   world/entity count, the command history's journal/cursor, the ui
//   selection, the config features, the renderer's per-pass skip flags,
//   the imgui popup state (through the wrapper), the serialized
//   game_config.json bytes.
// * Zero runtime allocation, no statics (rule 1a): the tables are
//   namespace-scope constexpr POD (the Z18 exemption, same class as the
//   embedded-defaults blobs); the run state is a member of the editor
//   session (one per session, like the command history and the cancel
//   arbiter).
//
// Widget addressing: instrumented windows report the rect of every
// harness-relevant widget once per frame (zircon_ui_test_track_widget —
// a no-op branch when no run is active) into the registry below; the
// harness resolves a step's (window, label) target to the rect's center
// from the PREVIOUS frame's draw (imgui layouts are frame-stable; a
// kWaitWidget step waits for a fresh entry, so popups/submenus that
// appear a frame after their opener click are handled without fragile
// fixed offsets).

class zircon_session_editor;
class zircon_editor_command_history;
class zircon_editor_ui_state;
class zircon_world;
class zircon_factory;
class zircon_config;
class zircon_renderer_bgfx;

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
class ktkIImguiWrapper;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

/// the widget rect registry (instrumented windows -> harness): one entry
/// per tracked (window, label) pair, refreshed every frame the widget
/// draws. Measured peak: the top bar (~20 entries incl. the 15 window
/// toggles) + the inspector's combo (~25 component rows) + the render
/// passes rows (~20) + entity rows — ~80 live entries; 160 is 2x
/// headroom (rule 9)
#define ZIRCON_DEF_UI_TEST_WIDGET_REGISTRY_MAX 160
/// window names are the imgui Begin titles ("Entity List", ...); the top
/// bar's logical group is "top_bar" (the main menu bar has no title)
#define ZIRCON_DEF_UI_TEST_WIDGET_WINDOW_MAX_LENGTH 32
/// the longest label today is "enabled:editor:" + the 63-char pass name
/// = 78; 96 gives headroom
#define ZIRCON_DEF_UI_TEST_WIDGET_LABEL_MAX_LENGTH 96
/// a widget entry is fresh when it was re-tracked within this many
/// frames (a hidden window's entries go stale and stop matching)
#define ZIRCON_DEF_UI_TEST_WIDGET_FRESHNESS_FRAMES 3

/// test names + the --ui_test=<name> value
#define ZIRCON_DEF_UI_TEST_NAME_MAX_LENGTH 64
/// the run list (the catalog holds ~15 tests; 32 is 2x)
#define ZIRCON_DEF_UI_TEST_MAX_RUN_TESTS 32
/// scheduled auto-release events (click mouse-ups, key-ups); a test
/// never has more than 2 in flight, 8 is generous
#define ZIRCON_DEF_UI_TEST_PENDING_EVENTS_MAX 8
/// per-test scratch slots for the capture/assert probe pairs (entity
/// ids, counts before the click, feature flags)
#define ZIRCON_DEF_UI_TEST_SCRATCH_SLOTS 8
/// kWaitWidget's default timeout when the step doesn't carry one
/// (params[0] == 0): ~1.5 s of frames at a fast headless rate; a widget
/// that never appears is a FAILED step, never a hang
#define ZIRCON_DEF_UI_TEST_WAIT_WIDGET_TIMEOUT_FRAMES 90
/// the frames the harness waits after activation before the first test —
/// the fresh imgui context needs a few frames to settle its layout
/// (fonts, ini, first draws fill the widget registry)
#define ZIRCON_DEF_UI_TEST_WARMUP_FRAMES 6
/// a click's release is auto-scheduled this many frames after the press
/// (the press processes at +1, the release at +3 — see the file header)
#define ZIRCON_DEF_UI_TEST_CLICK_RELEASE_DELAY_FRAMES 2
/// the minimum frames between an ACTION step (click/key/hover/focus) and
/// the next kProbe: makes probes self-synchronizing when a stalled wait
/// pushed the action past its nominal offset — a click at frame F fires
/// during frame F+3's draw (and a Push_Command flushes at F+4's loop
/// top), so 4 guarantees the effect is visible to the probe's tick
#define ZIRCON_DEF_UI_TEST_PROBE_MIN_GAP_AFTER_ACTION_FRAMES 4
/// the config backup byte buffer for the Render Passes "Save" test (the
/// file's byte backup/restore pattern of the Z20/Z22 config tests):
/// sized by the config's own serialize bound
#define ZIRCON_DEF_UI_TEST_CONFIG_BACKUP_CAPACITY 4096

/// the window-name strings the instrumented windows track under — the
/// imgui Begin titles (rename both sides together); "top_bar" is the
/// logical group for the main menu bar (it has no Begin window)
namespace zircon_ui_test_window_names
{
	constexpr const char* kTopBar = "top_bar";
	constexpr const char* kEntityList = "Entity List";
	constexpr const char* kComponentInspector = "Component Inspector";
	constexpr const char* kHistoryCommandLog = "History Command Log";
	constexpr const char* kRenderPasses = "Render Passes";
	constexpr const char* kSettings = "Settings";
} // namespace zircon_ui_test_window_names

struct zircon_ui_test_widget_t
{
	kotek::static_cstring_t<ZIRCON_DEF_UI_TEST_WIDGET_WINDOW_MAX_LENGTH>
		m_window_name;
	kotek::static_cstring_t<ZIRCON_DEF_UI_TEST_WIDGET_LABEL_MAX_LENGTH>
		m_label;
	float m_min_x;
	float m_min_y;
	float m_max_x;
	float m_max_y;
	/// the previous frame's rect (the sentinel -1 = never tracked twice)
	/// — popup windows REPOSITION on their second frame (imgui's
	/// auto-fit re-runs once the popup WasActive), so a click aimed at
	/// the first-frame rect misses; the wait step requires rect ==
	/// prev-rect (the widget settled) before it clears
	float m_prev_min_x;
	float m_prev_min_y;
	float m_prev_max_x;
	float m_prev_max_y;
	kotek::uint32_t m_last_frame_touched;
};

/// the widget rect sink — member of the harness, pointed into the
/// instrumented windows; lookup-table-on-vector (rule 2), string
/// comparison (a handful of lookups per frame over <=160 entries)
class zircon_ui_test_widget_registry
{
public:
	zircon_ui_test_widget_registry(void);
	~zircon_ui_test_widget_registry(void);

	void set_active(bool is_active) noexcept;
	bool is_active(void) const noexcept;

	/// the harness's frame counter is the freshness clock
	void set_frame_counter(kotek::uint32_t frame_counter) noexcept;
	kotek::uint32_t get_frame_counter(void) const noexcept;

	/// insert-or-update; a full registry drops the entry with ONE loud
	/// warning (the wait step that needs it fails visibly, never silent)
	void track(const char* p_window_name, const char* p_widget_label,
		float min_x, float min_y, float max_x, float max_y) noexcept;

	/// the rect center when the entry exists AND is fresh
	/// (ZIRCON_DEF_UI_TEST_WIDGET_FRESHNESS_FRAMES)
	bool find_fresh_center(const char* p_window_name,
		const char* p_widget_label, float& out_center_x,
		float& out_center_y) const noexcept;

	bool is_fresh(const char* p_window_name,
		const char* p_widget_label) const noexcept;

	/// fresh AND the rect is unchanged across the last two tracks (the
	/// widget settled — see the prev-rect note on the entry): the wait
	/// step's real contract, so clicks never aim at a popup's
	/// first-frame position
	bool is_fresh_and_stable(const char* p_window_name,
		const char* p_widget_label) const noexcept;

	/// entries whose (window, label-prefix) match and are fresh (the
	/// history window's row count assertion)
	kotek::uint32_t count_fresh_with_prefix(const char* p_window_name,
		const char* p_label_prefix) const noexcept;

private:
	bool m_is_active;
	/// the loud-drop dedupe (one warning per run, not per frame)
	bool m_was_overflow_warned;
	kotek::uint32_t m_frame_counter;
	kotek::static_vector_t<zircon_ui_test_widget_t,
		ZIRCON_DEF_UI_TEST_WIDGET_REGISTRY_MAX>
		m_widgets;
};

/// the instrumented windows' report point — a plain function (never
/// inline imgui calls from a game.ktk TU's own copy, see the file
/// header): reads the last item's rect through the wrapper and records
/// it. A no-op (one branch) while no run is active
void zircon_ui_test_track_widget(
	kotek::core::ktkIImguiWrapper* p_wrapper_imgui,
	zircon_ui_test_widget_registry* p_registry, const char* p_window_name,
	const char* p_widget_label) noexcept;

/// the step actions
enum class eZirconUiTestStepAction : kotek::uint8_t
{
	kNoop = 0,
	/// resolve the target widget's fresh center and queue a mouse move
	kMouseMoveToWidget,
	/// params[0] = imgui button index (0 = left), params[1] = 1 down / 0
	/// up; moves onto the target widget first when one is named
	kMouseButtonOnWidget,
	/// wheel notches: params[0] = horizontal, params[1] = vertical
	/// (positive = up); scrolls whatever the mouse currently hovers (the
	/// combo popup for deep component lists)
	kMouseWheel,
	/// move + press now, the release auto-schedules
	/// +ZIRCON_DEF_UI_TEST_CLICK_RELEASE_DELAY_FRAMES
	kClickWidget,
	/// params[0] = the GLFW key code (the wrapper's backend-callback seam
	/// speaks the platform backend's codes — GLFW_KEY_* on the default
	/// window library); the release auto-schedules like kClickWidget's
	kKeyPress,
	/// hold the table here until the target widget has a fresh rect
	/// (params[0] = timeout frames, 0 = the default); a timeout is a
	/// FAILED step
	kWaitWidget,
	/// pfn probe: assert (fails the current test on false) or capture
	/// (stores into the scratch slots — the probe returns true)
	kProbe,
	/// raise the named window above the others (SetWindowFocus through
	/// the wrapper) so an injected click can't land on an occluding
	/// window; the target WINDOW name is the imgui Begin title
	kFocusWindow
};

struct zircon_ui_test_context_t;

/// the probe seam (fnptr+void-state-free, the house pattern): reads (and
/// captures into) the per-tick context; returns false = assertion failed
using zircon_ui_test_probe_pfn_t = bool (*)(zircon_ui_test_context_t&,
	const struct zircon_ui_test_step_t&);

/// step flags
#define ZIRCON_DEF_UI_TEST_STEP_FLAG_NONE 0
/// the target label is a printf-style format with exactly one '%u',
/// filled from scratch[params[1]] (the entity-row labels "row:<id>" —
/// the created entity's id is runtime data)
#define ZIRCON_DEF_UI_TEST_STEP_FLAG_LABEL_FORMAT_SCRATCH 1

struct zircon_ui_test_step_t
{
	/// the earliest test-local frame this step runs on (the table is
	/// executed in order; waits can push later steps past their offset)
	kotek::uint16_t m_frame_offset;
	eZirconUiTestStepAction m_action;
	kotek::uint8_t m_flags;
	/// the registry (window, label) pair — both null when the action
	/// needs no widget target
	const char* p_target_window;
	const char* p_target_label;
	/// kProbe's assertion/capture function
	zircon_ui_test_probe_pfn_t pfn_probe;
	/// the step's human-readable contract for the failure line
	const char* p_debug;
	/// action/probe parameters (signed — count deltas)
	kotek::int32_t m_params[3];
};

struct zircon_ui_test_case_t
{
	const char* p_name;
	const zircon_ui_test_step_t* p_steps;
	kotek::uint16_t m_step_count;
	/// the last offset + settle margin — summed at activation for the
	/// --kotek_frames sanity warning
	kotek::uint16_t m_frame_budget;
};

/// everything a probe can touch, resolved once per tick by the harness
/// (nullptr where the boot has no such service — the probes guard)
struct zircon_ui_test_context_t
{
	kotek::core::ktkMainManager* p_main_manager;
	kotek::core::ktkIImguiWrapper* p_imgui_wrapper;
	zircon_session_editor* p_session;
	zircon_editor_command_history* p_history;
	zircon_editor_ui_state* p_ui_state;
	zircon_world* p_world;
	zircon_factory* p_factory;
	zircon_config* p_config;
	/// nullptr on a non-bgfx backend (activation refuses non-bgfx boots,
	/// so it is always set inside a run)
	zircon_renderer_bgfx* p_renderer_bgfx;
	zircon_ui_test_widget_registry* p_widgets;
	/// the game_config.json bytes captured at activation + their size
	/// (the Save test's byte-identical assertion)
	const kotek::uint8_t* p_config_backup;
	kotek::size_t m_config_backup_size;
	/// per-test scratch, zeroed at each test's start
	kotek::uint64_t scratch[ZIRCON_DEF_UI_TEST_SCRATCH_SLOTS];
};

/// the catalog (zircon_ui_test_catalog.cpp): the constexpr step tables +
/// their probes. "all" is not a name — the harness iterates the table
const zircon_ui_test_case_t* zircon_ui_test_catalog_get_cases(
	kotek::uint8_t& out_count) noexcept;
const zircon_ui_test_case_t* zircon_ui_test_catalog_find(
	const char* p_name) noexcept;

/// the harness — a member of the editor session (Z17; one per session,
/// the command-history/arbiter pattern). Activated by
/// --ui_test=<name|all>; driven per frame by the editor imgui pass's
/// OnUpdate between NewFrame and the window draws. Failure policy: a
/// failed probe marks the current test and the run continues (the [OK]/
/// [FAILED] lines stay grep-able per test); a finished run with ANY
/// failure asserts (the boot goes red); a requested run that never
/// finishes (frame budget too small / a stall) asserts at module
/// shutdown — both failure shapes are loud, never silent
class zircon_ui_test_harness
{
public:
	zircon_ui_test_harness(void);
	~zircon_ui_test_harness(void);

	/// resolves the run list from the catalog, captures the config
	/// backup, warms up. Asserts (user/CI error, fail fast) on: an
	/// unknown name, no imgui wrapper (not an --editor_imgui boot), a
	/// non-bgfx renderer (the editor graph exists on bgfx only)
	void activate(const char* p_test_name,
		kotek::core::ktkMainManager* p_main_manager,
		zircon_session_editor* p_session) noexcept;

	bool is_active(void) const noexcept;
	bool is_finished(void) const noexcept;
	bool was_activated_ok(void) const noexcept;

	/// the editor imgui pass's per-frame hook (after NewFrame + the
	/// cancel adapter, before the windows draw)
	void on_frame_pre_draw(kotek::core::ktkIImguiWrapper* p_wrapper,
		zircon_session_editor* p_session,
		kotek::core::ktkMainManager* p_main_manager) noexcept;

	/// the widget sink the instrumented windows are constructed with
	zircon_ui_test_widget_registry* get_widget_registry(void) noexcept;

	/// the game manager's Shutdown calls this: a requested, activated,
	/// unfinished run is a FAILED run (the frame budget was too small or
	/// a wait stalled) — assert loudly instead of exiting green
	void on_module_shutdown(void) noexcept;

private:
	struct pending_event_t
	{
		kotek::uint32_t m_due_frame;
		/// 0 = mouse button release, 1 = key release
		kotek::uint8_t m_kind;
		/// the mouse button index or the GLFW key code
		kotek::int32_t m_code;
	};

	void inject_pending_due(
		kotek::core::ktkIImguiWrapper* p_wrapper) noexcept;
	void execute_step(zircon_ui_test_context_t& context,
		const zircon_ui_test_step_t& step) noexcept;
	/// the silent lookup (kWaitWidget polls it — a miss there is the
	/// normal "not drawn yet", not a failure)
	bool find_widget_center(const zircon_ui_test_step_t& step,
		float& out_x, float& out_y) noexcept;
	/// the failing lookup (action steps: a miss fails the step loudly)
	bool resolve_widget_center(const zircon_ui_test_step_t& step,
		float& out_x, float& out_y) noexcept;
	void schedule_release(kotek::uint8_t kind,
		kotek::int32_t code) noexcept;
	/// the click-serialization guard (the swallowed-press lesson): true
	/// while a release for the step's button/key is outstanding
	bool has_pending_release_for(
		const zircon_ui_test_step_t& step) const noexcept;
	void conclude_test(void) noexcept;
	void finish_run(void) noexcept;
	void restore_config_file(void) noexcept;

private:
	bool m_is_active;
	bool m_is_finished;
	bool m_was_activated_ok;
	kotek::uint8_t m_current_test_index;
	kotek::uint8_t m_run_count;
	kotek::uint16_t m_current_step_index;
	kotek::uint32_t m_frame_counter;
	kotek::uint32_t m_test_frame_counter;
	/// the test-local frame the last ACTION step executed on (the probe
	/// gap above); 0 = no action yet in this test
	kotek::uint32_t m_last_action_frame;
	/// frames the current kWaitWidget has been blocking the table
	kotek::uint32_t m_wait_frames;
	kotek::uint32_t m_warmup_frames_remaining;
	kotek::uint32_t m_tests_failed;
	kotek::uint32_t m_current_test_failures;
	void* m_p_native_window_handle;
	kotek::core::ktkMainManager* m_p_main_manager;
	zircon_session_editor* p_session_cached;
	/// the renderer the probes reach (cached at activate — stable for the
	/// boot)
	zircon_renderer_bgfx* m_p_renderer_bgfx_cached;
	zircon_ui_test_widget_registry m_widget_registry;
	kotek::static_vector_t<const zircon_ui_test_case_t*,
		ZIRCON_DEF_UI_TEST_MAX_RUN_TESTS>
		m_run_list;
	kotek::static_vector_t<pending_event_t,
		ZIRCON_DEF_UI_TEST_PENDING_EVENTS_MAX>
		m_pending_events;
	zircon_ui_test_context_t m_context;
	kotek::array_t<kotek::uint8_t,
		ZIRCON_DEF_UI_TEST_CONFIG_BACKUP_CAPACITY>
		m_config_backup;
	kotek::size_t m_config_backup_size;
};
