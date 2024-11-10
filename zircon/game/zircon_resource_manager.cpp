#include "zircon_resource_manager.h"
#include "zircon_resource_loader_manager.h"

zircon_resource_manager::zircon_resource_manager(
	Kotek::Core::ktkMainManager* p_main_manager) :
	m_p_manager_resource_loader{},
	m_p_manager_resource_saver{}, m_p_manager_render_resource{},
	m_p_manager_main{p_main_manager}, m_p_manager_console{}
{
}

zircon_resource_manager::~zircon_resource_manager(void) {}

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
}

void zircon_resource_manager::Shutdown(void)
{
	KOTEK_ASSERT(
		this->m_p_manager_resource_loader, "didn't register resource loader");
	KOTEK_ASSERT(
		this->m_p_manager_resource_saver, "didn't register resource saver");

	this->m_p_manager_resource_loader->Shutdown();
	this->m_p_manager_resource_saver->Shutdown();
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

void zircon_resource_manager::update(void) noexcept {}

Kotek::ktk::uint32_t zircon_resource_manager::GenerateFileID(void) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");
	return this->m_p_manager_resource_saver->GenerateFileID();
}

kotek::shared_ptr_t<kotek::core::ktkResourceHandle> zircon_resource_manager::Load(
	const Kotek::Core::ktkLoadingRequest& request) noexcept
{
	kotek::shared_ptr_t<kotek::core::ktkResourceHandle> result{};

	if (request.Get_LoadingPolicy() ==
		Kotek::Core::eResourceLoadingPolicy::kAsync)
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
							request.Get_EntityID());

					this->m_p_manager_console->Execute(
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
	else if (request.Get_LoadingPolicy() ==
		Kotek::Core::eResourceLoadingPolicy::kSync)
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

// todo: re-write using task manager system not threads!
void zircon_resource_manager::Open(
	const Kotek::Core::ktkResourceWritingRequest& request) noexcept
{
	if (request.Get_Policy() == Kotek::Core::eResourceWritingPolicy::kSync)
	{
		if (this->m_p_manager_resource_saver)
		{
			bool result =
				this->m_p_manager_resource_saver->Open(request.Get_Path(),
					request.Get_ResourceType(), request.Get_Policy(),
					request.Get_WritingMode(), request.Get_ID());

			KOTEK_ASSERT(result, "failed to open file by path: [{}]",
				request.Get_Path());
		}
	}
	else
	{
	}
}

void zircon_resource_manager::Write(
	Kotek::ktk::uint32_t resource_id, const char* p_string) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, p_string);
	}
}

void zircon_resource_manager::Write(Kotek::ktk::uint32_t resource_id,
	const char* p_string, Kotek::ktk::size_t size) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, p_string, size);
	}
}

void zircon_resource_manager::Write(Kotek::ktk::uint32_t resource_id,
	const unsigned char* p_raw_memory) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, p_raw_memory);
	}
}

void zircon_resource_manager::Write(Kotek::ktk::uint32_t resource_id,
	const unsigned char* p_raw_memory, Kotek::ktk::size_t size) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(
			resource_id, p_raw_memory, size);
	}
}

void zircon_resource_manager::Write(
	Kotek::ktk::uint32_t resource_id, Kotek::ktk::int32_t value) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, value);
	}
}

void zircon_resource_manager::Write(
	Kotek::ktk::uint32_t resource_id, Kotek::ktk::float_t value) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, value);
	}
}

void zircon_resource_manager::Write(
	Kotek::ktk::uint32_t resource_id, Kotek::ktk::double_t value) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, value);
	}
}

void zircon_resource_manager::Write(Kotek::ktk::uint32_t resource_id,
	const Kotek::ktk::int32_t* p_arr, Kotek::ktk::size_t size) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, p_arr, size);
	}
}

void zircon_resource_manager::Write(Kotek::ktk::uint32_t resource_id,
	const Kotek::ktk::uint32_t* p_arr, Kotek::ktk::size_t size) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, p_arr, size);
	}
}

void zircon_resource_manager::Write(Kotek::ktk::uint32_t resource_id,
	const Kotek::ktk::float_t* p_arr, Kotek::ktk::size_t size) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, p_arr, size);
	}
}

void zircon_resource_manager::Write(Kotek::ktk::uint32_t resource_id,
	const Kotek::ktk::double_t* p_arr, Kotek::ktk::size_t size) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, p_arr, size);
	}
}

void zircon_resource_manager::Write(Kotek::ktk::uint32_t resource_id,
	const Kotek::ktk::int8_t* p_arr, Kotek::ktk::size_t size) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, p_arr, size);
	}
}

void zircon_resource_manager::Write(Kotek::ktk::uint32_t resource_id,
	const Kotek::ktk::int16_t* p_arr, Kotek::ktk::size_t size) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, p_arr, size);
	}
}

void zircon_resource_manager::Write(Kotek::ktk::uint32_t resource_id,
	const Kotek::ktk::uint16_t* p_arr, Kotek::ktk::size_t size) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, p_arr, size);
	}
}

void zircon_resource_manager::Write(Kotek::ktk::uint32_t resource_id,
	Kotek::Core::eFileWritingControlCharacterType type) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, type);
	}
}

void zircon_resource_manager::Write(
	Kotek::ktk::uint32_t resource_id, Kotek::ktk::size_t value) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Write(resource_id, value);
	}
}

void zircon_resource_manager::Seekg(Kotek::ktk::uint32_t resource_id,
	Kotek::ktk::size_t bytes, Kotek::Core::eFileSeekDirectionType type)
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Seekg(resource_id, bytes, type);
	}

}

void zircon_resource_manager::Seekp(Kotek::ktk::uint32_t resource_id,
	Kotek::ktk::size_t bytes, Kotek::Core::eFileSeekDirectionType type)
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Seekp(resource_id, bytes, type);
	}

}

Kotek::ktk::size_t zircon_resource_manager::Tellp(
	Kotek::ktk::uint32_t resource_id)
{
	Kotek::ktk::size_t result{};

	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		result = this->m_p_manager_resource_saver->Tellp(resource_id);
	}

	return result;
}

Kotek::ktk::size_t zircon_resource_manager::Tellg(Kotek::ktk::uint32_t resource_id)
{
	Kotek::ktk::size_t result{};

	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		result = this->m_p_manager_resource_saver->Tellg(resource_id);
	}

	return result;
}

void zircon_resource_manager::Read(Kotek::ktk::uint32_t resource_id, char* p_buffer, Kotek::ktk::size_t size) 
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");
	KOTEK_ASSERT(p_buffer, "pass a valid buffer!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Read(resource_id, p_buffer, size);
	}
}

bool zircon_resource_manager::Is_Open(Kotek::ktk::uint32_t resource_id)
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		return this->m_p_manager_resource_saver->Is_Open(resource_id);
	}

	return false;
}

void zircon_resource_manager::Close(Kotek::ktk::uint32_t resource_id) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_resource_saver, "initialize saver first!");

	if (this->m_p_manager_resource_saver)
	{
		this->m_p_manager_resource_saver->Close(resource_id);
	}
}
