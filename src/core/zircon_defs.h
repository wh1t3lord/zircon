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
	/// @brief \~english means we use 'no flags' but for
	/// resource manager it is just invalidation flag
	kNone = 0,

	// if resource wasn't specified as Cache or
	// kUnloadOnDestroyed it means CPU data was allocated by
	// new/delete operations
	kUseStaticCache = 1 << 1,

	/// @brief \~english we want to deferred loading (loading in
	/// worker's thread of resource manager)
	kAsync = 1 << 2,

	/// @brief \~english we want to immediate loading (loading
	/// in the same thread)
	kSync = 1 << 3,

	/// @brief \~english streams data by user defined stream
	/// buffer size (zircon implements 4Kb buffer size for
	/// streaming)
	kStreamWhenLoad = 1 << 4,

	/// @brief \~english literally means that when shared_ptr
	/// comes destroyed we issue cache invalidation like marking
	/// that cache is free
	kInvalidateCacheWhenResourceWasDestroyed = 1 << 5,

	/// @brief it means when we construct resource we use
	/// dynamic storage of caches, so if there's appropriate
	/// resource cache we try to re-use it otherwise it is new
	/// operation if kInvalidateCacheWhenResourceWasDestroyed
	/// was specified it will call delete operation when
	/// shared_ptr comes 'destroyed'
	kUseDynamicCache = 1 << 6
};

KOTEK_IMPLEMENTATION_ENUM_FLAG_OPERATORS(eZirconResourceLoadingFlags, std::underlying_type_t<eZirconResourceLoadingFlags>);

#define ZIRCON_DEF_RESOURCE_TEXT_JSON_TINY_FILE_LENGTH 512
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_SMALL_FILE_LENGTH 1024
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_MEDIUM_FILE_LENGTH 2048
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_BIG_FILE_LENGTH 4096
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_LARGE_FILE_LENGTH 8192
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_FAT_FILE_LENGTH 16384
#define ZIRCON_DEF_RESOURCE_TEXT_JSON_MASS_FILE_LENGTH 32768

#ifdef KOTEK_DEBUG
	#ifdef KOTEK_USE_SDK_IMGUI
		#define ZIRCON_DEF_MAX_WORLD_COUNT 1
	#else
		#define ZIRCON_DEF_MAX_WORLD_COUNT 1
	#endif
#else
	/// @brief so why we need many worlds if we show
    /// only one scene on screen?
	#define ZIRCON_DEF_MAX_WORLD_COUNT 1
#endif

#define ZIRCON_DEF_WORLD_NAME_MAX_STRING_LENGTH 16
#define ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT 128

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

/// @brief this is for casting from void*
enum class eZirconJsonType : kotek::uint8_t
{
	kTiny,
	kSmall,
	kMedium,
	kBig,
	kLarge,
	kFat,
	kMass
};

// if we expect that json is dynamic then it is better to
// reserve less memory than user expects, otherwise use static
// versions of json as resource text aliases

using zircon_resource_jsond_t =
	kotek::core::ktkResourceText<512, 1024, true>;
using zircon_resource_jsond_big_t =
	kotek::core::ktkResourceText<4096, 8192, true>;