#include "zircon_render_graph_pass_present_nri.h"

namespace no_streaming
{
	zircon_render_graph_pass_present_nri::
		zircon_render_graph_pass_present_nri(void)
	{
	}

	zircon_render_graph_pass_present_nri::
		~zircon_render_graph_pass_present_nri(void)
	{
	}

	void zircon_render_graph_pass_present_nri::Record(
		kotek::core::ktkIRenderFramePassContext* p_context)
	{
		KOTEK_ASSERT(p_context,
			"the NRI present pass needs a valid frame pass context");

		if (p_context == nullptr)
			return;

		p_context->ClearColor(
			ZIRCON_DEF_RENDER_PASS_PRESENT_NRI_CLEAR_COLOR_R,
			ZIRCON_DEF_RENDER_PASS_PRESENT_NRI_CLEAR_COLOR_G,
			ZIRCON_DEF_RENDER_PASS_PRESENT_NRI_CLEAR_COLOR_B, 1.0f);
	}

	const char* zircon_render_graph_pass_present_nri::Get_Name(
		void) const noexcept
	{
		return kZircon_RenderGraphPassPresentNri_Name;
	}
} // namespace no_streaming
