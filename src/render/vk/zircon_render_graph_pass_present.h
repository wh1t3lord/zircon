#pragma once

#include <kotek.render.vk/include/kotek_render_texture_manager.h>

namespace Kotek
{
	namespace Core
	{
		class ktkFileSystem;
	}

	namespace Render
	{
		namespace vk
		{
			class ktkRenderSwapchain;
		}
	} // namespace Render
} // namespace Kotek

namespace zircon
{
	namespace Render
	{
		namespace vk
		{
			// TODO: delete this
			class zircon_RenderGraphPassPresent
				: public Kotek::Render::vk::ktkRenderGraphRenderPass
			{
			public:
				zircon_RenderGraphPassPresent(
					Kotek::Render::vk::ktkRenderSwapchain* p_swapchain,
					Kotek::Render::vk::ktkRenderDevice* p_device);
				~zircon_RenderGraphPassPresent(void);

				void OnSetupInput(
					Kotek::Render::vk::ktkRenderGraphStorageInput& storage,
					Kotek::Render::vk::ktkRenderDevice* p_device,
					Kotek::Core::ktkIFileSystem* p_file_system) override;

				void OnSetupOutput(
					Kotek::Render::vk::ktkRenderGraphStorageOutput& storage,
					Kotek::Render::vk::ktkRenderDevice* p_device) override;

				void OnCreatedResources() override;

				void OnRender(const Kotek::Render::vk::ktkRenderGraphNode& node,
					VkCommandBuffer p_command_buffer) override;

			private:
				Kotek::Render::vk::ktkRenderSwapchain* m_p_swapchain;
				Kotek::Render::vk::ktkRenderDevice* m_p_render_device;
				VkImage m_p_backbuffer_image_handle;
			};
		} // namespace vk
	}     // namespace Render
} // namespace zircon