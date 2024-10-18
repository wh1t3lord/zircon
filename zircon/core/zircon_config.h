#pragma once

/**
 * \~english @brief some unique and rare features that can't be standardized
 */
enum eZirconGameFeatures
{
	kGame_Feature_Unknown = 0,
};

KOTEK_IMPLEMENTATION_ENUM_FLAG_OPERATORS(eZirconGameFeatures);

/**
 * \~english @brief some unique and rare feature that can't be standardized
 */
enum eZirconSDKFeatures
{
	kSDK_Feature_AddRequiredComponents_Automatically = 1 << 1,
	kSDK_Feature_SphereBoundingBox_Quality = 1 << 2,

	kSDK_Feature_Unknown = 0
};

KOTEK_IMPLEMENTATION_ENUM_FLAG_OPERATORS(eZirconSDKFeatures);

// TODO: replace to static_cstring_t
kotek::cstring_t translate_zircon_sdk_features(eZirconSDKFeatures features);
Kotek::ktk::cstring translate_zircon_game_features(
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

	void serialize(Kotek::Core::ktkIFileSystem* p_filesystem,
		Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept;
	void deserialize(Kotek::Core::ktkIFileSystem* p_filesystem,
		Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept;

private:
	void initialize_default() noexcept;

private:
	eZirconGameFeatures m_features_game;
	eZirconSDKFeatures m_features_sdk;

	Kotek::ktk::unordered_map<eZirconSDKFeatures,
		Kotek::ktk::variant<int, double, float, Kotek::ktk::cstring>>
		m_features_data_sdk;

	Kotek::ktk::unordered_map<eZirconSDKFeatures,
		Kotek::ktk::variant<int, double, float, Kotek::ktk::cstring>>
		m_features_data_game;
};