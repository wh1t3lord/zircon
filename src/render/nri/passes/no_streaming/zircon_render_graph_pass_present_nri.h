#pragma once

#include <kotek.core.api/include/kotek_api.h>

/// \file zircon_render_graph_pass_present_nri.h
/// \~english the NRI present/clear pass (task Z5 phase 2 / P4): records
/// the milestone clear through the narrow ktkIRenderFramePassContext
/// surface, reproducing the phase-1 monolithic Present output exactly
/// (RGB 0.2/0.3/0.6). The module-boundary rule holds: no NRI types, no
/// ::nri:: includes — the pass only talks to the kotek frame-pass
/// context. It holds no state at all (POD/reload-safe by construction).

/// the milestone clear color, kept 1:1 with the kotek.render.nri built-in
/// (KOTEK_DEF_RENDER_NRI_CLEAR_COLOR_* in kotek_render_swapchain.cpp) —
/// the boundary rule forbids reaching into that module, so the pass
/// spells its own constants until the clear color becomes configurable
#define ZIRCON_DEF_RENDER_PASS_PRESENT_NRI_CLEAR_COLOR_R 0.2f
#define ZIRCON_DEF_RENDER_PASS_PRESENT_NRI_CLEAR_COLOR_G 0.3f
#define ZIRCON_DEF_RENDER_PASS_PRESENT_NRI_CLEAR_COLOR_B 0.6f

namespace no_streaming
{
	/// the registered pass name (single source: the NRI passlib registry
	/// and Get_Name both spell it through this constant)
	constexpr const char* kZircon_RenderGraphPassPresentNri_Name =
		"no_streaming::zircon_render_graph_pass_present_nri";

	class zircon_render_graph_pass_present_nri
		: public kotek::core::ktkIRenderFramePass
	{
	public:
		zircon_render_graph_pass_present_nri(void);
		~zircon_render_graph_pass_present_nri(void);

		void Record(
			kotek::core::ktkIRenderFramePassContext* p_context) override;

		const char* Get_Name(void) const noexcept override;
	};
} // namespace no_streaming
