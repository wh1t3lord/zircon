#pragma once

/// \file zircon_renderer_nri.h
/// \~english zircon-side renderer for the NRI backend (task K11/Z5).
/// Phase 1: it owned nothing GPU-side and forwarded draw() to the
/// swapchain's monolithic Present. Phase 2 (P4): draw() drives an
/// installed frame-pass list through the swapchain's Present_With_Passes
/// — the kotek.render.nri swapchain keeps the whole frame discipline
/// (fence/acquire/barriers/submit/present) and lets the passes record
/// between the barriers through the narrow ktkIRenderFramePassContext
/// surface. It deliberately talks only to kotek interfaces — no NRI
/// types may appear in zircon. The renderer holds NON-OWNING pass
/// pointers: the game manager creates/destroys the passes through the
/// NRI passlib seam (the same ownership split as the bgfx pass seam).

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
class ktkIRenderFramePass;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

/// the frame-pass list capacity: 1 pass is installed today (the
/// present/clear pass), 8 leaves room for the planned grid/model/imgui
/// NRI passes without a recompile-tweak (rule 9 — capacities are named
/// and sized from the registry, not magic)
#define ZIRCON_DEF_RENDERER_NRI_MAX_FRAME_PASS_COUNT 8

class zircon_renderer_nri : public kotek::core::ktkIRenderer
{
public:
	zircon_renderer_nri(kotek::core::ktkMainManager* p_main_manager);
	~zircon_renderer_nri(void);

	void Initialize(void);

	void Shutdown(void) override;

	void draw() override;

	void Resize() override;

	const char* Get_Name(void) const noexcept override;

	/// \~english task Z5 phase 2 (P4): installs the frame-pass list
	/// draw() drives through Present_With_Passes, in order. NON-OWNING —
	/// the game manager created the passes through the NRI passlib and
	/// destroys them after this renderer is gone.
	void Set_Frame_Passes(
		kotek::core::ktkIRenderFramePass* const* pp_passes,
		kotek::uint8_t pass_count);

private:
	kotek::core::ktkMainManager* m_p_main_manager;

	/// the installed frame passes (non-owning pointers — see
	/// Set_Frame_Passes), driven in order once per draw(); an empty list
	/// keeps the swapchain's built-in clear frame
	kotek::static_vector_t<kotek::core::ktkIRenderFramePass*,
		ZIRCON_DEF_RENDERER_NRI_MAX_FRAME_PASS_COUNT>
		m_frame_passes;
};
