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

class zircon_resource_manager
{
public:
	zircon_resource_manager(
		Kotek::Core::ktkMainManager* p_main_manager
	);
	~zircon_resource_manager(void);

private:

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
