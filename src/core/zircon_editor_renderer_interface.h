#pragma once

/// one pass class name inside a graph slot's name list; measured: the
/// longest registered name today is 63 chars, 96 gives ~1.5x headroom.
#define ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH 96

/// the user-visible reload status of the graphics-development pass library.
/// Declared in core so editor UI never links the concrete renderer.
enum class eZirconRenderPassLibraryStatus : kotek::uint8_t
{
	kIdle,
	kChangeDetected,
	kReloading,
	kReloadFailed,
	kReloadSucceeded
};

/// backend-neutral snapshot of the GPU counters the editor displays and
/// tests. The initialized backend fills it; no bgfx type or second bgfx
/// context crosses the module boundary.
using zircon_editor_gpu_stats_t = kotek::core::ktkRenderGpuStats;

/// the editor's narrow renderer contract. UI windows and UI tests consume
/// this interface instead of zircon_renderer_bgfx, keeping the render ->
/// editor dependency one-directional in implicit-DLL builds. Pass data is
/// returned as scalars/string views, never as container references: no
/// container ABI or module-local container state crosses the DLL boundary.
class zircon_editor_renderer_interface
{
public:
	virtual ~zircon_editor_renderer_interface(void) {}

	virtual kotek::uint8_t get_render_graph_count(void) const noexcept = 0;
	virtual kotek::uint8_t get_render_graph_id_for_session_kind(
		bool is_game_session) const noexcept = 0;
	virtual kotek::uint8_t get_render_graph_pass_count(
		kotek::uint8_t render_graph_id) const noexcept = 0;
	virtual const char* get_render_graph_pass_name(
		kotek::uint8_t render_graph_id,
		kotek::uint8_t pass_index) const noexcept = 0;
	virtual bool is_render_graph_pass_enabled(
		kotek::uint8_t render_graph_id,
		kotek::uint8_t pass_index) const noexcept = 0;

	virtual void set_render_pass_enabled(kotek::uint8_t render_graph_id,
		kotek::uint8_t pass_index, bool enabled) noexcept = 0;
	virtual void request_render_pass_add(kotek::uint8_t render_graph_id,
		const char* p_pass_name) noexcept = 0;
	virtual void request_render_pass_remove(kotek::uint8_t render_graph_id,
		kotek::uint8_t pass_index) noexcept = 0;
	virtual void request_render_pass_move(kotek::uint8_t render_graph_id,
		kotek::uint8_t pass_index, bool move_up) noexcept = 0;

	virtual kotek::uint32_t get_pass_library_registry_generation(
		void) const noexcept = 0;
	virtual bool get_pass_library_registry(
		const char* const*& out_p_editor_pass_names,
		kotek::uint8_t& out_editor_pass_count,
		const char* const*& out_p_game_pass_names,
		kotek::uint8_t& out_game_pass_count) const noexcept = 0;
	virtual bool get_pass_library_status(
		eZirconRenderPassLibraryStatus& out_status,
		const char*& out_p_message) const noexcept = 0;

	virtual bool get_editor_gpu_stats(
		zircon_editor_gpu_stats_t& out_stats) const noexcept = 0;
	virtual void build_default_orbit_view_projection(float* p_out_view,
		float* p_out_projection, float aspect_ratio) const noexcept = 0;
};
