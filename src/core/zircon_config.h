#pragma once

#include "zircon_defs.h"

// TODO: replace to static_cstring_t
const char* translate_zircon_sdk_features(eZirconSDKFeatures features);
kotek::ktk::cstring translate_zircon_game_features(
	eZirconGameFeatures features);

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
		if (this->m_features_data_sdk.find(feature) ==
			this->m_features_data_sdk.end())
			return Type{};

		return std::get<Type>(this->m_features_data_sdk.at(feature));
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

	// todo: replace on more optimized bitset or just array of flags with O(1)
	kotek::ktk::unordered_map<eZirconSDKFeatures,
		kotek::ktk::variant<int, double, float, kotek::ktk::cstring>>
		m_features_data_sdk;
	// todo: replace on more optimized bitset or just array of flags with O(1)
	kotek::ktk::unordered_map<eZirconSDKFeatures,
		kotek::ktk::variant<int, double, float, kotek::ktk::cstring>>
		m_features_data_game;
};