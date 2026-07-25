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

// the translate functions return const char* (string literals, zero cost)
// — static_cstring_t would CONSTRUCT a string per call for nothing
const char* translate_zircon_sdk_features(eZirconSDKFeatures features);
const char* translate_zircon_game_features(eZirconGameFeatures features);

constexpr const char* kZirconConfig_FileName = "game_config.json";

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

private:
	void initialize_default() noexcept;

private:
	bool m_is_session_editor;
	kotek::uint8_t m_current_session_id;
	eZirconGameFeatures m_features_game;
	eZirconSDKFeatures m_features_sdk;

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
