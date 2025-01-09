#include "zircon_renderer.h"

// Game Passes
#include "zircon_render_graph_pass_imgui.h"
#include "zircon_render_graph_pass_present.h"
#include "zircon_render_graph_pass_model_static.h"

// Editor Passes
#include "zircon_render_graph_pass_editor_imgui.h"
#include "zircon_render_graph_pass_editor_present.h"
#include "zircon_render_graph_pass_editor_model_static.h"
#include "zircon_render_graph_pass_editor_debug.h"

#include <kotek.render.gl/include/kotek_render_device.h>
#include <kotek.render.gl/include/kotek_render_resource_manager.h>

zircon_renderer_gles3::zircon_renderer_gles3(
	Kotek::Core::ktkMainManager* p_main_manager) :
	m_p_main_manager{p_main_manager},
	m_p_render_device{static_cast<Kotek::Render::gl::ktkRenderDevice*>(
		p_main_manager->getRenderDevice())},
	m_p_render_resource_manager{}
{
}

zircon_renderer_gles3::~zircon_renderer_gles3(void) {}

void zircon_renderer_gles3::Initialize(
	const Kotek::ktk::vector<Kotek::Core::ktkISDKUIElement*>& ui_elements)
{
	this->m_imgui_ui_elements = ui_elements;

	this->Create_RenderGraph(this->m_imgui_ui_elements);

	this->m_p_render_resource_manager =
		dynamic_cast<Kotek::Render::gl::ktkRenderResourceManager*>(
			this->m_p_main_manager->GetRenderResourceManager());

	this->m_render_graph_simplified.Initialize(
		this->m_p_main_manager, this->m_p_render_resource_manager);
}

void zircon_renderer_gles3::Shutdown(void)
{
	this->Destroy_RenderGraph();
	this->Destroy_ImGuiUIElements();
}

void zircon_renderer_gles3::draw()
{
	this->Begin();

	// updating all stuff for uploading...
	this->m_p_render_resource_manager->Update();

	this->m_render_graph_simplified.Update_All();
	this->m_render_graph_simplified.Render_All();

	this->End();
}

void zircon_renderer_gles3::Resize() {}

const char* zircon_renderer_gles3::Get_Name(void) const noexcept
{
	return Kotek::kRenderer_OpenGLES_3_Name;
}

const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>&
zircon_renderer_gles3::Get_UIImGuiElements() const
{
	return this->m_imgui_ui_elements;
}

void zircon_renderer_gles3::Begin() noexcept {}

void zircon_renderer_gles3::End() noexcept
{
	Kotek::Core::ktkIRenderSwapchain* p_render_swapchain =
		this->m_p_main_manager->getRenderSwapchainManager();

	p_render_swapchain->Present(
		this->m_p_main_manager, this->m_p_render_device);
}

void zircon_renderer_gles3::Destroy_RenderGraph(void) noexcept
{
	this->m_render_graph_simplified.Shutdown();
}

void zircon_renderer_gles3::Create_RenderGraph(
	const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>&
		imgui_elements) noexcept
{
	KOTEK_ASSERT(
		this->m_p_main_manager, "you must initialize main manager first");

	this->Add_PassesEditor(imgui_elements);
	this->Add_PassesGame();
}

void zircon_renderer_gles3::Add_PassesEditor(
	const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>&
		imgui_elements) noexcept
{
#ifdef KOTEK_USE_SDK_IMGUI
	if (this->m_p_main_manager)
	{
		if (this->m_p_main_manager->Get_EngineConfig())
		{
			if (this->m_p_main_manager->Get_EngineConfig()->IsFeatureEnabled(
					kotek::core::kEngine_Feature_SDK_ImGui))
			{
				this->m_render_graph_simplified.Add_Pass(
					new zircon_render_graph_pass_editor_present_gles3(
						u8"render_editor_pass_gles3_present"));

				this->m_render_graph_simplified.Add_Pass(
					new zircon_render_graph_pass_editor_model_static_gles3(
						u8"render_editor_pass_gles3_static_geometry"));

				this->m_render_graph_simplified.Add_Pass(
					new zircon_render_graph_pass_editor_imgui_gles3(
						u8"render_editor_pass_gles3_imgui",
						this->m_p_main_manager, imgui_elements));
			}
		}
	}
#endif
}

void zircon_renderer_gles3::Add_PassesGame() noexcept
{
	// todo: implement that when you implement simulation button in editor and
	// you can test the game as standalone
	KOTEK_MESSAGE_WARNING(
		"you didn't register game render passes for renderer!");
}

void zircon_renderer_gles3::Destroy_ImGuiUIElements(void) noexcept
{
	for (auto* p_element : this->m_imgui_ui_elements)
	{
		delete p_element;
	}

	this->m_imgui_ui_elements.clear();
}