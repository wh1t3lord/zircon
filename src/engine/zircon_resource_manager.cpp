#include "zircon_resource_manager.h"

zircon_resource_manager::zircon_resource_manager() :
	m_was_shutdown_called{-1}, m_signaled_worker_thread{-1},
	m_current_desc_index{}, m_p_filesystem{}, m_p_config{}

{
	this->m_resources_desc.reserve(
		ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT
	);
	this->m_resources_view.reserve(
		ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT
	);
}

zircon_resource_manager::~zircon_resource_manager(void)
{
#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(
		m_was_shutdown_called == 1 ||
			m_was_shutdown_called == -1,
		"you forgot to call shutdown"
	);
#endif

	if (m_was_shutdown_called != 1)
	{
		m_was_shutdown_called = 1;
	}

#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	if (this->m_signaled_worker_thread != char(-1))
	{
		while (this->m_signaled_worker_thread == 0)
		{
		}
	}
#endif
}

void zircon_resource_manager::initialize(
	kotek::core::ktkMainManager* p_main_manager
)
{
	if (m_was_shutdown_called == -1)
	{
		m_was_shutdown_called = 0;
	}

	KOTEK_ASSERT(p_main_manager, "must be valid");

	if (p_main_manager)
	{
		KOTEK_ASSERT(
			p_main_manager->GetFileSystem(),
			"invalid filesystem"
		);
		KOTEK_ASSERT(
			p_main_manager->Get_EngineConfig(), "invalid config"
		);

		this->m_p_filesystem = p_main_manager->GetFileSystem();
		this->m_p_config = p_main_manager->Get_EngineConfig();
	}

#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	this->m_worker_thread = kotek::mt::thread_t(
		&zircon_resource_manager::worker_thread, this
	);
	// keep the worker JOINABLE — a detached thread outlives the object and
	// reads freed members after delete (was the Release-only segfault in
	// the Zircon_Game resource-manager tests)
	this->m_signaled_worker_thread = 0;
#endif
}

void zircon_resource_manager::shutdown(void)
{
	m_was_shutdown_called = 1;

#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	// the worker watches m_was_shutdown_called and exits on its own, but
	// only a join gives the happens-before edge that makes the object safe
	// to destroy (was a use-after-free segfault in Release)
	if (this->m_worker_thread.joinable())
	{
		this->m_worker_thread.join();
	}
#endif
}

kotek::shared_ptr_t<zircon_resource_t>
zircon_resource_manager::load(
	const kotek::static_path_t& path,
	eZirconResourceLoadingFlags flags,
	eZirconResourceType override_type
)
{
#ifdef KOTEK_DEBUG
	bool invalid_1 =
		((flags & eZirconResourceLoadingFlags::kAsync) ==
	     eZirconResourceLoadingFlags::kAsync) &&
		((flags & eZirconResourceLoadingFlags::kSync) ==
	     eZirconResourceLoadingFlags::kSync);

	KOTEK_ASSERT(invalid_1 == false, "don't use both");

#endif

	KOTEK_ASSERT(path.empty() == false, "passed empty path");

	if (path.empty())
		return kotek::shared_ptr_t<zircon_resource_t>();

#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	if ((flags & eZirconResourceLoadingFlags::kAsync) ==
	    eZirconResourceLoadingFlags::kAsync)
	{
		return this->make_request(path, flags);
	}
	else
#endif
	{
		bool can_allocate = this->is_can_allocate_desc() &&
			this->is_can_allocate_view();

		if (!can_allocate)
		{
			KOTEK_MESSAGE_WARNING("failed to allocate resource!"
			);
			return kotek::shared_ptr_t<zircon_resource_t>();
		}

		kotek::shared_ptr_t<zircon_resource_t> result =
			kotek::ktk::allocate_shared<zircon_resource_t>(
				m_allocator_shared_ptr.allocator
			);

		KOTEK_ASSERT(
			result.get(),
			"failed to allocate zircon_resource_t -> {}",
			path
		);

		// same contract as the async path (make_request): the
		// resource handle must own a desc and a view slot before
		// load touches m_resources_desc/m_resources_view
		result->desc_id = this->allocate_desc();
		result->view_id = this->allocate_view();

		KOTEK_ASSERT(
			result->desc_id != _kZirconInvalidResourceID &&
				result->view_id != _kZirconInvalidResourceID,
			"failed to allocate desc/view for resource -> {}",
			path
		);

		if (result->desc_id == _kZirconInvalidResourceID ||
		    result->view_id == _kZirconInvalidResourceID)
		{
			KOTEK_MESSAGE_ERROR(
				"failed to allocate desc/view for resource -> {}",
				path
			);
			return kotek::shared_ptr_t<zircon_resource_t>();
		}

		this->load(path, flags, result.get(), override_type);

		return result;
	}

	return kotek::shared_ptr_t<zircon_resource_t>();
}

void zircon_resource_manager::load(
	const kotek::static_path_t& path,
	eZirconResourceLoadingFlags flags,
	zircon_resource_t* p_result,
	eZirconResourceType override_type
)
{
	KOTEK_ASSERT(p_result, "pass valid pointer of resource");

	if (!p_result)
	{
		KOTEK_MESSAGE_WARNING(
			"you passed empty resource handle (probably failed "
			"to allocate)"
		);
		return;
	}

	KOTEK_ASSERT(
		p_result->desc_id != _kZirconInvalidResourceID,
		"you forgot to initialize resource or failed to "
		"allocate data for resource"
	);

	KOTEK_ASSERT(
		p_result->view_id != _kZirconInvalidResourceID,
		"you forgot to initialize resource or failed to "
		"allocate data for resource"
	);

	if (p_result->desc_id == _kZirconInvalidResourceID)
	{
		KOTEK_MESSAGE_ERROR(
			"resource has invalid id related to its "
			"description means resource was not properly "
			"created!"
		);
		return;
	}

	if (p_result->view_id == _kZirconInvalidResourceID)
	{
		KOTEK_MESSAGE_ERROR(
			"resource has invalid id related to its view "
			"representation means resource was not properly "
			"created!"
		);
		return;
	}

	KOTEK_ASSERT(
		this->m_p_filesystem,
		"you must have filesystem initialized"
	);

	if (!this->m_p_filesystem)
		return;

	// task Z23 (plan Part B4): NO native-only existence pre-check here
	// (this line used to gate on Is_Exists, which only ever asked the
	// native backend and so precluded pack-hosted resources — the
	// B3-noted defect). Existence is judged by the dispatcher's read
	// path below: Get_FileSize and Begin_Stream walk the whole override
	// chain (data_user/data_game native dirs AND mounted packs per the
	// priority machinery), and when NOTHING resolves the kText branch
	// installs the embedded default — a missing resource is never a
	// fault

	KOTEK_ASSERT(path.has_filename(), "must have filename");

	if (override_type == eZirconResourceType::kUnknown)
		KOTEK_ASSERT(
			path.has_extension(), "must have extension"
		);

	if (path.has_filename() == false)
	{
		KOTEK_MESSAGE_WARNING(
			"path doesn't contain filename: {}", path
		);
		return;
	}

	if (path.has_extension() == false &&
	    override_type == eZirconResourceType::kUnknown)
	{
		KOTEK_MESSAGE_WARNING(
			"path doesn't contain extension: {}", path
		);
		return;
	}

	eZirconResourceType determined_type = override_type;
	if (override_type == eZirconResourceType::kUnknown)
	{
		const auto& extension_name = path.extension();

		if (extension_name == ".json")
		{
			determined_type = eZirconResourceType::kText;
		}
		else if (extension_name == ".ktx")
		{
			KOTEK_ASSERT(false, "todo: implement");
			determined_type = eZirconResourceType::kTexture;
		}
		else if (extension_name == ".ogg")
		{
			KOTEK_ASSERT(false, "todo: implement");
			determined_type = eZirconResourceType::kSound;
		}
		else if (extension_name == ".km")
		{
			KOTEK_ASSERT(false, "todo: implement");
		}
		else
		{
			KOTEK_ASSERT(
				false,
				"unknown file extension: [{}]",
				extension_name.c_str()
			);
			return;
		}
	}

	switch (determined_type)
	{
	case eZirconResourceType::kText:
	{
		// todo: based on cache flags we need to use virtual
		// mapping on reading files

		// todo: provide cache implementation

		//	this->m_p_filesystem->Get_FileSize()

		bool is_static_cache = KOTEK_CHECK_FLAG(
			flags, eZirconResourceLoadingFlags::kUseStaticCache
		);
		bool is_dynamic_cache = KOTEK_CHECK_FLAG(
			flags, eZirconResourceLoadingFlags::kUseDynamicCache
		);
		bool is_try_static_then_dynamic =
			is_static_cache && is_dynamic_cache;

		if (is_static_cache == false &&
		    is_dynamic_cache == false)
		{
			kotek::core::ktkResourceText<4096, 4096, true>*
				p_data = new kotek::core::
					ktkResourceText<4096, 4096, true>();

			KOTEK_ASSERT(
				p_data, "failed to allocate from OS memory allocator...!"
			);

			if (p_data)
			{
				this->m_dynamic_resources.push_back(
					static_cast<void*>(p_data)
				);

				zircon_resource_desc_t& desc =
					this->m_resources_desc[p_result->desc_id];
				desc.cache_id =
					this->m_dynamic_resources.size() - 1;
				desc.flags = flags;
				desc.type = eZirconResourceType::kText;
				desc.is_default = false;

#ifdef KOTEK_DEBUG
				desc.debug_filename = path.c_str();
#endif

				// B3: the worker's real IO — the file is read through
				// ktkIFileSystem's streaming API (Begin/Read/End_Stream)
				// in bounded chunks and the accumulated text is parsed
				// into the resource's json DOM. The no-cache text
				// resource is bounded by
				// ZIRCON_DEF_RESOURCE_MANAGER_STREAM_BUFFER_SIZE (the
				// static cache's size classes are the bigger-text
				// answer — not this branch). B4: when the dispatcher
				// chain produces nothing usable, the EMBEDDED empty-json
				// default installs below instead of failing the resource
				bool is_content_ready = false;

				kotek::size_t file_size = 0;

				const bool has_size = this->m_p_filesystem->Get_FileSize(
					path, file_size
				);

				if (has_size &&
				    file_size > 0 &&
				    file_size <=
				        ZIRCON_DEF_RESOURCE_MANAGER_STREAM_BUFFER_SIZE)
				{
					unsigned char stream_scratch
						[ZIRCON_DEF_RESOURCE_MANAGER_STREAM_BUFFER_SIZE +
						 1];

					kotek::core::ktkFileHandleType stream =
						this->m_p_filesystem->Begin_Stream(path);

					if (stream != kotek::core::kInvalidFileHandleType)
					{
						kotek::size_t total_read = 0;
						bool is_stream_ok = true;

						while (
							this->m_p_filesystem
								->Get_RemainingStreamsCount(stream) >
							0)
						{
							kotek::size_t chunk =
								sizeof(stream_scratch) - total_read;

							if (!this->m_p_filesystem->Read_Stream(
									stream,
									stream_scratch + total_read,
									chunk
								))
							{
								is_stream_ok = false;
								break;
							}

							total_read += chunk;
						}

						this->m_p_filesystem->End_Stream(stream);

						if (is_stream_ok && total_read == file_size)
						{
							is_content_ready =
								p_data->Create_FromMemory(
									stream_scratch, total_read
								);

							if (!is_content_ready)
							{
								KOTEK_MESSAGE_WARNING(
									"failed to parse text resource "
									"as json: {}",
									path
								);
							}
						}
						else
						{
							KOTEK_MESSAGE_WARNING(
								"failed to stream text resource: {}",
								path
							);
						}
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"failed to open a stream for text "
							"resource: {}",
							path
						);
					}
				}
				else if (has_size &&
				         file_size >
				             ZIRCON_DEF_RESOURCE_MANAGER_STREAM_BUFFER_SIZE)
				{
					// real content exists but does not fit the no-cache
					// bound — the static cache's size classes are that
					// answer (unimplemented branches below); the
					// embedded default still installs so the resource
					// stays valid, and is_default records the
					// degradation for the editor's later badge
					KOTEK_MESSAGE_WARNING(
						"text resource is bigger than the no-cache "
						"bound ({} > {} bytes), the embedded empty-json "
						"default is used instead: {}",
						file_size,
						ZIRCON_DEF_RESOURCE_MANAGER_STREAM_BUFFER_SIZE,
						path
					);
				}
				// (a missing file already logged its one filesystem
				// warning; an empty file has nothing to say — the
				// loud-once default line below names the path)

				// task Z23 (plan Part B4): the chain produced nothing
				// usable (missing, empty, oversized for this branch, or
				// unparseable — each reason logged above or by the
				// filesystem itself). Install the EMBEDDED empty-json
				// default: the resource stays VALID (is_loaded=true,
				// is_default=true), never a fault on missing user data
				if (is_content_ready == false)
				{
					const zircon_embedded_default_t& blob =
						zircon_embedded_defaults::get_embedded_default(
							eZirconEmbeddedDefaultType::kJson_Empty
						);

					// Create_FromMemory takes a non-const buffer (the
					// parser never writes through it, but the signature
					// is shared) — copy the 2-byte blob into scratch
					// rather than const_cast the constexpr data
					unsigned char default_scratch[8] = {};

					static_assert(
						sizeof(default_scratch) >= 2,
						"the scratch must fit the empty-json blob"
					);

					KOTEK_ASSERT(
						blob.m_size <= sizeof(default_scratch),
						"the empty-json default outgrew its scratch"
					);

					kotek::ktk::memory::memcpy(
						default_scratch, blob.m_p_bytes, blob.m_size
					);

					is_content_ready = p_data->Create_FromMemory(
						default_scratch, blob.m_size
					);

					KOTEK_ASSERT(
						is_content_ready,
						"the embedded empty-json default must always "
						"parse"
					);

					desc.is_default = true;

					this->m_embedded_defaults.log_default_used_once(
						eZirconEmbeddedDefaultType::kJson_Empty, path
					);
				}

				zircon_view_handle_t& view_handle =
					this->m_resources_view[p_result->view_id];

				// the view binds AFTER the parse: Create_FromMemory
				// replaces the resource's json object storage, so a
				// view taken earlier would bind the pre-parse storage
				view_handle.p_view =
					new (view_handle._view_storage
				    ) kotek::core::ktkResourceViewText(*p_data);

				// is_loaded PUBLISHES the resource (task Z25 A2: the
				// flag must be the LAST write — consumers spin on it
				// without an acquire edge, so a flag written before
				// the view bind published "loaded" with a null view;
				// the scheduler's worker thread widened that window
				// into a deterministic test abort)
				desc.is_loaded = is_content_ready;
			}

		}
		else if (is_try_static_then_dynamic == false &&
		         is_static_cache)
		{

		}
		else if (is_try_static_then_dynamic == false &&
		         is_dynamic_cache)
		{

		}
		else if (is_try_static_then_dynamic)
		{
			
		}
		else
		{
			KOTEK_MESSAGE_ERROR("unreachable code!");
			return;
		}

		return;
	}
	default:
	{
		KOTEK_ASSERT(
			false,
			"provide additional enum fields in case if you "
			"forgot to add otherwise unreachable code and "
			"can't be!"
		);
		return;
	}
	}
}

void zircon_resource_manager::worker_thread()
{
#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	while (this->m_was_shutdown_called != char(1))
	{
		while (this->m_wt_queue_unload.empty() == false)
		{
			const zircon_async_unload_request_t& req =
				this->m_wt_queue_unload.back();

			const zircon_resource_desc_t* p_desc =
				this->get_desc(req.desc_id);

			KOTEK_ASSERT(p_desc, "must be valid");

			if (!p_desc)
			{
				KOTEK_MESSAGE_ERROR("invalid desc was obtained!"
				);

				this->m_wt_queue_unload.pop();
				continue;
			}

			bool is_static_cached = KOTEK_CHECK_FLAG(
				p_desc->flags,
				eZirconResourceLoadingFlags::kUseStaticCache
			);

			bool is_dynamic_cached = KOTEK_CHECK_FLAG(
				p_desc->flags,
				eZirconResourceLoadingFlags::kUseDynamicCache
			);

			bool is_cached =
				is_static_cached || is_dynamic_cached;

			if (is_cached)
			{
				if (KOTEK_CHECK_FLAG(
						p_desc->flags,
						eZirconResourceLoadingFlags::
							kInvalidateCacheWhenResourceWasDestroyed
					))
				{
					KOTEK_ASSERT(
						p_desc->cache_id !=
							_kZirconInvalidResourceID,
						"something is wrong!"
					);

					KOTEK_ASSERT(false, "todo: implement");
				}
				else
				{
					// do nothing since we just cache our
					// resource
				}
			}
			else
			{
				KOTEK_ASSERT(
					KOTEK_CHECK_FLAG(
						p_desc->flags,
						eZirconResourceLoadingFlags::
							kInvalidateCacheWhenResourceWasDestroyed
					),
					"must be this"
				);

				KOTEK_ASSERT(
					p_desc->cache_id !=
						_kZirconInvalidResourceID,
					"can't be!"
				);

				KOTEK_ASSERT(
					p_desc->cache_id <
						this->m_dynamic_cache.size(),
					"can't be!"
				);

				if (p_desc->cache_id <
				        this->m_dynamic_cache.size() &&
				    KOTEK_CHECK_FLAG(
						p_desc->flags,
						eZirconResourceLoadingFlags::
							kInvalidateCacheWhenResourceWasDestroyed
					))
				{
					zircon_dynamic_cache_desc_t& desc_cache =
						this->m_dynamic_cache[p_desc->cache_id];

					delete desc_cache.p_data;
					desc_cache.p_data = nullptr;
				}
			}

			this->m_wt_queue_unload.pop();
		}

		while (this->m_wt_queue.empty() == false)
		{
			const zircon_async_load_request_t& req =
				this->m_wt_queue.back();

			this->load(req.path, req.flags, req.p_resource);

			this->m_wt_queue.pop();
		}
	}

	this->m_signaled_worker_thread = 1;
#endif
}

void zircon_resource_manager::unload(
	zircon_resource_t* p_resource
)
{
	KOTEK_ASSERT(p_resource, "you must pass a valid pointer");

	if (p_resource)
	{
#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
		if (true)
		{
			KOTEK_ASSERT(
				this->m_wt_queue_unload.size() ==
					ZIRCON_DEF_RESOURCE_MANAGER_MAX_QUEUE_LOADING_REQUESTS,
				"overflow we can't handle a such amount of "
				"requests!"
			);

			if (this->m_wt_queue_unload.size() ==
			    ZIRCON_DEF_RESOURCE_MANAGER_MAX_QUEUE_LOADING_REQUESTS)
			{
				KOTEK_ASSERT(
					false, "todo: put immediate unload here"
				);
			}

	#ifdef KOTEK_DEBUG
			const zircon_resource_desc_t* p_desc =
				this->get_desc(p_resource->desc_id);

			KOTEK_ASSERT(p_desc, "can't be");
	#endif

			zircon_async_unload_request_t req;
			req.desc_id = p_resource->desc_id;

			kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(
				this->m_wt_queue_mutex_unload);

			this->m_wt_queue_unload.push(std::move(req));
		}
		else
#endif
		{
		}
	}
}

const zircon_view_handle_t*
zircon_resource_manager::get_view(zircon_resource_id_t id
) const noexcept
{
	if (id == _kZirconInvalidResourceID)
	{
		KOTEK_MESSAGE_WARNING("passed invalid view lookup id!");
		return nullptr;
	}

	if (id < this->m_resources_view.size())
	{
		return &this->m_resources_view[id];
	}

	KOTEK_MESSAGE_WARNING("passed out of range id: {}", id);
	return nullptr;
}

const zircon_embedded_defaults&
zircon_resource_manager::get_embedded_defaults(void) const noexcept
{
	return this->m_embedded_defaults;
}

const zircon_resource_desc_t*
zircon_resource_manager::get_desc(zircon_resource_id_t id
) const noexcept
{
	if (id == _kZirconInvalidResourceID)
	{
		KOTEK_MESSAGE_WARNING("passed invalid desc lookup id!");
		return nullptr;
	}

	if (id < this->m_resources_desc.size())
	{
		return &this->m_resources_desc[id];
	}

	KOTEK_MESSAGE_WARNING("passed out of range id: {}", id);
	return nullptr;
}

kotek::shared_ptr_t<zircon_resource_t>
zircon_resource_manager::make_request(
	const kotek::static_path_t& path,
	eZirconResourceLoadingFlags flags
)
{
#if ZIRCON_DEF_RESOURCE_MANAGER_ENABLE_WORKER_THREAD == 1
	zircon_async_load_request_t req;
	req.path = path;
	req.flags = flags;

	bool can_make_request = this->is_can_allocate_desc() &&
		this->is_can_allocate_view();

	KOTEK_ASSERT(
		can_make_request,
		"overflow and can't handle so much resources,change "
		"size of think how to optimize!!!"
	);

	if (!can_make_request)
		return kotek::shared_ptr_t<zircon_resource_t>();

	kotek::mt::lock_guard_t<kotek::mt::mutex_t> lock(
		this->m_wt_queue_mutex);

	req.desc_id = this->allocate_desc();
	KOTEK_ASSERT(
		req.desc_id != _kZirconInvalidResourceID,
		"failed to allocate desc"
	);

	if (req.desc_id == _kZirconInvalidResourceID)
		return kotek::shared_ptr_t<zircon_resource_t>();

	kotek::shared_ptr_t<zircon_resource_t> result =
		std::move(kotek::ktk::allocate_shared<zircon_resource_t>(
			m_allocator_shared_ptr.allocator
		));
	result->desc_id = req.desc_id;

	// the async path owes the resource the same contract the sync path
	// gives (the B3 worker does real IO through the private load now):
	// a view slot of its own and the request wired to THIS handle —
	// the worker's load(req...) touches m_resources_view by
	// p_resource->view_id and asserts on the invalid id
	result->view_id = this->allocate_view();

	KOTEK_ASSERT(
		result->view_id != _kZirconInvalidResourceID,
		"failed to allocate view"
	);

	if (result->view_id == _kZirconInvalidResourceID)
		return kotek::shared_ptr_t<zircon_resource_t>();

	req.p_resource = result.get();

	this->m_wt_queue.push(std::move(req));

	return result;
#else
	return kotek::ktk::allocate_shared<zircon_resource_t>(
		m_allocator_shared_ptr.allocator
	);
#endif
}

bool zircon_resource_manager::is_can_allocate_desc(void
) const noexcept
{
	return this->m_resources_desc.size() <
		ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT;
}

bool zircon_resource_manager::is_can_allocate_view(void
) const noexcept
{
	return this->m_resources_view.size() <
		ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT;
}

zircon_resource_id_t
zircon_resource_manager::allocate_desc() noexcept
{
	zircon_resource_id_t result = _kZirconInvalidResourceID;

	if (!this->m_resources_desc_free_indices.empty())
	{
		result = this->m_resources_desc_free_indices.back();
		this->m_resources_desc_free_indices.pop_back();
	}
	else
	{
		// a fresh index must reference a constructed slot: the
		// vectors are only reserved in the ctor, so grow here
		result = this->m_resources_desc.size();
		this->m_resources_desc.emplace_back();
	}

	return result;
}

zircon_resource_id_t
zircon_resource_manager::allocate_view() noexcept
{
	zircon_resource_id_t result = _kZirconInvalidResourceID;

	if (!this->m_resources_view_free_indices.empty())
	{
		result = this->m_resources_view_free_indices.back();
		this->m_resources_view_free_indices.pop_back();
	}
	else
	{
		// a fresh index must reference a constructed slot: the
		// vectors are only reserved in the ctor, so grow here
		result = this->m_resources_view.size();
		this->m_resources_view.emplace_back();
	}

	return result;
}
