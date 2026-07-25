#include "zircon_renderer_nri.h"

zircon_renderer_nri::zircon_renderer_nri(
	kotek::core::ktkMainManager* p_main_manager) :
	m_p_main_manager(p_main_manager)
{
	KOTEK_ASSERT(
		this->m_p_main_manager, "you must pass a valid main manager!");
}

zircon_renderer_nri::~zircon_renderer_nri(void) {}

void zircon_renderer_nri::Initialize(void)
{
	KOTEK_ASSERT(this->m_p_main_manager->getRenderDevice(),
		"the NRI render device must exist before the zircon renderer is "
		"created (kotek.render.nri initializes it)");
	KOTEK_ASSERT(this->m_p_main_manager->getRenderSwapchainManager(),
		"the NRI render swapchain must exist before the zircon renderer "
		"is created (kotek.render.nri initializes it)");
}

void zircon_renderer_nri::Shutdown(void) {}

void zircon_renderer_nri::draw()
{
	// per-frame uploads (a no-op default on the interface in phase 1)
	kotek::core::ktkIRenderResourceManager* p_resource_manager =
		this->m_p_main_manager->GetRenderResourceManager();

	if (p_resource_manager)
	{
		p_resource_manager->Update();
	}

	kotek::core::ktkIRenderSwapchain* p_swapchain =
		this->m_p_main_manager->getRenderSwapchainManager();
	kotek::core::ktkIRenderDevice* p_device =
		this->m_p_main_manager->getRenderDevice();

	if (p_swapchain && p_device)
	{
		// the clear-color present lives inside kotek.render.nri (acquire
		// -> barrier -> clear -> barrier -> submit -> present)
		p_swapchain->Present(this->m_p_main_manager, p_device);
	}
}

void zircon_renderer_nri::Resize() {}

const char* zircon_renderer_nri::Get_Name(void) const noexcept
{
	return "NRI (D3D12)";
}
