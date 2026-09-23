#pragma once

#include <kotek.core.api/include/kotek_api.h>

#include "no_streaming/zircon_render_graph_pass_meshlet_cluster_nri.h"
#include "no_streaming/zircon_render_graph_pass_present_nri.h"
#include "zircon_render_graph_pass_nri.h"

/// \file zircon_nri_passlib.h
/// \~english the hand-written registry of the NRI frame passes (task Z5
/// phase 2 / P4, extended with the geometry pass in task Z24 B3b). The bgfx
/// side uses heavyweight PRE_BUILD codegen (the generated
/// zircon_render_pass_factory scans the pass folder); NRI has a handful of
/// passes, so the registry is hand-written in the .cpp — the spirit (name
/// -> create/destroy through one seam) without the codegen. The surface
/// mirrors the bgfx passlib's C-ABI shape exactly, so the future
/// hot-swappable zircon.render.passes.nri.dll (the same STATIC/DLL
/// duality as bgfx, deferred) adds only the export macro.
///
/// Reload-safety laws (enforced by the callers, keep them true here):
///  - create and destroy BOTH execute inside this library (the cross-CRT
///    rule — today everything is one static closure inside game.ktk; the
///    law is what keeps the future DLL split safe);
///  - every created pass is destroyed through zircon_nri_passlib_destroy
///    BEFORE the library could be unloaded;
///  - passes hold POD/handles only.
///
/// The lifecycle seam: the passes are created by NAME (no constructor
/// arguments cross the C-ABI seam), so zircon_nri_passlib_initialize hands
/// the pass its main manager ONCE after creation (the base class's
/// Initialize_Nri hook, a no-op for state-less passes) — BEFORE the first
/// frame records.

/// the registry capacity ceiling callers size their validation name
/// arrays with; 2 passes are registered today, 8 leaves room for the
/// planned grid/imgui NRI passes without touching callers — raise WITH
/// the registry, never below its count
#define ZIRCON_DEF_NRI_PASSLIB_REGISTRY_MAX_COUNT 8

/// the built-in game-session pass set for the resolution chain (the NRI
/// counterpart of kZirconConfig_DefaultRenderPassesGame): the
/// present/clear pass FIRST (it opens the frame with the clear; the
/// meshlet pass draws ON TOP of it through the LOAD render pass) then the
/// meshlet cluster draw (task Z24 B3b)
constexpr const char* kZircon_NriPasslib_DefaultGamePasses =
	"no_streaming::zircon_render_graph_pass_present_nri,"
	"no_streaming::zircon_render_graph_pass_meshlet_cluster_nri";

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

	/// the post-create lifecycle hook: hands the pass the main manager
	/// BEFORE the first frame (loads content + creates the GPU resources
	/// through the geometry seam for the passes that need it; a no-op for
	/// the state-less ones). Never fails the pass — a missing content
	/// file or backend leaves the pass inert with a loud log (user data
	/// is not a programmer error)
	void zircon_nri_passlib_initialize(
		kotek::core::ktkIRenderFramePass* p_pass,
		kotek::core::ktkMainManager* p_main_manager);

	/// destroys a pass created by zircon_nri_passlib_create, INSIDE the
	/// library (the cross-CRT rule)
	void zircon_nri_passlib_destroy(
		kotek::core::ktkIRenderFramePass* p_pass);
}
