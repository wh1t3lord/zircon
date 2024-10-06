#include "zircon_renderer.h"
#include <kotek.render.vk/include/kotek_render_device.h>
#include <kotek.render.vk/include/kotek_render_graph.h>
#include <kotek.render.vk/include/kotek_render_graph_builder.h>
#include <kotek.render.vk/include/kotek_render_resource_manager.h>
#include <kotek.render.vk/include/kotek_render_swapchain.h>

#include "zircon_render_graph_pass_mesh.h"
#include "zircon_render_graph_pass_imgui.h"
#include "zircon_render_graph_pass_present.h"

#include "zircon_render_resource_manager.h"

namespace zircon
{
	namespace Render
	{
		namespace vk
		{
			zircon_Renderer::zircon_Renderer(
				Kotek::Core::ktkMainManager& main_manager) :
				m_p_device(static_cast<Kotek::Render::vk::ktkRenderDevice*>(
					main_manager.getRenderDevice())),
				m_p_resource_manager(
					static_cast<Kotek::Render::vk::ktkRenderResourceManager*>(
						main_manager.GetRenderResourceManager())),
				m_render_graph_resource_manager(main_manager),
				m_p_main_manager(&main_manager)
			{
			}

			zircon_Renderer::~zircon_Renderer() {}

			void zircon_Renderer::Initialize(
				const Kotek::ktk::vector<Kotek::Core::kotek_sdk_ui_element*>&
					ui_elements)
			{
				KOTEK_ASSERT(this->m_p_device, "you must have a valid device");

				KOTEK_ASSERT(this->m_p_resource_manager,
					"you must have a valid resource manager");

				KOTEK_ASSERT(Kotek::ktk::is_equal(
								 this->m_p_device->GetHeight(), 0.0f) == false,
					"invalid value for height from render device");

				KOTEK_ASSERT(Kotek::ktk::is_equal(
								 this->m_p_device->GetWidth(), 0.0f) == false,
					"invalid value width from render device");

				this->CreateRenderGraph(ui_elements);

				this->m_imgui_elements = ui_elements;

				this->m_render_graph.Initialize();

				KOTEK_MESSAGE("renderer is initialized");
			}

			void zircon_Renderer::Shutdown(void)
			{
				KOTEK_MESSAGE("renderer is shutdown");

				this->DestroyRenderGraph();

				for (auto*& p_element : this->m_imgui_elements)
				{
					delete p_element;
					p_element = nullptr;
				}

				this->m_imgui_elements.clear();
			}

			void zircon_Renderer::draw()
			{
				this->Begin();

				Kotek::Render::vk::ktkRenderSwapchain* p_render_swapchain =
					static_cast<Kotek::Render::vk::ktkRenderSwapchain*>(
						this->m_p_main_manager->getRenderSwapchainManager());

				Kotek::ktk::uint32_t swapchain_image_index =
					p_render_swapchain->Wait(this->m_p_device);

				this->m_render_graph.UpdateAll();
				this->m_render_graph.RenderAll();

				this->End();
			}

			void zircon_Renderer::Resize()
			{
				this->DestroyRenderGraph();
				this->CreateRenderGraph(this->m_imgui_elements);
			}

			Kotek::ktk::string zircon_Renderer::GetName(void) const noexcept
			{
				return Kotek::kRenderer_Vulkan_Name;
			}

			void zircon_Renderer::Begin(void) noexcept
			{
				Kotek::Render::vk::kotek_render_dynamic_buffer_ring* p_ring =
					this->m_p_resource_manager->GetDynamicBufferRing();

				KOTEK_ASSERT(p_ring, "must be a valid object");

				p_ring->OnBeginFrame();

				Kotek::Render::vk::kotek_render_command_list_ring*
					p_command_list = this->m_render_graph.GetCommandListRing();

				KOTEK_ASSERT(p_command_list, "must be a valid object");

				p_command_list->OnBeginFrame();
			}

			void zircon_Renderer::End(void) noexcept
			{
				Kotek::Render::vk::ktkRenderSwapchain* p_render_swapchain =
					static_cast<Kotek::Render::vk::ktkRenderSwapchain*>(
						this->m_p_main_manager->getRenderSwapchainManager());

				VkPipelineStageFlags submit_wait_stage =
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

				const VkSemaphore p_semaphores_wait[] = {
					p_render_swapchain->GetSemaphore_ImageAvailable()};

				const VkSemaphore p_semaphores_signal[] = {
					p_render_swapchain->GetSemaphore_RenderFinished()};

				VkFence p_fence =
					p_render_swapchain->GetFence_CommandExecuted();

				auto* p_command_list_ring =
					this->m_render_graph.GetCommandListRing();

				VkSubmitInfo info = {};

				info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				info.pNext = nullptr;
				info.waitSemaphoreCount = 1;
				info.pWaitSemaphores = p_semaphores_wait;
				info.pWaitDstStageMask = &submit_wait_stage;
				info.signalSemaphoreCount = 1;
				info.pSignalSemaphores = p_semaphores_signal;
				info.commandBufferCount =
					p_command_list_ring->GetCountOfCommandBuffersPerFrame();
				info.pCommandBuffers =
					p_command_list_ring->GetAllCommandsBufferForCurrentFrame()
						.data();

				VkQueue p_queue_graphics =
					this->m_p_device->GetQueue_Graphics();

				KOTEK_ASSERT(p_queue_graphics,
					"you must initialize device (VkQueue=Graphics)");

				VkResult status =
					vkQueueSubmit(p_queue_graphics, 1, &info, p_fence);

				KOTEK_ASSERT(status == VK_SUCCESS,
					"failed to vkQueueSubmit. See status");

				p_render_swapchain->Present(
					this->m_p_main_manager, this->m_p_device);
			}

			void zircon_Renderer::DestroyRenderGraph(void) noexcept
			{
				this->m_render_graph.Shutdown();
				this->m_render_graph_resource_manager.Shutdown();
			}

			void zircon_Renderer::CreateRenderGraph(
				const Kotek::ktk::vector<Kotek::Core::kotek_sdk_ui_element*>&
					imgui_elements) noexcept
			{
				Kotek::Render::vk::ktkRenderGraphBuilder builder(
					*this->m_p_main_manager);

				builder.Initialize(
					&this->m_render_graph_resource_manager, "present_image");

				builder.RegisterRenderPass("render pass mesh",
					new zircon_RenderGraphPassMesh(this->m_p_main_manager,
						static_cast<zircon_RenderResourceManager*>(
							this->m_p_main_manager->GetGameManager()
								->GetRenderResourceManager())));

				builder.RegisterRenderPass("render pass ui-imgui",
					new zircon_RenderGraphPassImGui(
						this->m_p_main_manager, imgui_elements));

				builder.RegisterRenderPass("render pass present (blitting)",
					new zircon_RenderGraphPassPresent(
						static_cast<Kotek::Render::vk::ktkRenderSwapchain*>(
							this->m_p_main_manager
								->getRenderSwapchainManager()),
						static_cast<Kotek::Render::vk::ktkRenderDevice*>(
							this->m_p_main_manager->getRenderDevice())));

				this->m_render_graph = builder.Compile();
			}

		} // namespace vk
	}     // namespace Render
} // namespace zircon
