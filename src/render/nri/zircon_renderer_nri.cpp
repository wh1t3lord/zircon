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

void zircon_renderer_nri::Shutdown(void)
{
	// the passes are not ours (the game manager owns them through the
	// passlib seam) — just drop the non-owning view
	this->m_frame_passes.clear();
}

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
		// task Z5 phase 2 (P4): the frame goes through the pass surface
		// — the swapchain runs its discipline (acquire -> barrier ->
		// record -> barrier -> submit -> present inside
		// kotek.render.nri) and the installed passes record between the
		// barriers; an empty list keeps the built-in clear frame
		// (Present_With_Passes falls back to Present)
		p_swapchain->Present_With_Passes(this->m_p_main_manager, p_device,
			this->m_frame_passes.empty() ? nullptr
										 : this->m_frame_passes.data(),
			static_cast<kotek::uint32_t>(this->m_frame_passes.size()));
	}
}

void zircon_renderer_nri::Resize() {}

const char* zircon_renderer_nri::Get_Name(void) const noexcept
{
	return "NRI (D3D12)";
}

void zircon_renderer_nri::Set_Frame_Passes(
	kotek::core::ktkIRenderFramePass* const* pp_passes,
	kotek::uint8_t pass_count)
{
	this->m_frame_passes.clear();

	if ((pp_passes == nullptr) || (pass_count == 0))
		return;

	KOTEK_ASSERT(pass_count <= ZIRCON_DEF_RENDERER_NRI_MAX_FRAME_PASS_COUNT,
		"too many NRI frame passes installed ({}), raise "
		"ZIRCON_DEF_RENDERER_NRI_MAX_FRAME_PASS_COUNT",
		pass_count);

	for (kotek::uint8_t pass_index = 0; pass_index < pass_count;
		 ++pass_index)
	{
		KOTEK_ASSERT(pp_passes[pass_index],
			"a null NRI frame pass must never be installed (index {})",
			pass_index);

		if (this->m_frame_passes.size() < this->m_frame_passes.capacity())
		{
			this->m_frame_passes.push_back(pp_passes[pass_index]);
		}
	}
}
