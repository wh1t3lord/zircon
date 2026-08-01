#pragma once

#include <kotek.render.shared.bgfx/include/kotek_render_graph_simplified_render_pass.h>

/// the C ABI of the passes library (task Z3 P3): the ONLY symbols the
/// executor resolves from the hot-swappable zircon.render.passes.bgfx.dll
/// (stable across CRTs/compilers — the never-break rule). In the default
/// static configuration the same four functions are plain internal links
/// compiled into the static passes library, so the loader seam
/// (zircon_render_pass_library_manager) is mode-agnostic.
///
/// Reload-safety laws (enforced by the callers, keep them true here):
///  - create and destroy BOTH execute inside the library (cross-CRT rule:
///    a pass allocated by the library's own CRT is freed by the same CRT);
///  - every created pass is destroyed through zircon_passlib_destroy
///    BEFORE the library can be unloaded (P3b adds the unload).

// ZIRCON_PASSLIB_SHARED is defined only on the shared (graphics
// development) build of this project; the static build and the dev-mode
// static fallback twin export nothing
#ifdef ZIRCON_PASSLIB_SHARED
	#define ZIRCON_PASSLIB_EXPORT __declspec(dllexport)
#else
	#define ZIRCON_PASSLIB_EXPORT
#endif

extern "C"
{
	/// the unified registry size: every generated game pass followed by
	/// every generated editor pass (the per-session generated tables
	/// zircon_render_game_passes_registry /
	/// zircon_render_editor_passes_registry concatenated)
	ZIRCON_PASSLIB_EXPORT unsigned zircon_passlib_get_count(void);

	/// the registered pass class name at index (< get_count()), nullptr
	/// out of range; the returned pointer is a string literal
	ZIRCON_PASSLIB_EXPORT const char* zircon_passlib_get_name(
		unsigned index);

	/// creates a pass by its registered class name (any name from the
	/// unified enumeration, either session); nullptr when the name is
	/// not registered
	ZIRCON_PASSLIB_EXPORT kotek::render::bgfx::
		ktkRenderGraphSimplifiedRenderPass*
		zircon_passlib_create(const char* p_pass_name);

	/// destroys a pass created by zircon_passlib_create: runs
	/// OnDestroyResources and frees the object INSIDE the library
	ZIRCON_PASSLIB_EXPORT void zircon_passlib_destroy(
		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass);
}
