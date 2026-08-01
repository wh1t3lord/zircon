#pragma once

#include "zircon_defs.h"

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

// the translate functions return const char* (string literals, zero cost)
// — static_cstring_t would CONSTRUCT a string per call for nothing
const char* translate_zircon_sdk_features(eZirconSDKFeatures features);
const char* translate_zircon_game_features(eZirconGameFeatures features);

constexpr const char* kZirconConfig_FileName = "game_config.json";

// render-pass set keys (task Z3 P1): each value is a comma-separated list
// of pass class names as registered in the generated
// zircon_render_pass_factory; absent/empty key = the defaults below, which
// reproduce the pre-config hardcoded sets exactly
constexpr const char* kZirconConfig_KeyRenderPassesEditor =
	"render_passes_editor";
constexpr const char* kZirconConfig_KeyRenderPassesGame =
	"render_passes_game";
constexpr const char* kZirconConfig_DefaultRenderPassesEditor =
	"no_streaming::zircon_render_graph_pass_editor_present_bgfx,"
	"no_streaming::zircon_render_graph_pass_editor_imgui_bgfx";
constexpr const char* kZirconConfig_DefaultRenderPassesGame =
	"no_streaming::zircon_render_graph_pass_present_bgfx";

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
