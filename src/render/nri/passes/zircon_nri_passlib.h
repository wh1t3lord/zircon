#pragma once

#include "no_streaming/zircon_render_graph_pass_present_nri.h"

/// \file zircon_nri_passlib.h
/// \~english the hand-written registry of the NRI frame passes (task Z5
/// phase 2 / P4). The bgfx side uses heavyweight PRE_BUILD codegen (the
/// generated zircon_render_pass_factory scans the pass folder); NRI has a
/// handful of passes, so the registry is hand-written in the .cpp — the
/// spirit (name -> create/destroy through one seam) without the codegen.
/// The surface mirrors the bgfx passlib's C-ABI shape exactly, so the
/// future hot-swappable zircon.render.passes.nri.dll (the same STATIC/DLL
/// duality as bgfx, deferred) adds only the export macro.
///
/// Reload-safety laws (enforced by the callers, keep them true here):
///  - create and destroy BOTH execute inside this library (the cross-CRT
///    rule — today everything is one static closure inside game.ktk; the
///    law is what keeps the future DLL split safe);
///  - every created pass is destroyed through zircon_nri_passlib_destroy
///    BEFORE the library could be unloaded;
///  - passes hold POD/handles only.

/// the registry capacity ceiling callers size their validation name
/// arrays with; 1 pass is registered today, 8 leaves room for the planned
/// grid/model/imgui NRI passes without touching callers — raise WITH the
/// registry, never below its count
#define ZIRCON_DEF_NRI_PASSLIB_REGISTRY_MAX_COUNT 8

/// the built-in game-session pass set for the resolution chain (the NRI
/// counterpart of kZirconConfig_DefaultRenderPassesGame): the single
/// present/clear pass — reproduces the phase-1 NRI output exactly
constexpr const char* kZircon_NriPasslib_DefaultGamePasses =
	no_streaming::kZircon_RenderGraphPassPresentNri_Name;

extern "C"
{
	/// the registered pass count (never exceeds
	/// ZIRCON_DEF_NRI_PASSLIB_REGISTRY_MAX_COUNT)
	unsigned zircon_nri_passlib_get_count(void);

	/// the registered pass name at index (< get_count()), nullptr out of
	/// range; the returned pointer is a string literal
	const char* zircon_nri_passlib_get_name(unsigned index);

	/// creates a pass by its registered name; nullptr when the name is
	/// not registered
	kotek::core::ktkIRenderFramePass* zircon_nri_passlib_create(
		const char* p_pass_name);

	/// destroys a pass created by zircon_nri_passlib_create, INSIDE the
	/// library (the cross-CRT rule)
	void zircon_nri_passlib_destroy(
		kotek::core::ktkIRenderFramePass* p_pass);
}
