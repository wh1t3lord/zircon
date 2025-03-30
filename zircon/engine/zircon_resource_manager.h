#pragma once

#include "zircon_ecs.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

#define ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL 4
#define ZIRCON_DEF_RESOURCE_MANAGER_LOAD_QUEUE_SIZE 32
#define ZIRCON_DEF_RESOURCE_MANAGER_SAVE_QUEUE_SIZE \
	ZIRCON_DEF_RESOURCE_MANAGER_LOAD_QUEUE_SIZE

class zircon_resource_manager : public Kotek::Core::ktkIResourceManager
{
public:
	zircon_resource_manager(Kotek::Core::ktkMainManager* p_main_manager);
	~zircon_resource_manager(void);

	void Initialize(void) override;
	void Shutdown(void) override;

	kotek::ktk::shared_ptr<kotek::core::ktkResourceHandle> Load(
		const Kotek::Core::ktkLoadingRequest& request) noexcept override;

	/// @brief \~english not thread safe but if you need isolated fstream that
	/// you can use in any thread (but isolated) you can just call this method
	/// for getting cfstream that resource manager implementation will provide
	/// instance (or it depends on implementation) of fstream std compatiable
	/// class
	/// @param request
	/// @return
	kotek::ktk::cfstream* Open_FileStream(
		const kotek::core::ktkResourceFileStreamRequest& request,
		std::ios::openmode om = std::ios::in | std::ios::out |
			std::ios::trunc) noexcept override;

	void Close_FileStream(kotek::ktk::cfstream* p_fstream) noexcept override;

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

private:
	void update(void) noexcept;

	void process_load_requests(void) noexcept;
	void process_save_requests(void) noexcept;

	void process_load_request(
		const kotek::core::ktkResourceAssetRequest& request) noexcept;
	void process_save_request(
		const kotek::core::ktkResourceAssetRequest& request) noexcept;

	kotek::ktk::uint8_t get_available_fstream(void) const noexcept;

private:
	bool m_fstreams_avail[ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL];
	Kotek::Core::ktkIResourceLoaderManager* m_p_manager_resource_loader;
	Kotek::Core::ktkIResourceSaverManager* m_p_manager_resource_saver;
	Kotek::Core::ktkIRenderResourceManager* m_p_manager_render_resource;
	Kotek::Core::ktkMainManager* m_p_manager_main;
	Kotek::Core::ktkConsole* m_p_manager_console;
	kotek::ktk::cfstream m_fstreams[ZIRCON_DEF_RESOURCE_MANAGER_FSTREAMS_POOL];
	kotek::static_queue_t<kotek::core::ktkResourceAssetRequest,
		ZIRCON_DEF_RESOURCE_MANAGER_LOAD_QUEUE_SIZE>
		m_load_queue_requests;
	kotek::static_queue_t<kotek::core::ktkResourceAssetRequest,
		ZIRCON_DEF_RESOURCE_MANAGER_SAVE_QUEUE_SIZE>
		m_save_queue_requests;
};
