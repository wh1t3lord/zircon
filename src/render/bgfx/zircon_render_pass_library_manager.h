#pragma once

#include <kotek.core.containers.dll/include/kotek_std_alias_dll.h>

// the watcher thread + the reload flag (task Z3 P3b): header-only kotek
// MT aliases (std:: in this configuration), they pull no extra link edges
#include <kotek.core.containers.multithreading.thread/include/kotek_std_alias_thread.h>
#include <kotek.core.containers.multithreading.atomic/include/kotek_std_alias_atomic.h>

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_RENDER
KOTEK_BEGIN_NAMESPACE_RENDER_BGFX
class ktkRenderGraphSimplifiedRenderPass;
KOTEK_END_NAMESPACE_RENDER_BGFX
KOTEK_END_NAMESPACE_RENDER
KOTEK_END_NAMESPACE_KOTEK

class zircon_renderer_bgfx;

/// the raw signatures of the pass library's C ABI (identical for the
/// hot-swappable DLL's exports and their statically-linked twins)
using zircon_render_pass_create_pfn_t =
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* (*
	)(const char* p_pass_name);

/// destroys a pass inside the module that created it (cross-CRT rule,
/// task Z3 P3)
using zircon_render_pass_destroy_pfn_t =
	void (*)(kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass);

/// the registry-enumeration pair of the pass library's C ABI
using zircon_passlib_get_count_pfn_t = unsigned (*)(void);
using zircon_passlib_get_name_pfn_t = const char* (*)(unsigned);

/// the renderer's installed pass create/destroy seam (task Z3 P3b, the
/// no-static-storage rule of 2026-08-02): an explicit OWNER pointer — the
/// zircon_render_pass_library_manager instance — travels with the
/// callback, so the wrapper reaches the owning manager's members. No
/// file-statics, no hidden module-local state anywhere on the seam
using zircon_render_pass_create_seam_pfn_t =
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* (*
	)(void* p_owner, const char* p_pass_name);
using zircon_render_pass_destroy_seam_pfn_t =
	void (*)(void* p_owner,
		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass);

/// the hot-swappable pass library of the graphics-development
/// configuration, loaded relative to the working directory (the boot
/// discipline runs kotek.exe from the repo root, next to data_game/
/// data_user/)
#define ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH \
	"passes/zircon.render.passes.bgfx.dll"

/// the engine never loads the real DLL (a loaded image is write-locked on
/// Windows, which would block the next rebuild at the linker) — every
/// load copies the real file to a fresh shadow next to it:
/// "passes/.shadow_<pid>_<counter>.dll" (task Z3 P3b). 128 fits the
/// prefix + 10-digit pid + counter with ~2x headroom
#define ZIRCON_DEF_RENDER_PASS_LIBRARY_SHADOW_PATH_MAX_LENGTH 128

/// the file watcher's poll cadence (task Z3 P3b) — polling, not
/// ReadDirectoryChanges (house-simple)
#define ZIRCON_DEF_RENDER_PASS_LIBRARY_WATCHER_POLL_INTERVAL_MS 500

/// a change to the real DLL is accepted only after this many consecutive
/// identical (mtime, size) reads — msbuild's link output is not atomic
#define ZIRCON_DEF_RENDER_PASS_LIBRARY_WATCHER_STABLE_READS_REQUIRED 2

/// sanity cap for the registry enumeration of a loaded pass library (a
/// sane library exposes a handful of passes; more means a corrupt export)
#define ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_PASS_COUNT 64

/// one enumerated pass name inside the manager's registry copies; mirrors
/// ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH (zircon_renderer.h —
/// that header includes this one, so its constant is not visible here)
#define ZIRCON_DEF_RENDER_PASS_LIBRARY_REGISTRY_NAME_MAX_LENGTH 96

/// the seam between the executor and the concrete render passes (task Z3
/// P3a). The manager OWNS the renderer's create callback in every
/// configuration (a wrapper bound to this instance) and routes internally
/// to the currently-active pass source: the statically-linked passlib
/// functions (default configuration and dev-mode fallback) or the loaded
/// DLL's exports (graphics-development with the feature on). Destruction
/// routes the same way in the dev configuration; the default
/// configuration keeps destruction on ktkRenderGraphSimplified::Shutdown
/// — byte-identical to the pre-split behavior.
///
/// The hot-reload loop (task Z3 P3b, dev configuration only): a watcher
/// thread polls the REAL DLL's (mtime, size) and sets an atomic flag once
/// a change stays stable across
/// ZIRCON_DEF_RENDER_PASS_LIBRARY_WATCHER_STABLE_READS_REQUIRED reads —
/// the thread touches nothing but its own member state and the two
/// atomics. The renderer's draw() consumes the flag at the frame boundary
/// and runs the three-phase swap: prepare_reload_candidate (copy to a
/// fresh shadow + load + resolve, never disturbing the running library)
/// -> the renderer destroys every pass through the OLD library ->
/// commit_reload_candidate (the candidate becomes active) -> the renderer
/// recreates the session pass sets -> finish_reload (unloads the replaced
/// library, deletes its shadow, re-enumerates the registry for the Render
/// Passes window). Any failure before the commit keeps the old library
/// running — never pass-less.
///
/// The reload-safety laws this seam exists for: passes are created AND
/// destroyed inside the library, always before any unload. All resolved
/// exports, counters and watcher state live as MEMBERS of this instance
/// (the no-static-storage rule) — the renderer's seam callbacks carry the
/// instance pointer explicitly.
class zircon_render_pass_library_manager
{
public:
	zircon_render_pass_library_manager(void);
	~zircon_render_pass_library_manager(void);

	/// prefer_shared_library: the runtime graphics_development feature
	/// (config key / --graphics_development). Ignored with a warning in
	/// builds without ZIRCON_USE_GRAPHICS_DEVELOPMENT. p_static_create /
	/// p_static_destroy: the statically-linked passlib functions (the
	/// only path in default builds, the fallback + flag-off path in dev
	/// builds). p_static_get_count / p_static_get_name: the static twins
	/// of the registry-enumeration exports (dev builds only — the manager
	/// keeps an executor-owned registry copy for the Render Passes
	/// window; unused in the default configuration)
	void Initialize(zircon_renderer_bgfx* p_renderer,
		bool prefer_shared_library,
		zircon_render_pass_create_pfn_t p_static_create,
		zircon_render_pass_destroy_pfn_t p_static_destroy,
		zircon_passlib_get_count_pfn_t p_static_get_count,
		zircon_passlib_get_name_pfn_t p_static_get_name) noexcept;

	/// joins the watcher thread (dev configuration), then unloads the
	/// pass library when one is loaded; every pass it created must
	/// already be destroyed through the installed destroy callback (the
	/// renderer's graph teardown runs first) — a creation/destruction
	/// count mismatch is asserted loudly
	void Shutdown(void) noexcept;

	bool is_shared_library_active(void) const noexcept;

	/// the renderer's seam entry points (the installed wrappers carry
	/// this instance as their owner): route creation/destruction to the
	/// currently-active pass source — the loaded library's exports when
	/// the shared library is active, the static twins otherwise
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* create_pass(
		const char* p_pass_name) noexcept;

#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
	void destroy_pass(
		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_pass) noexcept;

	/// the manual reload override (the reload_render_passes console
	/// command): sets the same atomic flag the file watcher sets — the
	/// swap itself runs at the top of the renderer's next draw(). A
	/// no-op with a warning when the watcher is not active (the
	/// graphics_development feature is off)
	void request_reload(void) noexcept;

	/// the renderer's draw() polls this at the frame boundary
	bool is_reload_requested(void) const noexcept;

	/// swap phase 1 (render thread, frame boundary): copies the real DLL
	/// to a fresh shadow, loads it and resolves the 4 exports WITHOUT
	/// disturbing the running library. On any failure: logs, cleans up,
	/// clears the flag and returns false — the old library keeps running
	bool prepare_reload_candidate(void) noexcept;

	/// swap phase 2: every pass of the old library is dead by now (the
	/// renderer destroyed them through the old exports) — the candidate
	/// becomes the active library: the active exports switch over and
	/// the replaced library is staged for finish_reload. The renderer's
	/// callbacks never change (they are bound to this instance and route
	/// through the active pointers)
	void commit_reload_candidate(void) noexcept;

	/// swap phase 3: the recreation through the new library has run —
	/// unloads the replaced library, deletes its shadow, re-enumerates
	/// the pass registry from the NEW library (the Render Passes window
	/// picks it up through the generation bump) and clears the flag
	void finish_reload(void) noexcept;

	/// bumped on every (re-)enumeration of the pass registry — the
	/// Render Passes window compares it in Draw and re-points its tables
	kotek::uint32_t get_registry_generation(void) const noexcept;

	/// the current registry snapshot, split per session kind (the
	/// codegen's containsEditor rule: the class name contains "editor").
	/// Executor-owned copies with stable inline storage — the returned
	/// pointers never dangle across library swaps. False when no
	/// registry was enumerated yet
	bool get_registry(const char* const*& out_p_editor_pass_names,
		kotek::uint8_t& out_editor_pass_count,
		const char* const*& out_p_game_pass_names,
		kotek::uint8_t& out_game_pass_count) const noexcept;
#endif

private:
#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
	/// the watcher thread body: polls the real DLL's (mtime, size) and
	/// sets m_reload_requested once a change stays stable — touches
	/// NOTHING but its own member state below and the two atomics (the
	/// render thread owns everything else; no logging from this thread)
	void watch_pass_library_file(void) noexcept;

	/// (re-)fills the executor-owned registry copies from a get_count/
	/// get_name pair (the loaded library's exports or the static twins)
	/// and bumps the generation
	void refill_registry(zircon_passlib_get_count_pfn_t pfn_get_count,
		zircon_passlib_get_name_pfn_t pfn_get_name) noexcept;
#endif

	bool m_is_shared_library_active;

	/// the statically-linked passlib functions (the fallback + flag-off
	/// path in dev builds, the only path in default builds)
	zircon_render_pass_create_pfn_t m_pfn_static_create{};
	zircon_render_pass_destroy_pfn_t m_pfn_static_destroy{};

#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
	zircon_renderer_bgfx* m_p_renderer{};

	/// the ACTIVE pass library: the owning handle (loaded from the
	/// active shadow) and its resolved exports. Only in the dev
	/// configuration — the default configuration carries no OS handle
	kotek::dll::shared_library m_library;
	zircon_passlib_get_count_pfn_t m_pfn_active_get_count{};
	zircon_passlib_get_name_pfn_t m_pfn_active_get_name{};
	zircon_render_pass_create_pfn_t m_pfn_active_create{};
	zircon_render_pass_destroy_pfn_t m_pfn_active_destroy{};

	/// the unload-safety invariant counters: every library-created pass
	/// must come back through the library destroy before the library
	/// unloads (counted only while the shared library is active — static
	/// twin passes are created and destroyed in balanced pairs of their
	/// own)
	unsigned m_passes_created_via_library{};
	unsigned m_passes_destroyed_via_library{};

	/// the phase-1 candidate: loaded and resolved, not yet committed
	kotek::dll::shared_library m_candidate_library;
	zircon_passlib_get_count_pfn_t m_pfn_candidate_get_count{};
	zircon_passlib_get_name_pfn_t m_pfn_candidate_get_name{};
	zircon_render_pass_create_pfn_t m_pfn_candidate_create{};
	zircon_render_pass_destroy_pfn_t m_pfn_candidate_destroy{};

	/// the library a successful commit replaced — finish_reload unloads
	/// it once the recreation through the new library has run (every
	/// object it created is already dead by the commit)
	kotek::dll::shared_library m_replaced_library;

	/// the shadow files of the three handles above (the active one is
	/// what m_library was loaded from; deleted after the matching
	/// unload)
	kotek::static_cstring_t<
		ZIRCON_DEF_RENDER_PASS_LIBRARY_SHADOW_PATH_MAX_LENGTH>
		m_active_shadow_path;
	kotek::static_cstring_t<
		ZIRCON_DEF_RENDER_PASS_LIBRARY_SHADOW_PATH_MAX_LENGTH>
		m_candidate_shadow_path;
	kotek::static_cstring_t<
		ZIRCON_DEF_RENDER_PASS_LIBRARY_SHADOW_PATH_MAX_LENGTH>
		m_replaced_shadow_path;

	/// monotonic shadow-name counter (init takes 1, every reload the
	/// next) — a fresh name per load so a just-unloaded shadow's name is
	/// never immediately reused
	unsigned m_shadow_counter{};

	kotek::mt::thread_t m_watcher_thread;
	kotek::mt::atomic_t<bool> m_watcher_stop{};
	kotek::mt::atomic_t<bool> m_reload_requested{};

	/// the watcher's own state (the no-static-storage rule: members of
	/// the owning manager, touched only by the watcher thread after the
	/// render thread initializes them): the last observed file state,
	/// the last ACCEPTED (fired-on) file state, the stability counter
	kotek::uint64_t m_watcher_last_write_time{};
	kotek::uint64_t m_watcher_last_file_size{};
	bool m_watcher_have_last{};
	kotek::uint64_t m_watcher_accepted_write_time{};
	kotek::uint64_t m_watcher_accepted_file_size{};
	bool m_watcher_have_accepted{};
	unsigned m_watcher_stable_reads{};

	/// the watcher runs when the graphics_development feature is on in a
	/// dev build — even when the initial load fell back to the static
	/// twin (a later-built DLL is picked up the same way)
	bool m_is_file_watcher_active{};

	/// the executor-owned pass registry copies (the window reads these
	/// through get_registry — DLL string literals would dangle on the
	/// next unload, these never move)
	kotek::uint32_t m_registry_generation{};
	kotek::static_vector_t<
		kotek::static_cstring_t<
			ZIRCON_DEF_RENDER_PASS_LIBRARY_REGISTRY_NAME_MAX_LENGTH>,
		ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_PASS_COUNT>
		m_registry_editor_pass_names;
	kotek::static_vector_t<
		kotek::static_cstring_t<
			ZIRCON_DEF_RENDER_PASS_LIBRARY_REGISTRY_NAME_MAX_LENGTH>,
		ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_PASS_COUNT>
		m_registry_game_pass_names;
	kotek::static_vector_t<const char*,
		ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_PASS_COUNT>
		m_registry_editor_pass_name_ptrs;
	kotek::static_vector_t<const char*,
		ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_PASS_COUNT>
		m_registry_game_pass_name_ptrs;
#endif
};
