#include "zircon_render_graph_pass_editor_model_static.h"
#include <kotek.render.bgfx/include/kotek_render_resource_manager.h>
#include <kotek.render.bgfx/include/kotek_render_geometry_manager.h>
#include <kotek.render.bgfx/include/kotek_render_shader_manager.h>

#include "../../../../ecs/zircon_factory.h"
#include "../../../../world/zircon_world.h"
#include "../../../../editor/session/zircon_session_editor.h"
#include "../../../../editor/session/zircon_session_editor_manager.h"

namespace no_streaming
{
	zircon_render_graph_pass_editor_model_static_bgfx::
		zircon_render_graph_pass_editor_model_static_bgfx(
			const kotek::static_u8string_view_t& render_pass_name) :
		zircon_render_graph_pass_editor(render_pass_name),
		m_p_manager_render_resource{}
	{
	}

	zircon_render_graph_pass_editor_model_static_bgfx::
		zircon_render_graph_pass_editor_model_static_bgfx() :
		zircon_render_graph_pass_editor(), m_p_manager_render_resource{}
	{
	}

	zircon_render_graph_pass_editor_model_static_bgfx::
		~zircon_render_graph_pass_editor_model_static_bgfx()
	{
	}

	void zircon_render_graph_pass_editor_model_static_bgfx::OnCreateResources(
		kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
	}

	void zircon_render_graph_pass_editor_model_static_bgfx::OnDestroyResources()
	{
	}

	void zircon_render_graph_pass_editor_model_static_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass)
	{
	}

	void zircon_render_graph_pass_editor_model_static_bgfx::OnRender(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass)
	{
	}

} // namespace no_streaming