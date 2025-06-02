#include "zircon_render_resource_manager.h"
#include <kotek.render.vk/include/kotek_render_device.h>

namespace zircon
{
	namespace Render
	{
		namespace vk
		{
			zircon_RenderResourceManager::zircon_RenderResourceManager(
				Kotek::Core::ktkMainManager* p_main_manager) :
				m_p_main_manager(p_main_manager)
			{
			}

			zircon_RenderResourceManager::~zircon_RenderResourceManager(void) {}

			void zircon_RenderResourceManager::Initialize(void)
			{
				auto* p_render_device =
					static_cast<Kotek::Render::vk::ktkRenderDevice*>(
						this->m_p_main_manager->getRenderDevice());

				// @ the size is depended on ComponentGeometry
				this->m_static_linear_buffer.Initialize(p_render_device,
					32 * Kotek::ktk::kMemoryConvertValue_Megabytes, 1000,
					"static buffer for dynamic geometry (linear allocator)");
			}

			void zircon_RenderResourceManager::Shutdown(void)
			{
				auto* p_render_device =
					static_cast<Kotek::Render::vk::ktkRenderDevice*>(
						this->m_p_main_manager->getRenderDevice());
				this->m_static_linear_buffer.Shutdown(p_render_device);
			}

			const Kotek::Core::ktkComponentAllocator<
				Game::ecs::backend::zircon_ComponentGeometry, 1000>&
			zircon_RenderResourceManager::GetStorageMeshes(void) const noexcept
			{
				return this->m_cache_meshes;
			}

			void zircon_RenderResourceManager::AddComponentMesh(
				Kotek::ktk::entity_t entity_id)
			{
				this->m_cache_meshes.Create(entity_id);
			}

			void zircon_RenderResourceManager::RemoveComponentMesh(
				Kotek::ktk::entity_t entity_id)
			{
				this->m_cache_meshes.Remove(entity_id);
			}
		} // namespace vk
	}     // namespace Render
} // namespace zircon