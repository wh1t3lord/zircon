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

class zircon_render_graph_pass_imgui_gles3
	: public Kotek::Render::gl::ktkRenderGraphSimplifiedRenderPass
{
public:
	zircon_render_graph_pass_imgui_gles3(
		Kotek::Core::ktkMainManager* p_main_manager,
		const Kotek::ktk::vector<Kotek::Core::ktkISDKUIElement*>& elements);
	~zircon_render_graph_pass_imgui_gles3(void);

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
	Kotek::Core::ktkIImguiWrapper* m_p_imgui_wrapper;
	const Kotek::ktk::vector<Kotek::Core::ktkISDKUIElement*>*
		m_p_imgui_ui_elements;
};
