#pragma once

#include "zircon_ecs.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_resource_manager : public Kotek::Core::ktkIResourceManager
{
public:
	zircon_resource_manager(Kotek::Core::ktkMainManager* p_main_manager);
	~zircon_resource_manager(void);

	void Initialize(void) override;
	void Shutdown(void) override;

	kotek::ktk::shared_ptr<kotek::core::ktkResourceHandle> Load(
		const Kotek::Core::ktkLoadingRequest& request) noexcept override;

	void Open(const Kotek::Core::ktkResourceWritingRequest& request) noexcept
		override;
	void Open(const kotek::core::ktkResourceReadingRequest& request) noexcept
		override;

	void Write(Kotek::ktk::uint32_t resource_id,

		const char* p_string) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id, const char* p_string,
		Kotek::ktk::size_t size) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,

		const unsigned char* p_raw_memory) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,

		const unsigned char* p_raw_memory,
		Kotek::ktk::size_t size) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,

		Kotek::ktk::int32_t value) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,

		Kotek::ktk::float_t value) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,

		Kotek::ktk::double_t value) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,

		const Kotek::ktk::int32_t* p_arr,
		Kotek::ktk::size_t size) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,

		const Kotek::ktk::uint32_t* p_arr,
		Kotek::ktk::size_t size) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,

		const Kotek::ktk::float_t* p_arr,
		Kotek::ktk::size_t size) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,

		const Kotek::ktk::double_t* p_arr,
		Kotek::ktk::size_t size) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,
		const Kotek::ktk::int8_t* p_arr,
		Kotek::ktk::size_t size) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,

		const Kotek::ktk::int16_t* p_arr,
		Kotek::ktk::size_t size) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,

		const Kotek::ktk::uint16_t* p_arr,
		Kotek::ktk::size_t size) noexcept override;
	void Write(Kotek::ktk::uint32_t resource_id,
		Kotek::Core::eFileWritingControlCharacterType type) noexcept override;

	void Write(Kotek::ktk::uint32_t resource_id,
		Kotek::ktk::size_t value) noexcept override;
	void Seekg(Kotek::ktk::uint32_t resource_id, Kotek::ktk::size_t bytes,
		Kotek::Core::eFileSeekDirectionType type) override;
	void Seekp(Kotek::ktk::uint32_t resource_id, Kotek::ktk::size_t bytes,
		Kotek::Core::eFileSeekDirectionType type) override;
	Kotek::ktk::size_t Tellp(Kotek::ktk::uint32_t resource_id) override;
	Kotek::ktk::size_t Tellg(Kotek::ktk::uint32_t resource_id) override;
	void Read(Kotek::ktk::uint32_t resource_id, char* p_buffer,
		Kotek::ktk::size_t size) override;
	bool Is_Open(Kotek::ktk::uint32_t resource_id) override;
	void Close_Saver(Kotek::ktk::uint32_t resource_id) noexcept override;
	void Close_Loader(kotek::ktk::uint32_t resource_id) noexcept override;
	// TODO: implement for saver too!
	void Set_ResourceLoader(
		Kotek::Core::ktkIResourceLoaderManager* p_instance) noexcept override;

	void Set_RenderResourceManager(
		Kotek::Core::ktkIRenderResourceManager* p_instance) noexcept override;

	Kotek::Core::ktkIResourceLoaderManager* Get_ResourceLoader(
		void) const noexcept override;

	void Set_ResourceSaver(
		Kotek::Core::ktkIResourceSaverManager* p_instance) noexcept override;

	Kotek::Core::ktkIResourceSaverManager* Get_ResourceSaver(
		void) const noexcept override;

	Kotek::Core::ktkIRenderResourceManager* Get_RenderResourceManager(
		void) const noexcept override;

	// TODO: does we really need to have this method for storing main
	// manager? if so just delete todo otherwise delete methods and todo
	void Set_MainManager(
		Kotek::Core::ktkMainManager* p_instance) noexcept override;

	Kotek::Core::ktkMainManager* Get_MainManager(void) const noexcept override;

	void update(void) noexcept;

	/// \~english @brief uses ktkIResourceSaverManager for generating resource
	/// (file) ID
	/// @return resource ID forktkIResourceSaverManager
	kotek::ktk::uint32_t GenerateFileIDFor_Writing() noexcept override;

	/// \~english @brief uses ktkIResourceLoaderManager for generating resource
	/// (file) ID
	/// @return resouce ID for ktkIResourceLoaderManager
	kotek::ktk::uint32_t GenerateFileIDFor_Reading() noexcept override;

private:
	Kotek::Core::ktkIResourceLoaderManager* m_p_manager_resource_loader;
	Kotek::Core::ktkIResourceSaverManager* m_p_manager_resource_saver;
	Kotek::Core::ktkIRenderResourceManager* m_p_manager_render_resource;
	Kotek::Core::ktkMainManager* m_p_manager_main;
	Kotek::Core::ktkConsole* m_p_manager_console;
};
