#include "zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include "zircon_resource_manager.h"

		#if ZIRCON_DEF_UNIT_TEST_RESOURCE_MANAGER == 1

// note: gtest defines for each thread/task own stack size so it
// is hard to use full stack and if you have a really low RAM it
// is better to disable/enable tests related to resource
// manager, by default it is unreal to test resource manager
// using only stack allocation since it exceeds 1Mb size :c
TEST(Zircon_Game, ResourceManagerCtorDtor)
{
	zircon_resource_manager* p_resource_manager =
		new zircon_resource_manager();
	delete p_resource_manager;
}

TEST(Zircon_Game, ResourceManagerInitShutdown)
{
	kotek::core::ktkMainManager main_manager;

	zircon_resource_manager* instance =
		new zircon_resource_manager();
	instance->initialize(&main_manager);
	instance->shutdown();
	delete instance;
}

TEST(Zircon_Game, ResourceManagerLoadTextResourceNoCache)
{
	kotek::core::ktkMainManager main_manager;

	zircon_resource_manager* p_rm =
		new zircon_resource_manager();

	p_rm->initialize(&main_manager);
	p_rm->shutdown();

	delete p_rm;
}

TEST(Zircon_Game, ResourceManagerLoadTextResourceCached) 
{
}

TEST(Zircon_Game, ResourceManagerLoadTextResourceCacheAndUnload)
{
}

TEST(
	Zircon_Game,
	ResourceManagerLoadTextResourceCacheAndUnloadAndDestroy
)
{
}

TEST(Zircon_Game, ResourceManagerLoadTextResourceMultithreading)
{
}

TEST(Zircon_Game, ResourceManagerLoadTextureResource) 
{

}

TEST(Zircon_Game, ResourceManagerLoadSoundResource) 
{

}

TEST(Zircon_Game, ResourceManagerLoad3DModel) 
{

}

TEST(Zircon_Game, ResourceManagerLoad3DModelAnimation) 
{

}

TEST(Zircon_Game, ResourceManagerLoadUI) 
{

}

		#endif

void zircon_register_unit_tests_game() {}
	#endif
#endif