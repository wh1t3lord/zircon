#include "zircon_renderer.h"

// Game Passes
#include "passes/no_streaming/zircon_render_graph_pass_imgui.h"
#include "passes/no_streaming/zircon_render_graph_pass_present.h"
#include "passes/no_streaming/zircon_render_graph_pass_model_static.h"

// Editor Passes
#include "passes/no_streaming/zircon_render_graph_pass_editor_imgui.h"
#include "passes/no_streaming/zircon_render_graph_pass_editor_present.h"
#include "passes/no_streaming/zircon_render_graph_pass_editor_model_static.h"
#include "passes/no_streaming/zircon_render_graph_pass_editor_debug.h"

// OS Passes
#include "../os/zircon_render_graph_pass_console.h"

#include <kotek.render.bgfx/include/kotek_render_device.h>
#include <kotek.render.bgfx/include/kotek_render_resource_manager.h>

// todo: move to config please
constexpr kotek::uint8_t _kInvalidRenderGraphID =
	std::numeric_limits<kotek::uint8_t>::max();

zircon_renderer_bgfx::zircon_renderer_bgfx(
	Kotek::Core::ktkMainManager* p_main_manager) :
	m_previous_render_graph_id{_kInvalidRenderGraphID},
	m_p_main_manager{p_main_manager}, m_p_session_game_manager{},
	m_p_session_editor_manager{},
	m_p_render_device{static_cast<kotek::render::bgfx::ktkRenderDevice*>(
		p_main_manager->getRenderDevice())},
	m_p_render_resource_manager{}, m_p_current_render_graph{}
{
	this->m_render_graphs.reserve(
		ZIRCON_DEF_RENDERER_BGFX_MAX_RENDER_GRAPH_COUNT);
}

zircon_renderer_bgfx::~zircon_renderer_bgfx(void) {}

void zircon_renderer_bgfx::Initialize(kotek::core::ktkWindowConsole* p_console,
	kotek::core::ktkConsole* p_con,
	zircon_session_game_manager* p_manager_game_session,
	zircon_session_editor_manager* p_manager_editor_session)
{
#ifdef KOTEK_USE_SDK_IMGUI
	KOTEK_ASSERT(p_manager_editor_session,
		"expected to valid editor session manager pointer");
	this->m_p_session_editor_manager = p_manager_editor_session;
#endif

	this->m_p_session_game_manager = p_manager_game_session;

	this->initialize_extensions(p_con);

	this->m_p_render_resource_manager =
		this->m_p_main_manager->GetRenderResourceManager();
}

void zircon_renderer_bgfx::Shutdown(void)
{
	this->destroy_render_graphs();
}

void zircon_renderer_bgfx::draw()
{
	this->Begin();

	// updating all stuff for uploading...
	this->m_p_render_resource_manager->Update();

	if (this->m_p_current_render_graph)
	{
		if (this->m_p_current_render_graph->Is_Initialized())
		{
			this->m_p_current_render_graph->Update_All();
			this->m_p_current_render_graph->Render_All();
		}
	}

	this->End();
}

void zircon_renderer_bgfx::Resize() {}

const char* zircon_renderer_bgfx::Get_Name(void) const noexcept
{
	return Kotek::kRenderer_OpenGLES_3_Name;
}

bool zircon_renderer_bgfx::is_render_graph_presented(
	kotek::uint8_t render_graph_id) const
{
	bool result{};

	result = this->m_render_graphs.size() > render_graph_id;

	return result;
}

bool zircon_renderer_bgfx::is_render_graph_initialized(
	kotek::uint8_t render_graph_id) const
{
	bool result{};

	result = this->is_render_graph_presented(render_graph_id);

	KOTEK_ASSERT(
		result, "render graph by id: {} wasn't presented in renderer!");

	if (result)
	{
		result = this->m_render_graphs[render_graph_id].graph.Is_Initialized();
	}

	return result;
}

bool zircon_renderer_bgfx::is_render_graph_for_session_editor(
	kotek::uint8_t render_graph_id) const
{
	bool result{};

	result = this->is_render_graph_presented(render_graph_id);
	KOTEK_ASSERT(
		result, "render graph by id {} must be presented in renderer!");

	if (result)
	{
		result =
			this->m_render_graphs[render_graph_id].is_game_session == false;
	}

	return result;
}

bool zircon_renderer_bgfx::is_render_graph_for_session_game(
	kotek::uint8_t render_graph_id) const
{
	return !(this->is_render_graph_for_session_editor(render_graph_id));
}

kotek::uint8_t zircon_renderer_bgfx::create_render_graph(
	kotek::uint8_t session_id, bool is_game_session,
	kotek::static_vector_t<
		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*,
		KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>& passes)
{
	if (this->m_render_graphs.size() ==
		ZIRCON_DEF_RENDERER_BGFX_MAX_RENDER_GRAPH_COUNT)
	{
		KOTEK_MESSAGE_WARNING("you can't add more render graph due to limit, "
							  "max amount is {} current amount is {}",
			ZIRCON_DEF_RENDERER_BGFX_MAX_RENDER_GRAPH_COUNT,
			this->m_render_graphs.size());

		return _kInvalidRenderGraphID;
	}

	kotek::uint8_t render_graph_id = this->m_render_graphs.size();
	this->m_render_graphs.push_back(
		zircon_render_graph_simplified_bgfx_info_t());

	zircon_render_graph_simplified_bgfx_info_t& info =
		this->m_render_graphs[render_graph_id];

	info.session_id = session_id;
	info.in_use = false;
	info.is_game_session = is_game_session;

	info.graph.Add_Passes(passes);

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("created render graph for session_{}#{}", session_id,
		is_game_session ? "game" : "editor");
#endif

	return render_graph_id;
}

void zircon_renderer_bgfx::initialize_render_graph(
	kotek::uint8_t render_graph_id, kotek::core::ktkMainManager* p_main_manager,
	kotek::core::ktkIRenderResourceManager* p_render_resource_manager,
	zircon_session_game_manager* p_manager_game_session,
	zircon_session_editor_manager* p_manager_editor_session)
{
	bool is_presented = this->is_render_graph_presented(render_graph_id);

	KOTEK_ASSERT(p_manager_game_session, "pass a valid game session manager!");
	KOTEK_ASSERT(
		p_manager_editor_session, "pass a valid editor session manager!");

	KOTEK_ASSERT(is_presented, "render graph is not presented!");

	if (is_presented)
	{
		KOTEK_ASSERT(!this->is_render_graph_initialized(render_graph_id),
			"you try to initialize an already initialized graph#{}",
			render_graph_id);

		auto& info = this->m_render_graphs[render_graph_id];

		const auto& passes = info.graph.Get_Passes();

		// user pre-initialization with user defined initialization where by
		// design we can't and don't want to use interfaces or something else in
		// order to call our "native" with user defined classes and structs
		for (kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass :
			passes)
		{
			KOTEK_ASSERT(p_pass, "must be valid!");

			constexpr const char* _kDebugNameZirconRenderGraphPass =
				"zircon_render_graph_pass";
			constexpr const char* _kDebugNameZirconRenderGraphPassEditor =
				"zircon_render_graph_pass_editor";

			KOTEK_ASSERT(info.is_game_session
					? !!(dynamic_cast<zircon_render_graph_pass_bgfx*>(p_pass))
					: !!(dynamic_cast<zircon_render_graph_pass_editor_bgfx*>(
						  p_pass)),
				"failed to cast! expected type is {} for {} session",
				info.is_game_session ? _kDebugNameZirconRenderGraphPass
									 : _kDebugNameZirconRenderGraphPassEditor,
				info.is_game_session ? "game" : "editor");

			if (info.is_game_session)
			{
				zircon_render_graph_pass_bgfx* p_game_pass =
					static_cast<zircon_render_graph_pass_bgfx*>(p_pass);

				p_game_pass->OnRegisterManagers(p_manager_game_session);
			}
			else
			{
				zircon_render_graph_pass_editor_bgfx* p_editor_pass =
					static_cast<zircon_render_graph_pass_editor_bgfx*>(p_pass);

				p_editor_pass->OnRegisterManagers(p_manager_editor_session);
			}
		}

		info.graph.Initialize(p_main_manager, p_render_resource_manager);
	}
}

void zircon_renderer_bgfx::set_current_render_graph(
	kotek::uint8_t render_graph_id)
{
	KOTEK_ASSERT(render_graph_id < this->m_render_graphs.size(),
		"something is wrong and you passed invalid id");

	this->m_p_current_render_graph =
		&this->m_render_graphs[render_graph_id].graph;

	if (this->m_previous_render_graph_id != _kInvalidRenderGraphID)
	{
		KOTEK_ASSERT(
			this->m_render_graphs[this->m_previous_render_graph_id].in_use,
			"must be true because you replace current that was in use!");
		this->m_render_graphs[this->m_previous_render_graph_id].in_use = false;
	}

	KOTEK_ASSERT(this->m_render_graphs[render_graph_id].in_use == false,
		"something is wrong and your logic is broken!");
	this->m_render_graphs[render_graph_id].in_use = true;

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("set current render graph for session#{}",
		this->m_render_graphs[render_graph_id].session_id);
#endif

	this->m_previous_render_graph_id = render_graph_id;
}

void zircon_renderer_bgfx::initialize_extensions(
	kotek::core::ktkConsole* p_console)
{
	KOTEK_ASSERT(::bgfx::getCaps(), "bgfx has invalid getCaps() instance! (nullptr)");

	bool gpu_feature_supported =
		!!(BGFX_CAPS_COMPUTE & ::bgfx::getCaps()->supported);
	
	if (!gpu_feature_supported)
	{
		KOTEK_MESSAGE_ERROR("compute doesn't support on this gpu :( Aborting...");

		if (p_console)
		{
			p_console->Push_Command(
				static_cast<kotek::ktk::enum_base_t>(kotek::core::
						eConsoleCommandIndex::kConsoleCommand_App_Close),
				{});
			return;
		}
	}

	gpu_feature_supported =
		!!(BGFX_CAPS_DRAW_INDIRECT & ::bgfx::getCaps()->supported);

	if (!gpu_feature_supported)
	{
		KOTEK_MESSAGE_ERROR(
			"draw indirect doesn't support on this gpu :( Aborting...");

		if (p_console)
		{
			p_console->Push_Command(static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::kConsoleCommand_App_Close));
		}
	}

	gpu_feature_supported =
		!!(BGFX_CAPS_INSTANCING & ::bgfx::getCaps()->supported);

	if (!gpu_feature_supported)
	{
		KOTEK_MESSAGE_ERROR(
			"instancing doesn't support on this gpu :( Aborting...");

		if (p_console)
		{
			p_console->Push_Command(static_cast<kotek::ktk::enum_base_t>(
				kotek::core::eConsoleCommandIndex::kConsoleCommand_App_Close));
		}
	}
}

void zircon_renderer_bgfx::Begin() noexcept {}

void zircon_renderer_bgfx::End() noexcept
{
	Kotek::Core::ktkIRenderSwapchain* p_render_swapchain =
		this->m_p_main_manager->getRenderSwapchainManager();

	p_render_swapchain->Present(
		this->m_p_main_manager, this->m_p_render_device);
}

void zircon_renderer_bgfx::destroy_render_graphs(void) noexcept
{
	for (auto& info : this->m_render_graphs)
	{
		info.graph.Shutdown();
	}

	this->m_render_graphs.clear();
}

void zircon_renderer_bgfx::create_render_graph(
	const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>& imgui_elements,
	kotek::core::ktkWindowConsole* p_console) noexcept
{
	KOTEK_ASSERT(
		this->m_p_main_manager, "you must initialize main manager first");

	this->Add_PassesEditor(imgui_elements);
	this->Add_PassesGame(p_console);
}

void zircon_renderer_bgfx::Add_PassesEditor(
	const kotek::ktk::vector<kotek::core::ktkISDKUIElement*>&
		imgui_elements) noexcept
{
#ifdef KOTEK_USE_SDK_IMGUI
	if (this->m_p_main_manager)
	{
		if (this->m_p_main_manager->Get_EngineConfig())
		{
			if (this->m_p_main_manager->Get_EngineConfig()->IsFeatureEnabled(
					kotek::core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui))
			{
				/*
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
				*/
			}
		}
	}
#endif
}

void zircon_renderer_bgfx::Add_PassesGame(
	kotek::core::ktkWindowConsole* p_console) noexcept
{
	// todo: implement that when you implement simulation button in editor and
	// you can test the game as standalone
	KOTEK_MESSAGE_WARNING(
		"you didn't register game render passes for renderer!");
	/*
	this->m_render_graph_simplified.Add_Pass(
	    new zircon_render_graph_pass_console(
	        this->m_p_main_manager, p_console));
	        */
}