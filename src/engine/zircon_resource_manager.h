#pragma once

#include "zircon_ecs.h"
#include "../core/zircon_defs.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

#define ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL 4
#define ZIRCON_DEF_RESOURCE_MANAGER_LOAD_QUEUE_SIZE 32
#define ZIRCON_DEF_RESOURCE_MANAGER_SAVE_QUEUE_SIZE \
	ZIRCON_DEF_RESOURCE_MANAGER_LOAD_QUEUE_SIZE

// note: honestly you should specify exact amount only when you
// know the data set based on real statistic of your pre-release
// data that will be use in game (or any other kinds of
// productions)

#define ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_TINY_AMOUNT \
	128
#define ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_SMALL_COUNT \
	64
#define ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_MEDIUM_COUNT \
	32
#define ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_BIG_COUNT \
	16
#define ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_LARGE_COUNT \
	8
#define ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_FAT_COUNT \
	4
#define ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_MASS_COUNT \
	2

struct zircon_cache_resource_text_handle
{
	kotek::uint32_t id = 0;
};

class zircon_static_cache_resource_text
{
public:
	zircon_cache_resource_text_handle
	add(kotek::size_t file_length, unsigned char* p_data)
	{
		zircon_cache_resource_text_handle result;

		if (file_length <=
		    ZIRCON_DEF_RESOURCE_TEXT_JSON_TINY_FILE_LENGTH)
		{

		}
		else if (file_length <=
		         ZIRCON_DEF_RESOURCE_TEXT_JSON_SMALL_FILE_LENGTH)
		{

		}
		else if (file_length <=
		         ZIRCON_DEF_RESOURCE_TEXT_JSON_MEDIUM_FILE_LENGTH)
		{

		}
		else if (file_length <=
		         ZIRCON_DEF_RESOURCE_TEXT_JSON_BIG_FILE_LENGTH)
		{
		}
		else if (file_length <=
		         ZIRCON_DEF_RESOURCE_TEXT_JSON_LARGE_FILE_LENGTH)
		{
		}
		else if (file_length <=
		         ZIRCON_DEF_RESOURCE_TEXT_JSON_FAT_FILE_LENGTH)
		{
		}
		else if (file_length <=
		         ZIRCON_DEF_RESOURCE_TEXT_JSON_MASS_FILE_LENGTH)
		{
		}
		else
		{
			KOTEK_ASSERT(
				false,
				"unsupported length of file, you should think "
				"carefully how to prepare data for your "
				"production, should you split it? requested "
			    "size to insert to cache is={}",
				file_length
			);
		}

		return result;
	}

private:
#if ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_TINY_AMOUNT > \
	0
	kotek::static_vector_t<
		zircon_resource_json_tiny_t,
		ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_TINY_AMOUNT>
		cache_tiny_jsons;
#endif

#if ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_SMALL_COUNT > \
	0
	kotek::static_vector_t<
		zircon_resource_json_small_t,
		ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_SMALL_COUNT>
		cache_small_jsons;
#endif

#if ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_MEDIUM_COUNT > \
	0
	kotek::static_vector_t<
		zircon_resource_json_medium_t,
		ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_MEDIUM_COUNT>
		cache_medium_jsons;
#endif

#if ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_BIG_COUNT > \
	0
	kotek::static_vector_t<
		zircon_resource_json_big_t,
		ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_BIG_COUNT>
		cache_big_jsons;
#endif

#if ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_LARGE_COUNT > \
	0
	kotek::static_vector_t<
		zircon_resource_json_large_t,
		ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_LARGE_COUNT>
		cache_large_jsons;
#endif

#if ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_FAT_COUNT > \
	0
	kotek::static_vector_t<
		zircon_resource_json_fat_t,
		ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_FAT_COUNT>
		cache_fat_jsons;
#endif

#if ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_MASS_COUNT > \
	0
	kotek::static_vector_t<
		zircon_resource_json_mass_t,
		ZIRCON_DEF_RESOURCE_MANAGER_STATIC_CACHE_RESOURCE_TEXT_MASS_COUNT>
		cache_mass_jsons;
#endif
};

class zircon_resource_manager
{
public:
	zircon_resource_manager(
		Kotek::Core::ktkMainManager* p_main_manager
	);
	~zircon_resource_manager(void);

	void initialize(void);
	void shutdown(void);

	void load(
		const kotek::static_path_t& path,
		eZirconResourceLoadingFlags flags
	);

private:
	zircon_static_cache_resource_text
		static_cache_resource_text;

private:
	/* todo: delete & re-write please
	    bool m_fstreams_avail
	        [ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL];
	    Kotek::Core::ktkIResourceLoaderManager*
	        m_p_manager_resource_loader;
	    Kotek::Core::ktkIResourceSaverManager*
	        m_p_manager_resource_saver;
	    Kotek::Core::ktkIRenderResourceManager*
	        m_p_manager_render_resource;
	    Kotek::Core::ktkMainManager* m_p_manager_main;
	    Kotek::Core::ktkConsole* m_p_manager_console;
	    kotek::ktk::cfstream
	        m_fstreams[ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL];
	    kotek::static_queue_t<
	        kotek::core::ktkResourceAssetRequest,
	        ZIRCON_DEF_RESOURCE_MANAGER_LOAD_QUEUE_SIZE>
	        m_load_queue_requests;
	    kotek::static_queue_t<
	        kotek::core::ktkResourceAssetRequest,
	        ZIRCON_DEF_RESOURCE_MANAGER_SAVE_QUEUE_SIZE>
	        m_save_queue_requests;*/
};
