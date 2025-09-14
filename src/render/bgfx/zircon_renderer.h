#pragma once

#include <kotek.render.bgfx/include/kotek_render_graph_simplified.h>

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE

KOTEK_BEGIN_NAMESPACE_RENDER
KOTEK_BEGIN_NAMESPACE_RENDER_BGFX
class ktkRenderDevice;
class ktkRenderResourceManager;
KOTEK_END_NAMESPACE_RENDER_BGFX
KOTEK_END_NAMESPACE_RENDER
KOTEK_END_NAMESPACE_KOTEK

class zircon_session_editor_manager;
class zircon_session_game_manager;

#define ZIRCON_DEF_RENDERER_BGFX_MAX_RENDER_GRAPH_COUNT 2

struct zircon_render_graph_simplified_info_t
{
	bool in_use{};
	bool is_game_session{};
	kotek::uint8_t session_id{};
	kotek::render::bgfx::ktkRenderGraphSimplified graph;
};

class zircon_renderer_bgfx : public kotek::core::ktkIRenderer
{
public:
	zircon_renderer_bgfx(kotek::core::ktkMainManager* p_main_manager);
	~zircon_renderer_bgfx(void);

	void Initialize(kotek::core::ktkWindowConsole* p_console,
		kotek::core::ktkConsole* p_con,
		zircon_session_game_manager* p_session_game_manager,
		zircon_session_editor_manager* p_session_editor_manager);

	void Shutdown(void) override;

	void draw() override;

	void Resize() override;

	const char* Get_Name(void) const noexcept override;

	bool is_render_graph_presented(kotek::uint8_t render_graph_id) const;
	bool is_render_graph_initialized(kotek::uint8_t render_graph_id) const;
	bool is_render_graph_for_session_editor(
		kotek::uint8_t render_graph_id) const;
	bool is_render_graph_for_session_game(kotek::uint8_t render_graph_id) const;

	kotek::uint8_t create_render_graph(kotek::uint8_t session_id,
		bool is_game_session,
		kotek::static_vector_t<
			kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*,
			KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>&
			passes);

	void initialize_render_graph(kotek::uint8_t render_graph_id,
		kotek::core::ktkMainManager* p_main_manager,
		kotek::core::ktkIRenderResourceManager* p_render_resource_manager,
		zircon_session_game_manager* p_manager_game_session,
		zircon_session_editor_manager* p_manager_editor_session);

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
	zircon_session_game_manager* m_p_session_game_manager;
	zircon_session_editor_manager* m_p_session_editor_manager;
	kotek::render::bgfx::ktkRenderDevice* m_p_render_device;
	kotek::render::bgfx::ktkRenderResourceManager* m_p_render_resource_manager;
	kotek::render::bgfx::ktkRenderGraphSimplified* m_p_current_render_graph;
	kotek::vector_t<zircon_render_graph_simplified_info_t,
		ZIRCON_DEF_RENDERER_BGFX_MAX_RENDER_GRAPH_COUNT>
		m_render_graphs;
};