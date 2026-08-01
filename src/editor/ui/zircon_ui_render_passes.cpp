#include "zircon_ui_render_passes.h"

#include <cstring>

#include "../../core/zircon_config.h"
#include "../../render/bgfx/zircon_renderer.h"

// eZirconWindowIDs (generated) arrives through this include chain,
// same as the other editor windows
#include "zircon_editor_ui_state.h"

namespace
{
	/// splits a comma-separated config pass list into name tokens
	/// (whitespace-trimmed), mirroring the boot-time split in
	/// zircon_game_manager's zircon_create_render_passes_from_config —
	/// used for the dirty check (live set vs saved set)
	void zircon_split_render_pass_list(const char* p_comma_separated_names,
		kotek::static_vector_t<
			kotek::static_cstring_t<
				ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH>,
			KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>&
			out_names) noexcept
	{
		out_names.clear();

		if (!p_comma_separated_names)
		{
			return;
		}

		const char* p_cursor = p_comma_separated_names;

		while (*p_cursor != '\0')
		{
			while ((*p_cursor == ' ') || (*p_cursor == '\t'))
			{
				++p_cursor;
			}

			const char* p_token_begin = p_cursor;

			while ((*p_cursor != '\0') && (*p_cursor != ','))
			{
				++p_cursor;
			}

			const char* p_token_end = p_cursor;

			while ((p_token_end > p_token_begin) &&
				((p_token_end[-1] == ' ') || (p_token_end[-1] == '\t')))
			{
				--p_token_end;
			}

			if (p_token_end > p_token_begin)
			{
				if (out_names.size() >= out_names.capacity())
				{
					KOTEK_MESSAGE_ERROR(
						"saved render pass list holds more than {} "
						"names — raise "
						"KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_"
						"MAX_PASS_COUNT",
						out_names.capacity());
					return;
				}

				kotek::static_cstring_t<
					ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH>
					name;

				// a token longer than the name capacity is not a
				// registered pass anyway — truncate-free guard
				if (static_cast<kotek::ktk::size_t>(
						p_token_end - p_token_begin) >
					ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH)
				{
					KOTEK_MESSAGE_ERROR(
						"saved render pass name is too long, raise "
						"ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH");
					return;
				}

				name.assign(p_token_begin,
					static_cast<kotek::ktk::size_t>(
						p_token_end - p_token_begin));

				out_names.push_back(name);
			}

			if (*p_cursor == ',')
			{
				++p_cursor;
			}
		}
	}

	/// display-only shortening: strips the constant
	/// "no_streaming::zircon_render_graph_pass_" prefix so a narrow
	/// left-docked window stays readable; unknown prefixes render in
	/// full. The stored/saved names are never touched
	const char* zircon_render_pass_display_name(
		const char* p_pass_name) noexcept
	{
		constexpr const char* _kPrefixNamespace = "no_streaming::";
		constexpr const char* _kPrefixClass =
			"zircon_render_graph_pass_";

		const char* p_result = p_pass_name;

		const kotek::ktk::size_t prefix_namespace_length =
			std::strlen(_kPrefixNamespace);

		if (std::strncmp(p_result, _kPrefixNamespace,
				prefix_namespace_length) == 0)
		{
			p_result += prefix_namespace_length;
		}

		const kotek::ktk::size_t prefix_class_length =
			std::strlen(_kPrefixClass);

		if (std::strncmp(p_result, _kPrefixClass, prefix_class_length) ==
			0)
		{
			p_result += prefix_class_length;
		}

		return p_result;
	}
} // namespace

zircon_editor_ui_window_render_passes::
	zircon_editor_ui_window_render_passes(zircon_config* p_config,
		zircon_renderer_bgfx* p_renderer_bgfx,
		const char* const* p_registry_editor_pass_names,
		kotek::uint8_t registry_editor_pass_count,
		const char* const* p_registry_game_pass_names,
		kotek::uint8_t registry_game_pass_count,
		kotek::static_cstring_t<
			ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>*
			p_render_passes_game_resolved_baseline) :
	m_is_window_show(false), m_dont_show_on_start(false),
	m_need_initial_focus(false), m_p_config{p_config},
	m_p_renderer_bgfx{p_renderer_bgfx},
	m_p_registry_editor_pass_names{p_registry_editor_pass_names},
	m_registry_editor_pass_count{registry_editor_pass_count},
	m_p_registry_game_pass_names{p_registry_game_pass_names},
	m_registry_game_pass_count{registry_game_pass_count},
	m_p_render_passes_game_resolved_baseline{
		p_render_passes_game_resolved_baseline}
{
	KOTEK_ASSERT(p_config, "you must pass a valid zircon_config instance!");

	if (this->m_p_config)
	{
		this->m_dont_show_on_start =
			!this->m_p_config->is_feature_enabled(
				eZirconSDKFeatures::kSDK_Feature_ShowPassManagerOnStart);
	}
}

zircon_editor_ui_window_render_passes::
	~zircon_editor_ui_window_render_passes(void)
{
}

void zircon_editor_ui_window_render_passes::Initialize(void) {}

void zircon_editor_ui_window_render_passes::Shutdown(void) {}

void zircon_editor_ui_window_render_passes::Draw(
	kotek::Core::ktkMainManager* p_main_manager)
{
	if (!this->m_is_window_show)
		return;

	if (!p_main_manager)
		return;

	auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

	if (!p_wrapper_imgui)
		return;

	// docked left by default (the persisted imgui.ini under
	// data_user/sdk/settings takes over after the first run), focused
	// once on open — the first-run presentation is the "wizard"
	if (this->m_need_initial_focus)
	{
		p_wrapper_imgui->SetNextWindowFocus();
		this->m_need_initial_focus = false;
	}

	p_wrapper_imgui->SetNextWindowPos(
		ImVec2(0.0f, 19.0f), ImGuiCond_FirstUseEver);
	p_wrapper_imgui->SetNextWindowSize(
		ImVec2(460.0f, 520.0f), ImGuiCond_FirstUseEver);

	if (p_wrapper_imgui->Begin("Render Passes"))
	{
		if (!this->m_p_renderer_bgfx)
		{
			p_wrapper_imgui->TextDisabled(
				"the bgfx renderer is not active — pass sets are "
				"unavailable on this backend");
		}
		else
		{
			this->draw_session_section(p_wrapper_imgui, false);
			this->draw_session_section(p_wrapper_imgui, true);
		}

		p_wrapper_imgui->Separator();

		if (p_wrapper_imgui->Checkbox("don't show on start again",
				&this->m_dont_show_on_start))
		{
			// intentionally NOT persisted here — the choice lands in
			// game_config.json through Save, same as the pass sets
		}
		p_wrapper_imgui->TextDisabled("(applies on Save)");

		bool is_any_dirty =
			this->is_session_dirty(false) || this->is_session_dirty(true);

		if (is_any_dirty)
		{
			p_wrapper_imgui->SameLine();
			p_wrapper_imgui->TextDisabled("[modified — unsaved]");
		}

		if (p_wrapper_imgui->Button("Save"))
		{
			this->save(p_main_manager);
		}
	}

	p_wrapper_imgui->End();
}

int zircon_editor_ui_window_render_passes::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_RenderPasses);
}

void zircon_editor_ui_window_render_passes::Show(void)
{
	this->refresh_registry();
	this->m_need_initial_focus = true;
	this->m_is_window_show = true;
}

void zircon_editor_ui_window_render_passes::Hide(void)
{
	this->m_is_window_show = false;
}

bool zircon_editor_ui_window_render_passes::Is_Shown(void) const
{
	return this->m_is_window_show;
}

void zircon_editor_ui_window_render_passes::refresh_registry(void) noexcept
{
	// P2a: the tables are the generated compile-time registry the ctor
	// received, so this only re-points at them; P3 (hot-reload) swaps
	// the source to the reloaded pass library's exported table and this
	// method becomes the real refresh — call sites (Show, and P3's
	// reload watcher) stay unchanged
}

void zircon_editor_ui_window_render_passes::draw_session_section(
	kotek::Core::ktkIImguiWrapper* p_wrapper_imgui,
	bool is_game_session) noexcept
{
	const char* p_section_title =
		is_game_session ? "Game session" : "Editor session";

	if (!p_wrapper_imgui->CollapsingHeader(
			p_section_title, ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	const kotek::uint8_t render_graph_id =
		this->m_p_renderer_bgfx->get_render_graph_id_for_session_kind(
			is_game_session);

	if (render_graph_id >= this->m_p_renderer_bgfx->get_render_graph_count())
	{
		p_wrapper_imgui->TextDisabled(
			"no render graph exists for this session");
		return;
	}

	const zircon_render_graph_simplified_bgfx_info_t& info =
		this->m_p_renderer_bgfx->get_render_graph_info(render_graph_id);

	// the live set, in execution order; a structural edit (move/
	// remove) stops the row pass immediately — the list it was drawn
	// from just changed under it, and imgui redraws everything next
	// frame anyway
	const kotek::uint8_t pass_count =
		static_cast<kotek::uint8_t>(info.pass_names.size());

	bool is_structure_changed = false;

	for (kotek::uint8_t i = 0; i < pass_count; ++i)
	{
		p_wrapper_imgui->PushID(static_cast<int>(i));

		bool is_enabled = info.pass_enabled[i];

		if (p_wrapper_imgui->Checkbox("##enabled", &is_enabled))
		{
			// instant: flips the executor's skip flag, the pass keeps
			// living (no destroy/create)
			this->m_p_renderer_bgfx->set_render_pass_enabled(
				render_graph_id, i, is_enabled);

			// the gizmo pair (gizmo_own vs gizmo_imguizmo, P2e/P2f) is
			// mutually exclusive: checking one unchecks the other (both
			// unchecked = no gizmo, which is legal — the rule only fires
			// on an enable)
			if (is_enabled)
			{
				this->apply_gizmo_exclusion(render_graph_id, info,
					info.pass_names[i].c_str());
			}
		}

		p_wrapper_imgui->SameLine();
		p_wrapper_imgui->TextUnformatted(zircon_render_pass_display_name(
			info.pass_names[i].c_str()));

		p_wrapper_imgui->SameLine();

		if (this->is_registered(
				is_game_session, info.pass_names[i].c_str()))
		{
			p_wrapper_imgui->TextDisabled("[active]");
		}
		else
		{
			// the slot lists it but the pass library can't create it
			// (a P3 hot-reload can remove passes) — the next rebuild
			// drops it loudly
			p_wrapper_imgui->TextDisabled("[missing from library]");
		}

		if (i > 0)
		{
			p_wrapper_imgui->SameLine();

			if (p_wrapper_imgui->ArrowButton("##move_up", ImGuiDir_Up))
			{
				this->m_p_renderer_bgfx->request_render_pass_move(
					render_graph_id, i, true);
				is_structure_changed = true;
			}
		}

		if (!is_structure_changed && i + 1 < pass_count)
		{
			p_wrapper_imgui->SameLine();

			if (p_wrapper_imgui->ArrowButton("##move_down", ImGuiDir_Down))
			{
				this->m_p_renderer_bgfx->request_render_pass_move(
					render_graph_id, i, false);
				is_structure_changed = true;
			}
		}

		if (!is_structure_changed)
		{
			p_wrapper_imgui->SameLine();

			if (p_wrapper_imgui->SmallButton("x##remove"))
			{
				this->m_p_renderer_bgfx->request_render_pass_remove(
					render_graph_id, i);
				is_structure_changed = true;
			}
		}

		p_wrapper_imgui->PopID();

		if (is_structure_changed)
		{
			break;
		}
	}

	// registered in the pass library but not in the session's set
	const char* const* p_registry_names =
		is_game_session ? this->m_p_registry_game_pass_names
		                : this->m_p_registry_editor_pass_names;

	const kotek::uint8_t registry_count =
		is_game_session ? this->m_registry_game_pass_count
		                : this->m_registry_editor_pass_count;

	bool is_available_header_shown = false;

	for (kotek::uint8_t i = 0; i < registry_count; ++i)
	{
		const char* p_pass_name = p_registry_names[i];

		bool is_in_live_set = false;

		for (const auto& live_name : info.pass_names)
		{
			if (live_name == p_pass_name)
			{
				is_in_live_set = true;
				break;
			}
		}

		if (is_in_live_set)
		{
			continue;
		}

		if (!is_available_header_shown)
		{
			p_wrapper_imgui->SeparatorText("registered, not in the set");
			is_available_header_shown = true;
		}

		p_wrapper_imgui->PushID(p_pass_name);

		p_wrapper_imgui->TextUnformatted(
			zircon_render_pass_display_name(p_pass_name));

		p_wrapper_imgui->SameLine();
		p_wrapper_imgui->TextDisabled("[available]");

		p_wrapper_imgui->SameLine();

		if (p_wrapper_imgui->SmallButton("add##add_pass"))
		{
			this->m_p_renderer_bgfx->request_render_pass_add(
				render_graph_id, p_pass_name);

			// a freshly added pass starts enabled at the rebuild — the
			// gizmo mutual exclusion (P2f) applies to it too: adding one
			// gizmo while the sibling is in the set disables the sibling
			// now (its skip flag is instant, no rebuild needed)
			this->apply_gizmo_exclusion(
				render_graph_id, info, p_pass_name);

			p_wrapper_imgui->PopID();

			// the live set changed — stop the row pass (same reason as
			// above), the section redraws next frame
			break;
		}

		p_wrapper_imgui->PopID();
	}

	if (this->is_session_dirty(is_game_session))
	{
		p_wrapper_imgui->TextDisabled("modified (unsaved)");
	}
}

int zircon_editor_ui_window_render_passes::compute_gizmo_exclusion(
	const char* p_just_enabled_pass_name, const char* const* p_pass_names,
	kotek::uint8_t pass_count) noexcept
{
	if (p_just_enabled_pass_name == nullptr || p_pass_names == nullptr)
		return -1;

	const char* p_sibling_name = nullptr;

	if (std::strcmp(p_just_enabled_pass_name,
			kZirconConfig_RenderPassEditorGizmoOwnName) == 0)
	{
		p_sibling_name = kZirconConfig_RenderPassEditorGizmoImguizmoName;
	}
	else if (std::strcmp(p_just_enabled_pass_name,
				   kZirconConfig_RenderPassEditorGizmoImguizmoName) == 0)
	{
		p_sibling_name = kZirconConfig_RenderPassEditorGizmoOwnName;
	}
	else
	{
		return -1;
	}

	// lookup-table-on-vector (rule 2): a handful of names, linear scan
	for (kotek::uint8_t i = 0; i < pass_count; ++i)
	{
		if (p_pass_names[i] &&
			std::strcmp(p_pass_names[i], p_sibling_name) == 0)
		{
			return static_cast<int>(i);
		}
	}

	return -1;
}

void zircon_editor_ui_window_render_passes::apply_gizmo_exclusion(
	kotek::uint8_t render_graph_id,
	const zircon_render_graph_simplified_bgfx_info_t& graph_info,
	const char* p_just_enabled_pass_name) noexcept
{
	if (this->m_p_renderer_bgfx == nullptr)
		return;

	const char* p_names
		[KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT]{};

	kotek::uint8_t name_count = static_cast<kotek::uint8_t>(
		graph_info.pass_names.size());

	if (name_count >
		KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT)
	{
		name_count =
			KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT;
	}

	for (kotek::uint8_t i = 0; i < name_count; ++i)
		p_names[i] = graph_info.pass_names[i].c_str();

	const int sibling_index = compute_gizmo_exclusion(
		p_just_enabled_pass_name, p_names, name_count);

	if (sibling_index >= 0)
	{
		this->m_p_renderer_bgfx->set_render_pass_enabled(render_graph_id,
			static_cast<kotek::uint8_t>(sibling_index), false);
	}
}

bool zircon_editor_ui_window_render_passes::is_registered(
	bool is_game_session, const char* p_pass_name) const noexcept
{
	const char* const* p_registry_names =
		is_game_session ? this->m_p_registry_game_pass_names
		                : this->m_p_registry_editor_pass_names;

	const kotek::uint8_t registry_count =
		is_game_session ? this->m_registry_game_pass_count
		                : this->m_registry_editor_pass_count;

	// lookup-table-on-vector (rule 2): a handful of names, linear scan
	for (kotek::uint8_t i = 0; i < registry_count; ++i)
	{
		if (std::strcmp(p_registry_names[i], p_pass_name) == 0)
		{
			return true;
		}
	}

	return false;
}

bool zircon_editor_ui_window_render_passes::is_session_dirty(
	bool is_game_session) noexcept
{
	if (!this->m_p_config || !this->m_p_renderer_bgfx)
	{
		return false;
	}

	const kotek::uint8_t render_graph_id =
		this->m_p_renderer_bgfx->get_render_graph_id_for_session_kind(
			is_game_session);

	if (render_graph_id >= this->m_p_renderer_bgfx->get_render_graph_count())
	{
		return false;
	}

	const zircon_render_graph_simplified_bgfx_info_t& info =
		this->m_p_renderer_bgfx->get_render_graph_info(render_graph_id);

	kotek::static_vector_t<
		kotek::static_cstring_t<
			ZIRCON_DEF_RENDERER_BGFX_PASS_NAME_MAX_LENGTH>,
		KOTEK_DEF_RENDER_GL_RENDER_GRAPH_SIMPLIFIED_MAX_PASS_COUNT>
		saved_names;

	// comparison source (task Z3 P2h): the editor session compares
	// against the config's editor set (levels never override it); the
	// GAME session compares against the RESOLVED set this boot loaded
	// with — the scene file's render_passes when the level overrides
	// the config default — so a level override does not read as
	// "modified" on a fresh boot (the user changed nothing); the
	// baseline falls back to the config value when no game graph was
	// created this boot (e.g. the NRI backend)
	const char* p_saved_list =
		is_game_session ? this->m_p_config->get_render_passes_game()
		                : this->m_p_config->get_render_passes_editor();

	if (is_game_session &&
		this->m_p_render_passes_game_resolved_baseline &&
		!this->m_p_render_passes_game_resolved_baseline->empty())
	{
		p_saved_list =
			this->m_p_render_passes_game_resolved_baseline->c_str();
	}

	zircon_split_render_pass_list(p_saved_list, saved_names);

	if (saved_names.size() != info.pass_names.size())
	{
		return true;
	}

	for (kotek::ktk::size_t i = 0; i < saved_names.size(); ++i)
	{
		if (!(saved_names[i] == info.pass_names[i]))
		{
			return true;
		}
	}

	return false;
}

void zircon_editor_ui_window_render_passes::save(
	kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	KOTEK_ASSERT(this->m_p_config,
		"you must pass a valid zircon_config instance!");

	if (!this->m_p_config || !p_main_manager)
	{
		return;
	}

	if (this->m_p_renderer_bgfx)
	{
		// both live sets flow into the config members (a session
		// without a graph keeps its saved list untouched)
		for (kotek::uint8_t session_kind = 0; session_kind < 2;
		     ++session_kind)
		{
			const bool is_game_session = session_kind == 1;

			const kotek::uint8_t render_graph_id =
				this->m_p_renderer_bgfx
					->get_render_graph_id_for_session_kind(
						is_game_session);

			if (render_graph_id >=
				this->m_p_renderer_bgfx->get_render_graph_count())
			{
				continue;
			}

			const zircon_render_graph_simplified_bgfx_info_t& info =
				this->m_p_renderer_bgfx->get_render_graph_info(
					render_graph_id);

			kotek::static_cstring_t<
				ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>
				joined;

			// measure first: etl::string asserts on overflow, the
			// window must fail loudly but gracefully instead
			kotek::ktk::size_t joined_length = 0;

			for (const auto& name : info.pass_names)
			{
				joined_length += name.size() + 1; // +1 for ','/NUL
			}

			if (joined_length >
				ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH)
			{
				KOTEK_MESSAGE_ERROR(
					"render pass set of the {} session doesn't fit the "
					"config value capacity ({}) — not saved; raise "
					"ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH",
					is_game_session ? "game" : "editor", joined_length);
				continue;
			}

			bool is_first = true;

			for (const auto& name : info.pass_names)
			{
				if (!is_first)
				{
					joined += ',';
				}

				joined += name.c_str();
				is_first = false;
			}

			if (is_game_session)
			{
				this->m_p_config->set_render_passes_game(joined.c_str());

				// task Z3 P2h: the just-saved live set is also the new
				// dirty-check baseline — the next boot restores exactly
				// it (the shutdown scene.json write persists the active
				// set into the level, and it is the new config
				// default), so the marker must clear after Save
				if (this->m_p_render_passes_game_resolved_baseline)
				{
					this->m_p_render_passes_game_resolved_baseline
						->assign(joined.c_str());
				}
			}
			else
			{
				this->m_p_config->set_render_passes_editor(
					joined.c_str());
			}
		}
	}

	this->m_p_config->set_feature(
		eZirconSDKFeatures::kSDK_Feature_ShowPassManagerOnStart,
		!this->m_dont_show_on_start);

	this->m_p_config->serialize(p_main_manager->GetFileSystem());

	KOTEK_MESSAGE(
		"Render Passes window: saved pass sets to game_config.json");
}
