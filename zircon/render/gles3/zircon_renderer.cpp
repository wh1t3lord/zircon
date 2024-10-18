#include "zircon_renderer.h"

#include "zircon_render_graph_pass_imgui.h"
#include "zircon_render_graph_pass_present.h"
#include "zircon_render_graph_pass_triangle.h"
#include "zircon_render_graph_pass_model_static.h"

#include <kotek.render.gl/include/kotek_render_device.h>
#include <kotek.render.gl/include/kotek_render_resource_manager.h>
#include <kotek.render.gl/include/kotek_render_graph_simplified_builder.h>
#include <kotek.render.gl/include/kotek_render_graph_simplified_node.h>

zircon_renderer_gles3::zircon_renderer_gles3(
	Kotek::Core::ktkMainManager* p_main_manager) :
	m_p_main_manager{p_main_manager},
	m_p_render_device{static_cast<Kotek::Render::gl::ktkRenderDevice*>(
		p_main_manager->getRenderDevice())},
	m_p_render_resource_manager{}, m_render_graph_simplified_resource_manager{
									   p_main_manager}
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

	this->m_render_graph_simplified.Initialize();
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

Kotek::ktk::cstring zircon_renderer_gles3::GetName(void) const noexcept
{
	return Kotek::kRenderer_OpenGLES_3_Name;
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
	this->m_render_graph_simplified_resource_manager.Shutdown();
}

void zircon_renderer_gles3::Create_RenderGraph(
	const Kotek::ktk::vector<Kotek::Core::ktkISDKUIElement*>&
		imgui_elements) noexcept
{
	KOTEK_ASSERT(
		this->m_p_main_manager, "you must initialize main manager first");

	Kotek::Render::gl::ktkRenderGraphSimplifiedBuilder builder(
		this->m_p_main_manager);

	builder.Initialize(&this->m_render_graph_simplified_resource_manager,
		"present_image_gl3_3",
		Kotek::Render::gl::eRenderGraphBuilderType::
			kRenderBuilderFor_Forward_Only,
		Kotek::Render::gl::eRenderGraphBuilderPipelineRenderingType::
			kRenderBuilderBasedOnPipeline_Orthodox);

	builder.Register_RenderPass("render_pass_gles3_present",
		new zircon_render_graph_pass_present_gles3());

	builder.Register_RenderPass("render_pass_gles3_static_geometry",
		new zircon_render_graph_pass_model_static_gles3());

	// builder.Register_RenderPass("render_pass_gles3_triangle",
	// new zircon_render_graph_pass_triangle_gles3());

	builder.Register_RenderPass("render_pass_gles3_imgui",
		new zircon_render_graph_pass_imgui_gles3(
			this->m_p_main_manager, imgui_elements));

	this->m_render_graph_simplified = builder.Compile();
}

void zircon_renderer_gles3::Destroy_ImGuiUIElements(void) noexcept
{
	for (auto* p_element : this->m_imgui_ui_elements)
	{
		delete p_element;
	}

	this->m_imgui_ui_elements.clear();
}