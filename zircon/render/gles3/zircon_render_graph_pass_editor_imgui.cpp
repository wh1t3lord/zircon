#include "zircon_render_graph_pass_editor_imgui.h"

zircon_render_graph_pass_editor_imgui_gles3::zircon_render_graph_pass_editor_imgui_gles3(
	const kotek::static_u8string_view_t& render_pass_name,
	kotek::core::ktkMainManager* p_main_manager,
	const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>& elements) :
	kotek::render::gl::ktkRenderGraphSimplifiedRenderPass(
		render_pass_name.data()),
	m_p_main_manager{p_main_manager},
	m_p_imgui_wrapper{p_main_manager->Get_ImguiWrapper()},
	m_p_imgui_ui_elements{&elements}
{
	bool is_imgui_enabled = false;
	bool is_sdk_enabled = false;

	Kotek::Core::ktkIEngineConfig* p_config =
		this->m_p_main_manager->Get_EngineConfig();

	if (p_config)
	{
		is_imgui_enabled = p_main_manager->Get_EngineConfig()->IsFeatureEnabled(
			Kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);

		is_sdk_enabled = p_main_manager->Get_EngineConfig()->IsFeatureEnabled(
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

				KOTEK_ASSERT(this->m_p_imgui_wrapper->ImGui_ImplOpenGL3_Init(),
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

zircon_render_graph_pass_editor_imgui_gles3::~zircon_render_graph_pass_editor_imgui_gles3(
	void)
{
	bool is_imgui_enabled = false;
	bool is_sdk_enabled = false;

	Kotek::Core::ktkIEngineConfig* p_config =
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

	this->m_p_imgui_ui_elements = nullptr;

	p_config->SetFeatureStatus(
		Kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui_Initialized,
		false);
}

void zircon_render_graph_pass_editor_imgui_gles3::OnCreateResources(
	kotek::core::ktkMainManager* p_manager_main,
	kotek::core::ktkIRenderResourceManager* p_manager_resource)
{
}

void zircon_render_graph_pass_editor_imgui_gles3::OnUpdate(
	const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
		p_previous_pass)
{
	Kotek::Core::ktkIEngineConfig* p_config =
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

		for (auto* p_element : *this->m_p_imgui_ui_elements)
		{
			p_element->Draw(this->m_p_main_manager);
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
	Kotek::Core::ktkIEngineConfig* p_config =
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
