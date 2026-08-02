#pragma once

#include <kotek.render.shared.bgfx/include/kotek_render_graph_simplified.h>

// the pass-library seam (task Z3 P3a): owns the hot-swappable passes DLL
// in the graphics-development configuration and installs the create/
// destroy callbacks below; also the home of the
// zircon_render_pass_create_pfn_t / zircon_render_pass_destroy_pfn_t
// aliases
#include "zircon_render_pass_library_manager.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE

KOTEK_BEGIN_NAMESPACE_RENDER
KOTEK_BEGIN_NAMESPACE_RENDER_BGFX
class ktkRenderDevice;
class ktkRenderResourceManager;
KOTEK_END_NAMESPACE_RENDER_BGFX
KOTEK_END_NAMESPACE_RENDER
KOTEK_END_NAMESPACE_KOTEK

class zircon_session_editor_manager;
class zircon_session_game_manager;

#define ZIRCON_DEF_RENDERER_BGFX_MAX_RENDER_GRAPH_COUNT 2

/// one pass class name inside a graph slot's name list; measured: the
/// longest registered name today is
/// "no_streaming::zircon_render_graph_pass_editor_model_static_bgfx"
/// (63 chars), 96 gives ~1.5x headroom (rule 9) — raise when a
/// legitimately longer pass name registers
#define ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH 96

struct zircon_render_graph_simplified_bgfx_info_t
{
	bool in_use{};
	bool is_game_session{};
	kotek::uint8_t session_id{};
	kotek::render::bgfx::ktkRenderGraphSimplified graph;

	/// set by the structural-edit requests below (the Render Passes
	/// window, task Z3 P2a); the rebuild runs at the top of the next
	/// draw() — never mid-pass from inside the imgui Draw call
	bool rebuild_pending{};

	/// the session's pass class-name list in execution order, aligned
	/// with pass_enabled and with graph.Get_Passes(); the names live
	/// here (not in the pass objects) so the editor window reads them
	/// without touching passes — a pass library unload (P3 hot-reload)
	/// can never dangle them
	kotek::static_vector_t<
		kotek::static_cstring_t<
			ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH>,
		KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>
		pass_names;

	/// per-pass skip flags aligned with pass_names (and the graph's
	/// pass order): false = draw() skips that pass's OnUpdate/OnRender.
	/// The window's enable checkbox flips these — instant, no
	/// destroy/create. Default: every pass enabled
	kotek::static_vector_t<bool,
		KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>
		pass_enabled;
};

class zircon_renderer_bgfx : public kotek::core::ktkIRenderer
{
public:
	zircon_renderer_bgfx(kotek::core::ktkMainManager* p_main_manager);
	~zircon_renderer_bgfx(void);

	void Initialize(kotek::core::ktkWindowConsole* p_console,
		kotek::core::ktkConsole* p_con,
		zircon_session_game_manager* p_session_game_manager,
		zircon_session_editor_manager* p_session_editor_manager);

	void Shutdown(void) override;

	void draw() override;

	void Resize() override;

	const char* Get_Name(void) const noexcept override;

	bool is_render_graph_presented(kotek::uint8_t render_graph_id) const;
	bool is_render_graph_initialized(kotek::uint8_t render_graph_id) const;
	bool is_render_graph_for_session_editor(
		kotek::uint8_t render_graph_id) const;
	bool is_render_graph_for_session_game(kotek::uint8_t render_graph_id) const;

	/// pass_names must align with passes (one created pass per name,
	/// same order) — the slot stores them so the Render Passes window
	/// reads/rebuilds the set without touching pass objects (Z3 P2a)
	kotek::uint8_t create_render_graph(kotek::uint8_t session_id,
		bool is_game_session,
		kotek::static_vector_t<
			kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*,
			KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>&
			passes,
		const kotek::static_vector_t<
			kotek::static_cstring_t<
				ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH>,
			KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>&
			pass_names);

	void initialize_render_graph(kotek::uint8_t render_graph_id,
		kotek::core::ktkMainManager* p_main_manager,
		kotek::core::ktkIRenderResourceManager* p_render_resource_manager,
		zircon_session_game_manager* p_manager_game_session,
		zircon_session_editor_manager* p_manager_editor_session);

	void set_current_render_graph(kotek::uint8_t render_graph_id);

	/// the create side of the pass-library seam (task Z3 P3a): installed
	/// by the pass library manager, bound to it via p_owner (the
	/// no-static-storage rule — the wrapper reaches the manager's
	/// members through this pointer, never through file statics)
	void set_render_pass_create_callback(
		zircon_render_pass_create_seam_pfn_t pfn_create,
		void* p_owner) noexcept;

	/// the destroy side of the pass-library seam (task Z3 P3a): when
	/// installed, graph teardown detaches the passes and destroys each
	/// through this callback (the creating library's own code) instead of
	/// ktkRenderGraphSimplified::Shutdown's plain delete
	void set_render_pass_destroy_callback(
		zircon_render_pass_destroy_seam_pfn_t pfn_destroy,
		void* p_owner) noexcept;

	/// one call wiring of the pass-library seam (task Z3 P3a): the
	/// manager decides DLL vs static from the build configuration and the
	/// runtime flag and installs the callbacks above; p_static_create /
	/// p_static_destroy (+ the p_static_get_count / p_static_get_name
	/// enumeration twins, task Z3 P3b) are the statically-linked
	/// zircon_passlib_* functions
	void initialize_pass_library(bool prefer_shared_library,
		zircon_render_pass_create_pfn_t p_static_create,
		zircon_render_pass_destroy_pfn_t p_static_destroy,
		zircon_passlib_get_count_pfn_t p_static_get_count,
		zircon_passlib_get_name_pfn_t p_static_get_name) noexcept;

	/// creates a pass through the installed create callback (the unified
	/// seam every graph creation flows through); nullptr when no callback
	/// is installed or the name is not registered
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
	create_render_pass(const char* p_pass_name) noexcept;

	/// the manual hot-reload override (task Z3 P3b — the
	/// reload_render_passes console command): sets the same atomic flag
	/// the pass-library file watcher sets; the swap runs at the top of
	/// the next draw(). Warns and does nothing in builds without
	/// ZIRCON_GRAPHICS_DEVELOPMENT
	void request_pass_library_reload(void) noexcept;

	/// the pass registry the Render Passes window shows (task Z3 P3b):
	/// the generation bumps on every (re-)enumeration of the pass library
	/// (init + each successful hot-reload) — the window compares it in
	/// Draw and re-points its tables through the getter. Default
	/// configuration: 0 / false forever (the window keeps its
	/// compile-time ctor tables — zero behavior change)
	kotek::uint32_t get_pass_library_registry_generation(
		void) const noexcept;
	bool get_pass_library_registry(
		const char* const*& out_p_editor_pass_names,
		kotek::uint8_t& out_editor_pass_count,
		const char* const*& out_p_game_pass_names,
		kotek::uint8_t& out_game_pass_count) const noexcept;

	/// Render Passes window reads (Z3 P2a): slot count + read-only slot
	/// state + the slot id of a session kind (returns
	/// kotek::uint8_t(-1) when no such slot exists — compare against
	/// get_render_graph_count())
	kotek::uint8_t get_render_graph_count(void) const noexcept;
	const zircon_render_graph_simplified_bgfx_info_t&
	get_render_graph_info(kotek::uint8_t render_graph_id) const noexcept;
	kotek::uint8_t get_render_graph_id_for_session_kind(
		bool is_game_session) const noexcept;

	/// instant enable/disable: flips the slot's skip flag, applied from
	/// the next frame — no pass destroy/create
	void set_render_pass_enabled(kotek::uint8_t render_graph_id,
		kotek::uint8_t pass_index, bool enabled) noexcept;

	/// structural edits: mutate the slot's name list and queue a
	/// frame-boundary rebuild of that graph (processed at the top of
	/// the next draw() — the window calls these from inside the imgui
	/// pass's Draw, where graph surgery would delete the running pass)
	void request_render_pass_add(kotek::uint8_t render_graph_id,
		const char* p_pass_name) noexcept;
	void request_render_pass_remove(kotek::uint8_t render_graph_id,
		kotek::uint8_t pass_index) noexcept;
	void request_render_pass_move(kotek::uint8_t render_graph_id,
		kotek::uint8_t pass_index, bool move_up) noexcept;

private:
	void initialize_extensions(kotek::core::ktkConsole* p_console);

	void Begin() noexcept;
	void End() noexcept;

	void destroy_render_graphs(void) noexcept;

	/// one graph's pass teardown: Detach_Passes + the installed destroy
	/// callback when the pass library seam provides one (dev
	/// configuration — the library destroys its own objects), the plain
	/// graph Shutdown() otherwise (default configuration, unchanged)
	void shutdown_render_graph_passes(
		zircon_render_graph_simplified_bgfx_info_t& info) noexcept;

	void process_pending_render_graph_rebuilds(void) noexcept;
	void rebuild_render_graph(kotek::uint8_t render_graph_id) noexcept;

#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
	/// the pass-library hot-reload (task Z3 P3b): consumes the watcher /
	/// console atomic flag at the top of draw() — the frame boundary
	/// where no pass is mid-execution. Three phases through the pass
	/// library manager: prepare (shadow-copy + load + resolve, the old
	/// library untouched) -> destroy every slot's passes through the OLD
	/// library -> commit (the candidate becomes active) -> recreate each
	/// slot's stored name set through the NEW library (the P2a rebuild
	/// logic) -> finish (the replaced library unloads, the registry
	/// re-enumerates for the Render Passes window). Any failure before
	/// the commit keeps the old library running — never pass-less
	void process_pending_pass_library_reload(void) noexcept;
#endif

private:
	kotek::uint8_t m_previous_render_graph_id;
	kotek::core::ktkMainManager* m_p_main_manager;
	zircon_session_game_manager* m_p_session_game_manager;
	zircon_session_editor_manager* m_p_session_editor_manager;
	kotek::render::bgfx::ktkRenderDevice* m_p_render_device;
	// routed through the interface: the concrete manager lives in the
	// backend plugin dll
	kotek::core::ktkIRenderResourceManager* m_p_render_resource_manager;
	kotek::render::bgfx::ktkRenderGraphSimplified* m_p_current_render_graph;
	zircon_render_pass_create_seam_pfn_t m_pfn_create_render_pass{};
	zircon_render_pass_destroy_seam_pfn_t m_pfn_destroy_render_pass{};
	/// the owner the two seam callbacks are bound to (the pass library
	/// manager instance — member of this renderer, outlives every pass)
	void* m_p_pass_seam_owner{};
	zircon_render_pass_library_manager m_pass_library_manager;
	kotek::static_vector_t<zircon_render_graph_simplified_bgfx_info_t,
		ZIRCON_DEF_RENDERER_BGFX_MAX_RENDER_GRAPH_COUNT>
		m_render_graphs;
};