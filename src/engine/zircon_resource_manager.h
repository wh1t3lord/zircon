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

#define ZIRCON_DEF_RESOURCE_MANAGER_USE_STATIC_CACHE_RESOURCE_TEXT \
	1

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

#define ZIRCON_DEF_RESOURCE_MANAGER_DYNAMIC_RESOURCE_COUNT 16

#define ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD 1

#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	#define ZIRCON_DEF_RESOURCE_MANAGER_MAX_QUEUE_LOADING_REQUESTS \
		8
	#define ZIRCON_DEF_RESOURCE_MANAGER_STREAM_BUFFER_SIZE 4096
#endif

#ifdef KOTEK_USE_TESTS_RUNTIME
	#define ZIRCON_DEF_UNIT_TEST_RESOURCE_MANAGER 1
#endif

using zircon_resource_id_t = kotek::uint16_t;

struct zircon_cache_resource_text_handle
{
	zircon_resource_id_t id = zircon_resource_id_t(-1);
};

class zircon_static_cache_resource_text
{
public:
	kotek::core::ktkResourceViewText
	add(kotek::size_t file_length, unsigned char* p_data)
	{
		kotek::core::ktkResourceViewText result;

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

enum class eZirconResourceType : kotek::ktk::uint8_t
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

class zircon_resource_manager;

constexpr zircon_resource_id_t _kZirconInvalidResourceID = -1;

struct zircon_resource_desc_t
{
	friend class zircon_resource_manager;

	/// @brief \~english indicates if current resource was
	/// streamed fully or loaded
	bool is_loaded = false;

	eZirconResourceLoadingFlags flags =
		eZirconResourceLoadingFlags::kNone;

	/// @brief use this field to cast to appropriate view struct
	/// according to resource type
	eZirconResourceType type = eZirconResourceType::kUnknown;

#ifdef KOTEK_DEBUG
	kotek::static_cstring_t<KOTEK_DEF_MAXIMUM_OS_PATH_LENGTH>
		debug_filename;
#endif

private:
	/// @brief lookupid for cache slot based on type field
	zircon_resource_id_t cache_id = _kZirconInvalidResourceID;
};

struct zircon_view_handle_t
{
	/// @brief \~english resource view reprensetation that was
	/// allocated using memory from own
	/// zircon_resource_desc_t::_view_storage field using
	/// placement new as allocation policy
	/// for reading only operations you can do nothing in terms
	/// of syncronization access but for writing operations you
	/// should syncronize since we expect that you use access to
	/// data in thread safety approach like separated threads
	/// and you access to different resources types based on
	/// different threads like render thread accesses rendering
	/// resource types and like simulation thread accesses more
	/// general data than 'graphics' otherwise writing
	/// operations use only through resource manager so the
	/// general tendency to reduce different thread accesses to
	/// same resource otherwise provide own sync routes to
	/// resource that you access
	void* p_view;

	/// @brief \~english placement new buffer storage of
	/// allocating ktkResourceViewXXX where XXX is
	/// ZirconResourceType
	unsigned char _view_storage[64];
};

struct zircon_resource_t
{
	zircon_resource_t() :
		desc_id{_kZirconInvalidResourceID},
		view_id{_kZirconInvalidResourceID}
	{
	}
	~zircon_resource_t() {}

	zircon_resource_id_t desc_id;
	zircon_resource_id_t view_id;
};

class zircon_resource_manager
{
	struct zircon_async_load_request_t
	{
		kotek::static_path_t path;
		eZirconResourceLoadingFlags flags;
		zircon_resource_id_t desc_id;
		zircon_resource_t* p_resource;
	};

	struct zircon_async_unload_request_t
	{
		zircon_resource_id_t desc_id =
			_kZirconInvalidResourceID;
	};

	struct zircon_shared_ptr_allocator_t
	{
		zircon_shared_ptr_allocator_t() :
			_resource{
				_buf,
				sizeof(_buf),
				std::pmr::null_memory_resource()
			},
			allocator{&_resource}
		{
		}

		~zircon_shared_ptr_allocator_t() {}

		std::pmr::monotonic_buffer_resource _resource;
		// todo: kotek provide implementation for <= C++11
		// polymorphic_allocator
		std::pmr::polymorphic_allocator<zircon_resource_t>
			allocator;
		unsigned char _buf
			[sizeof(zircon_resource_t) *
		     ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT];
	};

	struct zircon_dynamic_cache_desc_t
	{
		/// @brief \~english literally 'just' memory
		void* p_data = nullptr;

		/// @brief \~english description of cache's size we
		/// created
		size_t size = 0;
	};

public:
	zircon_resource_manager();
	~zircon_resource_manager(void);

	void initialize(kotek::Core::ktkMainManager* p_main_manager
	);
	void shutdown(void);

	kotek::shared_ptr_t<zircon_resource_t> load(
		const kotek::static_path_t& path,
		eZirconResourceLoadingFlags flags,
		eZirconResourceType override_type =
			eZirconResourceType::kUnknown
	);

	void unload(zircon_resource_t* p_resource);

	const zircon_resource_desc_t*
	get_desc(zircon_resource_id_t id) const noexcept;

private:
	kotek::shared_ptr_t<zircon_resource_t> make_request(
		const kotek::static_path_t& path,
		eZirconResourceLoadingFlags flags
	);

	bool is_can_allocate_desc(void) const noexcept;
	bool is_can_allocate_view(void) const noexcept;

	zircon_resource_id_t allocate_desc() noexcept;
	zircon_resource_id_t allocate_view() noexcept;

	void load(
		const kotek::static_path_t& path,
		eZirconResourceLoadingFlags flags,
		zircon_resource_t* p_result,
		eZirconResourceType override_type =
			eZirconResourceType::kUnknown
	);

	void worker_thread();

private:
	char m_was_shutdown_called;

#ifdef ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	char m_signaled_worker_thread;
#endif

	zircon_resource_id_t m_current_desc_index;
	kotek::core::ktkIFileSystem* m_p_filesystem;
	kotek::core::ktkIFrameworkConfig* m_p_config;
#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	kotek::mt::thread_t m_worker_thread;
	kotek::static_queue_t<
		zircon_async_unload_request_t,
		ZIRCON_DEF_RESOURCE_MANAGER_MAX_QUEUE_LOADING_REQUESTS>
		m_wt_queue_unload;
	kotek::mt::mutex_t m_wt_queue_mutex;
	kotek::mt::mutex_t m_wt_queue_mutex_unload;
#endif

private:
	/// @brief \~english resources that weren't allocated
	/// through caches (static or dynamic) and they formally
	/// don't belong to resource manager since it is just
	/// new/delete allocations and we just store them here
	kotek::static_vector_t<
		void*,
		ZIRCON_DEF_RESOURCE_MANAGER_DYNAMIC_RESOURCE_COUNT>
		m_dynamic_resources;

	/// @brief storage for resources that has in description as
	/// is_cached==false && is_temp == true
	kotek::static_vector_t<
		zircon_dynamic_cache_desc_t,
		ZIRCON_DEF_RESOURCE_MANAGER_DYNAMIC_RESOURCE_COUNT>
		m_dynamic_cache;

	kotek::static_vector_t<
		uint16_t,
		ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT>
		m_resources_desc_free_indices;

	kotek::static_vector_t<
		uint16_t,
		ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT>
		m_resources_view_free_indices;

	zircon_shared_ptr_allocator_t m_allocator_shared_ptr;

#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	kotek::static_queue_t<
		zircon_async_load_request_t,
		ZIRCON_DEF_RESOURCE_MANAGER_MAX_QUEUE_LOADING_REQUESTS>
		m_wt_queue;
#endif

	kotek::static_vector_t<
		zircon_view_handle_t,
		ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT>
		m_resources_view;

	kotek::static_vector_t<
		zircon_resource_desc_t,
		ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT>
		m_resources_desc;

#if ZIRCON_DEF_RESOURCE_MANAGER_USE_STATIC_CACHE_RESOURCE_TEXT == \
	1
	zircon_static_cache_resource_text
		static_cache_resource_text;
#endif
};
