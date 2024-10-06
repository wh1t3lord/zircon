#pragma once

#include <kotek.render.gl/include/kotek_render_graph_simplified.h>
#include <kotek.render.gl/include/kotek_render_graph_simplified_resource_manager.h>

namespace Kotek
{
	namespace Core
	{
		class ktkMainManager;
	}

	namespace Render
	{
		namespace gl
		{
			class ktkRenderDevice;
			class ktkRenderResourceManager;
		}
	} // namespace Render
} // namespace Kotek

class zircon_renderer_gles3 : public Kotek::Core::ktkIRenderer
{
public:
	zircon_renderer_gles3(Kotek::Core::ktkMainManager* p_main_manager);
	~zircon_renderer_gles3(void);

	void Initialize(
		const Kotek::ktk::vector<Kotek::Core::kotek_sdk_ui_element*>&
			ui_elements);

	void Shutdown(void) override;

	void draw() override;

	void Resize() override;

	Kotek::ktk::cstring GetName(void) const noexcept override;

private:
	void Begin() noexcept;
	void End() noexcept;

	void Destroy_RenderGraph(void) noexcept;
	void Create_RenderGraph(
		const Kotek::ktk::vector<Kotek::Core::kotek_sdk_ui_element*>&
			imgui_elements) noexcept;

	void Destroy_ImGuiUIElements(void) noexcept;

private:
	Kotek::Core::ktkMainManager* m_p_main_manager;
	Kotek::Render::gl::ktkRenderDevice* m_p_render_device;
	Kotek::Render::gl::ktkRenderResourceManager* m_p_render_resource_manager;
	Kotek::ktk::vector<Kotek::Core::kotek_sdk_ui_element*> m_imgui_ui_elements;
	Kotek::Render::gl::ktkRenderGraphSimplified m_render_graph_simplified;
	Kotek::Render::gl::ktkRenderGraphSimplifiedResourceManager
		m_render_graph_simplified_resource_manager;
};