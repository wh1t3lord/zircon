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
		// two registered passes today (task Z24 B3b: the meshlet draw
		// joined the present/clear pass)
		return 2;
	}

	const char* zircon_nri_passlib_get_name(unsigned index)
	{
		switch (index)
		{
		case 0:
			return no_streaming::kZircon_RenderGraphPassPresentNri_Name;
		case 1:
			return no_streaming::
				kZircon_RenderGraphPassMeshletClusterNri_Name;
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
				no_streaming::kZircon_RenderGraphPassPresentNri_Name) ==
			0)
		{
			return new no_streaming::zircon_render_graph_pass_present_nri();
		}

		if (std::strcmp(p_pass_name,
				no_streaming::
					kZircon_RenderGraphPassMeshletClusterNri_Name) == 0)
		{
			return new no_streaming::
				zircon_render_graph_pass_meshlet_cluster_nri();
		}

		return nullptr;
	}

	void zircon_nri_passlib_initialize(
		kotek::core::ktkIRenderFramePass* p_pass,
		kotek::core::ktkMainManager* p_main_manager)
	{
		if (p_pass == nullptr)
			return;

		// the same-module downcast: the pass was created inside this
		// library and dies here — the dynamic_cast is the future-DLL-split
		// safety (a foreign pass object never receives the hook)
		zircon_render_graph_pass_nri* p_nri_pass =
			dynamic_cast<zircon_render_graph_pass_nri*>(p_pass);

		if (p_nri_pass == nullptr)
		{
			KOTEK_MESSAGE_ERROR(
				"the NRI pass '{}' was not created by this passlib — "
				"the initialize hook is skipped",
				p_pass->Get_Name());

			return;
		}

		p_nri_pass->Initialize_Nri(p_main_manager);
	}

	void zircon_nri_passlib_destroy(
		kotek::core::ktkIRenderFramePass* p_pass)
	{
		delete p_pass;
	}
}
