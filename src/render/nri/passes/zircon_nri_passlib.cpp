#include "zircon_nri_passlib.h"

#include <cstring>

/// \file zircon_nri_passlib.cpp
/// \~english the hand-written NRI pass registry (see the header). A new
/// NRI pass registers by: (1) bumping the count in
/// zircon_nri_passlib_get_count, (2) adding its case to
/// zircon_nri_passlib_get_name, (3) adding its strcmp branch to
/// zircon_nri_passlib_create — all three spell the pass's own name
/// constant, nothing else changes.

extern "C"
{
	unsigned zircon_nri_passlib_get_count(void)
	{
		// one registered pass today
		return 1;
	}

	const char* zircon_nri_passlib_get_name(unsigned index)
	{
		switch (index)
		{
		case 0:
			return no_streaming::kZircon_RenderGraphPassPresentNri_Name;
		default:
			return nullptr;
		}
	}

	kotek::core::ktkIRenderFramePass* zircon_nri_passlib_create(
		const char* p_pass_name)
	{
		if (!p_pass_name)
			return nullptr;

		if (std::strcmp(p_pass_name,
				no_streaming::kZircon_RenderGraphPassPresentNri_Name) == 0)
		{
			return new no_streaming::zircon_render_graph_pass_present_nri();
		}

		return nullptr;
	}

	void zircon_nri_passlib_destroy(
		kotek::core::ktkIRenderFramePass* p_pass)
	{
		delete p_pass;
	}
}
