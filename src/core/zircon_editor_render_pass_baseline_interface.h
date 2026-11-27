#pragma once

/// read/write access to the game session's resolved pass-list baseline.
/// Text crosses the boundary as C strings; the owning static string and its
/// container ABI remain inside the game-manager module (task Z9).
class zircon_editor_render_pass_baseline_interface
{
public:
	virtual ~zircon_editor_render_pass_baseline_interface(void) {}

	virtual const char* get_render_passes_game_resolved_baseline_text(
		void) const noexcept = 0;
	virtual void set_render_passes_game_resolved_baseline_text(
		const char* p_text) noexcept = 0;
};
