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

// currently loaded and used by system
#define ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT 128

#define ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD 1

#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	#define ZIRCON_DEF_RESOURCE_MANAGER_MAX_QUEUE_LOADING_REQUESTS \
		8
	#define ZIRCON_DEF_RESOURCE_MANAGER_STREAM_BUFFER_SIZE 4096
#endif

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

enum class eZirconResourceType
{
	kText,
	kTexture,
	kSound,
	kAnimation3D,
	kAnimation2D,
	kAnimation3DGUI,
	kAnimation2DGUI,
	kMaterial,
	kLevel,
	kUnknown
};

struct zircon_resource_desc_t
{
	bool is_loaded = false;

	/// @brief means cache wasn't used and resource was
	/// constructed using new operator (and it means that we
	/// need to deallocate it and it goes to destructor of
	/// zircon_resource_t)
	bool is_temp = false;

	/// @brief has different interpretations since it is
	/// definition of ktkResourceText third template argument
	/// (_Realloc) or for any other resources that define a term
	/// as 'reallocation'
	bool is_reallocatable = false;

	eZirconResourceType type = eZirconResourceType::kUnknown;

	std::variant<eZirconJsonType> metadata;

	kotek::uint32_t _lookupid;

#ifdef KOTEK_DEBUG
	kotek::static_cstring_t<KOTEK_DEF_MAXIMUM_OS_PATH_LENGTH>
		filename;
#endif
};

class zircon_resource_manager;

struct zircon_resource_t
{
	friend class zircon_resource_manager;

	zircon_resource_t();
	~zircon_resource_t();

	const zircon_resource_desc_t* get_desc() const noexcept;

	/// @brief retrieve resource that you want to cast (use
	/// infromation from desc)
	/// @return
	void* get_data() const noexcept;

private:
	void set_desc(zircon_resource_desc_t* p_desc) noexcept;

private:
	const zircon_resource_desc_t* m_p_desc;
	void* m_p_data;
};

class zircon_resource_manager
{
public:
	zircon_resource_manager();
	~zircon_resource_manager(void);

	void initialize(Kotek::Core::ktkMainManager* p_main_manager
	);
	void shutdown(void);

	std::shared_ptr<zircon_resource_t> load(
		const kotek::static_path_t& path,
		eZirconResourceLoadingFlags flags
	);

	void
	unload(const std::shared_ptr<zircon_resource_t>& resource);

private:
#ifdef KOTEK_DEBUG
	bool m_was_shutdown_called;
#endif

#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	std::thread m_worker_thead;
#endif

private:
	kotek::static_vector_t<
		zircon_resource_desc_t,
		ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT>
		m_resources_desc;

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
