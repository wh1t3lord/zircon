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
	}     // namespace sdk
} // namespace zircon
#endif

namespace Kotek
{
	namespace Core
	{
		class ktkMainManager;
		class ktkWindow;
	} // namespace Core
} // namespace Kotek

enum class eZirconGameFeatures;
enum class eZirconSDKFeatures;

class zircon_command_history;
class zircon_renderer_gles3;
class zircon_resource_manager;
class zircon_config;
class zircon_factory_game;
class zircon_scene_manager;
class zircon_interface_session;
class zircon_manager_sdk_ui;
class zircon_session_game;
class zircon_session_editor;

class zircon_manager_game : public Kotek::Core::ktkIGameManager
{
public:
	zircon_manager_game(void);
	~zircon_manager_game(void);

	void Initialize(Kotek::Core::ktkMainManager* p_main_manager) override;
	void Shutdown(Kotek::Core::ktkMainManager* p_main_manager) override;

	void* GetWindowHandle(void) const noexcept override;

#ifdef KOTEK_USE_SDK
	void SetSDKRenderWindow(sdk::ui::zircon_RenderWindow* p_window) noexcept;
	void SetSDKMainWindow(sdk::ui::zircon_frame* p_window) noexcept;
	sdk::ui::zircon_frame* GetMainWindow(void) const noexcept;
#endif

	zircon_manager_sdk_ui* GetSDKUI(void) const noexcept;

	int GetWindowWidth(void) const noexcept override;
	int GetWindowHeight(void) const noexcept override;
	Kotek::Core::ktkProfiler* GetProfiler(void) const noexcept override;
	Kotek::Core::ktkConsole* GetConsole(void) const noexcept override;

	Kotek::Core::ktkIRenderer* GetRenderer(void) const noexcept override;
	void* GetRenderResourceManager(void) const noexcept override;
	void* CreateSurface(Kotek::Core::ktkMainManager* p_main_manager,
		void* p_instance, const void* p_callbacks) override;

	void Update(void) noexcept;

	void UpdateAllSystems(void) noexcept;

	zircon_scene_manager* GetSceneManager(void) const noexcept;
	zircon_factory_game* get_factory_game(void) const noexcept;
	Kotek::Core::ktkWindow* GetWindow(void) const noexcept;

	void Serialize(void) noexcept;
	void Deserialize(void) noexcept;

	Kotek::Core::ktkMainManager* GetMainManager(void) const noexcept;

	zircon_command_history* GetCommandHistoryManager(void) const noexcept;

	zircon_interface_session* GetSession_Editor(void) const noexcept;
	zircon_interface_session* GetSession_Game(void) const noexcept;

	zircon_config* get_config() const noexcept;

private:
	Kotek::ktk::entity_t Initialize_Actor(void) noexcept;

	void Initialize_Renderer(void) noexcept;
	void Destroy_Renderer(void) noexcept;

	void Initialize_Factory(void) noexcept;
	void Destroy_Factory(void) noexcept;

	void Initialize_SceneManager(void) noexcept;
	void Destroy_SceneManager(void) noexcept;

	void Initialize_ResourceManager(void) noexcept;
	void Destroy_ResourceManager(void) noexcept;

	void Initialize_Console(void) noexcept;

	void RegisterConsole_Commands(void) noexcept;
	void RegisterConsole_Commands_SDK(void) noexcept;

	void Destroy_Console(void) noexcept;

	void Initialize_SDKUIManager(zircon_factory_game* p_factory) noexcept;
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
	Kotek::Core::ktkProfiler* m_p_profiler;
	Kotek::Core::ktkConsole* m_p_console;
	Kotek::Core::ktkMainManager* m_p_main_manager;
	Kotek::Core::ktkIRenderer* m_p_current_renderer;
	zircon_renderer_gles3* m_p_renderer_gles3;

#ifdef KOTEK_USE_RENDER_VULKAN
	Render::vk::zircon_Renderer* m_p_renderer_vk;
#endif

#ifdef KOTEK_USE_SDK
	void* m_p_window_handle;
	sdk::ui::zircon_RenderWindow* m_p_sdk_render_window;
	sdk::ui::zircon_frame* m_p_sdk_main_window;
#endif

	zircon_manager_sdk_ui* m_p_sdk_ui_manager;
	zircon_factory_game* m_p_factory;
	zircon_scene_manager* m_p_scene_manager;
	zircon_resource_manager* m_p_resource_manager;
	zircon_session_editor* m_p_session_editor;
	zircon_session_game* m_p_session_game;
	zircon_config* m_p_config;
	zircon_command_history* m_p_sdk_history_manager;
};
