#include "zircon_game_manager.h"
#include "../ecs/components/zircon_factory.h"
#include "zircon_resource_manager.h"
#include "zircon_scene_manager.h"
#include "zircon_session_editor.h"
#include "zircon_session_game.h"

#include "ui/ImGui/zircon_ui_component_inspector.h"
#include "ui/ImGui/zircon_ui_object_list.h"
#include "ui/ImGui/zircon_ui_top_bar.h"
#include "ui/ImGui/zircon_ui_window_prefab.h"
#include "ui/ImGui/zircon_ui_window_prefab_browser.h"
#include "ui/ImGui/zircon_ui_window_history_command_log.h"
#include "ui/ImGui/zircon_ui_window_log.h"
#include "ui/ImGui/zircon_ui_window_render_stats.h"
#include "ui/ImGui/zircon_ui_window_settings.h"
#include "ui/ImGui/zircon_ui_window_debug_input.h"

#include "ui/Manager/zircon_sdk_ui.h"

#include "../core/zircon_sdk_ui.h"

#include "../render/gles3/zircon_renderer.h"
#include "../render/vk/zircon_renderer.h"

#include "zircon_command_history.h"
#include "zircon_command_create_entity.h"
#include "zircon_command_delete_entity.h"
#include "zircon_command_delete_component_from_entity.h"
#include "zircon_command_add_component_to_entity.h"
#include "../core/zircon_config.h"

#ifdef KOTEK_USE_RMLUI_LIBRARY
	#include <RmlUi/Core.h>
#endif

#ifdef KOTEK_USE_WINDOW_LIBRARY_GLFW
void WindowCallback_Resize(GLFWwindow* p_window, int width, int height)
{
	auto* p_manager = static_cast<kotek::core::ktkMainManager*>(
		glfwGetWindowUserPointer(p_window));

	KOTEK_ASSERT(
		p_manager, "you didn't register user pointer to ktkMainManager");

	p_manager->GetGameManager()->GetConsole()->PushCommand(
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_Resize),
		{{width}, {height}});

	#ifdef KOTEK_USE_RMLUI_LIBRARY
	if (!p_manager)
		return;

	if (!p_manager->Get_GameUIEngine())
		return;

	auto* p_casted = dynamic_cast<kotek::UI::ktkGameUI_RMLUI*>(
		p_manager->Get_GameUIEngine());

	if (!p_casted)
		return;
	#endif
}

void WindowCallback_Mouse(GLFWwindow* p_window, double xpos, double ypos)
{
	kotek::core::ktkMainManager* p_manager =
		static_cast<kotek::core::ktkMainManager*>(
			glfwGetWindowUserPointer(p_window));

	KOTEK_ASSERT(
		p_manager, "you didn't register user pointer to ktkMainManager");

	zircon_manager_game* p_game_manager =
		static_cast<zircon_manager_game*>(p_manager->GetGameManager());

	KOTEK_ASSERT(p_game_manager,
		"you didn't register user pointer to zircon_manager_game");

	/* TODO: delete this code
	auto entity_id = p_game_manager->GetSceneManager()
	                     ->GetCurrentScene()
	                     ->GetActor();

	if (p_game_manager->GetGameFactory()
	        ->HasComponent<zircon_component_input>(entity_id))
	{
	    auto component_input =
	        p_game_manager->GetGameFactory()
	            ->GetComponent<zircon_component_input>(entity_id);

	    component_input.SetPositionMouseX(xpos);
	    component_input.SetPositionMouseY(ypos);
	}*/

	auto* p_game_factory = p_game_manager->get_factory_game();

	#ifdef KOTEK_USE_SDK_IMGUI
	if (p_manager->Get_EngineConfig()->IsFeatureEnabled(kotek::core::
				eEngineFeatureSDK::kEngine_Feature_SDK_ImGui_Initialized))
	{
		auto* p_wrapper_imgui = p_manager->Get_ImguiWrapper();

		p_wrapper_imgui->ImGui_ImplGlfw_CursorPosCallback(p_window, xpos, ypos);

		if (p_game_factory)
		{
			auto& registry = p_game_factory->GetRegistry();

			auto entities_input = registry.view<zircon_component_sdk_input>();

			KOTEK_ASSERT(entities_input.size() <= 1,
				"you must have only one component that represent "
				"input");

			if (!entities_input.empty())
			{
				auto id = entities_input[0];

				auto& component_input =
					entities_input.get<zircon_component_sdk_input>(id);

				auto& input = component_input.get_input();

				if (input.is_first_iteration() == false)
				{
					input.set_position_mouse_x(xpos);
					input.set_position_mouse_y(ypos);

					input.set_first_iteration(true);
				}

				input.set_offset_mouse_position_x(
					xpos - input.get_position_mouse_x());
				input.set_position_mouse_x(xpos);

				input.set_offset_mouse_position_y(
					ypos - input.get_position_mouse_y());
				input.set_position_mouse_y(ypos);
			}
		}
	}
	#endif
}

void WindowCallback_Key(GLFWwindow* p_window, int glfw_key, int scancode,
	int glfw_action, int glfw_mods)
{
	#ifdef KOTEK_USE_RMLUI_LIBRARY
	auto* p_manager = static_cast<kotek::core::ktkMainManager*>(
		glfwGetWindowUserPointer(p_window));

	if (!p_manager)
		return;

	if (p_manager->Get_GameUIEngine())
	{
		#ifdef KOTEK_USE_RMLUI_LIBRARY
		#endif
	}

	#endif

	if (p_manager->Get_Input())
	{
		kotek::core::ktkInputPlatformBackendArgs_GLFW3 args;
		args.key = glfw_key;
		args.action = glfw_action;
		args.scancode = scancode;
		args.mods = glfw_mods;
		args.controller =
			kotek::core::eInputControllerType::kControllerKeyboard;

		p_manager->Get_Input()->Update(&args);
	}

	#ifdef KOTEK_USE_SDK_IMGUI
	if (p_manager->Get_EngineConfig()->IsFeatureEnabled(kotek::core::
				eEngineFeatureSDK::kEngine_Feature_SDK_ImGui_Initialized))
	{
		auto* p_wrapper_imgui = p_manager->Get_ImguiWrapper();

		p_wrapper_imgui->ImGui_ImplGlfw_KeyCallback(
			p_window, glfw_key, scancode, glfw_action, glfw_mods);
	}
	#endif
}

void WindowCallback_Char(GLFWwindow* p_window, unsigned int codepoint)
{
	#ifdef KOTEK_USE_RMLUI_LIBRARY
	kotek::core::ktkMainManager* p_manager =
		static_cast<kotek::core::ktkMainManager*>(
			glfwGetWindowUserPointer(p_window));

	if (!p_manager)
		return;

	auto* p_wrapper_imgui = p_manager->Get_ImguiWrapper();

	if (p_manager->Get_GameUIEngine())
	{
		kotek::UI::ktkGameUI_RMLUI* p_casted =
			dynamic_cast<kotek::UI::ktkGameUI_RMLUI*>(
				p_manager->Get_GameUIEngine());

		if (!p_casted)
			return;

		// todo: implement?
	}

	#endif

	#ifdef KOTEK_USE_SDK_IMGUI
	if (p_manager->Get_EngineConfig()->IsFeatureEnabled(kotek::core::
				eEngineFeatureSDK::kEngine_Feature_SDK_ImGui_Initialized))
	{
		auto* p_wrapper_imgui = p_manager->Get_ImguiWrapper();

		p_wrapper_imgui->ImGui_ImplGlfw_CharCallback(p_window, codepoint);
	}
	#endif
}

void WindowCallback_CursorEnter(GLFWwindow* p_window, int entered)
{
	#ifdef KOTEK_USE_RMLUI_LIBRARY
	auto* p_manager = static_cast<kotek::core::ktkMainManager*>(
		glfwGetWindowUserPointer(p_window));

	if (!p_manager)
		return;

	if (!p_manager->Get_GameUIEngine())
		return;

	auto* p_casted = dynamic_cast<kotek::UI::ktkGameUI_RMLUI*>(
		p_manager->Get_GameUIEngine());

	if (!p_casted)
		return;
	#endif

	#ifdef KOTEK_USE_SDK_IMGUI
	if (p_manager->Get_EngineConfig()->IsFeatureEnabled(kotek::core::
				eEngineFeatureSDK::kEngine_Feature_SDK_ImGui_Initialized))
	{
		auto* p_wrapper_imgui = p_manager->Get_ImguiWrapper();

		p_wrapper_imgui->ImGui_ImplGlfw_CursorEnterCallback(p_window, entered);
	}
	#endif
}

void WindowCallback_MouseButton(
	GLFWwindow* p_window, int button, int action, int mods)
{
	#ifdef KOTEK_USE_RMLUI_LIBRARY
	auto* p_manager = static_cast<kotek::core::ktkMainManager*>(
		glfwGetWindowUserPointer(p_window));

	if (!p_manager)
		return;

	if (!p_manager->Get_GameUIEngine())
		return;

	auto* p_casted = dynamic_cast<kotek::UI::ktkGameUI_RMLUI*>(
		p_manager->Get_GameUIEngine());

	if (!p_casted)
		return;
	#endif

	if (p_manager->Get_Input())
	{
		kotek::core::ktkInputPlatformBackendArgs_GLFW3 args;
		args.key = button;
		args.action = action;
		args.scancode = -1;
		args.mods = mods;
		args.controller =
			kotek::core::eInputControllerType::kControllerMouse;

		p_manager->Get_Input()->Update(&args);
	}

	#ifdef KOTEK_USE_SDK_IMGUI
	if (p_manager->Get_EngineConfig()->IsFeatureEnabled(kotek::core::
				eEngineFeatureSDK::kEngine_Feature_SDK_ImGui_Initialized))
	{
		auto* p_wrapper_imgui = p_manager->Get_ImguiWrapper();

		p_wrapper_imgui->ImGui_ImplGlfw_MouseButtonCallback(
			p_window, button, action, mods);

		auto* p_game_manager =
			static_cast<zircon_manager_game*>(p_manager->GetGameManager());

		if (p_game_manager)
		{
			auto* p_game_factory = p_game_manager->get_factory_game();

			if (p_game_factory)
			{
				auto& registry = p_game_factory->GetRegistry();

				auto entities_input =
					registry.view<zircon_component_sdk_input>();

				KOTEK_ASSERT(entities_input.size() <= 1,
					"you must have only one component that represent "
					"input");

				if (!entities_input.empty())
				{
					auto id = entities_input[0];

					auto& component_input =
						entities_input.get<zircon_component_sdk_input>(id);

					auto& input = component_input.get_input();

					int tick_left = input.get_mouse_left_hold_tick_count();
					int tick_right = input.get_mouse_right_hold_tick_count();

					if (button == GLFW_MOUSE_BUTTON_RIGHT)
					{
						if (action == GLFW_RELEASE)
						{
							input.set_mouse_right_hold_tick_count(0);
						}

						if (action == GLFW_PRESS)
						{
							++tick_right;
							input.set_mouse_right_hold_tick_count(tick_right);
						}
					}

					if (button == GLFW_MOUSE_BUTTON_LEFT)
					{
						if (action == GLFW_RELEASE)
						{
							input.set_mouse_left_hold_tick_count(0);
						}

						if (action == GLFW_PRESS)
						{
							++tick_left;
							input.set_mouse_left_hold_tick_count(tick_left);
						}
					}
				}
			}
		}
	}
	#endif
}

void WindowCallback_Scroll(GLFWwindow* p_window, double xoffset, double yoffset)
{
	#ifdef KOTEK_USE_RMLUI_LIBRARY
	auto* p_manager = static_cast<kotek::core::ktkMainManager*>(
		glfwGetWindowUserPointer(p_window));

	if (!p_manager)
		return;

	if (!p_manager->Get_GameUIEngine())
		return;

	auto* p_casted = dynamic_cast<kotek::UI::ktkGameUI_RMLUI*>(
		p_manager->Get_GameUIEngine());

	if (!p_casted)
		return;
	#endif

	#ifdef KOTEK_USE_SDK_IMGUI
	if (p_manager->Get_EngineConfig()->IsFeatureEnabled(kotek::core::
				eEngineFeatureSDK::kEngine_Feature_SDK_ImGui_Initialized))
	{
		auto* p_wrapper_imgui = p_manager->Get_ImguiWrapper();

		p_wrapper_imgui->ImGui_ImplGlfw_ScrollCallback(
			p_window, xoffset, yoffset);
	}
	#endif
}

void WindowCallback_WindowContentScale(
	GLFWwindow* p_window, float xscale, float yscale)
{
	#ifdef KOTEK_USE_RMLUI_LIBRARY
	auto* p_manager = static_cast<kotek::core::ktkMainManager*>(
		glfwGetWindowUserPointer(p_window));

	if (!p_manager)
		return;

	if (!p_manager->Get_GameUIEngine())
		return;

	auto* p_casted = dynamic_cast<kotek::UI::ktkGameUI_RMLUI*>(
		p_manager->Get_GameUIEngine());

	if (!p_casted)
		return;
	#endif
}

void WindowCallback_Monitor(GLFWmonitor* p_monitor, int event)
{
	#ifdef KOTEK_USE_SDK_IMGUI
	auto* p_manager = static_cast<kotek::core::ktkMainManager*>(
		glfwGetMonitorUserPointer(p_monitor));

	if (!p_manager)
		return;

	if (p_manager->Get_EngineConfig()->IsFeatureEnabled(kotek::core::
				eEngineFeatureSDK::kEngine_Feature_SDK_ImGui_Initialized))
	{
		auto* p_wrapper_imgui = p_manager->Get_ImguiWrapper();

		p_wrapper_imgui->ImGui_ImplGlfw_MonitorCallback(p_monitor, event);
	}
	#endif
}

void WindowCallback_WindowFocus(GLFWwindow* p_window, int focused)
{
	#ifdef KOTEK_USE_SDK_IMGUI
	auto* p_manager = static_cast<kotek::core::ktkMainManager*>(
		glfwGetWindowUserPointer(p_window));

	if (!p_manager)
		return;

	if (p_manager->Get_EngineConfig()->IsFeatureEnabled(kotek::core::
				eEngineFeatureSDK::kEngine_Feature_SDK_ImGui_Initialized))
	{
		auto* p_wrapper_imgui = p_manager->Get_ImguiWrapper();

		p_wrapper_imgui->ImGui_ImplGlfw_WindowFocusCallback(p_window, focused);
	}
	#endif
}

#elif defined(KOTEK_USE_WINDOW_LIBRARY_SDL)

#endif

zircon_manager_game::zircon_manager_game(void) :
	m_p_current_renderer{}, m_p_profiler{}, m_p_main_manager{},
	m_p_renderer_gles3{},

#ifdef KOTEK_USE_RENDER_VULKAN
	m_p_renderer_vk{},
#endif

#ifdef KOTEK_USE_SDK
	m_p_window_handle{}, m_p_sdk_render_window{}, m_p_sdk_main_window{},
#endif
	m_p_sdk_ui_manager{}, m_p_scene_manager{}, m_p_console{}, m_p_factory{},
	m_p_resource_manager{}, m_is_use_sdk{}, m_is_use_sdk_imgui{}, m_p_config{}
{
}

zircon_manager_game::~zircon_manager_game(void) {}

void zircon_manager_game::Initialize(
	kotek::core::ktkMainManager* p_main_manager)
{
	this->m_p_main_manager = p_main_manager;

	if (this->m_p_main_manager->Get_EngineConfig()
			->IsContainsConsoleCommandLineArgument(
				kotek::kConsoleCommandArg_Editor) == false)
	{
		kotek::core::ktkWindow* p_window = static_cast<kotek::core::ktkWindow*>(
			this->m_p_main_manager->Get_WindowManager()->Get_ActiveWindow());

		p_window->RegisterUserMainManager(this->m_p_main_manager);

#ifdef KOTEK_USE_WINDOW_LIBRARY_GLFW
		auto* p_handle_window = static_cast<GLFWwindow*>(p_window->GetHandle());
		glfwSetWindowSizeCallback(p_handle_window, &WindowCallback_Resize);
		glfwSetCursorPosCallback(p_handle_window, &WindowCallback_Mouse);
		glfwSetScrollCallback(p_handle_window, &WindowCallback_Scroll);
		glfwSetKeyCallback(p_handle_window, &WindowCallback_Key);
		glfwSetCharCallback(p_handle_window, &WindowCallback_Char);
		glfwSetMonitorCallback(&WindowCallback_Monitor);
		glfwSetMouseButtonCallback(
			p_handle_window, &WindowCallback_MouseButton);
		glfwSetCursorEnterCallback(
			p_handle_window, &WindowCallback_CursorEnter);
		glfwSetWindowFocusCallback(
			p_handle_window, &WindowCallback_WindowFocus);
#elif defined(KOTEK_USE_WINDOW_LIBRARY_SDL)
	#error not implemented
#else
	#error provide implementation
#endif
	}

	this->Initialize_Console();
	this->initialize_config();
	this->Initialize_Factory();
	this->Initialize_SceneManager();
	this->Initialize_ResourceManager();
	this->Initialize_HistoryCommandManager();

	// TODO: do we really need it here???????
	this->Initialize_SDKUIManager(this->m_p_factory);

	this->Initialize_Renderer();

	this->Initialize_Session();
	this->Initialize_UI();

	this->RegisterConsole_Commands();
	this->RegisterConsole_Commands_SDK();
}

void zircon_manager_game::Shutdown(kotek::core::ktkMainManager* p_main_manager)
{
	this->Destroy_UI();
	this->Destroy_HistoryCommandManager();
	this->Destroy_Renderer();
	this->Destroy_ResourceManager();
	this->Destroy_SDKUIManager();
	this->Destroy_Console();
	this->Destroy_Session();
	this->Destroy_SceneManager();
	this->Destroy_Factory();
	this->destroy_config();
}

void* zircon_manager_game::GetWindowHandle(void) const noexcept
{
	if (this->m_p_main_manager->Get_WindowManager()->Get_ActiveWindow())
	{
		return this->m_p_main_manager->Get_WindowManager()
			->ActiveWindow_GetHandle();
	}
#ifdef KOTEK_USE_SDK
	else if (this->m_p_sdk_render_window)
	{
	#ifdef KOTEK_PLATFORM_WINDOWS
		return this->m_p_sdk_render_window->GetHWND();
	#elif KOTEK_PLATFORM_LINUX
		KOTEK_ASSERT(false, "not implemented");
		return nullptr;
	#endif
	}
#endif
	else
	{
		KOTEK_ASSERT(
			false, "you must register window to Game Manager. Abort...");

		return nullptr;
	}
}
#ifdef KOTEK_USE_SDK
void zircon_manager_game::SetSDKRenderWindow(
	zircon::sdk::ui::zircon_RenderWindow* p_window) noexcept
{
	this->m_p_sdk_render_window = p_window;
}
#endif

#ifdef KOTEK_USE_SDK
void zircon_manager_game::SetSDKMainWindow(
	sdk::ui::zircon_frame* p_window) noexcept
{
	this->m_p_sdk_main_window = p_window;
}
#endif

int zircon_manager_game::GetWindowWidth(void) const noexcept
{
	if (this->m_p_main_manager->Get_WindowManager()->Get_ActiveWindow())
	{
		return this->m_p_main_manager->Get_WindowManager()
			->ActiveWindow_GetWidth();
	}
#ifdef KOTEK_USE_SDK
	else if (this->m_p_sdk_render_window)
	{
		return this->m_p_sdk_render_window->GetClientSize().x;
	}
#endif
	else
	{
		KOTEK_ASSERT(false,
			"you didn't register any window for Game Manager! "
			"Abort...");
		return 0;
	}
}

int zircon_manager_game::GetWindowHeight(void) const noexcept
{
	if (this->m_p_main_manager->Get_WindowManager()->Get_ActiveWindow())
	{
		return this->m_p_main_manager->Get_WindowManager()
			->ActiveWindow_GetHeight();
	}
#ifdef KOTEK_USE_SDK
	else if (this->m_p_sdk_render_window)
	{
		return this->m_p_sdk_render_window->GetClientSize().y;
	}
#endif
	else
	{
		KOTEK_ASSERT(false,
			"you didn't register any window for Game Manager! "
			"Abort...");
		return 0;
	}
}

kotek::core::ktkProfiler* zircon_manager_game::GetProfiler(void) const noexcept
{
	return this->m_p_profiler;
}

kotek::core::ktkConsole* zircon_manager_game::GetConsole(void) const noexcept
{
	return this->m_p_console;
}

kotek::core::ktkIRenderer* zircon_manager_game::GetRenderer(void) const noexcept
{
	return this->m_p_current_renderer;
}

void* zircon_manager_game::GetRenderResourceManager(void) const noexcept
{
	return nullptr;
}

void* zircon_manager_game::CreateSurface(
	kotek::core::ktkMainManager* p_main_manager, void* p_instance,
	const void* p_callbacks)
{
	KOTEK_ASSERT(p_main_manager, "must be valid");

	auto* p_engine_config = p_main_manager->Get_EngineConfig();

	KOTEK_ASSERT(p_engine_config,
		"you must initialize engine config, something is wrong");

	bool is_gl_initialized = p_engine_config->GetRendererVersionEnum() >=
			kotek::core::eEngineSupportedRenderer::kOpenGL_1_0 ||
		p_engine_config->GetRendererVersionEnum() <=
			kotek::core::eEngineSupportedRenderer::kOpenGL_Latest;
	bool is_vk_initialized = p_engine_config->GetRendererVersionEnum() >=
			kotek::core::eEngineSupportedRenderer::kVulkan_1_0 ||
		p_engine_config->GetRendererVersionEnum() <=
			kotek::core::eEngineSupportedRenderer::kVulkan_Latest;
	bool is_dx_initialized = p_engine_config->GetRendererVersionEnum() >=
			kotek::core::eEngineSupportedRenderer::kDirectX_7 ||
		p_engine_config->GetRendererVersionEnum() <=
			kotek::core::eEngineSupportedRenderer::kDirectX_Latest;
	bool is_gles_initialized = p_engine_config->GetRendererVersionEnum() >=
			kotek::core::eEngineSupportedRenderer::kOpenGLES_1 ||
		p_engine_config->GetRendererVersionEnum() <=
			kotek::core::eEngineSupportedRenderer::kOpenGLES_Latest;

	int how_many_is_initialized{};

	if (is_gl_initialized)
		++how_many_is_initialized;
	if (is_vk_initialized)
		++how_many_is_initialized;
	if (is_dx_initialized)
		++how_many_is_initialized;
	if (is_gles_initialized)
		++how_many_is_initialized;

	KOTEK_ASSERT(how_many_is_initialized == 1,
		"you have more than one initialized renderer version enum! It "
		"means that some code breaks logic and it sets one of enum to "
		"valid value, but it is not right, because you must have only "
		"on initialized renderer formally.");

	// TODO: think about DX12 here
	if (is_vk_initialized)
	{
		KOTEK_ASSERT(
			this->m_p_main_manager->Get_WindowManager()->Get_ActiveWindow(),
			"you must initialize window");

		VkInstance p_casted_instance = static_cast<VkInstance>(p_instance);
		const VkAllocationCallbacks* p_casted_callbacks =
			static_cast<const VkAllocationCallbacks*>(p_callbacks);

		KOTEK_ASSERT(p_casted_instance, "must be valid");

		VkSurfaceKHR p_surface = nullptr;
#ifdef KOTEK_USE_SDK
		// TODO: fix this when you will test SDK
		if (this->m_p_sdk_render_window)
		{
	#ifdef KOTEK_PLATFORM_WINDOWS
			VkWin32SurfaceCreateInfoKHR info = {};

			info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			info.hwnd =
				static_cast<HWND>(this->m_p_sdk_render_window->GetHWND());
			info.hinstance = GetModuleHandle(nullptr);

			VkResult status = vkCreateWin32SurfaceKHR(
				p_casted_instance, &info, nullptr, &p_surface);

			KOTEK_ASSERT(status == VkResult::VK_SUCCESS,
				"failed to vkCreateWin32SurfaceKHR");
	#elif KOTEK_PLATFORM_LINUX
			KOTEK_ASSERT(false, "not implemented");
	#endif
		}
		else
#endif
		{
			VkResult status = glfwCreateWindowSurface(p_casted_instance,
				static_cast<GLFWwindow*>(
					this->m_p_main_manager->Get_WindowManager()
						->ActiveWindow_GetHandle()),
				p_casted_callbacks, &p_surface);

			KOTEK_ASSERT(status == VkResult::VK_SUCCESS,
				"failed to glfwCreateWindowSurface");
		}

		return p_surface;
	}

	return nullptr;
}

void zircon_manager_game::Update(void) noexcept
{
	zircon_interface_session* p_current_session{};

	if (this->m_is_use_sdk_imgui)
	{
		p_current_session = this->m_p_session_editor;
	}
	else
	{
		p_current_session = this->m_p_session_game;
	}

	if (p_current_session)
	{
		p_current_session->update();
	}
}

void zircon_manager_game::UpdateAllSystems(void) noexcept
{
	this->UpdateInput();
	this->UpdateCamera();
}

zircon_scene_manager* zircon_manager_game::GetSceneManager(void) const noexcept
{
	return this->m_p_scene_manager;
}

zircon_factory_game* zircon_manager_game::get_factory_game(void) const noexcept
{
	return this->m_p_factory;
}

kotek::core::ktkWindow* zircon_manager_game::GetWindow(void) const noexcept
{
	return static_cast<kotek::core::ktkWindow*>(
		this->m_p_main_manager->Get_WindowManager()->Get_ActiveWindow());
}

void zircon_manager_game::Serialize(void) noexcept
{
	this->m_p_config->serialize(this->m_p_main_manager->GetFileSystem(),
		this->m_p_main_manager->GetResourceManager());
}

void zircon_manager_game::Deserialize(void) noexcept
{
	if (this->m_p_main_manager->Get_EngineConfig()
			->IsContainsConsoleCommandLineArgument(
				kotek::kConsoleCommandArg_Editor_ImGui))
	{
	}
	else
	{
	}

	this->m_p_config->deserialize(this->m_p_main_manager->GetFileSystem(),
		this->m_p_main_manager->GetResourceManager());
}

kotek::core::ktkMainManager* zircon_manager_game::GetMainManager(
	void) const noexcept
{
	return this->m_p_main_manager;
}

zircon_command_history* zircon_manager_game::GetCommandHistoryManager(
	void) const noexcept
{
	return this->m_p_sdk_history_manager;
}

zircon_interface_session* zircon_manager_game::GetSession_Editor(
	void) const noexcept
{
	return this->m_p_session_editor;
}

zircon_interface_session* zircon_manager_game::GetSession_Game(
	void) const noexcept
{
	return this->m_p_session_game;
}

zircon_config* zircon_manager_game::get_config() const noexcept
{
	return this->m_p_config;
}

#ifdef KOTEK_USE_SDK
sdk::ui::zircon_frame* zircon_manager_game::GetMainWindow(void) const noexcept
{
	return this->m_p_sdk_main_window;
}
#endif

zircon_sdk_ui_interface* zircon_manager_game::get_sdk_ui(void) const noexcept
{
	return this->m_p_sdk_ui_manager;
}

entt::entity zircon_manager_game::Initialize_Actor(void) noexcept
{
	auto actor_id = this->m_p_factory->CreateEntity();

	this->m_p_factory->CreateComponent<zircon_component_input>(actor_id);

	this->m_p_factory->CreateComponent<zircon_component_actor>(actor_id);

	this->m_p_factory->CreateComponent<zircon_component_camera>(actor_id);

	this->m_p_factory->CreateComponent<zircon_component_visibility>(actor_id);

	this->m_p_factory->CreateComponent<zircon_component_transform>(actor_id);

	return actor_id;
}

void zircon_manager_game::Initialize_Renderer(void) noexcept
{
	// TODO: think about ImGui preprocessor...
	kotek::ktk::vector<kotek::core::ktkISDKUIElement*> elements;
	elements.push_back(new zircon_sdk_ui_object_list());
	elements.push_back(new zircon_sdk_ui_top_bar());
	elements.push_back(new zircon_sdk_ui_window_prefab());
	elements.push_back(new zircon_sdk_ui_component_inspector(
		this->m_p_sdk_ui_manager, this->m_p_factory));
	elements.push_back(new zircon_sdk_ui_window_log());
	elements.push_back(new zircon_ui_window_history_command_log(
		this->m_p_sdk_history_manager));
	elements.push_back(new zircon_ui_window_render_stats());
	elements.push_back(new zircon_ui_window_settings());
	elements.push_back(new zircon_sdk_ui_window_debug_input());

	auto* p_engine_config = this->m_p_main_manager->Get_EngineConfig();

	KOTEK_ASSERT(p_engine_config,
		"you must initialize engine config for using this method");

	// TODO: add initialization with appropriate gl version of we have
	// the 4.6 for example we have to initialize with according version
	if (p_engine_config->IsFeatureEnabled(kotek::core::eEngineFeatureRenderer::
				kEngine_Feature_Renderer_OpenGL_Latest) ||
		p_engine_config->IsFeatureEnabled(kotek::core::eEngineFeatureRenderer::
				kEngine_Feature_Renderer_OpenGL_SpecifiedByUser))
	{
		switch (p_engine_config->GetRendererVersionEnum())
		{
		default:
			KOTEK_ASSERT(false, "not supported");
			break;
		}
	}
	else if (p_engine_config->IsFeatureEnabled(
				 kotek::core::eEngineFeatureRenderer::
					 kEngine_Feature_Renderer_Vulkan_Latest) ||
		p_engine_config->IsFeatureEnabled(kotek::core::eEngineFeatureRenderer::
				kEngine_Feature_Renderer_Vulkan_SpecifiedByUser))
	{
#ifdef KOTEK_USE_RENDER_VULKAN
		this->m_p_renderer_vk =
			new Render::vk::zircon_Renderer(*this->m_p_main_manager);

		this->m_p_renderer_vk->Initialize(elements);
		this->m_p_current_renderer = this->m_p_renderer_vk;
#endif
	}
	else if (p_engine_config->IsFeatureEnabled(
				 kotek::core::eEngineFeatureRenderer::
					 kEngine_Feature_Renderer_DirectX_Latest) ||
		p_engine_config->IsFeatureEnabled(kotek::core::eEngineFeatureRenderer::
				kEngine_Feature_Renderer_DirectX_SpecifiedByUser))
	{
		KOTEK_ASSERT(false, "not supported");
	}
	else if (p_engine_config->IsFeatureEnabled(
				 kotek::core::eEngineFeatureRenderer::
					 kEngine_Feature_Renderer_OpenGLES_Latest) ||
		p_engine_config->IsFeatureEnabled(kotek::core::eEngineFeatureRenderer::
				kEngine_Feature_Renderer_OpenGLES_SpecifiedByUser))
	{
		switch (p_engine_config->GetRendererVersionEnum())
		{
		case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_0:
		case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_1:
		case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_2:
		default:
		{
			this->m_p_renderer_gles3 =
				new zircon_renderer_gles3(this->m_p_main_manager);

			this->m_p_renderer_gles3->Initialize(elements);
			this->m_p_current_renderer = this->m_p_renderer_gles3;
			break;
		}
		}
	}
	else if (p_engine_config->IsFeatureEnabled(kotek::core::
					 eEngineFeatureRenderer::kEngine_Feature_Renderer_Software))
	{
		KOTEK_ASSERT(false, "not implemented yet");
	}
	else
	{
		KOTEK_ASSERT(false,
			"can't be, but engine contains undefined render "
			"feature!!!! Render Feature Name: {}",
			this->m_p_main_manager->Get_EngineConfig()->GetRenderName());
	}

	KOTEK_MESSAGE(
		"Initialized Renderer: [{}]", this->m_p_current_renderer->Get_Name());
}

void zircon_manager_game::Destroy_Renderer(void) noexcept
{
	auto* p_engine_config = this->m_p_main_manager->Get_EngineConfig();

	if (p_engine_config->IsFeatureEnabled(kotek::core::eEngineFeatureRenderer::
				kEngine_Feature_Renderer_OpenGL_Latest) ||
		p_engine_config->IsFeatureEnabled(kotek::core::eEngineFeatureRenderer::
				kEngine_Feature_Renderer_OpenGL_SpecifiedByUser))
	{
		switch (p_engine_config->GetRendererVersionEnum())
		{
		default:
			KOTEK_ASSERT(false, "not supported");
			break;
		}
	}
	else if (p_engine_config->IsFeatureEnabled(
				 kotek::core::eEngineFeatureRenderer::
					 kEngine_Feature_Renderer_Vulkan_Latest) ||
		p_engine_config->IsFeatureEnabled(kotek::core::eEngineFeatureRenderer::
				kEngine_Feature_Renderer_Vulkan_SpecifiedByUser))
	{
#ifdef KOTEK_USE_RENDER_VULKAN
		KOTEK_ASSERT(this->m_p_renderer_vk, "must be valid");

		delete this->m_p_renderer_vk;
		this->m_p_renderer_vk = nullptr;
#endif
	}
	else if (p_engine_config->IsFeatureEnabled(
				 kotek::core::eEngineFeatureRenderer::
					 kEngine_Feature_Renderer_DirectX_Latest) ||
		p_engine_config->IsFeatureEnabled(kotek::core::eEngineFeatureRenderer::
				kEngine_Feature_Renderer_DirectX_SpecifiedByUser))
	{
		KOTEK_ASSERT(false, "not implemented yet");
	}
	else if (p_engine_config->IsFeatureEnabled(
				 kotek::core::eEngineFeatureRenderer::
					 kEngine_Feature_Renderer_OpenGLES_Latest) ||
		p_engine_config->IsFeatureEnabled(kotek::core::eEngineFeatureRenderer::
				kEngine_Feature_Renderer_OpenGLES_SpecifiedByUser))
	{
		KOTEK_ASSERT(this->m_p_renderer_gles3, "must be valid");

		delete this->m_p_renderer_gles3;
		this->m_p_renderer_gles3 = nullptr;
	}
	else if (p_engine_config->IsFeatureEnabled(kotek::core::
					 eEngineFeatureRenderer::kEngine_Feature_Renderer_Software))
	{
		KOTEK_ASSERT(false, "not implemented yet");
	}
	else
	{
		KOTEK_ASSERT(false,
			"you must have initialized or (it is undefined and not "
			"supported flag) for eEngineFeatureRenderer field in "
			"engine config. It means that we can't understand what was "
			"your renderer and can't deallocate resources. Something "
			"is wrong if your initialize_renderer method calling was "
			"successful, because it means that something broke your "
			"code and you got invalid enum but it was supposed that "
			"everything would be fine and you would obtain your "
			"version for what you initialized before in "
			"initialize_renderer method respectively");
	}

	KOTEK_ASSERT(this->m_p_renderer_gles3 == nullptr,
		"you can't have two initialized renderers at once!");

#ifdef KOTEK_USE_RENDER_VULKAN
	KOTEK_ASSERT(this->m_p_renderer_vk == nullptr,
		"you can't have two initialized renderers at once!");
#endif

	this->m_p_current_renderer = nullptr;
}

void zircon_manager_game::Initialize_Factory(void) noexcept
{
	this->m_p_factory = new zircon_factory_game();
	this->m_p_factory->Initialize(this->m_p_config, this->m_p_console);
}

void zircon_manager_game::Destroy_Factory(void) noexcept
{
	KOTEK_ASSERT(this->m_p_factory, "must be valid");

	this->m_p_factory->Shutdown();
	delete this->m_p_factory;
	this->m_p_factory = nullptr;
}

void zircon_manager_game::Initialize_SceneManager(void) noexcept
{
	this->m_p_scene_manager = new zircon_scene_manager(this->m_p_factory, this);

	this->m_p_scene_manager->Initialize();
}

void zircon_manager_game::Destroy_SceneManager(void) noexcept
{
	KOTEK_ASSERT(this->m_p_scene_manager, "must be valid");

	this->m_p_scene_manager->Shutdown();
	delete this->m_p_scene_manager;
	this->m_p_scene_manager = nullptr;
}

void zircon_manager_game::Initialize_ResourceManager(void) noexcept
{
	/* TODO: think about user feature like user wants to replace our
	resource manager with his implementation, because we initialize our
	resource manager in force because resource manager requires for
	loading dlls as minimum

	this->m_p_resource_manager = new
	zircon_ResourceManager();

	this->m_p_resource_manager->Initialize(this->m_p_main_manager,
	    this->m_p_main_manager->GetRenderResourceManager());

	this->m_p_main_manager->SetResourceManager(
	    this->m_p_resource_manager);
	    */

	this->m_p_resource_manager =
		new zircon_resource_manager(this->m_p_main_manager);
	this->m_p_resource_manager->Set_RenderResourceManager(
		this->m_p_main_manager->GetRenderResourceManager());
	this->m_p_resource_manager->Set_ResourceLoader(
		new kotek::core::ktkResourceLoaderManager());
	this->m_p_resource_manager->Set_ResourceSaver(
		new kotek::core::ktkResourceSaverManager());
	this->m_p_resource_manager->Initialize();

	this->m_p_main_manager->SetResourceManager(this->m_p_resource_manager);
}

void zircon_manager_game::Destroy_ResourceManager(void) noexcept
{
	if (this->m_p_resource_manager)
	{
		this->m_p_resource_manager->Shutdown();
		delete this->m_p_resource_manager;
		this->m_p_resource_manager = nullptr;
	}
}

void zircon_manager_game::Initialize_Console(void) noexcept
{
	this->m_p_console = new kotek::core::ktkConsole();
	this->m_p_console->Initialize();
}

void zircon_manager_game::RegisterConsole_Commands(void) noexcept
{
	auto* p_window_manager = this->m_p_main_manager->Get_WindowManager();

	KOTEK_ASSERT(p_window_manager, "you must initialize window manager");

	this->m_p_console->RegisterCommand(
		static_cast<kotek::ktk::enum_base_t>(kotek::core::eConsoleCommandIndex::
				kConsoleCommand_App_AddTextToExistedWindowTitle),
		[p_window_manager](
			const kotek::ktk::console_command_args_t& args) -> bool
		{
			KOTEK_ASSERT(args.empty() == false,
				"you can't pass an empty arguments here");

			KOTEK_ASSERT(args.size() >= 2,
				"you must have two arguments, one is a id from "
				"eWindowTitleType enum and the second is the "
				"string for title");

			if (p_window_manager->Get_ActiveWindow())
			{
				p_window_manager->Get_ActiveWindow()->SetStringToTitle(
					std::get<kotek::ktk::enum_base_t>(args[0]),
					std::get<kotek::static_cstring_t<16>>(args[1]).c_str());
			}

			return true;
		});

	this->m_p_console->RegisterCommand(
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_Input_Type),
		[p_window_manager](
			const kotek::ktk::console_command_args_t& args) -> bool
		{
			KOTEK_ASSERT(
				args.empty() == false, "you must pass an argument here");

			if (p_window_manager->Get_ActiveWindow())
			{
				p_window_manager->Get_ActiveWindow()->Set_InputType(
					std::get<kotek::ktk::enum_base_t>(args[0]));
			}

			return true;
		});

	auto* p_main_manager = this->m_p_main_manager;

	auto* p_resource_manager = p_main_manager->GetResourceManager();

	this->m_p_console->RegisterCommand(
		static_cast<kotek::ktk::enum_base_t>(kotek::core::eConsoleCommandIndex::
				kConsoleCommand_Render_CalculateBoundingPrimitive),
		[this](const kotek::ktk::console_command_args_t& args) -> bool
		{
			KOTEK_ASSERT(args.empty() == false,
				"you must pass an arguments here. arg1=resource_geometry, "
				"arg2=primitive_type");
			KOTEK_ASSERT(args.size() > 2,
				"not enough arguments. Must be three arguments one defines "
				"geometry (index + vertex), second is primitive type (bounding "
				"type), third is entity to which we need to create bounding "
				"component");

			auto* p_factory = this->get_factory_game();

			KOTEK_ASSERT(p_factory,
				"you must call this command when factory is initialized, "
				"something is wrong!");

			if (p_factory)
			{
				auto p_resource_geometry =
					reinterpret_cast<kotek::Render::gl::ktkGeometry*>(
						std::get<kotek::ktk::shared_ptr<
							kun_kotek kun_core ktkResourceHandle>>(args[0])
							.get()
							->Get_Resource());
				auto bounding_type =
					static_cast<kotek::core::eRenderBoundingPrimitiveType>(
						std::get<kotek::ktk::enum_base_t>(args[1]));
				auto entity_id = static_cast<entt::entity>(
					std::get<kotek::uint32_t>(args[2]));

				auto status_geometry =
					p_factory->HasComponent<zircon_component_geometry>(
						entity_id);

				KOTEK_ASSERT(status_geometry,
					"you must have component geometry to calculate the "
					"primitive without geometry can't process this "
					"command!");

				if (!status_geometry)
					return false;

				switch (bounding_type)
				{
				case kotek::core::eRenderBoundingPrimitiveType::kBoundingSphere:
				{
					auto status =
						p_factory
							->HasComponent<zircon_component_bounding_sphere>(
								entity_id);

					if (!status)
					{
						this->GetConsole()->Execute(
							static_cast<kotek::enum_base_t>(
								kotek::core::eConsoleCommandIndex::
									kConsoleCommand_SDK_CreateComponentForEntity),
							{zircon_component_bounding_sphere::
									GetComponentName()
										.c_str(),
								static_cast<kotek::uint32_t>(entity_id)});
					}

					auto& component_bounding_sphere =
						p_factory
							->GetComponent<zircon_component_bounding_sphere>(
								entity_id);

					component_bounding_sphere =
						zircon_calculate_bounding_sphere(
							p_resource_geometry->Get_VertexData(), 3);

					break;
				}
				case kn_kotek::kn_core::eRenderBoundingPrimitiveType::
					kBoundingAABB:
				{
					break;
				}
				default:
				{
					KOTEK_ASSERT(false, "not implemented!");
				}
				}
			}

			return true;
		});

	this->m_p_console->RegisterCommand(
		static_cast<kotek::ktk::enum_base_t>(kotek::core::eConsoleCommandIndex::
				kConsoleCommand_ResourceManager_Load),
		[p_resource_manager](
			const kotek::ktk::console_command_args_t& args) -> bool
		{
			KOTEK_ASSERT(
				args.empty() == false, "you must pass an arguments here");
			KOTEK_ASSERT(p_resource_manager,
				"you must have valid resource manager here");

			auto request = std::get<kotek::core::ktkLoadingRequest>(args[0]);

			if (p_resource_manager)
			{
				p_resource_manager->Load(request);
			}

			return true;
		});

	auto p_command_resize = [p_main_manager](
								kotek::ktk::console_command_args_t data) -> bool
	{
		if (data.empty())
		{
			KOTEK_MESSAGE_WARNING(
				"can't execute resize command because data argument "
				"list is empty");
			return false;
		}

		if (data.size() < 2)
		{
			KOTEK_MESSAGE_WARNING("can't execute resize command "
								  "brcause data doesn't contain "
								  "enough arguments");
			return false;
		}

		int width = std::get<int>(data[0]);
		int height = std::get<int>(data[1]);

		if (width == p_main_manager->getRenderDevice()->GetWidth() &&
			height == p_main_manager->getRenderDevice()->GetHeight())
			return true;

		p_main_manager->getRenderDevice()->GPUFlush();
		p_main_manager->getRenderDevice()->Resize(
			p_main_manager->getRenderSwapchainManager(),
			p_main_manager->GetGameManager()->GetRenderer(),
			p_main_manager->GetRenderResourceManager(), width, height);

		return true;
	};

	auto p_command_close_application =
		[p_main_manager](kotek::ktk::console_command_args_t data) -> bool
	{
		if (data.empty())
		{
			KOTEK_MESSAGE_WARNING("can't execute application close command "
								  "because data argument list is empty");
			return false;
		}

		const auto& argument = std::get<kotek::static_cstring_t<8>>(data[0]);

		bool status = kotek::ktk::cast::to_bool(argument.c_str());

		p_main_manager->Get_EngineConfig()->SetApplicationWorking(status);

		return true;
	};

	kotek::core::ktkIRenderer* p_current_renderer = this->m_p_current_renderer;

	auto p_command_sdk_show_window =
		[p_main_manager, p_current_renderer](
			kotek::ktk::console_command_args_t data) -> bool
	{
		if (data.empty())
		{
			KOTEK_MESSAGE_WARNING(
				"can't execute command because it requires an ID for window "
				"showing, ID is a required argument for calling");
			return false;
		}

		if (!p_current_renderer)
		{
			KOTEK_MESSAGE_WARNING(
				"you can't call this command if renderer wasn't initialized");
			return false;
		}

		const auto& imgui_elements = p_current_renderer->Get_UIImGuiElements();

		if (imgui_elements.empty())
		{
			KOTEK_MESSAGE_WARNING("engine didn't register any window on "
								  "renderer side... Unable to proceed...");
			return false;
		}

		int window_id = std::get<int>(data[0]);

		auto window_iter =
			std::find_if(imgui_elements.begin(), imgui_elements.end(),
				[window_id](kotek::core::ktkISDKUIElement* p_element) -> bool
				{
					KOTEK_ASSERT(p_element,
						"something is broken and window is invalid!");
					return p_element->Get_ID() == window_id;
				});

		if (window_iter != imgui_elements.end())
		{
			kotek::core::ktkISDKUIElement* p_element = (*window_iter);

			if (p_element->Is_Shown() == false)
			{
				p_element->Show();
			}
		}

		return true;
	};

	auto p_command_sdk_hide_window =
		[p_main_manager, p_current_renderer](
			kotek::ktk::console_command_args_t data) -> bool
	{
		if (data.empty())
		{
			KOTEK_MESSAGE_WARNING(
				"can't execute command because it requires an ID for window "
				"showing, ID is a required argument for calling");
			return false;
		}

		if (!p_current_renderer)
		{
			KOTEK_MESSAGE_WARNING(
				"you can't call this command if renderer wasn't initialized");
			return false;
		}

		const auto& imgui_elements = p_current_renderer->Get_UIImGuiElements();

		if (imgui_elements.empty())
		{
			KOTEK_MESSAGE_WARNING("engine didn't register any window on "
								  "renderer side... Unable to proceed...");
			return false;
		}

		int window_id = std::get<int>(data[0]);

		auto window_iter =
			std::find_if(imgui_elements.begin(), imgui_elements.end(),
				[window_id](kotek::core::ktkISDKUIElement* p_element) -> bool
				{
					KOTEK_ASSERT(p_element,
						"something is broken and window is invalid!");
					return p_element->Get_ID() == window_id;
				});

		if (window_iter != imgui_elements.end())
		{
			kotek::core::ktkISDKUIElement* p_element = (*window_iter);

			if (p_element->Is_Shown() == true)
			{
				p_element->Hide();
			}
		}

		return true;
	};

	auto p_command_sdk_print_registered_windows =
		[p_main_manager](kotek::ktk::console_command_args_t data) -> bool
	{ return true; };

	this->m_p_console->RegisterCommand(
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_SDK_ShowWindow),
		p_command_sdk_show_window);

	this->m_p_console->RegisterCommand(
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_SDK_HideWindow),
		p_command_sdk_hide_window);

	this->m_p_console->RegisterCommand(
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_Resize),
		p_command_resize);

	this->m_p_console->RegisterCommand(
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_App_Close),
		p_command_close_application);
}

void zircon_manager_game::RegisterConsole_Commands_SDK(void) noexcept
{
	KOTEK_ASSERT(this->m_p_console,
		"you must initialize console before calling this method");

	// TODO: ?????????? ?.?. ???? ???????????? ???? ??????? ??????????
	// ???????? ?????????, ? ?? ????? ?? ???????? ? ???????????? ??? ???
	// ????? UI ??????? ? ???????
	if (this->m_p_main_manager->Get_EngineConfig()
			->IsContainsConsoleCommandLineArgument(
				kotek::kConsoleCommandArg_Editor_ImGui))
	{
		KOTEK_ASSERT(
			this->m_p_session_editor, "you must initialize session for editor");

		auto* p_session = this->m_p_session_editor;

		KOTEK_ASSERT(this->m_p_sdk_history_manager,
			"you must initialize command history manager");

		auto* p_history_manager = this->m_p_sdk_history_manager;

		this->m_p_console->RegisterCommand(
			static_cast<kotek::ktk::enum_base_t>(kotek::core::
					eConsoleCommandIndex::kConsoleCommand_SDK_LoadScene),
			[p_session, this](
				const kotek::ktk::console_command_args_t& args) -> bool
			{
				this->GetConsole()->Execute(
					static_cast<kotek::ktk::enum_base_t>(
						kotek::core::eConsoleCommandIndex::
							kConsoleCommand_SDK_CloseCurrentScene));

				KOTEK_ASSERT(args.empty() == false,
					"you can't pass an empty argument here");

				const auto& path_to_file =
					std::get<kotek::static_path_t>(args[0]);

				p_session->Deserialize(path_to_file);

				return true;
			});

		this->m_p_console->RegisterCommand(
			static_cast<kotek::ktk::enum_base_t>(kotek::core::
					eConsoleCommandIndex::kConsoleCommand_SDK_SaveScene),
			[p_history_manager, p_session, this](
				const kotek::ktk::console_command_args_t& args) -> bool
			{
				kotek::static_path_t filename;

				if (args.empty())
				{
#ifdef KOTEK_PLATFORM_WINDOWS
					OPENFILENAME ofn;
					TCHAR szFile[MAX_PATH] = {0};

					ZeroMemory(&ofn, sizeof(ofn));

					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = nullptr;
					ofn.lpstrFile = szFile;
					ofn.nMaxFile = sizeof(szFile);
					ofn.lpstrFilter = TEXT("JSON files (*.json)\0*.json\0");
					ofn.nFilterIndex = 1;
					ofn.lpstrFileTitle = NULL;
					ofn.nMaxFileTitle = 0;
					ofn.lpstrInitialDir = NULL;
					ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

					if (GetSaveFileName(&ofn) == TRUE)
					{
						const auto& utf8_path =
							ktk_filesystem_path(ofn.lpstrFile).u8string();

						const char* p_utf8_path =
							reinterpret_cast<const char*>(utf8_path.c_str());

						KOTEK_MESSAGE("saving scene: {}", p_utf8_path);
						filename = p_utf8_path;
					}
#endif
				}
				else
				{
					filename = std::get<kotek::static_path_t>(args[0]);

					KOTEK_MESSAGE(
						"saving scene (user passed args): {}", filename);
				}

				if (filename.empty() == false)
				{
					p_session->Serialize(filename.c_str());

					p_history_manager->set_changed(false);

					this->GetConsole()->PushCommand(
						static_cast<kotek::ktk::enum_base_t>(
							kotek::core::eConsoleCommandIndex::
								kConsoleCommand_App_AddTextToExistedWindowTitle),
						{{static_cast<kotek::ktk::enum_base_t>(
							 kotek::core::eWindowTitleType::
								 kTitle_CurrentSceneEditStatus)},
							kotek::static_cstring_t<16>("-- saved")});

					KOTEK_MESSAGE("scene is saved: {}", filename);
				}
				else
				{
					KOTEK_MESSAGE("user cancelled file saving");
				}

				return true;
			});

		this->m_p_console->RegisterCommand(
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::kConsoleCommand_SDK_Redo),
			[p_history_manager](
				const kotek::ktk::console_command_args_t& args) -> bool
			{
				p_history_manager->Redo();
				return true;
			});

		this->m_p_console->RegisterCommand(
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::kConsoleCommand_SDK_Undo),
			[p_history_manager](
				const kotek::ktk::console_command_args_t& args) -> bool
			{
				p_history_manager->Undo();
				return true;
			});

		this->m_p_console->RegisterCommand(
			static_cast<kotek::ktk::enum_base_t>(kotek::core::
					eConsoleCommandIndex::kConsoleCommand_SDK_DeleteEntity),
			[p_history_manager, this](
				const kotek::ktk::console_command_args_t& args) -> bool
			{
				KOTEK_ASSERT(args.empty() == false,
					"you can't pass an empty args here!");

				KOTEK_ASSERT(p_history_manager,
					"history manager in lambda is nullptr! can't be");

				entt::entity id = static_cast<entt::entity>(
					std::get<kotek::uint32_t>(args[0]));

				auto* p_placement_new_memory =
					p_history_manager->allocate_memory_for_command(
						sizeof(zircon_command_delete_entity),
						"zircon_command_delete_entity");

				zircon_command_delete_entity* p_command =
					new (p_placement_new_memory)
						zircon_command_delete_entity(p_history_manager,
							this->GetSceneManager()->GetCurrentScene(),
							this->get_factory_game(), id);

				p_history_manager->ExecuteCommand(p_command);

				return true;
			});

		this->m_p_console->RegisterCommand(
			static_cast<kotek::ktk::enum_base_t>(kotek::core::
					eConsoleCommandIndex::kConsoleCommand_SDK_CreateEntity),
			[p_history_manager, this](
				const kotek::ktk::console_command_args_t& args) -> bool
			{
				auto* p_placement_new_memory =
					p_history_manager->allocate_memory_for_command(
						sizeof(zircon_command_create_entity),
						"zircon_command_create_entity");

				zircon_command_create_entity* p_command =
					new (p_placement_new_memory)
						zircon_command_create_entity(p_history_manager,
							this->GetSceneManager()->GetCurrentScene());

				p_history_manager->ExecuteCommand(p_command);

				return true;
			});

		this->m_p_console->RegisterCommand(
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::
					kConsoleCommand_SDK_CreateComponentForEntity),
			[p_history_manager, this](
				const kotek::ktk::console_command_args_t& args) -> bool
			{
				KOTEK_ASSERT(
					args.empty() == false, "you can't an empty arguments here");

				KOTEK_ASSERT(args.size() >= 2,
					"not enough arguments. First argument is component "
					"name, second is entity id");

				const auto& component_name = std::get<const char*>(args[0]);

				entt::entity id = static_cast<entt::entity>(
					std::get<kotek::uint32_t>(args[1]));

				if (this->get_factory_game()->GetComponentByName(
						id, component_name) == nullptr)
				{
					if (this->get_factory_game()
							->HasRequiredComponentsForCreation(
								id, component_name) == false)
					{
						KOTEK_MESSAGE("Can't create component [{}] for "
									  "entity [{}] because it doesn't "
									  "have required components!",
							component_name, static_cast<kotek::uint32_t>(id));

						return true;
					}

					auto* p_placement_new_memory =
						p_history_manager->allocate_memory_for_command(
							sizeof(zircon_command_add_component_to_entity),
							"zircon_command_add_component_to_entity");

					zircon_command_add_component_to_entity* p_command =
						new (p_placement_new_memory)
							zircon_command_add_component_to_entity(
								this->get_factory_game(), id, component_name);

					p_history_manager->ExecuteCommand(p_command);
				}

				return true;
			});

		this->m_p_console->RegisterCommand(
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::
					kConsoleCommand_SDK_DeleteComponentFromEntity),
			[p_history_manager, this](
				const kotek::ktk::console_command_args_t& args) -> bool
			{
				KOTEK_ASSERT(args.empty() == false,
					"you can't pass an empty arguments here");

				KOTEK_ASSERT(args.size() >= 2,
					"not enough arguments. First argument is "
					"component_name, Second argument is id");

				const auto& p_component_name = std::get<const char*>(args[0]);

				entt::entity id = static_cast<entt::entity>(
					std::get<kotek::uint32_t>(args[1]));

				if (this->get_factory_game()->GetComponentByName(
						id, p_component_name))
				{
					auto* p_placement_new_memory =
						p_history_manager->allocate_memory_for_command(
							sizeof(zircon_command_delete_component_from_entity),
							"zircon_command_delete_component_from_entity");

					zircon_command_delete_component_from_entity* p_command =
						new (p_placement_new_memory)
							zircon_command_delete_component_from_entity(
								this->get_factory_game(), id, p_component_name);

					p_history_manager->ExecuteCommand(p_command);
				}

				return true;
			});

		this->m_p_console->RegisterCommand(
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::
					kConsoleCommand_SDK_CloseCurrentScene),
			[p_history_manager, this](
				const kotek::ktk::console_command_args_t& args) -> bool
			{
				if (this->GetSession_Editor())
				{
					this->GetSession_Editor()->shutdown();
					this->GetConsole()->PushCommand(
						static_cast<kotek::ktk::enum_base_t>(
							kotek::core::eConsoleCommandIndex::
								kConsoleCommand_App_AddTextToExistedWindowTitle),
						{{static_cast<kotek::ktk::enum_base_t>(kotek::core::
								 eWindowTitleType::kTitle_CurrentSceneName)},
							kotek::static_cstring_t<16>("")});
				}

				return true;
			});

		this->m_p_console->RegisterCommand(
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::
					kConsoleCommand_SDK_ShowModalWindow_SaveAndCloseOrCloseScene),
			[this](const kotek::ktk::console_command_args_t& args) -> bool
			{
				if (this->get_sdk_ui())
				{
					this->get_sdk_ui()->set_imgui_show_modal_save_scene(true);
				}

				return true;
			});
	}
}

void zircon_manager_game::Destroy_Console(void) noexcept
{
	KOTEK_ASSERT(this->m_p_console, "must be valid");

	this->m_p_console->Shutdown();
	delete this->m_p_console;
	this->m_p_console = nullptr;
}

void zircon_manager_game::Initialize_SDKUIManager(
	zircon_factory_game* p_factory) noexcept
{
	// TODO: change to flag
	this->m_is_use_sdk_imgui = this->m_p_main_manager->Get_EngineConfig()
								   ->IsContainsConsoleCommandLineArgument(
									   kotek::kConsoleCommandArg_Editor_ImGui);

#ifdef KOTEK_USE_SDK
	this->m_is_use_sdk = this->m_p_main_manager->Get_EngineConfig()
							 ->IsContainsConsoleCommandLineArgument(
								 kotek::kConsoleCommandArg_Editor);

	this->m_p_sdk_ui_manager =
		new sdk::ui::zircon_SDKUIManager(this->m_is_use_sdk,
			this->m_is_use_sdk_imgui, this->m_p_sdk_main_window);
#else
	this->m_p_sdk_ui_manager = new zircon_sdk_ui();
	this->m_p_sdk_ui_manager->initialize(p_factory);
#endif
}

void zircon_manager_game::Destroy_SDKUIManager(void) noexcept
{
	KOTEK_ASSERT(this->m_p_sdk_ui_manager, "must be valid");
	delete this->m_p_sdk_ui_manager;
	this->m_p_sdk_ui_manager = nullptr;
}

void zircon_manager_game::Initialize_Session(void) noexcept
{
	if (this->m_p_main_manager->Get_EngineConfig()
			->IsContainsConsoleCommandLineArgument(
				kotek::kConsoleCommandArg_Editor_ImGui))
	{
		this->m_p_session_editor = new zircon_session_editor();

		KOTEK_ASSERT(
			this->m_p_scene_manager, "you must initialize scene manager");
		KOTEK_ASSERT(this->m_p_scene_manager->GetCurrentScene(),
			"you must initialize scene");

		this->m_p_session_editor->initialize(
			this->m_p_scene_manager->GetCurrentScene(), this);
	}
	else
	{
		// TODO: probably we need to make a UI handling here, but think
		// here well!!!!!
		this->m_p_session_game = new zircon_session_game();
	}
}

void zircon_manager_game::Destroy_Session(void) noexcept
{
	if (this->m_p_main_manager->Get_EngineConfig()
			->IsContainsConsoleCommandLineArgument(
				kotek::kConsoleCommandArg_Editor_ImGui))
	{
		if (this->m_p_session_editor)
		{
			this->m_p_session_editor->shutdown();
			delete this->m_p_session_editor;
			this->m_p_session_editor = nullptr;

			if (this->m_p_session_game)
			{
				this->m_p_session_game->shutdown();
				delete this->m_p_session_game;
				this->m_p_session_game = nullptr;
			}
		}
	}
	else
	{
		KOTEK_ASSERT(this->m_p_session_editor == nullptr,
			"you can't have initialized session of editor, something "
			"is wrong!");

		if (this->m_p_session_game)
		{
			this->m_p_session_game->shutdown();
			delete this->m_p_session_game;
			this->m_p_session_game = nullptr;
		}
	}
}

void zircon_manager_game::Initialize_HistoryCommandManager(void) noexcept
{
	this->m_p_sdk_history_manager = new zircon_command_history();
	this->m_p_sdk_history_manager->initialize(
		this->m_p_main_manager->GetFileSystem(), this->m_p_scene_manager,
		this->m_p_factory, this->m_p_main_manager->GetResourceManager());
}

void zircon_manager_game::Destroy_HistoryCommandManager(void) noexcept
{
	if (this->m_p_sdk_history_manager)
	{
		this->m_p_sdk_history_manager->shutdown();

		delete this->m_p_sdk_history_manager;
		this->m_p_sdk_history_manager = nullptr;
	}
}

void zircon_manager_game::Initialize_UI(void) noexcept
{
	KOTEK_ASSERT(this->m_p_main_manager->Get_GameUIEngine(),
		"you have to initialize game ui engine");

	this->m_p_main_manager->Get_GameUIEngine()->Initialize(
		this->m_p_main_manager->Get_EngineConfig(),
		this->m_p_main_manager->Get_WindowManager()->ActiveWindow_GetHandle(),
		this->m_p_main_manager->Get_WindowManager()->ActiveWindow_GetWidth(),
		this->m_p_main_manager->Get_WindowManager()->ActiveWindow_GetHeight());
}

void zircon_manager_game::Destroy_UI(void) noexcept
{
	this->m_p_main_manager->Get_GameUIEngine()->Shutdown();
}

void zircon_manager_game::initialize_config(void) noexcept
{
	this->m_p_config = new zircon_config();
}

void zircon_manager_game::destroy_config(void) noexcept
{
	if (this->m_p_config)
	{
		delete this->m_p_config;
		this->m_p_config = nullptr;
	}
}

// TODO: delete this
void zircon_manager_game::UpdateInput(void) noexcept {}

// TODO: delete this
void zircon_manager_game::UpdateCamera(void) noexcept {}