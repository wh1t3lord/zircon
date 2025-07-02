#include "zircon_render_graph_pass_editor_imgui.h"
#include "../../../../editor/session/zircon_session_editor.h"
#include "../../../../editor/session/zircon_session_editor_manager.h"

namespace no_streaming
{
	zircon_render_graph_pass_editor_imgui_gles3::
		zircon_render_graph_pass_editor_imgui_gles3() :
		zircon_render_graph_pass_editor(), m_p_main_manager{},
		m_p_imgui_wrapper{}
	{
	}

	zircon_render_graph_pass_editor_imgui_gles3::
		~zircon_render_graph_pass_editor_imgui_gles3(void)
	{
		bool is_imgui_enabled = false;
		bool is_sdk_enabled = false;

		Kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_main_manager->Get_EngineConfig();

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

	void zircon_render_graph_pass_editor_imgui_gles3::OnCreateResources(
		kotek::core::ktkMainManager* p_main_manager,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
		KOTEK_ASSERT(p_main_manager, "must be valid!");
		KOTEK_ASSERT(p_main_manager->Get_ImguiWrapper(), "must be valid!");

		this->m_p_main_manager = p_main_manager;
		this->m_p_imgui_wrapper = p_main_manager->Get_ImguiWrapper();

		bool is_imgui_enabled = false;
		bool is_sdk_enabled = false;

		Kotek::Core::ktkIFrameworkConfig* p_config =
			p_main_manager->Get_EngineConfig();

		if (p_config)
		{
			is_imgui_enabled =
				p_main_manager->Get_EngineConfig()->IsFeatureEnabled(
					Kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);

			is_sdk_enabled =
				p_main_manager->Get_EngineConfig()->IsFeatureEnabled(
					Kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK);
		}

		if (is_imgui_enabled)
		{
			IMGUI_CHECKVERSION();

			if (this->m_p_imgui_wrapper)
			{
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
						p_main_manager->GetGameManager()->GetWindowHandle());

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

			p_main_manager->Get_EngineConfig()->SetFeatureStatus(
				Kotek::Core::eEngineFeatureSDK::
					kEngine_Feature_SDK_ImGui_Initialized,
				true);
		}
	}

	void zircon_render_graph_pass_editor_imgui_gles3::OnUpdate(
		const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass)
	{
		Kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_main_manager->Get_EngineConfig();

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

			KOTEK_ASSERT(this->m_p_manager_session_editor,
				"Did you call OnRegisterManagers because session manager "
			    "editor is "
				"not initialized!");

			zircon_session_editor* p_session =
				this->m_p_manager_session_editor->get_session(
					this->m_p_manager_session_editor->get_current_session_id());

			KOTEK_ASSERT(p_session, "failed to obtain session editor by id: {}",
				this->m_p_manager_session_editor->get_current_session_id());

			if (!p_session)
			{
				KOTEK_MESSAGE_WARNING(
					"failed to obtain session editor by id: {}",
					this->m_p_manager_session_editor->get_current_session_id());
				return;
			}

			auto& imgui_ui_elements = p_session->get_imgui_ui_elements();

			for (auto* p_element : imgui_ui_elements)
			{
				if (p_element)
				{
					p_element->Draw(this->m_p_main_manager);
				}
			}

			if (this->m_p_imgui_wrapper)
			{
				this->m_p_imgui_wrapper->Render();
			}
		}
	}

	void zircon_render_graph_pass_editor_imgui_gles3::OnRender(
		const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass)
	{
		Kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_main_manager->Get_EngineConfig();

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
}