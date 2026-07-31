#include "zircon_render_graph_pass_imgui.h"

#ifdef KOTEK_USE_WINDOW_LIBRARY_WIN32
	// task K17 phase 2: the imgui WndProc chain hook on the concrete
	// window class (the interface stays frozen)
	#include <kotek.core.window.win32/include/kotek_std_window_win32.h>
	#include <kotek.ui.imgui/include/imgui_impl_win32.h>

	// the official imgui win32 handler: dear imgui intentionally keeps
	// this declaration in a #if 0 block and tells consumers to
	// forward-declare it themselves (so the helper header stays
	// windows.h-free); the definition lives in imgui_impl_win32.cpp
	extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
		HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
#include "../../../../game/session/zircon_session_game.h"
#include "../../../../game/session/zircon_session_game_manager.h"

namespace no_streaming
{
	zircon_render_graph_pass_imgui_bgfx::zircon_render_graph_pass_imgui_bgfx(
		void) : zircon_render_graph_pass_bgfx(), m_p_imgui_wrapper{}
	{
	}

	zircon_render_graph_pass_imgui_bgfx::~zircon_render_graph_pass_imgui_bgfx(
		void)
	{
		bool is_imgui_enabled = false;
		bool is_sdk_enabled = false;

		kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_manager_main->Get_EngineConfig();

		if (p_config)
		{
			is_imgui_enabled = p_config->IsFeatureEnabled(
				kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);
			is_sdk_enabled = p_config->IsFeatureEnabled(
				kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK);
		}

		if (is_imgui_enabled)
		{
			if (this->m_p_imgui_wrapper)
			{
				this->m_p_imgui_wrapper->ImGui_ImplOpenGL3_Shutdown();

				if (is_sdk_enabled)
				{
				}
				else
				{
#ifdef KOTEK_USE_WINDOW_LIBRARY_GLFW
					this->m_p_imgui_wrapper->ImGui_ShutdownPlatform();
#elif defined(KOTEK_USE_WINDOW_LIBRARY_WIN32)
					// the wrapper maps the glfw-named calls onto the win32
					// backend in this configuration (task K17)
					this->m_p_imgui_wrapper->ImGui_ShutdownPlatform();
#else
	#error not implemented
#endif
				}

				this->m_p_imgui_wrapper->DestroyContext();
			}
		}

		p_config->SetFeatureStatus(kotek::Core::eEngineFeatureSDK::
									   kEngine_Feature_SDK_ImGui_Initialized,
			false);
	}

	void zircon_render_graph_pass_imgui_bgfx::OnCreateResources(
		kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
		this->m_p_manager_main = p_manager_main;
		this->m_p_imgui_wrapper = p_manager_main->Get_ImguiWrapper();

		bool is_imgui_enabled = false;
		bool is_sdk_enabled = false;

		kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_manager_main->Get_EngineConfig();

		if (p_config)
		{
			is_imgui_enabled =
				this->m_p_manager_main->Get_EngineConfig()->IsFeatureEnabled(
					kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);

			is_sdk_enabled =
				this->m_p_manager_main->Get_EngineConfig()->IsFeatureEnabled(
					kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK);
		}

		if (is_imgui_enabled)
		{
			if (this->m_p_imgui_wrapper)
			{
				this->m_p_imgui_wrapper->DebugCheckVersionAndDataLayout(
					IMGUI_VERSION,
					sizeof(ImGuiIO),
					sizeof(ImGuiStyle),
					sizeof(ImVec2),
					sizeof(ImVec4),
					sizeof(ImDrawVert),
					sizeof(ImDrawIdx));

				this->m_p_imgui_wrapper->CreateContext();

				this->m_p_imgui_wrapper->GetIO().ConfigFlags |=
					ImGuiConfigFlags_NavEnableKeyboard;

#ifdef KOTEK_USE_IMGUI_DOCKING
				this->m_p_imgui_wrapper->GetIO().ConfigFlags |=
					ImGuiConfigFlags_DockingEnable;
#endif

				this->m_p_imgui_wrapper->StyleColorsDark();

#ifdef KOTEK_USE_IMGUI_DOCKING
				if (this->m_p_imgui_wrapper->GetIO().ConfigFlags &
					ImGuiConfigFlags_ViewportsEnable)
				{
					this->m_p_imgui_wrapper->GetStyle().WindowRounding = 0.0f;
					this->m_p_imgui_wrapper->GetStyle()
						.Colors[ImGuiCol_WindowBg]
						.w = 1.0f;
				}
#endif

				if (is_sdk_enabled)
				{
				}
				else
				{
#ifdef KOTEK_USE_WINDOW_LIBRARY_GLFW
					GLFWwindow* p_handle = static_cast<GLFWwindow*>(
						this->m_p_manager_main->GetGameManager()
							->GetWindowHandle());

					KOTEK_ASSERT(
						this->m_p_imgui_wrapper->ImGui_InitForOpenGL(
							p_handle, false),
						"failed to ImGui_InitForOpenGL");

					KOTEK_ASSERT(
						this->m_p_imgui_wrapper->ImGui_ImplOpenGL3_Init(),
						"failed to ImGui_ImplOpenGL3_Init");
#elif defined(KOTEK_USE_WINDOW_LIBRARY_WIN32)
					// the own Win32 window backend (task K17): the wrapper
					// maps the init onto ImGui_ImplWin32_Init — the handle
					// is the HWND from GetWindowHandle
					auto* p_handle = this->m_p_manager_main->GetGameManager()
										 ->GetWindowHandle();

					KOTEK_ASSERT(
						this->m_p_imgui_wrapper->ImGui_InitForOpenGL(
							p_handle, false),
						"failed to ImGui_ImplWin32_Init (via the wrapper)");

					KOTEK_ASSERT(
						this->m_p_imgui_wrapper->ImGui_ImplOpenGL3_Init(),
						"failed to ImGui_ImplOpenGL3_Init");

					// task K17 phase 2: imgui's official win32 handler
					// rides the window's proc chain — without it imgui
					// sees no mouse/keyboard at all on this backend
					if (auto* p_active_window =
							this->m_p_manager_main->Get_WindowManager()
								->Get_ActiveWindow())
					{
						auto* p_win32_window =
							dynamic_cast<kotek::core::ktkWindowWin32*>(
								p_active_window);
						if (p_win32_window)
						{
							p_win32_window->Set_WndProcChain(
								&ImGui_ImplWin32_WndProcHandler);
						}
					}
#endif
				}
			}

			this->m_p_manager_main->Get_EngineConfig()->SetFeatureStatus(
				kotek::Core::eEngineFeatureSDK::
					kEngine_Feature_SDK_ImGui_Initialized,
				true);
		}
	}

	void zircon_render_graph_pass_imgui_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_manager_main->Get_EngineConfig();

		bool is_imgui_enabled = false;

		if (p_config)
		{
			is_imgui_enabled = p_config->IsFeatureEnabled(
				kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);
		}

		if (is_imgui_enabled)
		{
			if (this->m_p_imgui_wrapper)
			{
				this->m_p_imgui_wrapper->ImGui_ImplOpenGL3_NewFrame();
				this->m_p_imgui_wrapper->ImGui_NewFramePlatform();

				if (this->m_p_imgui_wrapper->GetIO().DeltaTime <= 0.0f)
					this->m_p_imgui_wrapper->GetIO().DeltaTime = 0.00001f;

				this->m_p_imgui_wrapper->NewFrame();
			}

			KOTEK_ASSERT(this->m_p_manager_session_game,
				"Did you call OnRegisterManagers because game session manager "
				"is "
				"not initialized!");

			zircon_session_game* p_session =
				this->m_p_manager_session_game->get_session(
					this->m_p_manager_session_game->get_current_session_id());

			KOTEK_ASSERT(p_session, "failed to obtain session game by id: {}",
				this->m_p_manager_session_game->get_current_session_id());

			// todo: provide imgui elements like in editor session impl
			KOTEK_ASSERT(false, "not implemented!");

			if (this->m_p_imgui_wrapper)
			{
				this->m_p_imgui_wrapper->Render();
			}
		}
	}

	void zircon_render_graph_pass_imgui_bgfx::OnRender(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_manager_main->Get_EngineConfig();

		bool is_imgui_enabled = false;

		if (p_config)
		{
			is_imgui_enabled = p_config->IsFeatureEnabled(
				kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);
		}

		if (is_imgui_enabled)
		{
			if (this->m_p_imgui_wrapper)
			{
				this->m_p_imgui_wrapper->ImGui_ImplOpenGL3_RenderDrawData(
					this->m_p_imgui_wrapper->GetDrawData());

#ifdef KOTEK_USE_IMGUI_DOCKING
				if (this->m_p_imgui_wrapper->GetIO().ConfigFlags &
					ImGuiConfigFlags_ViewportsEnable)
				{
	#ifdef KOTEK_USE_WINDOW_LIBRARY_GLFW
					GLFWwindow* p_current_context = glfwGetCurrentContext();

					this->m_p_imgui_wrapper->UpdatePlatformWindows();
					this->m_p_imgui_wrapper->RenderPlatformWindowsDefault();
					glfwMakeContextCurrent(p_current_context);
	#elif defined(KOTEK_USE_WINDOW_LIBRARY_WIN32)
					// docking viewports need the platform-windows wiring of
					// the window backend — phase 2 (task K17); the win32
					// backend still renders the main viewport
	#else
		#error not implemented
	#endif
				}
#endif
			}
		}
	}
} // namespace no_streaming