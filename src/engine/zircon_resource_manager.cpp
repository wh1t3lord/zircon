#include "zircon_resource_manager.h"

zircon_resource_manager::zircon_resource_manager() :
#ifdef KOTEK_DEBUG
	m_was_shutdown_called{-1},
#endif
	m_current_desc_index{},
	m_p_filesystem{}

{
	this->m_resources_desc.resize(
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
}

void zircon_resource_manager::initialize(
	Kotek::core::ktkMainManager* p_main_manager
)
{
#ifdef KOTEK_DEBUG
	if (m_was_shutdown_called == -1)
	{
		m_was_shutdown_called = 0;
	}
#endif

	KOTEK_ASSERT(p_main_manager, "must be valid");

	if (p_main_manager)
	{
		KOTEK_ASSERT(
			p_main_manager->GetFileSystem(),
			"invalid filesystem"
		);

		this->m_p_filesystem = p_main_manager->GetFileSystem();
	}
}

void zircon_resource_manager::shutdown(void)
{
#ifdef KOTEK_DEBUG
	m_was_shutdown_called = 1;
#endif
}

std::shared_ptr<zircon_resource_t>
zircon_resource_manager::load(
	const kotek::static_path_t& path,
	eZirconResourceLoadingFlags flags,
	eZirconResourceType override_type
)
{
#ifdef KOTEK_DEBUG
	bool invalid_1 =
		(flags & eZirconResourceLoadingFlags::kCache) ==
			eZirconResourceLoadingFlags::kCache &&
		(flags & eZirconResourceLoadingFlags::kCacheTemp) ==
			eZirconResourceLoadingFlags::kCacheTemp;

	KOTEK_ASSERT(invalid_1 == false, "don't use both");

	bool invalid_2 =
		((flags & eZirconResourceLoadingFlags::kAsync) ==
	     eZirconResourceLoadingFlags::kAsync) &&
		((flags & eZirconResourceLoadingFlags::kSync) ==
	     eZirconResourceLoadingFlags::kSync);

	KOTEK_ASSERT(invalid_2 == false, "don't use both");

#endif

	KOTEK_ASSERT(path.empty() == false, "passed empty path");

	if (path.empty())
		return std::shared_ptr<zircon_resource_t>();

	if ((flags & eZirconResourceLoadingFlags::kAsync) ==
	    eZirconResourceLoadingFlags::kAsync)
	{
		return this->make_request(path, flags);
	}
	else
	{
		std::shared_ptr<zircon_resource_t> result =
			std::make_shared<zircon_resource_t>();

		KOTEK_ASSERT(
			result.get(),
			"failed to allocate zircon_resource_t -> {}",
			path
		);

		this->load(path, flags, result.get(), override_type);

		return result;
	}

	return std::shared_ptr<zircon_resource_t>();
}

void zircon_resource_manager::load(
	const kotek::static_path_t& path,
	eZirconResourceLoadingFlags flags,
	zircon_resource_t* p_result,
	eZirconResourceType override_type
)
{
	KOTEK_ASSERT(
		this->m_p_filesystem,
		"you must have filesystem initialized"
	);

	if (!this->m_p_filesystem)
		return;

	KOTEK_ASSERT(
		this->m_p_filesystem->Is_Exists(path, true),
		"file is not exists!"
	);

	if (!this->m_p_filesystem->Is_Exists(path, true))
	{
		KOTEK_MESSAGE_WARNING(
			"the following path [{}] is not presented in OS",
			path
		);
		return;
	}

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
	}

	switch (determined_type)
	{
	case eZirconResourceType::kText:
	{
		return;
	}
	case eZirconResourceType::kTexture:
	{
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

void zircon_resource_manager::unload(
	const std::shared_ptr<zircon_resource_t>& resource
)
{
}

std::shared_ptr<zircon_resource_t>
zircon_resource_manager::make_request(
	const kotek::static_path_t& path,
	eZirconResourceLoadingFlags flags
)
{
	zircon_async_request_t req;
	req.path = path;
	req.flags = flags;

	bool can_make_request = this->is_free_desc_slots();

	KOTEK_ASSERT(
		can_make_request,
		"overflow and can't handle so much resources,change "
	    "size of think how to optimize!!!"
	);

	if (!can_make_request)
		return std::make_shared<zircon_resource_t>();

	while (this->m_wt_queue_spinlock.test_and_set(
		std::memory_order_acquire
	))
	{
	}

	req.desc_id = this->allocate_desc();
	KOTEK_ASSERT(
		req.desc_id != kotek::uint16_t(-1),
		"failed to allocate desc"
	);

	if (req.desc_id == kotek::uint16_t(-1))
		return std::make_shared<zircon_resource_t>();

	this->m_wt_queue.push(std::move(req));

	std::shared_ptr<zircon_resource_t> result =
		std::move(std::make_shared<zircon_resource_t>());

	result->set_desc(&this->m_resources_desc[req.desc_id]);

	this->m_wt_queue_spinlock.clear(
		std::memory_order::memory_order_release
	);

	return result;
}

bool zircon_resource_manager::is_free_desc_slots(void
) const noexcept
{
	return this->m_resources_desc.size() <
		ZIRCON_DEF_RESOURCE_MANAGER_RESOURCE_COUNT;
}


kotek::uint16_t
zircon_resource_manager::allocate_desc() noexcept
{
	kotek::uint16_t result = kotek::uint16_t(-1);

	if (!this->m_resources_desc_free_indices.empty())
	{
		result = this->m_resources_desc_free_indices.back();
		this->m_resources_desc_free_indices.pop_back();
	}
	else
	{
		result = this->m_current_desc_index;
		++this->m_current_desc_index;
	}

	return result;
}

zircon_resource_t::zircon_resource_t() : m_p_desc{}, m_p_view{}
{
}

zircon_resource_t::~zircon_resource_t()
{
#ifdef KOTEK_DEBUG
	if (this->m_p_desc)
	{
		if (this->m_p_desc->is_temp)
		{
			if (this->m_p_desc->is_cached)
			{
	#ifdef KOTEK_DEBUG
				KOTEK_MESSAGE(
					"[resource]: destroyed cached temp "
					"resource -> {}",
					this->m_p_desc->debug_filename
				);
	#endif
			}
			else
			{
	#ifdef KOTEK_DEBUG
				KOTEK_MESSAGE(
					"[resource]: destroyed temp resource -> {}",
					this->m_p_desc->debug_filename
				);
	#endif
			}
		}
		else
		{
			if (this->m_p_desc->is_cached)
			{
	#ifdef KOTEK_DEBUG
				KOTEK_MESSAGE(
					"[resource]: destroyed cached resource -> "
					"{}",
					this->m_p_desc->debug_filename
				);
	#endif
			}
			else
			{
	#ifdef KOTEK_DEBUG
				KOTEK_MESSAGE(
					"[resource]: destroyed resource -> {}",
					this->m_p_desc->debug_filename
				);
	#endif
			}
		}
	}
#endif

	if (this->m_p_view)
	{
		KOTEK_ASSERT(
			this->m_p_desc, "don't null before this operation"
		);

		if (this->m_p_desc)
		{
			switch (this->m_p_desc->type)
			{
			default:
			{
				KOTEK_ASSERT(
					false,
					"unreachable code, is data was corrupted "
					"by memory leaks or something?"
				);
			}
			}
		}
	}
}

const zircon_resource_desc_t*
zircon_resource_t::get_desc() const noexcept
{
	return this->m_p_desc;
}

void* zircon_resource_t::get_view_of_resource(void
) const noexcept
{
	return m_p_view;
}

void zircon_resource_t::set_desc(zircon_resource_desc_t* p_desc
) noexcept
{
	KOTEK_ASSERT(
		this->m_p_desc == nullptr, "you can't override it !"
	);

	if (p_desc)
	{
		this->m_p_desc = p_desc;
	}
}
