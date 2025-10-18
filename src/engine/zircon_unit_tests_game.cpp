#include "zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

#include <gtest/gtest.h>

#include "zircon_resource_manager.h"

TEST(Zircon_Game, ResourceManagerNoInit)
{
	zircon_resource_manager instance;
}

TEST(Zircon_Game, ResourceManagerInit) 
{
	kotek::core::ktkMainManager main_manager;


	zircon_resource_manager instance;
	instance.initialize(&main_manager);
	instance.shutdown();
}

TEST(Zircon_Game, ResourceManagerLoadTextResource) 
{

}


void zircon_register_unit_tests_game() {}
	#endif
#endif