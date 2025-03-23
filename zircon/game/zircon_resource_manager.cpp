#include "zircon_resource_manager.h"
#include "zircon_resource_loader_manager.h"

zircon_resource_manager::zircon_resource_manager(
	Kotek::Core::ktkMainManager* p_main_manager) :
	m_fstreams_avail{}, m_p_manager_resource_loader{},
	m_p_manager_resource_saver{}, m_p_manager_render_resource{},
	m_p_manager_main{p_main_manager}, m_p_manager_console{}
{
	std::memset(this->m_fstreams_avail, true,
		sizeof(this->m_fstreams_avail[0]) *
			ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL);
}

zircon_resource_manager::~zircon_resource_manager(void)
{
#ifdef KOTEK_DEBUG

	for (kotek::uint8_t i = 0; i < ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL;
		++i)
	{
		KOTEK_ASSERT(this->m_fstreams_avail[i],
			"must be false! You forgot to close files before shutdowing it is "
			"not good");
	}
#endif
}

void zircon_resource_manager::Initialize(void)
{
	KOTEK_ASSERT(
		this->m_p_manager_resource_saver, "must register first resource saver");
	KOTEK_ASSERT(this->m_p_manager_resource_loader,
		"must register first resource loader");
	KOTEK_ASSERT(this->m_p_manager_main, "must register main manager");

	this->m_p_manager_resource_saver->Initialize(
		this->m_p_manager_main->GetFileSystem(), this->m_p_manager_main);
	this->m_p_manager_resource_loader->Initialize(
		this->m_p_manager_main->GetFileSystem(), this->m_p_manager_main);
	this->m_p_manager_console =
		this->m_p_manager_main->GetGameManager()->GetConsole();

	KOTEK_ASSERT(this->m_p_manager_console,
		"you must have an initialized console manager");
	KOTEK_ASSERT(
		this->m_p_manager_main->Get_EngineConfig(), "must exist and be valid");
	KOTEK_ASSERT(
		this->m_p_manager_main->Get_EngineConfig()->IsApplicationWorking(),
		"must working");

	std::thread(
		[&]()
		{
			while (true)
			{
				if (this->m_p_manager_main)
				{
					if (this->m_p_manager_main->Get_EngineConfig())
					{
						if (this->m_p_manager_main->Get_EngineConfig()
								->IsApplicationWorking() == false)
						{
							break;
						}
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"[thread][resource manager]: engine config wasn't "
							"initialized shutdown the thread...");
						std::this_thread::sleep_for(
							std::chrono::milliseconds(1000));
						break;
					}
				}
				else
				{
					KOTEK_MESSAGE_WARNING(
						"[thread][resource manager]: main manager wasn't "
						"initialized shutdown the thread...");
					std::this_thread::sleep_for(
						std::chrono::milliseconds(1000));
					break;
				}

				this->update();
			}
		})
		.detach();
}

void zircon_resource_manager::Shutdown(void)
{
	KOTEK_ASSERT(
		this->m_p_manager_resource_loader, "didn't register resource loader");
	KOTEK_ASSERT(
		this->m_p_manager_resource_saver, "didn't register resource saver");

	this->m_p_manager_resource_loader->Shutdown();
	this->m_p_manager_resource_saver->Shutdown();

	delete this->m_p_manager_resource_loader;
	this->m_p_manager_resource_loader = nullptr;

	delete this->m_p_manager_resource_saver;
	this->m_p_manager_resource_saver = nullptr;
}

void zircon_resource_manager::Set_ResourceLoader(
	Kotek::Core::ktkIResourceLoaderManager* p_instance) noexcept
{
	this->m_p_manager_resource_loader = p_instance;
}

void zircon_resource_manager::Set_RenderResourceManager(
	Kotek::Core::ktkIRenderResourceManager* p_instance) noexcept
{
	this->m_p_manager_render_resource = p_instance;
}

Kotek::Core::ktkIResourceLoaderManager*
zircon_resource_manager::Get_ResourceLoader(void) const noexcept
{
	return this->m_p_manager_resource_loader;
}

void zircon_resource_manager::Set_ResourceSaver(
	Kotek::Core::ktkIResourceSaverManager* p_instance) noexcept
{
	this->m_p_manager_resource_saver = p_instance;
}

Kotek::Core::ktkIResourceSaverManager*
zircon_resource_manager::Get_ResourceSaver(void) const noexcept
{
	return this->m_p_manager_resource_saver;
}

Kotek::Core::ktkIRenderResourceManager*
zircon_resource_manager::Get_RenderResourceManager(void) const noexcept
{
	return this->m_p_manager_render_resource;
}

void zircon_resource_manager::Set_MainManager(
	Kotek::Core::ktkMainManager* p_instance) noexcept
{
	this->m_p_manager_main = p_instance;
}

Kotek::Core::ktkMainManager* zircon_resource_manager::Get_MainManager(
	void) const noexcept
{
	return this->m_p_manager_main;
}

void zircon_resource_manager::update(void) noexcept
{
	this->process_load_requests();
	this->process_save_requests();
}

void zircon_resource_manager::process_load_requests(void) noexcept
{
	if (this->m_load_queue_requests.empty() == false)
	{
	}
}

void zircon_resource_manager::process_save_requests(void) noexcept
{
	if (this->m_save_queue_requests.empty() == false)
	{
	}
}

void zircon_resource_manager::process_load_request(
	const kotek::core::ktkResourceAssetRequest& request) noexcept
{
}

void zircon_resource_manager::process_save_request(
	const kotek::core::ktkResourceAssetRequest& request) noexcept
{
}

kotek::ktk::uint8_t zircon_resource_manager::get_available_fstream(
	void) const noexcept
{
	kotek::ktk::uint8_t result{};

	for (; result <= ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL; ++result)
	{
		if (result == ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL)
			break;

		if (this->m_fstreams_avail[result])
		{
			break;
		}
	}

	return result;
}

kotek::shared_ptr_t<kotek::core::ktkResourceHandle>
zircon_resource_manager::Load(
	const Kotek::Core::ktkLoadingRequest& request) noexcept
{
	kotek::shared_ptr_t<kotek::core::ktkResourceHandle> result{};

	if (request.Get_ThreadingPolicy() ==
		Kotek::Core::eResourceThreadingPolicy::kAsync)
	{
		switch (request.Get_ResourceType())
		{
		case Kotek::Core::eResourceLoadingType::kModelStatic_Triangle:
		{
			if (request.Get_CachingPolicy() ==
				Kotek::Core::eResourceCachingPolicy::kWithoutCache)
			{
				if (this->m_p_manager_render_resource)
				{
					auto p_resource_geometry =
						this->m_p_manager_render_resource->LoadGeometry(
							static_cast<Kotek::ktk::enum_base_t>(
								request.Get_ResourceType()),
							0);

					this->m_p_manager_console->Execute_Command(
						static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eConsoleCommandIndex::
								kConsoleCommand_Render_CalculateBoundingPrimitive),
						{p_resource_geometry,
							static_cast<Kotek::ktk::enum_base_t>(
								Kotek::Core::eRenderBoundingPrimitiveType::
									kBoundingSphere),
							static_cast<kotek::uint32_t>(
								request.Get_EntityID())});

					result = std::move(p_resource_geometry);
				}
			}
			else
			{
			}

			break;
		}
		default:
		{
			KOTEK_ASSERT(false, "not supported");
		}
		}
	}
	else if (request.Get_ThreadingPolicy() ==
		Kotek::Core::eResourceThreadingPolicy::kSync)
	{
		if (request.Get_CachingPolicy() ==
			Kotek::Core::eResourceCachingPolicy::kWithoutCache)
		{
			if (this->m_p_manager_resource_loader)
			{
				// think about it
				// result = this->m_p_manager_resource_loader->Load(
				//	request.Get_ResourcePath());
				KOTEK_ASSERT(false, "not implemented");
			}
		}
		else
		{
			KOTEK_ASSERT(false, "not implemented");
		}
	}
#ifdef KOTEK_DEBUG
	else
	{
		KOTEK_ASSERT(false, "something is wrong! maybe request is corrupted!");
	}
#endif

	return result;
}

kotek::ktk::cfstream* zircon_resource_manager::Open_FileStream(
	const kotek::core::ktkResourceFileStreamRequest& request,
	std::ios::openmode om) noexcept
{
	kotek::cfstream_t* p_result{};

	kotek::uint8_t array_index = this->get_available_fstream();

	KOTEK_ASSERT(array_index < ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL,
		"failed to obtain a free slot of fstream you have to optimize your "
		"workflow otherwise allocate more fstream instances!");

	if (array_index < ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL)
	{
		p_result = &this->m_fstreams[array_index];

		KOTEK_ASSERT(request.path_to_file.empty() == false,
			"something is wrong and your request doesn't contain a path at "
			"all!");

		switch (request.resource_type)
		{
		case kotek::core::eResourceRequestResourceType::kText:
		{
			p_result->open(request.path_to_file.c_str(), om);
			break;
		}
		case kotek::core::eResourceRequestResourceType::kBinary:
		{
			om |= std::ios::binary;
			p_result->open(request.path_to_file.c_str(), om);
			break;
		}
		default:
		{
			KOTEK_ASSERT(false, "you must pass kText or kBinary!");
			break;
		}
		}

		this->m_fstreams_avail[array_index] = false;
	}

	return p_result;
}

void zircon_resource_manager::Close_FileStream(
	kotek::ktk::cfstream* p_fstream) noexcept
{
	KOTEK_ASSERT(p_fstream,
		"you must pass instance of cfstream what your resource manager "
		"returned!");

	if (p_fstream)
	{
		p_fstream->close();

		for (kotek::uint8_t i = 0;
			i < ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL; ++i)
		{
			kotek::cfstream_t* p_pointer = &this->m_fstreams[i];

			if (p_pointer == p_fstream)
			{
				this->m_fstreams_avail[i] = true;
				break;
			}
		}
	}
}
