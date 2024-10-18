#pragma once

#include <kotek.render.vk/include/kotek_render_graph.h>
#include <kotek.render.vk/include/kotek_render_graph_resource_manager.h>

namespace Kotek
{
	namespace Core
	{
		class ktkMainManager;
	}
} // namespace Kotek

namespace Kotek
{
	namespace Render
	{
		namespace vk
		{
			class ktkRenderResourceManager;
			class ktkRenderDevice;
			class ktkRenderSwapchain;
		} // namespace vk
	}     // namespace Render
} // namespace Kotek

namespace zircon
{
	namespace Render
	{
		namespace vk
		{
			class zircon_Renderer : public Kotek::Core::ktkIRenderer
			{
			public:
				zircon_Renderer(Kotek::Core::ktkMainManager& main_manager);
				~zircon_Renderer(void);

				void Initialize(const Kotek::ktk::vector<
					Kotek::Core::ktkISDKUIElement*>& ui_elements);
				void Shutdown(void) override;

				void draw() override;

				void Resize() override;

                Kotek::ktk::cstring GetName(void) const noexcept override;

			private:
				void Begin() noexcept;
				void End() noexcept;

				void DestroyRenderGraph(void) noexcept;

				// TODO: Specify render graph for different API version callings
				// function
				void CreateRenderGraph(const Kotek::ktk::vector<
					Kotek::Core::ktkISDKUIElement*>&
						imgui_elements) noexcept;

			private:
				Kotek::Render::vk::ktkRenderDevice* m_p_device;
				Kotek::Render::vk::ktkRenderResourceManager*
					m_p_resource_manager;
				Kotek::Core::ktkMainManager* m_p_main_manager;
				Kotek::ktk::vector<Kotek::Core::ktkISDKUIElement*>
					m_imgui_elements;
				Kotek::Render::vk::ktkRenderGraph m_render_graph;
				Kotek::Render::vk::ktkRenderGraphResourceManager
					m_render_graph_resource_manager;
			};
		} // namespace vk
	}     // namespace Render
} // namespace zircon
