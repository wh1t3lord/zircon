#pragma once

#include "../../zircon_render_graph_pass_editor.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_RENDER
KOTEK_BEGIN_NAMESPACE_RENDER_GL

class ktkRenderResourceManager;
class ktkRenderGeometryManager;
class ktkRenderShaderManager;

KOTEK_END_NAMESPACE_RENDER_GL
KOTEK_END_NAMESPACE_RENDER
KOTEK_END_NAMESPACE_KOTEK

class zircon_factory;

namespace no_streaming
{
	class zircon_render_graph_pass_editor_model_static_gles3
		: public zircon_render_graph_pass_editor
	{
	public:
		zircon_render_graph_pass_editor_model_static_gles3();
		~zircon_render_graph_pass_editor_model_static_gles3();

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;
		void OnDestroyResources() override;
		void OnUpdate(
			const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass) override;

		void OnRender(
			const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass) override;

		void on_transform_component_created(
			entt::registry& registry, entt::entity id);
		void on_transform_component_updated(
			entt::registry& registry, entt::entity id);

		// when geometry exists and then we add animation component we should
		// remove from this pass our "model"/instance and define as dynamic
		// geometry
		void on_animation_component_created(
			entt::registry& registry, entt::entity id);
		// when animation component removed from entity we should remove from
		// dynamic geometry pass and add to this static geometry pass
		void on_animation_component_removed(
			entt::registry& registry, entt::entity id);

	private:
		void update_sdk_camera();
		void update_instances();
		void render_instances();

	private:
		kotek::render::gl::ktkRenderResourceManager*
			m_p_manager_render_resource;
		kotek::render::gl::ktkRenderGeometryManager*
			m_p_manager_render_geometry;
		kotek::render::gl::ktkRenderShaderManager* m_p_manager_render_shader;

		GLuint m_shaders_geometry_color_only;
		kotek::render::gl::ktkBufferModule m_shader_buffer_camera;
		kotek::render::gl::ktkBufferModule m_shader_buffer_instancing_data;
	};
}