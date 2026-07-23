cmake_minimum_required(VERSION 3.21)
project(zircon)

# compiler globals (CRT, C++ standard, /FS, NOMINMAX) must apply to the WHOLE
# tree — kotek applies them in its own subdirectory scope otherwise, and zircon
# targets silently fall back to MSVC defaults (/MD), causing LNK2038.
if (WIN32)
	include("kotek/cmake/windows/compiler_globals.cmake")
else()
	message(FATAL_ERROR "todo: impl")
endif()

set(KOTEK_ENGINE_ROOT_FOLDER_NAME "kotek")

# TODO: when CMake will fix that we can restore but for now we can't use this due to breaking USE_FOLDERS feature...
# TODO2: Rename cmake default filter folder CMakePredefinedTargets as just plain 'targets' word
#set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "${KOTEK_ENGINE_ROOT_FOLDER_NAME}")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

get_property(is_enabled GLOBAL PROPERTY USE_FOLDERS)

if(is_enabled)
else()
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
	set(CMAKE_BUILD_TYPE Debug)
endif()

message("[${PROJECT_NAME}]: build type is '${CMAKE_BUILD_TYPE}'")

message("[${PROJECT_NAME}]: CMAKE_SYSTEM_VERSION='${CMAKE_SYSTEM_VERSION}'")

if ("${KOTEK_DEPS_FOLDER}" STREQUAL "")
else()
	message("[${PROJECT_NAME}]: your dependency folder is '${KOTEK_DEPS_FOLDER}'")
endif()

include_directories(${KOTEK_ENGINE_ROOT_FOLDER_NAME}/src)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_compile_definitions(
		KOTEK_DEBUG
	)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message("[${PROJECT_NAME}]: output dir is '${CMAKE_BINARY_DIR}'")

# full-static configuration (KOTEK_DEVELOPMENT_TYPE=STATIC): kotek.game
# calls the game-module entry points directly and needs the declaring
# header — kotek's defines module hard-fails (FATAL_ERROR at
# kotek.core.defines.static.cpp/CMakeLists.txt:551) without these vars.
# zircon_game.h is self-contained (only the ktkMainManager fwd-decl), so it
# is safe to include from kotek.game's TU. Keep the paths relocatable:
# bare quoted header name + its directory as KOTEK_USER_GAME_PROJECT_NAME
# (kotek.game turns it into ../../../<name> from its own source dir, which
# lands on this repo's root).
if ("${KOTEK_DEVELOPMENT_TYPE}" STREQUAL "STATIC")
	set(KOTEK_GAME_MODULE_FUNCTIONS_FILE "\"zircon_game.h\"" CACHE INTERNAL "header declaring the user's game-module entry points (STATIC dev type)")
	set(KOTEK_USER_GAME_PROJECT_NAME "src/engine" CACHE INTERNAL "include dir of the user's game-module entry header, relative to the layer root")
	set(KOTEK_USER_GAME_MODULE_FOR_LINK_NAME "zircon" CACHE INTERNAL "CMake target of the user's game module for kotek.game to link (STATIC dev type)")
	message("[${PROJECT_NAME}]: full-static game module, functions file: ${KOTEK_GAME_MODULE_FUNCTIONS_FILE}")
endif()

add_subdirectory("${KOTEK_ENGINE_ROOT_FOLDER_NAME}")

# the linkage-scenario define (KOTEK_USE_LINKAGE_STATIC/SHARED/PLUGIN) is
# applied by kotek/cmake/library.cmake in kotek's own directory scope only;
# zircon targets need it too (they use the KOTEK_INVOKE_MODULE_* macros and
# guard test code with it)
add_compile_definitions(KOTEK_USE_LINKAGE_${KOTEK_LINKAGE})

# Z12 embedded-config guard: force-include into every zircon.* target defined
# BELOW (directory scope — kotek's targets were already added above and are
# unaffected, so kotek itself stays configurable with any library type).
# Fails the build unless ktk_* aliases resolve to static containers.
if (WIN32)
	add_compile_options("/FI${CMAKE_CURRENT_SOURCE_DIR}/src/core/include/zircon_config_guard.h")
else()
	add_compile_options("-include" "${CMAKE_CURRENT_SOURCE_DIR}/src/core/include/zircon_config_guard.h")
endif()

# mirror of kotek's option (cache var is global): game.ktk's CRT routes its
# asserts to stderr too when automation runs without modal dialogs
if ("${KOTEK_ASSERT_STDERR_ROUTING}" STREQUAL "ON")
	add_compile_definitions(KOTEK_USE_ASSERT_STDERR_ROUTING)
endif()

message("[${PROJECT_NAME}]: KOTEK_UNICODE=${KOTEK_UNICODE}")
message("[${PROJECT_NAME}]: KOTEK_TESTS_RUNTIME=${KOTEK_TESTS_RUNTIME}")
message("[${PROJECT_NAME}]: KOTEK_TESTS=${KOTEK_TESTS}")
message("[${PROJECT_NAME}]: KOTEK_CPU_PROFILER=${KOTEK_CPU_PROFILER}")
message("[${PROJECT_NAME}]: KOTEK_GPU_PROFILER=${KOTEK_GPU_PROFILER}")
message("[${PROJECT_NAME}]: math library is KOTEK_MATH_LIBRARY=${KOTEK_MATH_LIBRARY}")


add_subdirectory("src/core")
add_subdirectory("src/ecs")
add_subdirectory("src/game")
add_subdirectory("src/game/session")
add_subdirectory("src/editor")
add_subdirectory("src/editor/commands")
add_subdirectory("src/editor/ui")
add_subdirectory("src/editor/session")
add_subdirectory("src/engine")
add_subdirectory("src/render")
add_subdirectory("src/world")

# PLUGIN mode: finalize erased-edge usage for the zircon layer too (kotek's
# own pass already ran at the end of kotek/CMakeLists.txt)
if (COMMAND kotek_finalize_plugin_links)
	kotek_finalize_plugin_links()
endif()

add_dependencies(${KOTEK_ENGINE_ROOT_FOLDER_NAME} zircon)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${KOTEK_PROJECT_NAME_OVERRIDE_KOTEK})