#include "zircon_render_graph_pass_mesh.h"
#include "zircon_render_resource_manager.h"

namespace zircon
{
	namespace Render
	{
		namespace vk
		{
			zircon_RenderGraphPassMesh::zircon_RenderGraphPassMesh(
				Kotek::Core::ktkMainManager* p_main_manager,
				zircon_RenderResourceManager* p_game_render_resource_manager) :
				m_p_main_manager(p_main_manager),
				m_p_render_resource_manager(p_game_render_resource_manager)
			{
			}

			zircon_RenderGraphPassMesh::~zircon_RenderGraphPassMesh(void) {}

			void zircon_RenderGraphPassMesh::OnSetupInput(
				Kotek::Render::vk::ktkRenderGraphStorageInput& storage,
				Kotek::Render::vk::ktkRenderDevice* p_device,
				Kotek::Core::ktkIFileSystem* p_file_system)
			{
			}

			void zircon_RenderGraphPassMesh::OnSetupOutput(
				Kotek::Render::vk::ktkRenderGraphStorageOutput& storage,
				Kotek::Render::vk::ktkRenderDevice* p_device)
			{
			}

			void zircon_RenderGraphPassMesh::OnCreatedResources(void) {}

			void zircon_RenderGraphPassMesh::OnUpdate() 
			{
			}

			void zircon_RenderGraphPassMesh::OnRender(
				const Kotek::Render::vk::ktkRenderGraphNode& node,
				VkCommandBuffer p_command_buffer)
			{
			}
		} // namespace vk
	}     // namespace Render
} // namespace zircon