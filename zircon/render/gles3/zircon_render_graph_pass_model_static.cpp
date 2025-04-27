#include "zircon_render_graph_pass_model_static.h"
#include <kotek.render.gl/include/kotek_render_resource_manager.h>
#include <kotek.render.gl/include/kotek_render_geometry_manager.h>
#include <kotek.render.gl/include/kotek_render_shader_manager.h>

#include "../../ecs/zircon_factory.h"

zircon_render_graph_pass_model_static_gles3::
	zircon_render_graph_pass_model_static_gles3(
		const kotek::static_u8string_view_t& render_pass_name) :
	zircon_render_graph_pass(render_pass_name.data()), m_p_factory{},
	m_p_manager_render_resource{}
{
}

zircon_render_graph_pass_model_static_gles3::
	zircon_render_graph_pass_model_static_gles3() :
	zircon_render_graph_pass(), m_p_factory{}, m_p_manager_render_resource{}
{
}

zircon_render_graph_pass_model_static_gles3::
	~zircon_render_graph_pass_model_static_gles3()
{
}

void zircon_render_graph_pass_model_static_gles3::OnCreateResources(
	kotek::core::ktkMainManager* p_manager_main,
	kotek::core::ktkIRenderResourceManager* p_manager_resource)
{
}

void zircon_render_graph_pass_model_static_gles3::OnDestroyResources()
{
	KOTEK_ASSERT(
		this->m_p_manager_render_shader, "you must register shader manager!");
}

void zircon_render_graph_pass_model_static_gles3::OnUpdate(
	const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
		p_previous_pass)
{
}

void zircon_render_graph_pass_model_static_gles3::OnRender(
	const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
		p_previous_pass)
{
}