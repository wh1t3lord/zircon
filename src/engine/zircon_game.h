#pragma once

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

#ifdef KOTEK_USE_DEVELOPMENT_TYPE_SHARED

	#ifdef KOTEK_PLATFORM_WINDOWS

		#ifdef zircon_game_EXPORTS
			#define MODULE_EXPORT __declspec(dllexport)
		#else
			#define MODULE_EXPORT __declspec(dllimport)
		#endif
	#elif KOTEK_PLATFORM_LINUX
        #ifdef zircon_game_EXPORTS
            #define MODULE_EXPORT
        #else
            #define MODULE_EXPORT
        #endif
	#endif

	#define MODULE_EXTERN_C extern "C"
#elif defined(KOTEK_USE_DEVELOPMENT_TYPE_STATIC)
	#define MODULE_EXPORT
#else
	#error engine supports only STATIC or SHARED. See what you passed to your CMake generation
#endif

// suppose you write your game part in C language
#ifdef __cplusplus
	#define MODULE_EXTERN_C extern "C"
#else
	#define MODULE_EXTERN_C extern "C"
#endif

MODULE_EXTERN_C bool MODULE_EXPORT InitializeModule_Game(
	kotek::Core::ktkMainManager* p_main_manager);
MODULE_EXTERN_C bool MODULE_EXPORT ShutdownModule_Game(
	kotek::Core::ktkMainManager* p_main_manager);
MODULE_EXTERN_C void MODULE_EXPORT UpdateModule_Game(
	kotek::Core::ktkMainManager* p_main_manager);
MODULE_EXTERN_C bool MODULE_EXPORT InitializeModule_Render(
	kotek::Core::ktkMainManager* p_main_manager);
