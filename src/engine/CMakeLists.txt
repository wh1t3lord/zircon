cmake_minimum_required(VERSION 3.21)
project(zircon VERSION 1 LANGUAGES CXX)


if ("${KOTEK_SDK}" STREQUAL "")
	set(KOTEK_SDK "OFF")
endif()

if ("${KOTEK_DEVELOPMENT_TYPE}" STREQUAL "")
	set(KOTEK_DEVELOPMENT_TYPE "SHARED")
endif()

message("[${PROJECT_NAME}]: SDK is '${KOTEK_SDK}'")

if ("${KOTEK_SDK}" STREQUAL "ON")
	add_definitions(-DKOTEK_SDK)
else()
endif()

if ("${KOTEK_DEPS_FOLDER_ENABLE}" STREQUAL "ON")
	if ("${KOTEK_DEPS_FOLDER}" STREQUAL "vcpkg")
	else()

	endif()
endif()

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

# TODO: do I really need this?
set(to_include_game_resource_management_files
	"zircon_resource_manager.h"
	"zircon_resource_manager.cpp" 
)

# Core filter folder
set(to_include_game_files
	"zircon_serialization.h"
	"zircon_deserialization.h"
	"zircon_render_pass_set_resolver.h"
	"zircon_game_manager.h"
	"zircon_game_manager.cpp"
)

set(to_include_module_files 
	"zircon_game.h"
	"main_game_dll.cpp"
)

# tests are a static-linkage feature: they instantiate concrete classes
# from kotek feature modules (ktkFrameworkConfig, ktkFileSystem), which is
# forbidden in PLUGIN mode (no link edges between plugin dlls)
if ("${KOTEK_LINKAGE}" STREQUAL "PLUGIN")
	set(to_include_unit_tests "")
else()
	set(to_include_unit_tests
		"zircon_unit_tests_game.cpp"
		"zircon_unit_tests_editor.cpp"
		"tests/zircon_unit_tests_command_history.cpp"
		"tests/zircon_unit_tests_render_passes.cpp"
		"tests/zircon_unit_tests_gltf_loader.cpp"
	)
endif()

add_library(${PROJECT_NAME} ${KOTEK_DEVELOPMENT_TYPE}
	${to_include_game_files}
	${to_include_game_resource_management_files}
	${to_include_module_files}
	${to_include_game_session_editor_files}
	${to_include_unit_tests}
)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine")

source_group("Game/Session/Interface" FILES ${to_include_game_session_interface_files})
source_group("Game/Scene" FILES ${to_include_game_scene_files})
source_group("Game/Resource Management" FILES ${to_include_game_resource_management_files})
source_group("Game/Core" FILES ${to_include_game_files})
source_group("Module" FILES ${to_include_module_files})

kotek_target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.game)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.render)
# concrete render passes: the game manager creates them through the
# pass-library seam (zircon_passlib_create/destroy + the generated pass
# factory's registry tables). Default configuration: the static passes
# library; graphics-development configuration (task Z3 P3a): the static
# twin (the hot-swappable DLL itself is LoadLibrary'd at runtime and is
# NEVER linked — an implicit link would pin it and defeat the swap)
if (ZIRCON_GRAPHICS_DEVELOPMENT)
	kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.render.passes.bgfx.static)
else()
	kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.render.passes.bgfx)
endif()
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.editor)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC kotek.ui)

set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES 
        OUTPUT_NAME "game"
        SUFFIX ".ktk"
)

# task Z3 P3a (graphics-development configuration): game.ktk re-exports
# its whole static closure (/WHOLEARCHIVE + a PRE_LINK-generated .def) so
# the hot-swappable passes DLL resolves every engine/bgfx/imgui symbol
# from the single copy inside game.ktk. Called here — the target's own
# directory — because add_custom_command(TARGET) refuses cross-directory
# targets; the dump itself runs at PRE_LINK time and sees every library.
if (ZIRCON_GRAPHICS_DEVELOPMENT)
	include("${CMAKE_SOURCE_DIR}/cmake/zircon_game_ktk_exports.cmake")
	zircon_setup_game_ktk_exports(${PROJECT_NAME})
endif()

if ("${KOTEK_DEPS_FOLDER}" STREQUAL "vcpkg")
else()
endif()