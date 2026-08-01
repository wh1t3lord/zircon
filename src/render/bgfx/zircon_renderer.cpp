#include "zircon_renderer.h"

#include <cstring>

// pass bases (complete types for the dynamic_casts in
// initialize_render_graph); concrete passes live in the
// zircon.render.passes.bgfx project and reach the graphs through
// zircon_game_manager + the generated pass factory
#include "zircon_render_graph_pass.h"
#include "zircon_render_graph_pass_editor.h"

#include <kotek.render.bgfx/include/kotek_render_device.h>
#include <kotek.render.bgfx/include/kotek_render_resource_manager.h>

// todo: move to config please
constexpr kotek::uint8_t _kInvalidRenderGraphID =
	std::numeric_limits<kotek::uint8_t>::max();

zircon_renderer_bgfx::zircon_renderer_bgfx(
	kotek::Core::ktkMainManager* p_main_manager) :
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

	// structural pass-set edits from the Render Passes window (Z3 P2a)
	// run at the frame boundary, before any pass of this frame — the
	// window itself executes inside a pass's OnUpdate, where deleting
	// passes would pull the rug from under the running pass
	this->process_pending_render_graph_rebuilds();

	if (this->m_p_current_render_graph)
	{
		if (this->m_p_current_render_graph->Is_Initialized())
		{
			// the slot info of the current graph carries the per-pass
			// skip flags; find it by pointer identity (2 slots max)
			const zircon_render_graph_simplified_bgfx_info_t*
				p_current_info = nullptr;

			for (const auto& info : this->m_render_graphs)
			{
				if (&info.graph == this->m_p_current_render_graph)
				{
					p_current_info = &info;
					break;
				}
			}

			KOTEK_ASSERT(p_current_info,
				"the current render graph must belong to one of the "
				"renderer slots");

			// mirror ktkRenderGraphSimplified::Update_All/Render_All
			// semantics exactly — the previous-pass pointer and the
			// queue id advance for EVERY slot, disabled or not — so a
			// fully-enabled graph behaves byte-identically to
			// Update_All(); Render_All(); only the disabled passes'
			// OnUpdate/OnRender calls are skipped
			if (p_current_info)
			{
				auto& passes = this->m_p_current_render_graph->Get_Passes();

				KOTEK_ASSERT(
					p_current_info->pass_enabled.size() == passes.size(),
					"skip flags must align with the graph's passes "
					"({} vs {}) — create_render_graph/"
					"rebuild_render_graph keep them in sync",
					p_current_info->pass_enabled.size(), passes.size());

				kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
					p_previous_pass{};
				kotek::ktk::uint32_t id{};
				kotek::ktk::size_t pass_index{};

				for (kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
						 p_pass : passes)
				{
					if (p_pass && pass_index <
							p_current_info->pass_enabled.size() &&
						p_current_info->pass_enabled[pass_index])
					{
						p_pass->OnUpdate(p_previous_pass, id);
					}

					p_previous_pass = p_pass;
					++id;
					++pass_index;
				}

				p_previous_pass = nullptr;
				id = 0;
				pass_index = 0;

				for (kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
						 p_pass : passes)
				{
					if (p_pass && pass_index <
							p_current_info->pass_enabled.size() &&
						p_current_info->pass_enabled[pass_index])
					{
						p_pass->OnRender(p_previous_pass, id);
					}

					p_previous_pass = p_pass;
					++id;
					++pass_index;
				}
			}
		}
	}

	this->End();
}

void zircon_renderer_bgfx::Resize() {}

const char* zircon_renderer_bgfx::Get_Name(void) const noexcept
{
	return kotek::kRenderer_BGFX_Name;
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
		KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>& passes,
	const kotek::static_vector_t<
		kotek::static_cstring_t<
			ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH>,
		KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>&
		pass_names)
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

	KOTEK_ASSERT(passes.size() == pass_names.size(),
		"every created pass must carry its registered class name ({} "
		"passes vs {} names) — the slot stores the names for the Render "
		"Passes window (Z3 P2a)",
		passes.size(), pass_names.size());

	kotek::uint8_t render_graph_id = this->m_render_graphs.size();
	this->m_render_graphs.push_back(
		zircon_render_graph_simplified_bgfx_info_t());

	zircon_render_graph_simplified_bgfx_info_t& info =
		this->m_render_graphs[render_graph_id];

	info.session_id = session_id;
	info.in_use = false;
	info.is_game_session = is_game_session;

	info.pass_names = pass_names;

	// every pass starts enabled (the window's checkboxes flip these
	// skip flags at runtime)
	info.pass_enabled.assign(pass_names.size(), true);

	info.graph.Add_Passes(passes);

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("created render graph for session_{}#{}", session_id,
		is_game_session ? "game" : "editor");

	// the ordered pass set: the creation order IS the execution order
	// (the queue id) — traced so a boot log proves what a session
	// renders and in which sequence
	kotek::uint8_t pass_index = 0;

	for (const auto& pass_name : info.pass_names)
	{
		KOTEK_MESSAGE("render graph #{} pass[{}]: {}", render_graph_id,
			pass_index, pass_name.c_str());

		++pass_index;
	}
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

void zircon_renderer_bgfx::set_render_pass_create_callback(
	zircon_render_pass_create_pfn_t pfn_create) noexcept
{
	KOTEK_ASSERT(pfn_create,
		"pass a valid pass-creation callback (the generated pass "
		"factory's create-by-name entry)");

	this->m_pfn_create_render_pass = pfn_create;
}

kotek::uint8_t zircon_renderer_bgfx::get_render_graph_count(
	void) const noexcept
{
	return static_cast<kotek::uint8_t>(this->m_render_graphs.size());
}

const zircon_render_graph_simplified_bgfx_info_t&
zircon_renderer_bgfx::get_render_graph_info(
	kotek::uint8_t render_graph_id) const noexcept
{
	KOTEK_ASSERT(render_graph_id < this->m_render_graphs.size(),
		"render graph by id {} is not presented in renderer!",
		render_graph_id);

	return this->m_render_graphs[render_graph_id];
}

kotek::uint8_t zircon_renderer_bgfx::get_render_graph_id_for_session_kind(
	bool is_game_session) const noexcept
{
	for (kotek::uint8_t i = 0;
	     i < static_cast<kotek::uint8_t>(this->m_render_graphs.size()); ++i)
	{
		if (this->m_render_graphs[i].is_game_session == is_game_session)
		{
			return i;
		}
	}

	return kotek::uint8_t(-1);
}

void zircon_renderer_bgfx::set_render_pass_enabled(
	kotek::uint8_t render_graph_id, kotek::uint8_t pass_index,
	bool enabled) noexcept
{
	if (render_graph_id >= this->m_render_graphs.size())
	{
		KOTEK_MESSAGE_ERROR(
			"set_render_pass_enabled: render graph#{} is not presented",
			render_graph_id);
		return;
	}

	auto& info = this->m_render_graphs[render_graph_id];

	if (pass_index >= info.pass_enabled.size())
	{
		KOTEK_MESSAGE_ERROR(
			"set_render_pass_enabled: pass index {} is out of range "
			"(graph#{}, {} passes)",
			pass_index, render_graph_id, info.pass_enabled.size());
		return;
	}

	// pass_names and pass_enabled stay aligned even across a queued
	// structural edit, so the flip rides into the rebuild unchanged
	info.pass_enabled[pass_index] = enabled;
}

void zircon_renderer_bgfx::request_render_pass_add(
	kotek::uint8_t render_graph_id, const char* p_pass_name) noexcept
{
	KOTEK_ASSERT(p_pass_name && p_pass_name[0] != '\0',
		"pass a valid registered pass class name");

	if (!p_pass_name || p_pass_name[0] == '\0')
	{
		return;
	}

	if (render_graph_id >= this->m_render_graphs.size())
	{
		KOTEK_MESSAGE_ERROR(
			"request_render_pass_add: render graph#{} is not presented",
			render_graph_id);
		return;
	}

	auto& info = this->m_render_graphs[render_graph_id];

	for (const auto& name : info.pass_names)
	{
		if (name == p_pass_name)
		{
			KOTEK_MESSAGE_WARNING(
				"render pass '{}' is already in graph#{} — duplicates "
				"are not supported",
				p_pass_name, render_graph_id);
			return;
		}
	}

	if (info.pass_names.size() >= info.pass_names.capacity())
	{
		KOTEK_MESSAGE_ERROR(
			"render graph#{} pass name list is full ({}), raise "
			"KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT",
			render_graph_id, info.pass_names.capacity());
		return;
	}

	if (std::strlen(p_pass_name) >
		ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH)
	{
		KOTEK_MESSAGE_ERROR(
			"render pass name '{}' is too long, raise "
			"ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH",
			p_pass_name);
		return;
	}

	kotek::static_cstring_t<ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH>
		name;
	name.assign(p_pass_name);
	info.pass_names.push_back(name);
	// pass_names and pass_enabled keep the same size at ALL times (the
	// graph's pass array catches up at the rebuild) — the window reads
	// both in the same Draw call that issued this request
	info.pass_enabled.push_back(true);
	info.rebuild_pending = true;
}

void zircon_renderer_bgfx::request_render_pass_remove(
	kotek::uint8_t render_graph_id, kotek::uint8_t pass_index) noexcept
{
	if (render_graph_id >= this->m_render_graphs.size())
	{
		KOTEK_MESSAGE_ERROR(
			"request_render_pass_remove: render graph#{} is not "
			"presented",
			render_graph_id);
		return;
	}

	auto& info = this->m_render_graphs[render_graph_id];

	if (pass_index >= info.pass_names.size())
	{
		KOTEK_MESSAGE_ERROR(
			"request_render_pass_remove: pass index {} is out of range "
			"(graph#{}, {} passes)",
			pass_index, render_graph_id, info.pass_names.size());
		return;
	}

	info.pass_names.erase(info.pass_names.begin() + pass_index);
	info.pass_enabled.erase(info.pass_enabled.begin() + pass_index);
	info.rebuild_pending = true;
}

void zircon_renderer_bgfx::request_render_pass_move(
	kotek::uint8_t render_graph_id, kotek::uint8_t pass_index,
	bool move_up) noexcept
{
	if (render_graph_id >= this->m_render_graphs.size())
	{
		KOTEK_MESSAGE_ERROR(
			"request_render_pass_move: render graph#{} is not presented",
			render_graph_id);
		return;
	}

	auto& info = this->m_render_graphs[render_graph_id];

	const kotek::uint8_t pass_count =
		static_cast<kotek::uint8_t>(info.pass_names.size());

	if (pass_index >= pass_count)
	{
		KOTEK_MESSAGE_ERROR(
			"request_render_pass_move: pass index {} is out of range "
			"(graph#{}, {} passes)",
			pass_index, render_graph_id, pass_count);
		return;
	}

	if (move_up && pass_index == 0)
	{
		return;
	}

	if (!move_up && pass_index + 1 >= pass_count)
	{
		return;
	}

	const kotek::uint8_t swap_index =
		move_up ? pass_index - 1 : pass_index + 1;

	// manual swaps: no dependency question (etl strings move fine, but
	// a named temp is the dumbest correct form); names and skip flags
	// stay aligned at all times
	auto moved_name = info.pass_names[pass_index];
	info.pass_names[pass_index] = info.pass_names[swap_index];
	info.pass_names[swap_index] = moved_name;

	bool moved_enabled = info.pass_enabled[pass_index];
	info.pass_enabled[pass_index] = info.pass_enabled[swap_index];
	info.pass_enabled[swap_index] = moved_enabled;

	info.rebuild_pending = true;
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
	kotek::Core::ktkIRenderSwapchain* p_render_swapchain =
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

void zircon_renderer_bgfx::process_pending_render_graph_rebuilds(
	void) noexcept
{
	for (kotek::uint8_t i = 0;
	     i < static_cast<kotek::uint8_t>(this->m_render_graphs.size()); ++i)
	{
		if (this->m_render_graphs[i].rebuild_pending)
		{
			this->m_render_graphs[i].rebuild_pending = false;
			this->rebuild_render_graph(i);
		}
	}
}

void zircon_renderer_bgfx::rebuild_render_graph(
	kotek::uint8_t render_graph_id) noexcept
{
	KOTEK_ASSERT(render_graph_id < this->m_render_graphs.size(),
		"render graph by id {} is not presented in renderer!",
		render_graph_id);

	auto& info = this->m_render_graphs[render_graph_id];

	if (!this->m_pfn_create_render_pass)
	{
		KOTEK_MESSAGE_ERROR(
			"can't rebuild render graph#{}: no pass-creation callback "
			"was injected (zircon_game_manager must call "
			"set_render_pass_create_callback)",
			render_graph_id);
		return;
	}

	// names + skip flags move together (kept aligned by the request_*
	// edits), so a disabled pass stays disabled across the rebuild
	const kotek::static_vector_t<
		kotek::static_cstring_t<
			ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH>,
		KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>
		names_to_create = info.pass_names;

	const kotek::static_vector_t<bool,
		KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>
		enabled_to_restore = info.pass_enabled;

	// deletes every pass of this graph (the passes hold POD/handles
	// only, all GPU resources stay on the executor side) — the same
	// destroy-before-recreate order P3's hot-reload of the pass
	// library requires
	info.graph.Shutdown();

	info.pass_names.clear();
	info.pass_enabled.clear();

	kotek::static_vector_t<
		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*,
		KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>
		passes;

	for (kotek::ktk::size_t i = 0; i < names_to_create.size(); ++i)
	{
		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass =
			this->m_pfn_create_render_pass(names_to_create[i].c_str());

		if (!p_pass)
		{
			// a name the current pass library can't create (removed
			// pass, stale config) — drop it from the slot loudly
			// instead of resurrecting it on every rebuild
			KOTEK_MESSAGE_ERROR(
				"render pass '{}' is not registered in the pass "
				"library anymore — dropping it from render graph#{}",
				names_to_create[i].c_str(), render_graph_id);
			continue;
		}

		passes.push_back(p_pass);
		info.pass_names.push_back(names_to_create[i]);
		info.pass_enabled.push_back(enabled_to_restore[i]);
	}

	info.graph.Add_Passes(passes);

	// the regular initialize path (mirrors the boot flow): pass-base
	// OnRegisterManagers dynamic_casts + graph.Initialize, which runs
	// every pass's OnCreateResources
	this->initialize_render_graph(render_graph_id, this->m_p_main_manager,
		this->m_p_render_resource_manager, this->m_p_session_game_manager,
		this->m_p_session_editor_manager);

	KOTEK_MESSAGE("rebuilt render graph#{} with {} pass(es)",
		render_graph_id, passes.size());
}