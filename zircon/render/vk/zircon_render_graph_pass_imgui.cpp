#include "zircon_render_graph_pass_imgui.h"
#include <kotek.render.vk/include/kotek_render_device.h>
#include <kotek.render.vk/include/kotek_render_graph_resource_manager.h>
#include <kotek.render.vk/include/kotek_render_resource_manager.h>
#include <kotek.render.vk/include/kotek_render_swapchain.h>

namespace zircon
{
	namespace Render
	{
		namespace vk
		{
			struct VERTEX_CONSTANT_BUFFER
			{
				float mvp[4][4];
			};

			void CheckImGuiCreationVulkanCallback(VkResult status) noexcept
			{
				KOTEK_ASSERT(status == VkResult::VK_SUCCESS,
					"[ImGui/Vulkan] failed to, check log");
			}

			zircon_RenderGraphPassImGui::zircon_RenderGraphPassImGui(
				Kotek::Core::ktkMainManager* p_manager,
				const Kotek::ktk::vector<Kotek::Core::kotek_sdk_ui_element*>&
					elements) :
				inherited(),
				m_imgui_elements(elements), m_p_main_manager(p_manager),
				m_p_sampler(nullptr), m_scissor({}), m_viewport({})
			{
				KOTEK_CPU_PROFILE();

				IMGUI_CHECKVERSION();
				ImGui::CreateContext();

				auto& io = ImGui::GetIO();

				io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
				io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

				ImGui::StyleColorsDark();

				if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				{
					ImGuiStyle& style = ImGui::GetStyle();

					style.WindowRounding = 0.0f;
					style.Colors[ImGuiCol_WindowBg].w = 1.0f;
				}

				// TODO: think about wxWidgets here, but it's a natively HWND
				// TODO: remove checking on command line
				if (p_manager->Get_EngineConfig()
						->IsContainsConsoleCommandLineArgument(
							Kotek::kConsoleCommandArg_Editor) == false)
				{
#ifdef KOTEK_USE_WINDOW_LIBRARY_GLFW
					GLFWwindow* p_handle = static_cast<GLFWwindow*>(
						p_manager->GetGameManager()->GetWindowHandle());

					ImGui_ImplGlfw_InitForVulkan(p_handle, true);
#else
					// TODO: implement for SDL
	#error not implemented
#endif
				}
				else
				{
#ifdef KOTEK_PLATFORM_WINDOWS
					HWND p_handle = static_cast<HWND>(
						p_manager->GetGameManager()->GetWindowHandle());

					KOTEK_ASSERT(
						p_handle, "you must have a valid handle of SDK window");

					ImGui_ImplWin32_Init(p_handle);
#elif KOTEK_PLATFORM_LINUX
					KOTEK_ASSERT(false, "not implemented");
#endif
				}

				this->m_is_use_sdk = p_manager->Get_EngineConfig()
										 ->IsContainsConsoleCommandLineArgument(
											 Kotek::kConsoleCommandArg_Editor);
			}

			zircon_RenderGraphPassImGui::~zircon_RenderGraphPassImGui(void)
			{
				KOTEK_CPU_PROFILE();

				ImGui_ImplVulkan_Shutdown();

				// TODO: think
				if (this->m_p_main_manager->Get_EngineConfig()
						->IsContainsConsoleCommandLineArgument(
							Kotek::kConsoleCommandArg_Editor) == false)
				{
					ImGui_ImplGlfw_Shutdown();
				}
				else
				{
					ImGui_ImplWin32_Shutdown();
				}

				ImGui::DestroyContext();
			}

			void zircon_RenderGraphPassImGui::OnSetupInput(
				Kotek::Render::vk::ktkRenderGraphStorageInput& storage,
				Kotek::Render::vk::ktkRenderDevice* p_device,
				Kotek::Core::ktkIFileSystem* p_file_system)
			{
				KOTEK_CPU_PROFILE();
			}

			void zircon_RenderGraphPassImGui::OnSetupOutput(
				Kotek::Render::vk::ktkRenderGraphStorageOutput& storage,
				Kotek::Render::vk::ktkRenderDevice* p_device)
			{
			}

			void zircon_RenderGraphPassImGui::OnCreatedResources(void)
			{
				KOTEK_CPU_PROFILE();

				Kotek::Render::vk::ktkRenderDevice* p_device =
					static_cast<Kotek::Render::vk::ktkRenderDevice*>(
						this->m_p_main_manager->getRenderDevice());

				KOTEK_ASSERT(
					p_device, "you must initialize your render device");

				ImGui_ImplVulkan_InitInfo info = {};

				info.Device = p_device->GetDevice();
				info.PhysicalDevice = p_device->GetCurrentPhysicalDevice();
				info.CheckVkResultFn = CheckImGuiCreationVulkanCallback;
				info.DescriptorPool =
					this->m_p_manager_resource->getDescriptorPool();
				info.Instance = p_device->GetInstance();
				info.ImageCount = Kotek::Render::vk::_kSwapchainBackBuffers;
				info.MinImageCount = Kotek::Render::vk::_kSwapchainBackBuffers;
				info.PipelineCache = nullptr; // TODO: think
				info.Queue = p_device->GetQueue_Graphics();
				info.QueueFamily = p_device->GetFamilyQueueIndex_Graphics();
				info.Allocator = nullptr;

				VkRenderPass p_pass =
					this->m_p_manager_resource_graph->GetRenderPass(
						this->GetName());

				KOTEK_ASSERT(p_pass,
					"you must have an initialized VkRenderPass for render "
					"pass: {}",
					this->GetName().get_as_is());

				ImGui_ImplVulkan_Init(&info, p_pass);

				VkCommandBuffer p_command_buffer =
					this->m_p_manager_resource->getUploadHeap()
						->getCommandBuffer();

				ImGui_ImplVulkan_CreateFontsTexture(p_command_buffer);

				this->m_p_manager_resource->getUploadHeap()->flushAndFinish(
					p_device);

				ImGui_ImplVulkan_DestroyFontUploadObjects();
			}

			void zircon_RenderGraphPassImGui::OnUpdate()
			{
				// TODO: use correction matrix instead of this, don't negate the
				// height
				this->m_viewport.height =
					-this->m_p_main_manager->getRenderDevice()->GetHeight();
				this->m_viewport.width =
					this->m_p_main_manager->getRenderDevice()->GetWidth();
				this->m_viewport.minDepth = 0.0f;
				this->m_viewport.maxDepth = 1.0f;
				this->m_viewport.x = 0.0f;
				this->m_viewport.y =
					this->m_p_main_manager->getRenderDevice()->GetHeight();

				this->m_scissor.extent.width =
					this->m_p_main_manager->getRenderDevice()->GetWidth();
				this->m_scissor.extent.height =
					this->m_p_main_manager->getRenderDevice()->GetHeight();
				this->m_scissor.offset.x = 0;
				this->m_scissor.offset.y = 0;

				ImGui_ImplVulkan_NewFrame();

				if (this->m_is_use_sdk == false)
				{
					ImGui_ImplGlfw_NewFrame();
				}
				else
				{
#ifdef KOTEK_PLATFORM_WINDOWS
					ImGui_ImplWin32_NewFrame();
#elif KOTEK_PLATFORM_LINUX
					KOTEK_ASSERT(false, "not implemented");
#endif
				}

				ImGui::NewFrame();

				for (auto* p_element : this->m_imgui_elements)
				{
					p_element->Draw(this->m_p_main_manager);
				}

				ImGui::Render();
			}

			void zircon_RenderGraphPassImGui::OnRender(
				const Kotek::Render::vk::ktkRenderGraphNode& node,
				VkCommandBuffer p_command_buffer)
			{
				VkRenderPassBeginInfo info_begin_render_pass = {};

				const VkClearValue p_color_rt[] = {
					{.color{.float32 = {1.0f, 0.2f, 1.0f, 1.0f}}}};

				info_begin_render_pass.sType =
					VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				info_begin_render_pass.pNext = nullptr;
				info_begin_render_pass.renderPass = node.GetRenderPass();
				info_begin_render_pass.framebuffer = node.GetFrameBuffer();
				info_begin_render_pass.renderArea.offset.x = 0;
				info_begin_render_pass.renderArea.offset.y = 0;
				info_begin_render_pass.renderArea.extent.width =
					this->m_p_main_manager->getRenderDevice()->GetWidth();
				info_begin_render_pass.renderArea.extent.height =
					this->m_p_main_manager->getRenderDevice()->GetHeight();
				info_begin_render_pass.clearValueCount = 1;
				info_begin_render_pass.pClearValues = p_color_rt;

				vkCmdBeginRenderPass(p_command_buffer, &info_begin_render_pass,
					VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);

				ImGui_ImplVulkan_RenderDrawData(
					ImGui::GetDrawData(), p_command_buffer);

				vkCmdEndRenderPass(p_command_buffer);
			}
		} // namespace vk
	}     // namespace Render
} // namespace zircon