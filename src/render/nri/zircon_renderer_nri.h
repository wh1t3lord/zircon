#pragma once

/// \file zircon_renderer_nri.h
/// \~english minimal zircon-side renderer for the NRI backend (task
/// K11/Z5 phase 1). It owns nothing GPU-side: the kotek.render.nri module
/// owns the device/swapchain, and draw() forwards to the swapchain's
/// Present (the clear-color frame). It deliberately talks only to kotek
/// interfaces — no NRI types may appear in zircon. The passes split
/// (zircon render-graph passes on top of this backend) is the deferred Z5
/// work; until then this class is what lets the engine boot and present
/// with the NRI backend selected.

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

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

private:
	kotek::core::ktkMainManager* m_p_main_manager;
};
