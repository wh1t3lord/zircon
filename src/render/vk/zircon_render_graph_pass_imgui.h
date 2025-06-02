#pragma once

#include <kotek.render.vk/include/kotek_render_texture_manager.h>

namespace Kotek
{
	namespace Core
	{
		class ktkFileSystem;
	}
} // namespace Kotek

namespace zircon
{
	namespace Render
	{
		namespace vk
		{
			class zircon_RenderGraphPassImGui
				: public Kotek::Render::vk::ktkRenderGraphRenderPass
			{
				using inherited = Kotek::Render::vk::ktkRenderGraphRenderPass;

			public:
				zircon_RenderGraphPassImGui(
					Kotek::Core::ktkMainManager* p_main_manager,
					const Kotek::ktk::vector<Kotek::Core::kotek_sdk_ui_element*>& elements);
				~zircon_RenderGraphPassImGui(void);

				void OnSetupInput(Kotek::Render::vk::ktkRenderGraphStorageInput& storage,
					Kotek::Render::vk::ktkRenderDevice* p_device,
					Kotek::Core::ktkIFileSystem* p_file_system) override;

				void OnSetupOutput(Kotek::Render::vk::ktkRenderGraphStorageOutput& storage,
					Kotek::Render::vk::ktkRenderDevice* p_device) override;

				void OnCreatedResources(void) override;

				void OnUpdate() override;

				void OnRender(const Kotek::Render::vk::ktkRenderGraphNode& node,
					VkCommandBuffer p_command_buffer) override;

			private:
				bool m_is_use_sdk;
				VkViewport m_viewport;
				VkRect2D m_scissor;
				VkSampler m_p_sampler;
				Kotek::Core::ktkMainManager* m_p_main_manager;
				Kotek::ktk::vector<Kotek::Core::kotek_sdk_ui_element*> m_imgui_elements;
			};

		} // namespace vk
	}     // namespace Render
} // namespace Kotek