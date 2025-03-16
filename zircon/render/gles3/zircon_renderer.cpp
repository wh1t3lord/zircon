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

// OS Passes
#include "../os/zircon_render_graph_pass_console.h"

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
	const Kotek::ktk::vector<Kotek::Core::ktkISDKUIElement*>& ui_elements,
	kotek::core::ktkWindowConsole* p_console, kotek::core::ktkConsole* p_con)
{
	this->m_imgui_ui_elements = ui_elements;

	this->initialize_extensions(p_con);

	this->Create_RenderGraph(this->m_imgui_ui_elements, p_console);

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

template <unsigned char Size>
bool validate_extensions(const const char* (&extension_names)[Size],
	kotek::core::ktkConsole* p_console)
{
	KOTEK_ASSERT(p_console, "must be valid!");

	bool presented[Size]{};
	bool status = true;
	int NumberOfExtensions;
	glGetIntegerv(GL_NUM_EXTENSIONS, &NumberOfExtensions);
	for (int i = 0; i < NumberOfExtensions; i++)
	{
		const GLubyte* extension_name = glGetStringi(GL_EXTENSIONS, i);

#ifdef KOTEK_DEBUG
		KOTEK_MESSAGE(
			"extension: {}", reinterpret_cast<const char*>(extension_name));
#endif

		for (unsigned char j = 0; j < Size; j++)
		{
			if (strcmp((const char*)extension_name, extension_names[j]) == 0)
			{
				presented[j] = true;

				KOTEK_MESSAGE("device supported extension: {}",
					(const char*)extension_name);
			}
		}
	}

	for (unsigned char i = 0; i < Size; ++i)
	{
		if (presented[i] == false)
		{
			KOTEK_ASSERT(false,
				"your device's driver doesn't support a such extension [{}] "
				"thus we "
				"can't run application :(",
				extension_names[i]);

			KOTEK_MESSAGE("//////////");

			for (unsigned char j = 0; j < Size; ++j)
			{
				if (presented[j] == false)
				{
					KOTEK_MESSAGE("device unsupported extensions: {}",
						extension_names[j]);
				}
			}

			KOTEK_MESSAGE("//////////");

			if (p_console)
			{
				status = false;
				p_console->Push_Command(
					static_cast<kotek::ktk::enum_base_t>(kotek::core::
							eConsoleCommandIndex::kConsoleCommand_App_Close),
					{});
				break;
			}
		}
	}

	return status;
}

void zircon_renderer_gles3::initialize_extensions(
	kotek::core::ktkConsole* p_console)
{
	const char* extensions[] = {"GL_EXT_multi_draw_indirect",
		"GL_EXT_draw_elements_base_vertex", "GL_EXT_base_instance"
#ifdef KOTEK_DEF_RENDER_GL_ES_BINDLESS_TEXTURES_ENABLED

#endif

	};

	bool is_valid = validate_extensions(extensions, p_console);

	if (!glDrawElementsIndirect)
	{
		KOTEK_MESSAGE_ERROR(
			"if you're gles 3.1 it loads glDrawElementsIndirect by default in "
			"GLAD but it wasn't load so probably bug of driver of your device "
			"doesn't support it at all! Report to developers of driver and to "
			"us too!");

		if (p_console)
		{
			p_console->Push_Command(
				static_cast<kotek::ktk::enum_base_t>(kotek::core::
						eConsoleCommandIndex::kConsoleCommand_App_Close),
				{});
		}
	}

	if (is_valid)
	{
		if (!glMultiDrawArraysIndirect)
		{
			glMultiDrawArraysIndirect =
				(decltype(glMultiDrawArraysIndirect))(glfwGetProcAddress(
					"glMultiDrawArraysIndirectEXT"));

			KOTEK_ASSERT(glMultiDrawArraysIndirect,
				"failed to load proc address: {}",
				"glMultiDrawArraysIndirectEXT");

			if (glMultiDrawArraysIndirect)
			{
				KOTEK_MESSAGE(
					"loaded function: {}", "glMultiDrawArraysIndirectEXT");
			}
			else
			{
				KOTEK_MESSAGE_ERROR("failed to load function: {}",
					"glMultiDrawArraysIndirectEXT");
			}
		}

		if (!glMultiDrawElementsIndirect)
		{
			glMultiDrawElementsIndirect =
				(decltype(glMultiDrawElementsIndirect))(glfwGetProcAddress(
					"glMultiDrawElementsIndirectEXT"));

			KOTEK_ASSERT(glMultiDrawElementsIndirect,
				"failed to load proc address: {}",
				"glMultiDrawElementsIndirectEXT");

			if (glMultiDrawElementsIndirect)
			{
				KOTEK_MESSAGE(
					"loaded function: {}", "glMultiDrawElementsIndirectEXT");
			}
			else
			{
				KOTEK_MESSAGE_ERROR("failed to load function: {}",
					"glMultiDrawElementsIndirectEXT");
			}
		}

		KOTEK_MESSAGE("extensions were loaded successfully!");
	}
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
	const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>& imgui_elements,
	kotek::core::ktkWindowConsole* p_console) noexcept
{
	KOTEK_ASSERT(
		this->m_p_main_manager, "you must initialize main manager first");

	this->Add_PassesEditor(imgui_elements);
	this->Add_PassesGame(p_console);
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

void zircon_renderer_gles3::Add_PassesGame(
	kotek::core::ktkWindowConsole* p_console) noexcept
{
	// todo: implement that when you implement simulation button in editor and
	// you can test the game as standalone
	KOTEK_MESSAGE_WARNING(
		"you didn't register game render passes for renderer!");

	this->m_render_graph_simplified.Add_Pass(
		new zircon_render_graph_pass_console(
			this->m_p_main_manager, p_console));
}

void zircon_renderer_gles3::Destroy_ImGuiUIElements(void) noexcept
{
	for (auto* p_element : this->m_imgui_ui_elements)
	{
		delete p_element;
	}

	this->m_imgui_ui_elements.clear();
}