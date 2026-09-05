#pragma once

#include "zircon_defs.h"

// for ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH and
// kZirconLocalization_DefaultLanguage — the config persists the
// localization manager's active tags, so both sides share the one
// capacity/default definition (no drift)
#include "zircon_localization_manager.h"

/// the data-carrying SDK features are a handful by design (bool features
/// live directly in the flag enums, see m_features_sdk/m_features_game) —
/// lookup-table-on-vector (rule 2), never a hash map; capacities are named
/// and bounded (rule 9)
#define ZIRCON_DEF_CONFIG_MAX_FEATURE_DATA_COUNT 8
/// string-valued features stay allocation-free (a static string inside the
/// variant; 32 chars covers ids/names of future features)
#define ZIRCON_DEF_CONFIG_FEATURE_STRING_MAX_LENGTH 32
/// one comma-separated render-pass list: the longest registered pass name
/// today is "no_streaming::zircon_render_graph_pass_editor_model_static_
/// bgfx" (62 chars), so 256 holds three such names with separators — raise
/// when a session legitimately needs more simultaneous passes
#define ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH 256

/// the fixed json DOM memory of the config document (ktkResourceText's
/// static_resource, no realloc). MEASURED (2026-09-05, task Z22): the
/// 8 pre-localization keys sat just under the old 2048 — adding the two
/// language keys threw bad_alloc mid-serialize (an uncaught boost::json
/// exception = a message-less abort); 4096 covers the 10-key document
/// (~0.7 KB serialized) including rehash peaks with >2x margin. When a
/// future key outgrows this, raise the define — the failure mode is an
/// abort, so measure before shipping a bigger config
#define ZIRCON_DEF_CONFIG_JSON_MEMORY_SIZE 4096
/// the PARSE leg of the config document: ktkResourceText's first
/// template parameter is a separate static_resource the
/// Create_FromMemory parser builds the DOM in (NOT the write leg
/// above). MEASURED (2026-09-05, task Z22): the 8-key file parsed in
/// 1024; the 10-key file's parse DOM exceeds 1024 and the exhaustion
/// throw escapes the noexcept deserialize as a message-less abort;
/// 2048 covers the 10-key parse with margin
#define ZIRCON_DEF_CONFIG_JSON_PARSER_MEMORY_SIZE 2048

// the translate functions return const char* (string literals, zero cost)
// — static_cstring_t would CONSTRUCT a string per call for nothing
const char* translate_zircon_sdk_features(eZirconSDKFeatures features);
const char* translate_zircon_game_features(eZirconGameFeatures features);

constexpr const char* kZirconConfig_FileName = "game_config.json";

// render-pass set keys (task Z3 P1): each value is a comma-separated list
// of pass class names as registered in the generated
// zircon_render_pass_factory; absent/empty key = the defaults below, which
// reproduce the pre-config hardcoded sets exactly. The editor set keeps
// imgui LAST (it draws the UI over everything); the grid (task Z3 P2d)
// sits between the present clear and the gizmo, under the scene geometry;
// the gizmo (task Z3 P2e) is a depth-test-off overlay between the grid and
// imgui so the handles always read over the scene
constexpr const char* kZirconConfig_KeyRenderPassesEditor =
	"render_passes_editor";
constexpr const char* kZirconConfig_KeyRenderPassesGame =
	"render_passes_game";
constexpr const char* kZirconConfig_DefaultRenderPassesEditor =
	"no_streaming::zircon_render_graph_pass_editor_present_bgfx,"
	"no_streaming::zircon_render_graph_pass_editor_grid_bgfx,"
	"no_streaming::zircon_render_graph_pass_editor_gizmo_own_bgfx,"
	"no_streaming::zircon_render_graph_pass_editor_imgui_bgfx";
constexpr const char* kZirconConfig_DefaultRenderPassesGame =
	"no_streaming::zircon_render_graph_pass_present_bgfx,"
	"no_streaming::zircon_render_graph_pass_model_static_bgfx";

// the gizmo pair (tasks Z3 P2e/P2f): the two editor gizmo variants are
// MUTUALLY EXCLUSIVE members of the editor pass set — the Render Passes
// window enforces it (enabling one disables the other; both disabled = no
// gizmo, which is legal). The own gizmo is the house default (it is in the
// default editor set above); the ImGuizmo variant is opt-in through the
// window/config. These single-source names keep the window, the
// imguizmo-hosting ui window and the tests spelling the same string
constexpr const char* kZirconConfig_RenderPassEditorGizmoOwnName =
	"no_streaming::zircon_render_graph_pass_editor_gizmo_own_bgfx";
constexpr const char* kZirconConfig_RenderPassEditorGizmoImguizmoName =
	"no_streaming::zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx";

// graphics-development mode (task Z3 P3a): the CLI override
// (--graphics_development, folded into the config feature at boot) — the
// persisted side is the kSDK_Feature_GraphicsDevelopment flag, written
// through translate_zircon_sdk_features as "graphics_development"
constexpr const char* kZirconConfig_ConsoleArg_GraphicsDevelopment =
	"--graphics_development";

// localization (task Z22): the active language tag per
// zircon_localization_manager instance — the value names the
// data_game/configs/locale/<editor|game>/<tag>.json file the instance
// loads. Default "en" (the ctor initializes it, an absent key keeps the
// default); 28/26 chars — under the config Write's 32-char key
// truncation limit, keep them there
constexpr const char* kZirconConfig_KeyLocalizationEditorLanguage =
	"localization_editor_language";
constexpr const char* kZirconConfig_KeyLocalizationGameLanguage =
	"localization_game_language";

class zircon_config
{
public:
	zircon_config(void);
	~zircon_config(void);

	void set_feature(eZirconSDKFeatures feature, bool status) noexcept;
	void set_feature(eZirconSDKFeatures feature, int data) noexcept;

	template <typename Type>
	Type get_feature(eZirconSDKFeatures feature) const noexcept
	{
		for (const auto& entry : this->m_features_data_sdk)
		{
			if (entry.first == feature &&
				std::holds_alternative<Type>(entry.second))
			{
				return std::get<Type>(entry.second);
			}
		}

		return Type{};
	}

	void set_feature(eZirconGameFeatures feature, bool status) noexcept;

	bool is_feature_enabled(eZirconSDKFeatures feature) const;
	bool is_feature_enabled(eZirconGameFeatures feature) const;

	void serialize(kotek::Core::ktkIFileSystem* p_filesystem) noexcept;
	void deserialize(kotek::Core::ktkIFileSystem* p_filesystem) noexcept;

	bool is_current_session_editor(void) const;
	void set_current_session(
		kotek::uint8_t session_id, bool is_editor) noexcept;

	/// raw comma-separated pass-name lists (see the key constants above);
	/// never empty in practice — constructed with the defaults and
	/// deserialize only overwrites on a non-empty value
	const char* get_render_passes_editor(void) const noexcept;
	const char* get_render_passes_game(void) const noexcept;

	/// overwrite the in-memory lists (the Render Passes window's Save
	/// path, task Z3 P2a); the value must fit
	/// ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH — the caller checks
	/// the joined length before calling (a loud assert fires otherwise)
	void set_render_passes_editor(const char* p_comma_separated_names) noexcept;
	void set_render_passes_game(const char* p_comma_separated_names) noexcept;

	/// the localization instances' active language tags (task Z22);
	/// never empty in practice — constructed with the default ("en") and
	/// deserialize only overwrites on a non-empty value
	const char* get_localization_editor_language(void) const noexcept;
	const char* get_localization_game_language(void) const noexcept;

	/// internal data (the game manager mirrors the localization
	/// manager's live tags into the config at save) — the value must fit
	/// ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH, a loud assert
	/// fires otherwise
	void set_localization_editor_language(const char* p_language) noexcept;
	void set_localization_game_language(const char* p_language) noexcept;

private:
	void initialize_default() noexcept;

private:
	bool m_is_session_editor;
	kotek::uint8_t m_current_session_id;
	eZirconGameFeatures m_features_game;
	eZirconSDKFeatures m_features_sdk;

	kotek::static_cstring_t<ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>
		m_render_passes_editor;
	kotek::static_cstring_t<ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>
		m_render_passes_game;

	// the localization instances' persisted language tags (task Z22) —
	// same capacity as the manager's own member, so a value the manager
	// accepted always fits here and vice versa
	kotek::static_cstring_t<ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH>
		m_localization_editor_language;
	kotek::static_cstring_t<ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH>
		m_localization_game_language;

	// the variant carries every non-bool feature value; the string
	// alternative is a static_cstring_t so no value ever allocates
	using zircon_feature_data_t =
		kotek::ktk::variant<int, double, float,
			kotek::static_cstring_t<ZIRCON_DEF_CONFIG_FEATURE_STRING_MAX_LENGTH>>;

	kotek::static_vector_t<
		kotek::ktk::pair<eZirconSDKFeatures, zircon_feature_data_t>,
		ZIRCON_DEF_CONFIG_MAX_FEATURE_DATA_COUNT>
		m_features_data_sdk;
};
