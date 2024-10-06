#pragma once

#include "../ecs/zircon_ecs.h"
#include <kotek.render.vk/include/kotek_render_static_buffer_pool_with_linear_allocator.h>

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
			// TODO: только один класс ресурс манагера должен быть, а так будет здесь интерфейс
			class zircon_RenderResourceManager
			{
			public:
				zircon_RenderResourceManager(
					Kotek::Core::ktkMainManager* p_main_manager);
				~zircon_RenderResourceManager(void);

				void Initialize(void);
				void Shutdown(void);

				const Kotek::Core::ktkComponentAllocator<
					Game::ecs::backend::zircon_ComponentGeometry, 1000>&
				GetStorageMeshes(void) const noexcept;

				void AddComponentMesh(Kotek::ktk::entity_t entity_id);
				void RemoveComponentMesh(Kotek::ktk::entity_t entity_id);

			private:
				Kotek::Core::ktkMainManager* m_p_main_manager;
				Kotek::Render::vk::ktkRenderStaticBufferPool_LinearAllocator
					m_static_linear_buffer;
				Kotek::Core::ktkComponentAllocator<
					Game::ecs::backend::zircon_ComponentGeometry, 1000>
					m_cache_meshes;
			};
		} // namespace vk
	}     // namespace Render
} // namespace zircon