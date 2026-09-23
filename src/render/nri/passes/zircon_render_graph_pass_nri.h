#pragma once

#include <kotek.core.api/include/kotek_api.h>

/// \file zircon_render_graph_pass_nri.h
/// \~english the common base of the NRI frame passes (task Z24 B3b): the
/// kotek ktkIRenderFramePass surface plus the ONE zircon-side lifecycle
/// hook the passlib needs — the passes are created by NAME through the
/// C-ABI seam (no constructor arguments may cross it), so the host passes
/// the main manager ONCE after creation through
/// zircon_nri_passlib_initialize, which lands here. The default is a
/// no-op (the present/clear pass needs nothing); the meshlet pass
/// overrides it to load its cluster + create its GPU resources BEFORE
/// the first frame records. Handles only after that (the reload-safety
/// laws) — the pass never owns the manager or the filesystem.

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_render_graph_pass_nri : public kotek::core::ktkIRenderFramePass
{
public:
	zircon_render_graph_pass_nri(void) = default;
	~zircon_render_graph_pass_nri(void) override = default;

	/// \~english the post-create, pre-frame setup hook (default: nothing
	/// to set up)
	virtual void Initialize_Nri(kotek::core::ktkMainManager* p_main_manager
	)
	{
		(void)p_main_manager;
	}
};
