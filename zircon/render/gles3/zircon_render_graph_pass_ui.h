#pragma once

#include <kotek.render.gl/include/kotek_render_graph_simplified_render_pass.h>

namespace Kotek
{
	namespace Render
	{
		namespace gl
		{
			class ktkRenderGraphSimplifiedNode;
		}
	} // namespace Render
} // namespace Kotek

class zircon_render_graph_pass_ui_gles3
	: public Kotek::Render::gl::ktkRenderGraphSimplifiedRenderPass
{
public:
	zircon_render_graph_pass_ui_gles3(
		Kotek::Core::ktkMainManager* p_main_manager);
	~zircon_render_graph_pass_ui_gles3();

	void OnSetupInput(
		Kotek::Render::gl::ktkRenderGraphSimplifiedStorageInput& storage,
		Kotek::Core::ktkIRenderDevice* p_device,
		Kotek::Core::ktkFileSystem* p_file_system) override;

	void OnSetupOutput(
		Kotek::Render::gl::ktkRenderGraphSimplifiedStorageOutput& storage,
		Kotek::Core::ktkIRenderDevice* p_device) override;

	void OnCreatedResources(void) override;

	void OnUpdate() override;

	void OnRender(
		const Kotek::Render::gl::ktkRenderGraphSimplifiedNode& node) override;

private:
	Kotek::Core::ktkMainManager* m_p_main_manager;
	Kotek::Core::ktkIGameUIManager* m_p_ui_manager;
};
