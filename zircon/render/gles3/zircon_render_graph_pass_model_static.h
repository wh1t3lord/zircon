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
	: public kn_kotek::kn_render::kn_render_gl::
		  ktkRenderGraphSimplifiedRenderPass
{
public:
	zircon_render_graph_pass_model_static_gles3();
	~zircon_render_graph_pass_model_static_gles3();

	void OnSetupInput(
		kn_kotek::kn_render::kn_render_gl::ktkRenderGraphSimplifiedStorageInput&
			storage,
		kn_kotek::kn_core::ktkIRenderDevice* p_device,
		kn_kotek::kn_core::ktkFileSystem* p_file_system) override;

	void OnSetupOutput(kn_kotek::kn_render::kn_render_gl::
						   ktkRenderGraphSimplifiedStorageOutput& storage,
		kn_kotek::kn_core::ktkIRenderDevice* p_device) override;

	void OnCreatedResources(void) override;

	void OnUpdate() override;

	void OnRender(
		const kn_kotek::kn_render::kn_render_gl::ktkRenderGraphSimplifiedNode&
			node) override;

private:
	void render_sdk_camera(const kn_kotek::kn_render::kn_render_gl::ktkBufferModule& buffer_camera);
	void render_instances(
		const kn_kotek::kn_render::kn_render_gl::ktkBufferModule&
			buffer_instances, GLuint program_id);

private:
	zircon_factory_game* m_p_factory;
	kn_kotek::kn_render::kn_render_gl::ktkRenderResourceManager*
		m_p_manager_render_resource;
};