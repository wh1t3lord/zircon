#pragma once

#include <kotek.render.vk/include/kotek_render_texture_manager.h>

namespace zircon
{
	namespace Render
	{
		namespace vk
		{
			class zircon_RenderResourceManager;
		}
	} // namespace Render
} // namespace zircon

namespace Kotek
{
	namespace Core
	{
		class ktkMainManager;
	}
} // namespace Kotek

namespace zircon
{
	namespace Render
	{
		namespace vk
		{
			class zircon_RenderGraphPassMesh
				: public Kotek::Render::vk::ktkRenderGraphRenderPass
			{
				using inherited = Kotek::Render::vk::ktkRenderGraphRenderPass;

			public:
				zircon_RenderGraphPassMesh(
					Kotek::Core::ktkMainManager* p_main_manager,
					zircon_RenderResourceManager* p_game_render_resource_manager);
				~zircon_RenderGraphPassMesh(void);

				void OnSetupInput(
					Kotek::Render::vk::ktkRenderGraphStorageInput& storage,
					Kotek::Render::vk::ktkRenderDevice* p_device,
					Kotek::Core::ktkIFileSystem* p_file_system) override;

				void OnSetupOutput(
					Kotek::Render::vk::ktkRenderGraphStorageOutput& storage,
					Kotek::Render::vk::ktkRenderDevice* p_device) override;

				void OnCreatedResources(void) override;

				void OnUpdate() override;

				void OnRender(const Kotek::Render::vk::ktkRenderGraphNode& node,
					VkCommandBuffer p_command_buffer) override;

			private:
				Kotek::Core::ktkMainManager* m_p_main_manager;
				zircon_RenderResourceManager* m_p_render_resource_manager;
			};
		} // namespace vk
	}     // namespace Render
} // namespace zircon