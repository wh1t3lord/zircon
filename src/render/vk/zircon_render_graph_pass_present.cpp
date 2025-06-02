#include "zircon_render_graph_pass_present.h"
#include <kotek.render.vk/include/kotek_render_device.h>
#include <kotek.render.vk/include/kotek_render_graph_resource_manager.h>
#include <kotek.render.vk/include/kotek_render_swapchain.h>

namespace zircon
{
	namespace Render
	{
		namespace vk
		{
			struct Vertex
			{
				float position[3];
				float color[3];
			};

			zircon_RenderGraphPassPresent::zircon_RenderGraphPassPresent(
				Kotek::Render::vk::ktkRenderSwapchain* p_swapchain,
				Kotek::Render::vk::ktkRenderDevice* p_device) :
				m_p_swapchain(p_swapchain),
				m_p_render_device(p_device), m_p_backbuffer_image_handle{}
			{
				KOTEK_ASSERT(this->m_p_swapchain,
					"you must have a valid pointer of swapchain class");

				KOTEK_ASSERT(this->m_p_render_device,
					"you must have a valid pointer of render device class");
			}

			zircon_RenderGraphPassPresent::~zircon_RenderGraphPassPresent(void) {}

			void zircon_RenderGraphPassPresent::OnSetupInput(
				Kotek::Render::vk::ktkRenderGraphStorageInput& storage,
				Kotek::Render::vk::ktkRenderDevice* p_device,
				Kotek::Core::ktkIFileSystem* p_file_system)
			{
				Kotek::ktk::filesystem::path vertex_shader_path;
				Kotek::ktk::filesystem::path fragment_shader_path;

				fragment_shader_path = p_file_system->GetFolderByEnum(
					Kotek::Core::eFolderIndex::kFolderIndex_Shaders);
				fragment_shader_path /= "present.frag";

				vertex_shader_path = p_file_system->GetFolderByEnum(
					Kotek::Core::eFolderIndex::kFolderIndex_Shaders);
				vertex_shader_path /= "present.vert";

				Kotek::ktk::string pipeline_name = "present_pipeline";

				storage.AddShader(pipeline_name,
					Kotek::Render::vk::shader_type_t::kShaderType_Vertex,
					Kotek::Render::vk::shader_loading_data_t(
						Kotek::Render::vk::shader_loading_data_type_t::
							kShaderLoadingDataType_FilePathString,
						vertex_shader_path.c_str()));

				storage.AddShader(pipeline_name,
					Kotek::Render::vk::shader_type_t::kShaderType_Pixel,
					Kotek::Render::vk::shader_loading_data_t(
						Kotek::Render::vk::shader_loading_data_type_t::
							kShaderLoadingDataType_FilePathString,
						fragment_shader_path.c_str()));

				storage.AddPipelineInfo(pipeline_name,
					{Kotek::Render::vk::helper::
							InitializePipelineInputAssemblyState(
								VkPrimitiveTopology::
									VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
						Kotek::Render::vk::helper::
							InitializePipelineRasterizationState(
								VkPolygonMode::VK_POLYGON_MODE_FILL,
								VkCullModeFlagBits::VK_CULL_MODE_NONE,
								VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE),
						Kotek::Render::vk::helper::
							InitializePipelineViewportState(1, 1),
						{Kotek::Render::vk::helper::
								InitializePipelineColorBlendAttachmentState(
									VK_TRUE)},
						Kotek::Render::vk::helper::
							InitializePipelineDepthStencilState(0, 0, 0),
						{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
						Kotek::Render::vk::helper::
							InitializePipelineMultisampleState(),
						{{.location = 0,
							 .binding = 0,
							 .format = VK_FORMAT_R32G32B32_SFLOAT,
							 .offset = 0},
							{.location = 1,
								.binding = 0,
								.format = VK_FORMAT_R32G32B32_SFLOAT,
								.offset = sizeof(float) * 3}},
						{{.binding = 0,
							.stride = sizeof(Vertex),
							.inputRate = VkVertexInputRate::
								VK_VERTEX_INPUT_RATE_VERTEX}}});
			}

			void zircon_RenderGraphPassPresent::OnSetupOutput(
				Kotek::Render::vk::ktkRenderGraphStorageOutput& storage,
				Kotek::Render::vk::ktkRenderDevice* p_device)
			{
				storage.SetUseBackBuffer(false);
			}

			void zircon_RenderGraphPassPresent::OnCreatedResources()
			{
				auto* p_texture =
					this->m_p_manager_resource_graph->GetBackBufferTexture();

				KOTEK_ASSERT(p_texture,
					"you must create backbuffer image or your pointer is "
					"invalid!");

				this->m_p_backbuffer_image_handle = p_texture->GetImageHandle();
			}

			void zircon_RenderGraphPassPresent::OnRender(
				const Kotek::Render::vk::ktkRenderGraphNode& node,
				VkCommandBuffer p_command_buffer)
			{
				VkImageMemoryBarrier info_memory_barrier_image_source;

				info_memory_barrier_image_source.sType =
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				info_memory_barrier_image_source.pNext = nullptr;
				info_memory_barrier_image_source.oldLayout =
					VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				info_memory_barrier_image_source.newLayout =
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				info_memory_barrier_image_source.srcAccessMask =
					VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				info_memory_barrier_image_source.dstAccessMask =
					VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
				info_memory_barrier_image_source.image =
					this->m_p_backbuffer_image_handle;
				info_memory_barrier_image_source.dstQueueFamilyIndex =
					VK_QUEUE_FAMILY_IGNORED;
				info_memory_barrier_image_source.srcQueueFamilyIndex =
					VK_QUEUE_FAMILY_IGNORED;
				info_memory_barrier_image_source.subresourceRange.aspectMask =
					VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
				info_memory_barrier_image_source.subresourceRange
					.baseArrayLayer = 0;
				info_memory_barrier_image_source.subresourceRange.baseMipLevel =
					0;
				info_memory_barrier_image_source.subresourceRange.layerCount =
					1;
				info_memory_barrier_image_source.subresourceRange.levelCount =
					1;

				VkImageMemoryBarrier info_memory_barrier_image_destination;

				info_memory_barrier_image_destination.sType =
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				info_memory_barrier_image_destination.pNext = nullptr;
				info_memory_barrier_image_destination.oldLayout =
					VK_IMAGE_LAYOUT_UNDEFINED;
				info_memory_barrier_image_destination.newLayout =
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				info_memory_barrier_image_destination.srcAccessMask =
					VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
				info_memory_barrier_image_destination.dstAccessMask =
					VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
				info_memory_barrier_image_destination.image =
					this->m_p_manager_resource->GetSwapchainImage(
						this->m_p_swapchain->GetAcquiredImageIndex());
				info_memory_barrier_image_destination.dstQueueFamilyIndex =
					VK_QUEUE_FAMILY_IGNORED;
				info_memory_barrier_image_destination.srcQueueFamilyIndex =
					VK_QUEUE_FAMILY_IGNORED;
				info_memory_barrier_image_destination.subresourceRange
					.aspectMask =
					VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
				info_memory_barrier_image_destination.subresourceRange
					.baseArrayLayer = 0;
				info_memory_barrier_image_destination.subresourceRange
					.baseMipLevel = 0;
				info_memory_barrier_image_destination.subresourceRange
					.layerCount = 1;
				info_memory_barrier_image_destination.subresourceRange
					.levelCount = 1;

				Kotek::ktk::array<VkImageMemoryBarrier, 2> barriers;

				barriers[0] = info_memory_barrier_image_source;
				barriers[1] = info_memory_barrier_image_destination;

				vkCmdPipelineBarrier(p_command_buffer,
					VkPipelineStageFlagBits::
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
					0, nullptr, 0, nullptr, 2, barriers.data());

				VkImageBlit info_blit = {};

				info_blit.dstOffsets[0] = VkOffset3D{0, 0, 0};
				info_blit.dstOffsets[1] =
					VkOffset3D{this->m_p_render_device->GetWidth(),
						this->m_p_render_device->GetHeight(), 1};
				info_blit.srcOffsets[0] = VkOffset3D{0, 0, 0};
				info_blit.srcOffsets[1] =
					VkOffset3D{.x = this->m_p_render_device->GetWidth(),
						.y = this->m_p_render_device->GetHeight(),
						.z = 1};
				info_blit.srcSubresource = {
					.aspectMask =
						VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1};
				info_blit.dstSubresource = {
					.aspectMask =
						VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1};

				vkCmdBlitImage(p_command_buffer,
					this->m_p_backbuffer_image_handle,
					VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					this->m_p_manager_resource->GetSwapchainImage(
						this->m_p_swapchain->GetAcquiredImageIndex()),
					VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
					&info_blit, VkFilter::VK_FILTER_LINEAR);

				VkImageMemoryBarrier info_memory_barrier_image_swapchain;

				info_memory_barrier_image_swapchain.sType =
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				info_memory_barrier_image_swapchain.pNext = nullptr;
				info_memory_barrier_image_swapchain.oldLayout =
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				info_memory_barrier_image_swapchain.newLayout =
					VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				info_memory_barrier_image_swapchain.srcAccessMask =
					VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
				info_memory_barrier_image_swapchain.dstAccessMask =
					VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
				info_memory_barrier_image_swapchain.image =
					this->m_p_manager_resource->GetSwapchainImage(
						this->m_p_swapchain->GetAcquiredImageIndex());
				info_memory_barrier_image_swapchain.dstQueueFamilyIndex =
					VK_QUEUE_FAMILY_IGNORED;
				info_memory_barrier_image_swapchain.srcQueueFamilyIndex =
					VK_QUEUE_FAMILY_IGNORED;
				info_memory_barrier_image_swapchain.subresourceRange
					.aspectMask =
					VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
				info_memory_barrier_image_swapchain.subresourceRange
					.baseArrayLayer = 0;
				info_memory_barrier_image_swapchain.subresourceRange
					.baseMipLevel = 0;
				info_memory_barrier_image_swapchain.subresourceRange
					.layerCount = 1;
				info_memory_barrier_image_swapchain.subresourceRange
					.levelCount = 1;

				vkCmdPipelineBarrier(p_command_buffer,
					VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
					VkPipelineStageFlagBits::
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					0, 0, nullptr, 0, nullptr, 1,
					&info_memory_barrier_image_swapchain);
			}
		} // namespace vk
	}     // namespace Render
} // namespace zircon