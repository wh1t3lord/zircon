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

#define ZIRCON_DEF_RENDERER_GLES3_MAX_RENDER_GRAPH_COUNT 2

struct zircon_render_graph_simplified_info_t
{
	bool in_use{};
	kotek::uint8_t session_id{};
	kotek::vector_t<kotek::core::ktkISDKUIElement*> ui_elements;
	kotek::render::gl::ktkRenderGraphSimplified graph;
};

class zircon_renderer_gles3 : public kotek::core::ktkIRenderer
{
public:
	zircon_renderer_gles3(kotek::core::ktkMainManager* p_main_manager);
	~zircon_renderer_gles3(void);

	void Initialize(kotek::core::ktkWindowConsole* p_console,
		kotek::core::ktkConsole* p_con);

	void Shutdown(void) override;

	void draw() override;

	void Resize() override;

	const char* Get_Name(void) const noexcept override;

	const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>&
	get_ui_imgui_elements(kotek::uint8_t render_graph_id) const;

	bool is_render_graph_presented(kotek::uint8_t render_graph_id) const;

	kotek::uint8_t create_render_graph(kotek::uint8_t session_id,
		kotek::static_vector_t<
			kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*,
			KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>& passes,
		const kotek::vector_t<kotek::core::ktkISDKUIElement*>& ui_elements);

	void set_current_render_graph(kotek::uint8_t render_graph_id);

private:
	void initialize_extensions(kotek::core::ktkConsole* p_console);

	void Begin() noexcept;
	void End() noexcept;

	void destroy_render_graphs(void) noexcept;
	void create_render_graph(
		const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>&
			imgui_elements,
		kotek ::core::ktkWindowConsole* p_console) noexcept;

	void Add_PassesEditor(
		const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>&
			imgui_elements) noexcept;
	void Add_PassesGame(kotek::core::ktkWindowConsole* p_console) noexcept;

private:
	kotek::uint8_t m_previous_render_graph_id;
	kotek::core::ktkMainManager* m_p_main_manager;
	kotek::render::gl::ktkRenderDevice* m_p_render_device;
	kotek::render::gl::ktkRenderResourceManager* m_p_render_resource_manager;
	kotek::render::gl::ktkRenderGraphSimplified* m_p_current_render_graph;
	kotek::vector_t<zircon_render_graph_simplified_info_t,
		ZIRCON_DEF_RENDERER_GLES3_MAX_RENDER_GRAPH_COUNT>
		m_render_graphs;
};