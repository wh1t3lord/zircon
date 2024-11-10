#pragma once

#include <kotek.render.gl/include/kotek_render_graph_simplified.h>

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE

KOTEK_BEGIN_NAMESPACE_RENDER
KOTEK_BEGIN_NAMESPACE_RENDER_GL
class ktkRenderDevice;
class ktkRenderResourceManager;
KOTEK_END_NAMESPACE_RENDER_GL
KOTEK_END_NAMESPACE_RENDER
KOTEK_END_NAMESPACE_KOTEK

class zircon_renderer_gles3 : public kotek::core::ktkIRenderer
{
public:
	zircon_renderer_gles3(kotek::core::ktkMainManager* p_main_manager);
	~zircon_renderer_gles3(void);

	void Initialize(
		const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>& ui_elements);

	void Shutdown(void) override;

	void draw() override;

	void Resize() override;

	const char* Get_Name(void) const noexcept override;

private:
	void Begin() noexcept;
	void End() noexcept;

	void Destroy_RenderGraph(void) noexcept;
	void Create_RenderGraph(
		const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>&
			imgui_elements) noexcept;

	void Destroy_ImGuiUIElements(void) noexcept;

private:
	kotek::core::ktkMainManager* m_p_main_manager;
	kotek::render::gl::ktkRenderDevice* m_p_render_device;
	kotek::render::gl::ktkRenderResourceManager* m_p_render_resource_manager;
	kotek::ktk::vector<kotek::core::ktkISDKUIElement*> m_imgui_ui_elements;
	kotek::render::gl::ktkRenderGraphSimplified m_render_graph_simplified;
};