#pragma once

/**
 * \~english @brief some unique and rare features that can't be
 * standardized
 */
enum class eZirconGameFeatures : kotek::ktk::uint16_t
{
	kGame_Feature_Unknown = 0,
};

KOTEK_IMPLEMENTATION_ENUM_FLAG_OPERATORS(
	eZirconGameFeatures, kotek::ktk::uint16_t
);

/**
 * \~english @brief some unique and rare feature that can't be
 * standardized
 */
enum class eZirconSDKFeatures : kotek::ktk::uint16_t
{
	kSDK_Feature_AddRequiredComponents_Automatically = 1 << 1,
	kSDK_Feature_SphereBoundingBox_Quality = 1 << 2,

	kSDK_Feature_Unknown = 0
};

KOTEK_IMPLEMENTATION_ENUM_FLAG_OPERATORS(
	eZirconSDKFeatures, kotek::ktk::uint16_t
);

enum class eZirconResourceLoadingFlags : kotek::uint8_t
{
	kNone = 0,
	kCache = 1 << 1
};

KOTEK_IMPLEMENTATION_ENUM_FLAG_OPERATORS(eZirconResourceLoadingFlags, std::underlying_type_t<eZirconResourceLoadingFlags>);

#define ZIRCON_DEF_RESOURCE_TEXT_JSON_TINY_FILE_LENGTH 512
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_SMALL_FILE_LENGTH 1024
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_MEDIUM_FILE_LENGTH 2048
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_BIG_FILE_LENGTH 4096
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_LARGE_FILE_LENGTH 8192
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_FAT_FILE_LENGTH 16384
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_MASS_FILE_LENGTH 32768

using zircon_resource_json_tiny_t =
	kotek::core::ktkResourceText<
		ZIRCON_DEF_RESOURCE_TEXT_JSON_TINY_FILE_LENGTH,
		1024,
		false>;
using zircon_resource_json_small_t =
	kotek::core::ktkResourceText<
		ZIRCON_DEF_RESOURCE_TEXT_JSON_SMALL_FILE_LENGTH,
		2048,
		false>;
using zircon_resource_json_medium_t =
	kotek::core::ktkResourceText<
		ZIRCON_DEF_RESOURCE_TEXT_JSON_MEDIUM_FILE_LENGTH,
		4096,
		false>;
using zircon_resource_json_big_t = kotek::core::ktkResourceText<
	ZIRCON_DEF_RESOURCE_TEXT_JSON_BIG_FILE_LENGTH,
	8192,
	false>;
using zircon_resource_json_large_t =
	kotek::core::ktkResourceText<
		ZIRCON_DEF_RESOURCE_TEXT_JSON_LARGE_FILE_LENGTH,
		16384,
		false>;
using zircon_resource_json_fat_t = kotek::core::ktkResourceText<
	ZIRCON_DEF_RESOURCE_TEXT_JSON_FAT_FILE_LENGTH,
	32768,
	false>;
using zircon_resource_json_mass_t =
	kotek::core::ktkResourceText<
		ZIRCON_DEF_RESOURCE_TEXT_JSON_MASS_FILE_LENGTH,
		131072,
		false>;

// if we expect that json is dynamic then it is better to
// reserve less memory than user expects, otherwise use static
// versions of json as resource text aliases

using zircon_resource_jsond_t =
	kotek::core::ktkResourceText<512, 1024, true>;
using zircon_resource_jsond_big_t =
	kotek::core::ktkResourceText<4096, 8192, true>;