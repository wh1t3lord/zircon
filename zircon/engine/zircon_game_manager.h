#pragma once

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

enum eZirconGameFeatures;
enum eZirconSDKFeatures;

class zircon_editor_command_history;
class zircon_renderer_gles3;
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

class zircon_manager_game : public kotek::core::ktkIGameManager
{
public:
	zircon_manager_game(void);
	~zircon_manager_game(void);

	void Initialize(kotek::core::ktkMainManager* p_main_manager) override;
	void Shutdown(kotek::core::ktkMainManager* p_main_manager) override;

	void* GetWindowHandle(void) const noexcept override;

#ifdef KOTEK_USE_SDK
	void SetSDKRenderWindow(sdk::ui::zircon_RenderWindow* p_window) noexcept;
	void SetSDKMainWindow(sdk::ui::zircon_frame* p_window) noexcept;
	sdk::ui::zircon_frame* GetMainWindow(void) const noexcept;
#endif

	zircon_editor_ui_state_interface* get_sdk_ui(void) const noexcept;

	int GetWindowWidth(void) const noexcept override;
	int GetWindowHeight(void) const noexcept override;
	kotek::core::ktkProfiler* GetProfiler(void) const noexcept override;
	kotek::core::ktkConsole* GetConsole(void) const noexcept override;

	kotek::core::ktkIRenderer* GetRenderer(void) const noexcept override;
	void* GetRenderResourceManager(void) const noexcept override;
	void* CreateSurface(kotek::core::ktkMainManager* p_main_manager,
		void* p_instance, const void* p_callbacks) override;

	void Update(void) noexcept;

	void UpdateAllSystems(void) noexcept;

	zircon_world_manager* GetSceneManager(void) const noexcept;
	zircon_factory* get_factory_game(void) const noexcept;
	kotek::core::ktkWindow* GetWindow(void) const noexcept;

	void Serialize(void) noexcept;
	void Deserialize(void) noexcept;

	kotek::core::ktkMainManager* GetMainManager(void) const noexcept;

	zircon_editor_command_history* GetCommandHistoryManager(void) const noexcept;

	zircon_interface_session* GetSession_Editor(void) const noexcept;
	zircon_interface_session* GetSession_Game(void) const noexcept;

	zircon_config* get_config() const noexcept;

private:
	entt::entity Initialize_Actor(void) noexcept;

	void Initialize_Renderer(void) noexcept;
	void Destroy_Renderer(void) noexcept;

	void Initialize_Factory(void) noexcept;
	void Destroy_Factory(void) noexcept;

	void Initialize_SceneManager(void) noexcept;
	void Destroy_SceneManager(void) noexcept;

	void Initialize_ResourceManager(void) noexcept;
	void Destroy_ResourceManager(void) noexcept;

	void Initialize_Console(void) noexcept;

	void initialize_input(void) noexcept;

	void RegisterConsole_Commands(void) noexcept;
	void RegisterConsole_Commands_SDK(void) noexcept;

	void Destroy_Console(void) noexcept;

	void Initialize_SDKUIManager(zircon_factory* p_factory) noexcept;
	void Destroy_SDKUIManager(void) noexcept;

	void Initialize_Session(void) noexcept;
	void Destroy_Session(void) noexcept;

	void Initialize_HistoryCommandManager(void) noexcept;
	void Destroy_HistoryCommandManager(void) noexcept;

	void Initialize_UI(void) noexcept;
	void Destroy_UI(void) noexcept;

	void initialize_config(void) noexcept;
	void destroy_config(void) noexcept;

	void UpdateInput(void) noexcept;
	void UpdateCamera(void) noexcept;

private:
	bool m_is_use_sdk;
	bool m_is_use_sdk_imgui;
	kotek::core::ktkProfiler* m_p_profiler;
	kotek::core::ktkConsole* m_p_console;
	kotek::core::ktkMainManager* m_p_main_manager;
	kotek::core::ktkIRenderer* m_p_current_renderer;
	kotek::core::ktkWindowConsole* m_p_window_console;

	zircon_renderer_gles3* m_p_renderer_gles3;

#ifdef KOTEK_USE_RENDER_VULKAN
	Render::vk::zircon_Renderer* m_p_renderer_vk;
#endif

#ifdef KOTEK_USE_SDK
	void* m_p_window_handle;
	sdk::ui::zircon_RenderWindow* m_p_sdk_render_window;
	sdk::ui::zircon_frame* m_p_sdk_main_window;
#endif

	zircon_factory* m_p_factory;
	zircon_world_manager* m_p_scene_manager;
	zircon_resource_manager* m_p_resource_manager;
	zircon_config* m_p_config;
	zircon_session_game_manager* m_p_session_game_manager;
#ifdef KOTEK_USE_SDK_IMGUI
	zircon_session_editor_manager* m_p_session_editor_manager;
#endif
};
