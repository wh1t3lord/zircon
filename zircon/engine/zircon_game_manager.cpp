#include "zircon_game_manager.h"
#include "../ecs/components/zircon_factory.h"
#include "zircon_resource_manager.h"
#include "../world/zircon_world_manager.h"

#include "zircon_session_game.h"
#include "zircon_session_game_manager.h"

#ifdef KOTEK_USE_SDK_IMGUI
	#include "../editor/zircon_session_editor.h"
	#include "../editor/zircon_session_editor_manager.h"
	#include "../editor/ui/zircon_ui_component_inspector.h"
	#include "../editor/ui/zircon_ui_object_list.h"
	#include "../editor/ui/zircon_ui_top_bar.h"
	#include "../editor/ui/zircon_ui_window_prefab.h"
	#include "../editor/ui/zircon_ui_window_prefab_browser.h"
	#include "../editor/ui/zircon_ui_window_history_command_log.h"
	#include "../editor/ui/zircon_ui_window_log.h"
	#include "../editor/ui/zircon_ui_window_render_stats.h"
	#include "../editor/ui/zircon_ui_window_settings.h"
	#include "../editor/ui/zircon_ui_window_debug_input.h"
	#include "../editor/ui/zircon_editor_ui_state.h"
	#include "../editor/commands/zircon_command_history.h"
	#include "../editor/commands/zircon_command_create_entity.h"
	#include "../editor/commands/zircon_command_delete_entity.h"
	#include "../editor/commands/zircon_command_delete_component_from_entity.h"
	#include "../editor/commands/zircon_command_add_component_to_entity.h"
#endif

#include "../render/gles3/zircon_renderer.h"
#include "../render/vk/zircon_renderer.h"
#include "../core/zircon_config.h"
#include "../core/zircon_console.h"
#include "../world/zircon_world.h"

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

	p_manager->GetGameManager()->GetConsole()->Push_Command(
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_Render_Resize),
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
	auto entity_id = p_game_manager->get_world_manager()
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

	if (p_manager)
	{
		kotek::core::ktkIInput* p_input = p_manager->Get_Input();

		if (p_input)
		{
			/*
			p_input->Set_ControllerData(
			    kotek::core::eInputControllerType::kControllerMouse,
			    kotek::core::eInputControllerMouseData::kMouseCoordinateX, );
			*/

			p_input->Set_ControllerData(
				kotek::core::eInputControllerType::kControllerMouse,
				kotek::core::eInputControllerMouseData::
					kMousePreviousCoordinateXInPixels,
				p_input->Get_ControllerData(
					kotek::core::eInputControllerType::kControllerMouse,
					kotek::core::eInputControllerMouseData::
						kMouseCoordinateXInPixels));
			p_input->Set_ControllerData(
				kotek::core::eInputControllerType::kControllerMouse,
				kotek::core::eInputControllerMouseData::
					kMousePreviousCoordinateYInPixels,
				p_input->Get_ControllerData(
					kotek::core::eInputControllerType::kControllerMouse,
					kotek::core::eInputControllerMouseData::
						kMouseCoordinateYInPixels));

			p_input->Set_ControllerData(
				kotek::core::eInputControllerType::kControllerMouse,
				kotek::core::eInputControllerMouseData::
					kMouseCoordinateXInPixels,
				xpos);
			p_input->Set_ControllerData(
				kotek::core::eInputControllerType::kControllerMouse,
				kotek::core::eInputControllerMouseData::
					kMouseCoordinateYInPixels,
				ypos);
			float width = static_cast<float>(
				p_manager->Get_WindowManager()->ActiveWindow_GetWidth());
			float height = static_cast<float>(
				p_manager->Get_WindowManager()->ActiveWindow_GetHeight());

			KOTEK_ASSERT(
				kotek::ktk::is_equal(width, 0.0f) == false, "wrong data");
			KOTEK_ASSERT(
				kotek::ktk::is_equal(height, 0.0f) == false, "wrong data");

			bool is_valid = true;
			if (kotek::ktk::is_equal(width, 0.0f))
			{
				is_valid = false;
			}

			if (kotek::ktk::is_equal(height, 0.0f))
			{
				is_valid = false;
			}

			if (is_valid)
			{
				p_input->Set_ControllerData(
					kotek::core::eInputControllerType::kControllerMouse,
					kotek::core::eInputControllerMouseData::
						kMouseCoordinateXNormalized,
					xpos / width);
				p_input->Set_ControllerData(
					kotek::core::eInputControllerType::kControllerMouse,
					kotek::core::eInputControllerMouseData::
						kMouseCoordinateYNormalized,
					ypos / height);
			}

			p_input->Set_ControllerUpdate(
				kotek::core::eInputControllerType::kControllerMouse);

			if constexpr (false)
			{
				float delta_x = xpos -
					p_input->Get_ControllerData(
						kotek::core::eInputControllerType::kControllerMouse,
						kotek::core::eInputControllerMouseData::
							kMousePreviousCoordinateXInPixels);
				float delta_y = ypos -
					p_input->Get_ControllerData(
						kotek::core::eInputControllerType::kControllerMouse,
						kotek::core::eInputControllerMouseData::
							kMousePreviousCoordinateYInPixels);

				KOTEK_TRACE("dx: {} - {} = {} dy: {} - {} = {}", xpos,
					p_input->Get_ControllerData(
						kotek::core::eInputControllerType::kControllerMouse,
						kotek::core::eInputControllerMouseData::
							kMousePreviousCoordinateXInPixels),
					delta_x, ypos,
					p_input->Get_ControllerData(
						kotek::core::eInputControllerType::kControllerMouse,
						kotek::core::eInputControllerMouseData::
							kMousePreviousCoordinateYInPixels),
					delta_y);
			}
		}
	}

	#ifdef KOTEK_USE_SDK_IMGUI
	if (p_manager->Get_EngineConfig()->IsFeatureEnabled(kotek::core::
				eEngineFeatureSDK::kEngine_Feature_SDK_ImGui_Initialized))
	{
		auto* p_wrapper_imgui = p_manager->Get_ImguiWrapper();

		p_wrapper_imgui->ImGui_ImplGlfw_CursorPosCallback(p_window, xpos, ypos);
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

		p_manager->Get_Input()->Update_Controller(&args);
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
		args.controller = kotek::core::eInputControllerType::kControllerMouse;

		p_manager->Get_Input()->Update_Controller(&args);
	}

	#ifdef KOTEK_USE_SDK_IMGUI
	if (p_manager->Get_EngineConfig()->IsFeatureEnabled(kotek::core::
				eEngineFeatureSDK::kEngine_Feature_SDK_ImGui_Initialized))
	{
		auto* p_wrapper_imgui = p_manager->Get_ImguiWrapper();

		p_wrapper_imgui->ImGui_ImplGlfw_MouseButtonCallback(
			p_window, button, action, mods);
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
	m_is_use_sdk{}, m_is_use_sdk_imgui{},
#ifdef KOTEK_USE_SDK_IMGUI
	m_session_editor_id{},
#endif
	m_world_id{}, m_session_game_id{},
	m_current_session_id{std::numeric_limits<kotek::uint8_t>::max()},
	m_p_profiler{}, m_p_console{}, m_p_main_manager{}, m_p_current_renderer{},
	m_p_window_console{}, m_p_current_session{}, m_p_renderer_gles3{},
	m_p_world_manager{}, m_p_resource_manager{}, m_p_config{},
	m_p_session_game_manager{}
#ifdef KOTEK_USE_SDK_IMGUI
	,
	m_p_session_editor_manager{}
#endif
{
}

zircon_manager_game::~zircon_manager_game(void) {}

void zircon_manager_game::Initialize(
	kotek::core::ktkMainManager* p_main_manager)
{
	this->m_p_main_manager = p_main_manager;

	this->initialize_input();

	this->Initialize_Console();
	this->initialize_config();
	//	this->Initialize_Factory();
	//	this->Initialize_SceneManager();
	this->Initialize_ResourceManager();
	//	this->Initialize_HistoryCommandManager();

	// TODO: do we really need it here???????
	//	this->Initialize_SDKUIManager(this->m_p_factory);

	// this->Initialize_Session();

	this->Initialize_Renderer();

	this->Initialize_UI();

	this->RegisterConsole_Commands();

	this->m_p_world_manager = new zircon_world_manager();
	this->m_world_id = this->m_p_world_manager->create_world();

	zircon_world* p_world =
		this->m_p_world_manager->get_world(this->m_world_id);

	KOTEK_ASSERT(p_world, "can't allocate world or something else");

#ifdef KOTEK_USE_SDK_IMGUI
	this->m_p_session_editor_manager = new zircon_session_editor_manager();
	this->m_session_editor_id =
		this->m_p_session_editor_manager->create_session();
	bool is_startup_imgui{};
	kotek::core::ktkIFrameworkConfig* pFrameworkConfig =
		p_main_manager->Get_EngineConfig();
	KOTEK_ASSERT(pFrameworkConfig, "must be initialized");

	if (pFrameworkConfig)
	{
		is_startup_imgui = pFrameworkConfig->IsFeatureEnabled(
			kotek::core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);

		if (is_startup_imgui)
		{
			zircon_session_editor* p_session =
				this->m_p_session_editor_manager->get_session(
					this->m_session_editor_id);

			if (p_session)
			{
				auto* p_engine_config =
					this->m_p_main_manager->Get_EngineConfig();

				KOTEK_ASSERT(p_engine_config,
					"you must initialize engine config for using this method");

				kotek::core::eEngineSupportedRenderer renderer =
					static_cast<kotek::core::eEngineSupportedRenderer>(
						p_engine_config->GetRendererVersion());

				kotek::uint8_t render_graph_id{-1};
				switch (renderer)
				{
				case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_0:
				case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_1:
				case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_2:
				{
					KOTEK_ASSERT(
						this->m_p_renderer_gles3, "must be iniitialized");

					if (this->m_p_renderer_gles3)
					{
						kotek::static_vector_t<
							kotek::render::gl::
								ktkRenderGraphSimplifiedRenderPass*,
							KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>
							passes;
						kotek::vector_t<kotek::core::ktkISDKUIElement*>
							ui_elements = {
								new zircon_editor_ui_state_object_list(),
								new zircon_editor_ui_state_top_bar(),
								new zircon_editor_ui_state_window_prefab(),
								new zircon_editor_ui_state_component_inspector(
									p_session->get_ui_state(),
									p_world->get_factory()),
								new zircon_editor_ui_state_window_log(),
								new zircon_ui_window_history_command_log(
									p_session->get_command_history()),
								new zircon_ui_window_render_stats(),
								new zircon_ui_window_settings(),
								new zircon_editor_ui_state_window_debug_input()};

						render_graph_id =
							this->m_p_renderer_gles3->create_render_graph(
								this->m_session_editor_id, passes, ui_elements);
					}
					break;
				}
				default:
				{
					KOTEK_ASSERT(false, "unsupported renderer");
					break;
				}
				}

				p_session->set_render_graph_id(render_graph_id);

				p_session->initialize("editor", this->m_session_editor_id,
					p_world, this->m_p_main_manager, this->m_p_console,
					this->m_p_main_manager->GetFileSystem(),
					this->m_p_resource_manager);

				if (this->m_p_console)
				{
					this->m_p_console->Push_Command(
						static_cast<kotek::ktk::enum_base_t>(
							eZirconConsoleCommands::
								set_current_editor_session_for_engine),
						{this->m_session_editor_id});
				}
			}
		}
	}
#endif

	// if editor create only when simulate goes otherwise create now and load
	// startup json config
	this->m_p_session_game_manager = new zircon_session_game_manager();
	this->m_session_game_id = this->m_p_session_game_manager->create_session();

	zircon_session_game* p_session_game =
		this->m_p_session_game_manager->get_session(this->m_session_game_id);

	KOTEK_ASSERT(p_session_game, "can't allocate game session!");

	if (p_session_game)
	{
		kotek::uint8_t render_graph_id{-1};

		auto* p_engine_config = this->m_p_main_manager->Get_EngineConfig();

		KOTEK_ASSERT(p_engine_config,
			"you must initialize engine config for using this method");

		kotek::core::eEngineSupportedRenderer renderer =
			static_cast<kotek::core::eEngineSupportedRenderer>(
				p_engine_config->GetRendererVersion());

#ifdef KOTEK_USE_SDK_IMGUI
		if (!is_startup_imgui)
#endif
		{
			switch (renderer)
			{
			case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_0:
			case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_1:
			case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_2:
			{
				kotek::static_vector_t<
					kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*,
					KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>
					passes;

				kotek::vector_t<kotek::core::ktkISDKUIElement*> ui_elements;

				KOTEK_ASSERT(this->m_p_renderer_gles3, "must be valid");

				render_graph_id = this->m_p_renderer_gles3->create_render_graph(
					p_session_game->get_id(), passes, ui_elements);

				break;
			}
			default:
			{
				KOTEK_ASSERT(false, "unsupported renderer!");
				break;
			}
			}

			p_session_game->set_render_graph_id(render_graph_id);

			KOTEK_ASSERT(this->m_p_console, "must be valid");
			if (this->m_p_console)
			{
				this->m_p_console->Push_Command(
					static_cast<kotek::enum_base_t>(eZirconConsoleCommands::
							set_current_render_graph_for_renderer),
					{render_graph_id});
			}
		}

		p_session_game->initialize("game", this->m_session_game_id, p_world);
	}

	this->RegisterConsole_Commands_SDK();
}

void zircon_manager_game::Shutdown(kotek::core::ktkMainManager* p_main_manager)
{
	this->Destroy_UI();
	//	this->Destroy_HistoryCommandManager();
	this->Destroy_Renderer();
	this->Destroy_ResourceManager();
	//	this->Destroy_SDKUIManager();
	this->Destroy_Console();
	//	this->Destroy_Session();
	//	this->Destroy_Factory();
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
	if (this->m_p_current_session)
	{
		this->m_p_current_session->update();
	}
}

void zircon_manager_game::UpdateAllSystems(void) noexcept
{
	this->UpdateInput();
	this->UpdateCamera();
}

zircon_world_manager* zircon_manager_game::get_world_manager(
	void) const noexcept
{
	return this->m_p_world_manager;
}

zircon_session_editor_manager* zircon_manager_game::get_session_editor_manager(
	void) const noexcept
{
	return this->m_p_session_editor_manager;
}

zircon_session_game_manager* zircon_manager_game::get_session_game_manager(
	void) const noexcept
{
	return this->m_p_session_game_manager;
}

kotek::uint8_t zircon_manager_game::get_session_editor_id(void) const noexcept
{
	return this->m_session_editor_id;
}

kotek::uint8_t zircon_manager_game::get_session_game_id(void) const noexcept
{
	return this->m_session_game_id;
}

zircon_session_editor* zircon_manager_game::get_session_editor(
	kotek::uint8_t session_id) const noexcept
{
	zircon_session_editor* p_result = nullptr;

	KOTEK_ASSERT(this->m_p_session_editor_manager, "early calling? Initialize please");

	if (!this->m_p_session_editor_manager)
	{
		KOTEK_MESSAGE_WARNING("failed to obtain session because you didn't initialize session editor manager!");
		return p_result;
	}

	if (this->m_p_session_editor_manager)
	{
		p_result = this->m_p_session_editor_manager->get_session(session_id);

		if (!p_result)
		{
			KOTEK_MESSAGE_WARNING("failed to obtain editor session by id: {}", session_id)
		}
	}


	return p_result;
}

zircon_session_game* zircon_manager_game::get_session_game(
	kotek::uint8_t session_id) const noexcept
{
	return nullptr;
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

void zircon_manager_game::Initialize_Renderer(void) noexcept
{
	this->m_p_window_console = new kotek::core::ktkWindowConsole();

	auto path = this->m_p_main_manager->GetFileSystem()->GetFolderByEnum(
		kotek::core::eFolderIndex::kFolderIndex_UserData);

	path /= KOTEK_USE_LOG_OUTPUT_FILE_NAME;

	int imgui_height = 0;

#ifdef KOTEK_USE_SDK_IMGUI
	// todo: think how to get imgui's height
	imgui_height = 19;
#endif

	if (this->m_p_main_manager->Get_Logger())
	{
		this->m_p_main_manager->Get_Logger()->Flush_All();
	}

	this->m_p_window_console->Initialize(
		this->m_p_main_manager->Get_WindowManager()->Get_ActiveWindow(),
		this->m_p_resource_manager, this->m_p_main_manager->Get_Input(),
		this->m_p_main_manager->Get_Logger(), this->m_p_console, imgui_height,
		path);

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

			this->m_p_renderer_gles3->Initialize(
				this->m_p_window_console, this->m_p_console);
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
	if (this->m_p_window_console)
	{
		this->m_p_window_console->Shutdown();
	}

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
	this->m_p_console->Initialize(zircon_user_console_translation_callback);
}

void zircon_manager_game::initialize_input(void) noexcept
{
	KOTEK_ASSERT(this->m_p_main_manager, "must be initialized!");
	KOTEK_ASSERT(this->m_p_main_manager->Get_Input(), "must be initialized!");

	if (this->m_p_main_manager && this->m_p_main_manager->Get_Input())
	{
		// todo: implement serialization thing and you take that information
		// from sys_info.json
		this->m_p_main_manager->Get_Input()->Initialize(
			kotek::core::eInputPlatformBackend::kPlatformBackend_GLFW3);
	}

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
}

void zircon_manager_game::RegisterConsole_Commands(void) noexcept
{
	auto* p_window_manager = this->m_p_main_manager->Get_WindowManager();

	KOTEK_ASSERT(p_window_manager, "you must initialize window manager");

	this->m_p_console->Register_Command(
		[p_window_manager](kotek::ktk::enum_base_t title_id,
			kotek::static_cstring_t<16> title) -> bool
		{
			if (p_window_manager->Get_ActiveWindow())
			{
				p_window_manager->Get_ActiveWindow()->SetStringToTitle(
					title_id, title.c_str());
			}

			return true;
		},
		static_cast<kotek::ktk::enum_base_t>(kotek::core::eConsoleCommandIndex::
				kConsoleCommand_App_AddTextToExistedWindowTitle));

	kotek::core::ktkIInput* p_input_manager =
		this->m_p_main_manager->Get_Input();
	this->m_p_console->Register_Command(
		[p_window_manager, p_input_manager](
			kotek::ktk::enum_base_t input_type) -> bool
		{
			if (p_window_manager->Get_ActiveWindow())
			{
#ifdef KOTEK_DEBUG // for debugging purposes, lazing without specifying in
                   // debugger manually (watch)
				kotek::core::eInputType type =
					static_cast<kotek::core::eInputType>(input_type);
#endif
				p_window_manager->Get_ActiveWindow()->Set_InputType(input_type);
			}

			return true;
		},
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_Input_Type));

	auto* p_main_manager = this->m_p_main_manager;

	auto* p_resource_manager = p_main_manager->GetResourceManager();

	this->m_p_console->Register_Command(

		[this](
			kotek::ktk::shared_ptr<kotek::core::ktkResourceHandle> ptr_resource,
			kotek::ktk::enum_base_t bounding_primitive_type,
			kotek::uint32_t entity) -> bool
		{
			KOTEK_ASSERT(ptr_resource.get(),
				"must be initialize pointer, did you have a corrupted memory "
				"or something broken in codebase?");

			if (!this->m_p_current_session)
			{
				KOTEK_MESSAGE_WARNING(
					"you need to set session before calling this command!");
				return false;
			}

			zircon_factory* p_factory = nullptr;

			if (this->m_p_current_session)
			{
				eZirconSessionType session_type =
					this->m_p_current_session->get_type();

				switch (session_type)
				{
				case eZirconSessionType::kEditor:
				{
					zircon_session_editor* p_editor =
						dynamic_cast<zircon_session_editor*>(
							this->m_p_current_session);

					KOTEK_ASSERT(p_editor,
						"are you sure that it is editor session? Otherwise "
						"correct return type of ::get_type method in your "
						"implementation!");

					if (p_editor)
					{
						if (p_editor->get_world())
						{
							p_factory = p_editor->get_world()->get_factory();
						}
						else
						{
							KOTEK_MESSAGE_WARNING(
								"you session didn't register world thus we "
								"can't obtain factory!");
							return false;
						}
					}

					break;
				}
				case eZirconSessionType::kGame:
				{
					KOTEK_MESSAGE_WARNING(
						"this command is for \"editor\" session type!");
					return false;
				}
				}
			}

			KOTEK_ASSERT(p_factory,
				"you must call this command when factory is initialized, "
				"something is wrong!");

			if (!p_factory)
			{
				KOTEK_MESSAGE_WARNING("failed to obtain factory in world!");
				return false;
			}

			// todo: you should stream geometry and upload at the same time!
			KOTEK_ASSERT(false,
				"you should stream geometry and upload at the same time!");

			if (p_factory)
			{
				auto p_resource_geometry =
					reinterpret_cast<kotek::Render::gl::ktkGeometry*>(
						ptr_resource.get()->Get_Resource());
				auto bounding_type =
					static_cast<kotek::core::eRenderBoundingPrimitiveType>(
						bounding_primitive_type);
				auto entity_id = static_cast<entt::entity>(entity);

				auto status_geometry =
					p_factory->HasComponent<zircon_component_geometry>(
						entity_id);

				if (!status_geometry)
				{
					KOTEK_MESSAGE_WARNING(
						"you must have component geometry to calculate the "
						"primitive without geometry can't process this "
						"command!");
					return false;
				}

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
						this->GetConsole()->Execute_Command(
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
		},
		static_cast<kotek::ktk::enum_base_t>(kotek::core::eConsoleCommandIndex::
				kConsoleCommand_Render_CalculateBoundingPrimitive));

	this->m_p_console->Register_Command(
		[p_resource_manager](kotek::core::ktkLoadingRequest req) -> bool
		{
			KOTEK_ASSERT(p_resource_manager,
				"you must have valid resource manager here");

			if (p_resource_manager)
			{
				p_resource_manager->Load(req);
			}

			return true;
		},
		static_cast<kotek::ktk::enum_base_t>(kotek::core::eConsoleCommandIndex::
				kConsoleCommand_ResourceManager_Load));

	auto p_command_resize = [p_main_manager](int width, int height) -> bool
	{
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

	auto p_command_close_application = [p_main_manager]() -> bool
	{
		p_main_manager->Get_EngineConfig()->SetApplicationWorking(false);

		return true;
	};

	kotek::core::ktkIRenderer* p_current_renderer = this->m_p_current_renderer;

	auto p_command_sdk_show_window = [p_main_manager, p_current_renderer, this](
										 int window_id) -> bool
	{
		if (!p_current_renderer)
		{
			KOTEK_MESSAGE_WARNING(
				"you can't call this command if renderer wasn't initialized");
			return false;
		}

		const auto& imgui_elements = this->get_ui_imgui_elements();

		if (imgui_elements.empty())
		{
			KOTEK_MESSAGE_WARNING("engine didn't register any window on "
								  "renderer side... Unable to proceed...");
			return false;
		}

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

	auto p_command_sdk_hide_window = [p_main_manager, p_current_renderer, this](
										 int window_id) -> bool
	{
		if (!p_current_renderer)
		{
			KOTEK_MESSAGE_WARNING(
				"you can't call this command if renderer wasn't initialized");
			return false;
		}

		const auto& imgui_elements = this->get_ui_imgui_elements();

		if (imgui_elements.empty())
		{
			KOTEK_MESSAGE_WARNING("engine didn't register any window on "
								  "renderer side... Unable to proceed...");
			return false;
		}

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

	auto p_command_set_current_game_session_for_engine =
		[p_main_manager, this](kotek::uint8_t session_id) -> bool
	{
		KOTEK_ASSERT(this->m_p_session_game_manager, "must be valid");
		zircon_session_game* p_session_game =
			this->m_p_session_game_manager->get_session(session_id);
		KOTEK_ASSERT(p_session_game, "obtain session failed!");

		this->m_p_current_session = p_session_game;

		if (p_session_game)
		{
			kotek::core::eEngineSupportedRenderer renderer_version =
				static_cast<kotek::core::eEngineSupportedRenderer>(
					p_main_manager->Get_EngineConfig()->GetRendererVersion());

			switch (renderer_version)
			{
			case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_0:
			case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_1:
			case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_2:
			{
				KOTEK_ASSERT(this->m_p_renderer_gles3, "must be initialized");

				if (this->m_p_renderer_gles3)
				{
					this->m_p_renderer_gles3->set_current_render_graph(
						p_session_game->get_render_graph_id());
				}

				break;
			}
			default:
			{
				KOTEK_ASSERT(false, "unsupported renderer!");
				break;
			}
			}
		}
		else
		{
			KOTEK_MESSAGE_WARNING("can't set session#{} as current because it "
								  "doesn't present in session pool!",
				session_id);
		}

		return !!(p_session_game);
	};

	auto p_command_set_current_editor_session_for_engine =
		[p_main_manager, this](kotek::uint8_t session_id) -> bool
	{
		KOTEK_ASSERT(this->m_p_session_editor_manager, "must be valid");
		zircon_session_editor* p_session_editor =
			this->m_p_session_editor_manager->get_session(session_id);
		KOTEK_ASSERT(p_session_editor, "obtain session failed!");

		if (p_session_editor)
		{
			this->m_p_current_session = p_session_editor;

#ifdef KOTEK_DEBUG
			KOTEK_MESSAGE("set current editor session#{}", session_id);
#endif

			kotek::core::eEngineSupportedRenderer renderer_version =
				static_cast<kotek::core::eEngineSupportedRenderer>(
					p_main_manager->Get_EngineConfig()->GetRendererVersion());

			switch (renderer_version)
			{
			case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_0:
			case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_1:
			case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_2:
			{
				KOTEK_ASSERT(this->m_p_renderer_gles3, "must be initialized");

				if (this->m_p_renderer_gles3)
				{
					this->m_p_renderer_gles3->set_current_render_graph(
						p_session_editor->get_render_graph_id());
				}

				break;
			}
			default:
			{
				KOTEK_ASSERT(false, "unsupported renderer!");
				break;
			}
			}
		}
		else
		{
			KOTEK_MESSAGE_WARNING("can't set session#{} as current because it "
								  "doesn't present in session pool!",
				session_id);
		}

		return !!(p_session_editor);
	};

	auto p_command_set_current_render_graph_for_renderer =
		[p_main_manager, this](kotek::uint8_t render_graph_id) -> bool
	{
		kotek::core::eEngineSupportedRenderer renderer_version =
			static_cast<kotek::core::eEngineSupportedRenderer>(
				p_main_manager->Get_EngineConfig()->GetRendererVersion());

		switch (renderer_version)
		{
		case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_0:
		case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_1:
		case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_2:
		{
			KOTEK_ASSERT(this->m_p_renderer_gles3, "must be initialized");

			if (this->m_p_renderer_gles3)
			{
				this->m_p_renderer_gles3->set_current_render_graph(
					render_graph_id);
			}

			break;
		}
		default:
		{
			KOTEK_ASSERT(false, "unsupported renderer!");
			break;
		}
		}

		return true;
	};

	auto p_command_sdk_print_registered_windows = [p_main_manager]() -> bool
	{ return true; };

	this->m_p_console->Register_Command(p_command_sdk_show_window,
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_SDK_ShowWindow));

	this->m_p_console->Register_Command(p_command_sdk_hide_window,
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_SDK_HideWindow));

	this->m_p_console->Register_Command(p_command_resize,
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_Render_Resize));

	this->m_p_console->Register_Command(p_command_close_application,
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_App_Close));

	/* user defined custom commands */
	this->m_p_console->Register_Command(
		p_command_set_current_game_session_for_engine,
		static_cast<kotek::ktk::enum_base_t>(
			eZirconConsoleCommands::set_current_game_session_for_engine));

	this->m_p_console->Register_Command(
		p_command_set_current_editor_session_for_engine,
		static_cast<kotek::ktk::enum_base_t>(
			eZirconConsoleCommands::set_current_editor_session_for_engine));

	this->m_p_console->Register_Command(
		p_command_set_current_render_graph_for_renderer,
		static_cast<kotek::ktk::enum_base_t>(
			eZirconConsoleCommands::set_current_render_graph_for_renderer));
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
		KOTEK_ASSERT(this->m_p_session_editor_manager,
			"must be initialized before calling this method!");

		this->m_p_console->Register_Command(
			[this](kotek::static_path_t path_to_file) -> bool
			{
				KOTEK_ASSERT(this->m_p_session_editor_manager,
					"initialize this editor session manager please!");

				if (!this->m_p_session_editor_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"you didn't initialize editor session manager can't "
						"proceed further...");
					return false;
				}

				zircon_session_editor* p_session = nullptr;

				if (this->m_p_session_editor_manager)
				{
					p_session = this->m_p_session_editor_manager->get_session(
						this->m_session_editor_id);
				}

				KOTEK_ASSERT(p_session,
					"failed to obtain editor session by id: {}",
					this->m_session_editor_id);

				if (!p_session)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain editor session by id: {}",
						this->m_session_editor_id);
					return false;
				}

				this->GetConsole()->Execute_Command(
					static_cast<kotek::ktk::enum_base_t>(
						kotek::core::eConsoleCommandIndex::
							kConsoleCommand_SDK_CloseCurrentScene));

				p_session->Deserialize(path_to_file);

				return true;
			},
			static_cast<kotek::ktk::enum_base_t>(kotek::core::
					eConsoleCommandIndex::kConsoleCommand_SDK_LoadScene));

		this->m_p_console->Register_Command(
			[this](kotek::static_path_t custom_path) -> bool
			{
				KOTEK_ASSERT(this->m_p_session_editor_manager,
					"initialize this editor session manager please!");

				if (!this->m_p_session_editor_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"you didn't initialize editor session manager thus we "
						"can't proceed further...");
					return false;
				}

				zircon_session_editor* p_session = nullptr;
				zircon_editor_command_history* p_history_manager = nullptr;

				if (this->m_p_session_editor_manager)
				{
					p_session = this->m_p_session_editor_manager->get_session(
						this->m_session_editor_id);
				}

				KOTEK_ASSERT(p_session, "failed to obtain session by id: {}",
					this->m_session_editor_id);

				if (!p_session)
				{
					KOTEK_MESSAGE_WARNING("failed to obtain session by id: {}",
						this->m_session_editor_id);
					return false;
				}

				if (p_session)
				{
					p_history_manager = p_session->get_command_history();
				}

				KOTEK_ASSERT(p_history_manager,
					"failed to obtain history manager of editor "
					"session_{}#{}!",
					p_session->get_session_name(), p_session->get_id());

				if (!p_history_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain history manager of editor "
						"session_{}#{}!",
						p_session->get_session_name(), p_session->get_id());
					return false;
				}

				kotek::static_path_t filename;

				if (custom_path.empty())
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
					filename = custom_path;

					bool is_valid =
						this->GetMainManager()->GetFileSystem()->IsValidPath(
							filename.parent_path());

					KOTEK_ASSERT(
						is_valid, "can't save to non-existed path on your OS");

					if (!is_valid)
					{
						KOTEK_MESSAGE_WARNING("can't save scene to the "
											  "following path: {} filename: {}",
							filename.parent_path(), filename.filename());
						return false;
					}

					KOTEK_MESSAGE(
						"saving scene (user passed args): {}", filename);
				}

				if (filename.empty() == false)
				{
					p_session->Serialize(filename.c_str());

					p_history_manager->set_changed(false);

					this->GetConsole()->Push_Command(
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
			},
			static_cast<kotek::ktk::enum_base_t>(kotek::core::
					eConsoleCommandIndex::kConsoleCommand_SDK_SaveScene));

		this->m_p_console->Register_Command(
			[this]() -> bool
			{
				KOTEK_ASSERT(this->m_p_session_editor_manager,
					"you need to initialize session editor manager!");

				if (!this->m_p_session_editor_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"you didn't initialize session editor manager so can't "
						"proceed further...");
					return false;
				}

				zircon_editor_command_history* p_history_manager = nullptr;

				if (this->m_p_session_editor_manager)
				{
					zircon_session_editor* p_session =
						this->m_p_session_editor_manager->get_session(
							this->m_session_editor_id);

					KOTEK_ASSERT(p_session,
						"failed to obtain editor session by id: {}",
						this->m_session_editor_id);

					if (p_session)
					{
						p_history_manager = p_session->get_command_history();
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"failed to obtain editor session by id: {}",
							this->m_session_editor_id);
						return false;
					}
				}

				KOTEK_ASSERT(
					p_history_manager, "failed to obtain history manager");

				if (!p_history_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain history manager in editor session!");
					return false;
				}

				p_history_manager->Redo();
				return true;
			},
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::kConsoleCommand_SDK_Redo));

		this->m_p_console->Register_Command(
			[this]() -> bool
			{
				KOTEK_ASSERT(this->m_p_session_editor_manager,
					"you need to initialize session editor manager!");

				if (!this->m_p_session_editor_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"you didn't initialize session editor manager so can't "
						"proceed further...");
					return false;
				}

				zircon_editor_command_history* p_history_manager = nullptr;

				if (this->m_p_session_editor_manager)
				{
					zircon_session_editor* p_session =
						this->m_p_session_editor_manager->get_session(
							this->m_session_editor_id);

					KOTEK_ASSERT(p_session,
						"failed to obtain editor session by id: {}",
						this->m_session_editor_id);

					if (p_session)
					{
						p_history_manager = p_session->get_command_history();
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"failed to obtain editor session by id: {}",
							this->m_session_editor_id);
						return false;
					}
				}

				KOTEK_ASSERT(
					p_history_manager, "failed to obtain history manager");

				if (!p_history_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain history manager in editor session!");
					return false;
				}

				p_history_manager->Undo();
				return true;
			},
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::kConsoleCommand_SDK_Undo));

		this->m_p_console->Register_Command(
			[this](kotek::uint32_t entity) -> bool
			{
				KOTEK_ASSERT(
					this->m_p_session_editor_manager, "early calling?");

				if (!this->m_p_session_editor_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"you didn't initialize editor session manager so can't "
						"proceed further...");
					return false;
				}

				entt::entity id = static_cast<entt::entity>(entity);

				zircon_session_editor* p_session = nullptr;
				zircon_editor_command_history* p_history_manager = nullptr;
				zircon_factory* p_factory = nullptr;

				if (this->m_p_session_editor_manager)
				{
					p_session = this->m_p_session_editor_manager->get_session(
						this->m_session_editor_id);
				}

				KOTEK_ASSERT(p_session, "must be initialized!");

				if (!p_session)
				{
					KOTEK_MESSAGE_WARNING("failed to obtain session by id: {}",
						this->m_session_editor_id);
					return false;
				}

				if (p_session)
				{
					if (p_session->get_world())
					{
						p_factory = p_session->get_world()->get_factory();
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"you didn't set world to session#{}",
							p_session->get_id());
						return false;
					}
				}

				KOTEK_ASSERT(
					p_factory, "world doesn't have factory, initialize please");

				if (!p_factory)
				{
					KOTEK_MESSAGE_WARNING("failed to obtain factory from "
										  "world_{}#{} of session_{}#{}",
						p_session->get_world()->get_name(),
						p_session->get_world()->get_id(),
						p_session->get_session_name(), p_session->get_id());
					return false;
				}

				KOTEK_ASSERT(p_history_manager,
					"history manager in lambda is nullptr! can't be");

				if (!p_history_manager)
				{
					KOTEK_MESSAGE_WARNING("failed to obtain command history "
										  "manager from your session#{}",
						p_session->get_id());
					return false;
				}

				auto* p_placement_new_memory =
					p_history_manager->allocate_memory_for_command(
						sizeof(zircon_command_delete_entity),
						"zircon_command_delete_entity");

				zircon_command_delete_entity* p_command =
					new (p_placement_new_memory)
						zircon_command_delete_entity(p_history_manager,
							p_session->get_world(), p_factory, id);

				p_history_manager->ExecuteCommand(p_command);

				return true;
			},
			static_cast<kotek::ktk::enum_base_t>(kotek::core::
					eConsoleCommandIndex::kConsoleCommand_SDK_DeleteEntity));

		this->m_p_console->Register_Command(
			[this]() -> bool
			{
				KOTEK_ASSERT(this->m_p_session_editor_manager,
					"you need to initialize session editor manager!");

				if (!this->m_p_session_editor_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"you didn't initialize session editor manager so can't "
						"proceed further...");
					return false;
				}

				zircon_editor_command_history* p_history_manager = nullptr;
				zircon_world* p_world = nullptr;

				if (this->m_p_session_editor_manager)
				{
					zircon_session_editor* p_session =
						this->m_p_session_editor_manager->get_session(
							this->m_session_editor_id);

					KOTEK_ASSERT(p_session,
						"failed to obtain editor session by id: {}",
						this->m_session_editor_id);

					if (p_session)
					{
						p_history_manager = p_session->get_command_history();
						p_world = p_session->get_world();
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"failed to obtain editor session by id: {}",
							this->m_session_editor_id);
						return false;
					}
				}

				KOTEK_ASSERT(
					p_world, "failed to obtain world in editor session!");

				if (!p_world)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain world in editor session!");
					return false;
				}

				KOTEK_ASSERT(
					p_history_manager, "failed to obtain history manager");

				if (!p_history_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain history manager in editor session!");
					return false;
				}

				auto* p_placement_new_memory =
					p_history_manager->allocate_memory_for_command(
						sizeof(zircon_command_create_entity),
						"zircon_command_create_entity");

				zircon_command_create_entity* p_command =
					new (p_placement_new_memory) zircon_command_create_entity(
						p_history_manager, p_world);

				p_history_manager->ExecuteCommand(p_command);

				return true;
			},
			static_cast<kotek::ktk::enum_base_t>(kotek::core::
					eConsoleCommandIndex::kConsoleCommand_SDK_CreateEntity));

		this->m_p_console->Register_Command(
			[this](const char* component_name, kotek::uint32_t entity) -> bool
			{
				KOTEK_ASSERT(this->m_p_session_editor_manager,
					"you need to initialize session editor manager!");

				if (!this->m_p_session_editor_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"you didn't initialize session editor manager so can't "
						"proceed further...");
					return false;
				}

				zircon_editor_command_history* p_history_manager = nullptr;
				zircon_factory* p_factory = nullptr;

				if (this->m_p_session_editor_manager)
				{
					zircon_session_editor* p_session =
						this->m_p_session_editor_manager->get_session(
							this->m_session_editor_id);

					KOTEK_ASSERT(p_session,
						"failed to obtain editor session by id: {}",
						this->m_session_editor_id);

					if (p_session)
					{
						p_history_manager = p_session->get_command_history();

						if (p_session->get_world())
						{
							p_factory = p_session->get_world()->get_factory();
						}
						else
						{
							KOTEK_MESSAGE_WARNING(
								"failed to get world in session_{}#{}",
								p_session->get_session_name(),
								p_session->get_id());
							return false;
						}
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"failed to obtain editor session by id: {}",
							this->m_session_editor_id);
						return false;
					}
				}

				KOTEK_ASSERT(
					p_factory, "failed to obtain factory in editor session!");

				if (!p_factory)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain factory in editor session!");
					return false;
				}

				KOTEK_ASSERT(
					p_history_manager, "failed to obtain history manager");

				if (!p_history_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain history manager in editor session!");
					return false;
				}

				entt::entity id = static_cast<entt::entity>(entity);

				if (p_factory->GetComponentByName(id, component_name) ==
					nullptr)
				{
					if (p_factory->HasRequiredComponentsForCreation(
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
								p_factory, id, component_name);

					p_history_manager->ExecuteCommand(p_command);
				}

				return true;
			},
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::
					kConsoleCommand_SDK_CreateComponentForEntity));

		this->m_p_console->Register_Command(
			[this](const char* p_component_name, kotek::uint32_t entity) -> bool
			{
				KOTEK_ASSERT(this->m_p_session_editor_manager,
					"you need to initialize session editor manager!");

				if (!this->m_p_session_editor_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"you didn't initialize session editor manager so can't "
						"proceed further...");
					return false;
				}

				zircon_editor_command_history* p_history_manager = nullptr;
				zircon_factory* p_factory = nullptr;

				if (this->m_p_session_editor_manager)
				{
					zircon_session_editor* p_session =
						this->m_p_session_editor_manager->get_session(
							this->m_session_editor_id);

					KOTEK_ASSERT(p_session,
						"failed to obtain editor session by id: {}",
						this->m_session_editor_id);

					if (p_session)
					{
						p_history_manager = p_session->get_command_history();

						if (p_session->get_world())
						{
							p_factory = p_session->get_world()->get_factory();
						}
						else
						{
							KOTEK_MESSAGE_WARNING(
								"failed to get world in session_{}#{}",
								p_session->get_session_name(),
								p_session->get_id());
							return false;
						}
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"failed to obtain editor session by id: {}",
							this->m_session_editor_id);
						return false;
					}
				}

				KOTEK_ASSERT(
					p_factory, "failed to obtain factory in editor session!");

				if (!p_factory)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain factory in editor session!");
					return false;
				}

				KOTEK_ASSERT(
					p_history_manager, "failed to obtain history manager");

				if (!p_history_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain history manager in editor session!");
					return false;
				}

				entt::entity id = static_cast<entt::entity>(entity);

				if (p_factory->GetComponentByName(id, p_component_name))
				{
					auto* p_placement_new_memory =
						p_history_manager->allocate_memory_for_command(
							sizeof(zircon_command_delete_component_from_entity),
							"zircon_command_delete_component_from_entity");

					zircon_command_delete_component_from_entity* p_command =
						new (p_placement_new_memory)
							zircon_command_delete_component_from_entity(
								p_factory, id, p_component_name);

					p_history_manager->ExecuteCommand(p_command);
				}

				return true;
			},
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::
					kConsoleCommand_SDK_DeleteComponentFromEntity));

		this->m_p_console->Register_Command(
			[this]() -> bool
			{
				KOTEK_ASSERT(this->m_p_session_editor_manager,
					"you need to initialize editor session manager!");

				if (!this->m_p_session_editor_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"you didn't initialize session editor manager can't "
						"proceed further!");
					return false;
				}

				zircon_session_editor* p_session = nullptr;

				if (this->m_p_session_editor_manager)
				{
					p_session = this->m_p_session_editor_manager->get_session(
						this->m_session_editor_id);
				}

				KOTEK_ASSERT(p_session,
					"failed to obtain editor session by id: {}",
					this->m_session_editor_id);

				if (!p_session)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain editor session by id: {}",
						this->m_session_editor_id);
					return false;
				}

				if (p_session)
				{
					//	p_session->shutdown();
				    // todo: correct that thing and call for unload in session
				    // (because we don't need to shutdown session we need to
				    // unload world of current session)

					KOTEK_ASSERT(false, "not implemented!");
					this->GetConsole()->Push_Command(
						static_cast<kotek::ktk::enum_base_t>(
							kotek::core::eConsoleCommandIndex::
								kConsoleCommand_App_AddTextToExistedWindowTitle),
						{{static_cast<kotek::ktk::enum_base_t>(kotek::core::
								 eWindowTitleType::kTitle_CurrentSceneName)},
							kotek::static_cstring_t<16>("")});
				}

				return true;
			},
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::
					kConsoleCommand_SDK_CloseCurrentScene));

		this->m_p_console->Register_Command(
			[this]() -> bool
			{
				KOTEK_ASSERT(this->m_p_session_editor_manager,
					"you didn't initialize session editor manager!");

				if (!this->m_p_session_editor_manager)
				{
					KOTEK_MESSAGE_WARNING(
						"you didn't initialize session editor manager can't "
						"proceed further!");
					return false;
				}

				zircon_editor_ui_state* p_state = nullptr;
				if (this->m_p_session_editor_manager)
				{
					zircon_session_editor* p_session =
						this->m_p_session_editor_manager->get_session(
							this->m_session_editor_id);

					if (p_session)
					{
						p_state = p_session->get_ui_state();
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"failed to obtain editor session by id: {}",
							this->m_session_editor_id);
						return false;
					}
				}

				KOTEK_ASSERT(
					p_state, "failed to obtain ui state in editor session!");

				if (!p_state)
				{
					KOTEK_MESSAGE_WARNING(
						"failed to obtain ui state in editor session!");
					return false;
				}

				if (p_state)
				{
					p_state->set_imgui_show_modal_save_scene(true);
				}

				return true;
			},
			static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::
					kConsoleCommand_SDK_ShowModalWindow_SaveAndCloseOrCloseScene));
	}
}

void zircon_manager_game::Destroy_Console(void) noexcept
{
	KOTEK_ASSERT(this->m_p_console, "must be valid");

	this->m_p_console->Shutdown();
	delete this->m_p_console;
	this->m_p_console = nullptr;
}

void zircon_manager_game::Initialize_UI(void) noexcept
{
	KOTEK_ASSERT(this->m_p_main_manager->Get_GameUIEngine(),
		"you have to initialize game ui engine");
	KOTEK_ASSERT(this->m_p_resource_manager, "must initialize this first!");

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

const kotek::vector_t<kotek::core::ktkISDKUIElement*>&
zircon_manager_game::get_ui_imgui_elements()
{
	kotek::vector_t<kotek::core::ktkISDKUIElement*> empty;

	if (!this->m_p_current_session)
	{
		KOTEK_ASSERT(false, "early calling? Otherwise set current session!");
		return empty;
	}

	if (this->m_p_current_session)
	{
		eZirconSessionType session_type = this->m_p_current_session->get_type();

		switch (session_type)
		{
		case eZirconSessionType::kEditor:
		{
			zircon_session_editor* p_session =
				dynamic_cast<zircon_session_editor*>(this->m_p_current_session);

			KOTEK_ASSERT(p_session,
				"are you sure that you didn't mistaken return type of "
				"::get_type method?");

			if (p_session)
			{
				return this->get_ui_imgui_elements(
					p_session->get_render_graph_id());
			}

			break;
		}
		case eZirconSessionType::kGame:
		{
			zircon_session_game* p_session =
				dynamic_cast<zircon_session_game*>(this->m_p_current_session);

			KOTEK_ASSERT(p_session,
				"are you sure that you didn't mistaken return type of "
				"::get_type method?");

			if (p_session)
			{
				return this->get_ui_imgui_elements(
					p_session->get_render_graph_id());
			}

			break;
		}
		default:
		{
			KOTEK_ASSERT(false,
				"unknown session type report to developers! Otherwise memory "
				"corruptuion");
			return empty;
		}
		}
	}

	return empty;
}

const kotek::vector_t<kotek::core::ktkISDKUIElement*>&
zircon_manager_game::get_ui_imgui_elements(kotek::uint8_t render_graph_id)
{
	auto* p_engine_config = this->m_p_main_manager->Get_EngineConfig();

	KOTEK_ASSERT(p_engine_config,
		"you must initialize engine config for using this method");

	kotek::core::eEngineSupportedRenderer renderer =
		static_cast<kotek::core::eEngineSupportedRenderer>(
			p_engine_config->GetRendererVersion());

	switch (renderer)
	{
	case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_0:
	case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_1:
	case kotek::core::eEngineSupportedRenderer::kOpenGLES_3_2:
	{
		KOTEK_ASSERT(this->m_p_renderer_gles3, "must be iniitialized");

		if (this->m_p_renderer_gles3)
		{
			if (this->m_p_renderer_gles3->is_render_graph_presented(
					render_graph_id))
			{
				return this->m_p_renderer_gles3->get_ui_imgui_elements(
					render_graph_id);
			}
		}

		break;
	}
	default:
	{
		KOTEK_ASSERT(false, "unsupported renderer");
		break;
	}
	}

	return {};
}
