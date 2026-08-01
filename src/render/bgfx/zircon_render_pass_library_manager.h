#pragma once

#include <kotek.core.containers.dll/include/kotek_std_alias_dll.h>

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_RENDER
KOTEK_BEGIN_NAMESPACE_RENDER_BGFX
class ktkRenderGraphSimplifiedRenderPass;
KOTEK_END_NAMESPACE_RENDER_BGFX
KOTEK_END_NAMESPACE_RENDER
KOTEK_END_NAMESPACE_KOTEK

class zircon_renderer_bgfx;

/// creates a pass instance by its registered class name
using zircon_render_pass_create_pfn_t =
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* (*
	)(const char* p_pass_name);

/// destroys a pass inside the module that created it (cross-CRT rule,
/// task Z3 P3)
using zircon_render_pass_destroy_pfn_t =
	void (*)(kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass);

/// the hot-swappable pass library of the graphics-development
/// configuration, loaded relative to the working directory (the boot
/// discipline runs kotek.exe from the repo root, next to data_game/
/// data_user/)
#define ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH \
	"passes/zircon.render.passes.bgfx.dll"

/// the seam between the executor and the concrete render passes (task Z3
/// P3a). Default configuration: installs the statically-linked
/// zircon_passlib_create into the renderer (creation flows through the
/// same seam as the dev mode, destruction stays on
/// ktkRenderGraphSimplified::Shutdown — byte-identical to the pre-split
/// behavior). Graphics-development build (ZIRCON_USE_GRAPHICS_DEVELOPMENT)
/// with the graphics_development feature on: loads
/// passes/zircon.render.passes.bgfx.dll once at renderer init, resolves
/// the four C-ABI exports and installs create/destroy callbacks that
/// execute inside the DLL; a failed load falls back to the static
/// callbacks loudly (the dev build carries a static twin of the passes
/// for exactly this case).
///
/// The reload-safety laws this seam exists for: passes are created AND
/// destroyed inside the library, always before any unload (P3b adds the
/// watcher-driven unload/reload on top of this foundation).
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
	/// builds).
	void Initialize(zircon_renderer_bgfx* p_renderer,
		bool prefer_shared_library,
		zircon_render_pass_create_pfn_t p_static_create,
		zircon_render_pass_destroy_pfn_t p_static_destroy) noexcept;

	/// unloads the pass library when one is loaded; every pass it created
	/// must already be destroyed through the installed destroy callback
	/// (the renderer's graph teardown runs first) — a creation/
	/// destruction count mismatch is asserted loudly
	void Shutdown(void) noexcept;

	bool is_shared_library_active(void) const noexcept;

private:
	bool m_is_shared_library_active;

#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
	/// the loaded pass library (owning handle); only in the dev
	/// configuration — the default configuration carries no OS handle and
	/// pulls no dll-container module symbols
	kotek::dll::shared_library m_library;
#endif
};
