#pragma once

#include <kotek.render.gl/include/kotek_render_graph_simplified_render_pass.h>

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_RENDER
KOTEK_BEGIN_NAMESPACE_RENDER_GL

class ktkRenderResourceManager;

KOTEK_END_NAMESPACE_RENDER_GL
KOTEK_END_NAMESPACE_RENDER
KOTEK_END_NAMESPACE_KOTEK

class zircon_factory_game;

class zircon_render_graph_pass_model_static_gles3
	: public kotek::render::gl::ktkRenderGraphSimplifiedRenderPass
{
public:
	zircon_render_graph_pass_model_static_gles3(
		const kotek::static_u8string_view_t& render_pass_name);
	zircon_render_graph_pass_model_static_gles3();
	~zircon_render_graph_pass_model_static_gles3();

	void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource) override;
	void OnUpdate(const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass) override;

	void OnRender(const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass) override;

private:
	void render_sdk_camera(
		const kn_kotek::kn_render::kn_render_gl::ktkBufferModule&
			buffer_camera);
	void render_instances(
		const kn_kotek::kn_render::kn_render_gl::ktkBufferModule&
			buffer_instances,
		GLuint program_id);

private:
	zircon_factory_game* m_p_factory;
	kotek::render::gl::ktkRenderResourceManager* m_p_manager_render_resource;
};