#include "zircon_render_graph_pass_imgui.h"
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

		Kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_manager_main->Get_EngineConfig();

		if (p_config)
		{
			is_imgui_enabled = p_config->IsFeatureEnabled(
				Kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);
			is_sdk_enabled = p_config->IsFeatureEnabled(
				Kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK);
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
					this->m_p_imgui_wrapper->ImGui_ImplGlfw_Shutdown();
#else
	#error not implemented
#endif
				}

				this->m_p_imgui_wrapper->DestroyContext();
			}
		}

		p_config->SetFeatureStatus(Kotek::Core::eEngineFeatureSDK::
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

		Kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_manager_main->Get_EngineConfig();

		if (p_config)
		{
			is_imgui_enabled =
				this->m_p_manager_main->Get_EngineConfig()->IsFeatureEnabled(
					Kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);

			is_sdk_enabled =
				this->m_p_manager_main->Get_EngineConfig()->IsFeatureEnabled(
					Kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK);
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
						this->m_p_imgui_wrapper->ImGui_ImplGlfw_InitForOpenGL(
							p_handle, false),
						"failed to ImGui_ImplGlfw_InitForOpenGL");

					KOTEK_ASSERT(
						this->m_p_imgui_wrapper->ImGui_ImplOpenGL3_Init(),
						"failed to ImGui_ImplOpenGL3_Init");
#endif
				}
			}

			this->m_p_manager_main->Get_EngineConfig()->SetFeatureStatus(
				Kotek::Core::eEngineFeatureSDK::
					kEngine_Feature_SDK_ImGui_Initialized,
				true);
		}
	}

	void zircon_render_graph_pass_imgui_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		Kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_manager_main->Get_EngineConfig();

		bool is_imgui_enabled = false;

		if (p_config)
		{
			is_imgui_enabled = p_config->IsFeatureEnabled(
				Kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);
		}

		if (is_imgui_enabled)
		{
			if (this->m_p_imgui_wrapper)
			{
				this->m_p_imgui_wrapper->ImGui_ImplOpenGL3_NewFrame();
				this->m_p_imgui_wrapper->ImGui_ImplGlfw_NewFrame();

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
		Kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_manager_main->Get_EngineConfig();

		bool is_imgui_enabled = false;

		if (p_config)
		{
			is_imgui_enabled = p_config->IsFeatureEnabled(
				Kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);
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
	#else
		#error not implemented
	#endif
				}
#endif
			}
		}
	}
} // namespace no_streaming