#pragma once

#include "../core/zircon_defs.h"
#include "../core/zircon_config.h"

#ifdef KOTEK_USE_SDK
namespace zircon
{
	namespace sdk
	{
		namespace ui
		{
			class zircon_RenderWindow;
			class zircon_frame;
		} // namespace ui
	} // namespace sdk
} // namespace zircon
#endif

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
class ktkWindow;
class ktkWindowConsole;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

enum class eZirconGameFeatures : kotek::ktk::uint16_t;
enum class eZirconSDKFeatures : kotek::ktk::uint16_t;

class zircon_editor_command_history;
class zircon_resource_manager;
class zircon_config;
class zircon_factory;
class zircon_world_manager;
class zircon_interface_session;
class zircon_editor_ui_state_interface;
class zircon_editor_ui_state;
class zircon_session_game;
class zircon_session_game_manager;
class zircon_session_editor;
class zircon_session_editor_manager;
class zircon_renderer_bgfx;
class zircon_renderer_nri;

#ifdef KOTEK_DEBUG
	#ifdef KOTEK_USE_TESTS_RUNTIME
void zircon_register_unit_tests_game();

		#ifdef KOTEK_USE_SDK_IMGUI
void zircon_register_unit_tests_editor();
		#endif
	#endif
#endif

class zircon_game_manager : public kotek::core::ktkIGameManager
{
	union renderers_t
	{
#ifdef KOTEK_USE_BGFX
		zircon_renderer_bgfx* p_bgfx;
#endif

#ifdef KOTEK_USE_RENDER_NRI
		zircon_renderer_nri* p_nri;
#endif
	};

public:
	zircon_game_manager(void);
	~zircon_game_manager(void);

	void Initialize(kotek::core::ktkMainManager* p_main_manager
	) override;
	void Shutdown(kotek::core::ktkMainManager* p_main_manager
	) override;

	void* GetWindowHandle(void) const noexcept override;

#ifdef KOTEK_USE_SDK
	void
	SetSDKRenderWindow(sdk::ui::zircon_RenderWindow* p_window
	) noexcept;
	void SetSDKMainWindow(sdk::ui::zircon_frame* p_window
	) noexcept;
	sdk::ui::zircon_frame* GetMainWindow(void) const noexcept;
#endif

	int GetWindowWidth(void) const noexcept override;
	int GetWindowHeight(void) const noexcept override;
	kotek::core::ktkProfiler* GetProfiler(void
	) const noexcept override;
	kotek::core::ktkIConsole* GetConsole(void
	) const noexcept override;

	kotek::core::ktkIRenderer* GetRenderer(void
	) const noexcept override;
	void* GetRenderResourceManager(void
	) const noexcept override;
	void* CreateSurface(
		kotek::core::ktkMainManager* p_main_manager,
		void* p_instance,
		const void* p_callbacks
	) override;

	void Update(void) noexcept;

	void UpdateAllSystems(void) noexcept;

	zircon_world_manager* get_world_manager(void
	) const noexcept;
	zircon_session_editor_manager*
	get_session_editor_manager(void) const noexcept;
	zircon_session_game_manager* get_session_game_manager(void
	) const noexcept;

	zircon_session_editor*
	get_session_editor(kotek::uint8_t session_id
	) const noexcept;
	zircon_session_game*
	get_session_game(kotek::uint8_t session_id) const noexcept;

	kotek::core::ktkWindow* GetWindow(void) const noexcept;

	void Serialize(void) noexcept;
	void Deserialize(void) noexcept;

	kotek::core::ktkMainManager* GetMainManager(void
	) const noexcept;

	zircon_config* get_config() const noexcept;

	/// @brief \~english task Z3 P2h: the game pass set this boot
	/// RESOLVED (the loaded scene's scene.json render_passes -> config
	/// render_passes_game -> built-in default) and created the game
	/// render graph with; empty until the game session's graph exists.
	/// The Render Passes window compares its "modified" marker against
	/// this (not the raw config default — a level override must not
	/// read as user-modified) and refreshes it on Save; mutable
	/// precisely for that refresh. The pointed-to member never moves
	/// (the game manager is a stable global), so the pointer stays
	/// valid for the whole UI lifetime
	kotek::static_cstring_t<ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>*
	get_render_passes_game_resolved_baseline(void) noexcept;

	void initialize_render_graph(
		kotek::uint8_t render_graph_id,
		kotek::core::ktkMainManager* p_main_manager,
		kotek::core::ktkIRenderResourceManager*
			p_render_resource_manager
	) noexcept;

	bool
	is_render_graph_initialized(kotek::uint8_t render_graph_id);

private:
	void initialize_factory() noexcept;
	void destroy_factory() noexcept;

	void Initialize_Renderer(void) noexcept;
	void Destroy_Renderer(void) noexcept;

	void Initialize_ResourceManager(void) noexcept;
	void Destroy_ResourceManager(void) noexcept;

	void Initialize_Console(void) noexcept;

	void initialize_input(void) noexcept;

	void RegisterConsole_Commands(void) noexcept;
	void RegisterConsole_Commands_SDK(void) noexcept;

	void Destroy_Console(void) noexcept;

	void Initialize_UI(void) noexcept;
	void Destroy_UI(void) noexcept;

	void initialize_config(void) noexcept;
	void destroy_config(void) noexcept;

	void UpdateInput(void) noexcept;
	void UpdateCamera(void) noexcept;

	const zircon_imgui_elements_t&
	get_ui_imgui_elements();

	/// @brief \~english task Z3 P2h: the streaming folder of the
	/// currently loaded scene (the current editor session's command
	/// history path, data_user/sdk/scenes/<name>/); nullptr when no
	/// scene is loaded (no editor session — a non-SDK boot)
	const char* get_active_scene_streaming_folder_path(void
	) const noexcept;

	/// @brief \~english task Z3 P2h: persists the ACTIVE game pass set
	/// (the game render graph slot's live name list; the config value
	/// when no game graph exists — or when the live set is degenerate:
	/// empty, i.e. every pass removed, which can never be a meaningful
	/// level override) into the loaded scene's scene.json — called
	/// from Serialize (module save at shutdown, when the renderer and
	/// the sessions are still alive). A no-op when no scene is loaded
	void write_active_game_render_pass_set_to_scene(void) noexcept;

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG
	void run_unit_tests();
	#endif
#endif

private:
	bool m_is_use_sdk;
	bool m_is_use_sdk_imgui;
	kotek::uint8_t m_world_id;
	kotek::core::ktkProfiler* m_p_profiler;
	kotek::core::ktkConsole* m_p_console;
	/// @brief \~english the console instance that kotek.core.console
	/// registered into the main manager (owned by that module, it is
	/// only saved and restored around our own instance so ownership
	/// and destruction stay in the constructing module)
	kotek::core::ktkIConsole* m_p_console_exe_owned{};
	kotek::core::ktkMainManager* m_p_main_manager;
	kotek::core::ktkIRenderer* m_p_current_renderer;
	kotek::core::ktkWindowConsole* m_p_window_console;
	zircon_interface_session* m_p_current_session;
	renderers_t m_renderers;
	zircon_imgui_elements_t empty_ui_elements;
#ifdef KOTEK_USE_SDK
	void* m_p_window_handle;
	sdk::ui::zircon_RenderWindow* m_p_sdk_render_window;
	sdk::ui::zircon_frame* m_p_sdk_main_window;
#endif

	// zircon_factory* m_p_factory;
	zircon_world_manager* m_p_world_manager;
	zircon_resource_manager* m_p_resource_manager;
	zircon_config* m_p_config;
	zircon_session_game_manager* m_p_session_game_manager;
	zircon_factory* m_p_factory;
	/// @brief \~english the game pass set the running game session was
	/// created with (the resolved scene->config->built-in winner, task
	/// Z3 P2h) — the Render Passes window's dirty-check baseline; empty
	/// until the game render graph is created
	kotek::static_cstring_t<ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>
		m_render_passes_game_resolved_baseline;
#ifdef KOTEK_USE_SDK_IMGUI
	zircon_session_editor_manager* m_p_session_editor_manager;
#endif
};
