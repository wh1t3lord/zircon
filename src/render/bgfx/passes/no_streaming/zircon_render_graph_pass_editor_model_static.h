#pragma once

#include "../../zircon_render_graph_pass_editor.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_RENDER
KOTEK_BEGIN_NAMESPACE_RENDER_BGFX

class ktkRenderResourceManager;
class ktkRenderGeometryManager;
class ktkRenderShaderManager;

KOTEK_END_NAMESPACE_RENDER_BGFX
KOTEK_END_NAMESPACE_RENDER
KOTEK_END_NAMESPACE_KOTEK

class zircon_factory;

namespace no_streaming
{
	class zircon_render_graph_pass_editor_model_static_bgfx
		: public zircon_render_graph_pass_editor_bgfx
	{
	public:
		zircon_render_graph_pass_editor_model_static_bgfx();
		~zircon_render_graph_pass_editor_model_static_bgfx();

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;
		void OnDestroyResources() override;
		void OnUpdate(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;

		void OnRender(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;

	private:
 

	private:
		kotek::render::bgfx::ktkRenderResourceManager*
			m_p_manager_render_resource;
		kotek::render::bgfx::ktkRenderGeometryManager*
			m_p_manager_render_geometry;
		kotek::render::bgfx::ktkRenderShaderManager* m_p_manager_render_shader;
	};
}