cmake_minimum_required(VERSION 3.19.3)
project(zircon VERSION 1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
	"zircon_game_manager.h"
	"zircon_game_manager.cpp"
)

set(to_include_game_session_game_files
	"zircon_session_game.h"
	"zircon_session_game.cpp"
	"zircon_session_game_manager.h"
	"zircon_session_game_manager.cpp"
)

set(to_include_module_files 
	"zircon_game.h"
	"main_game_dll.cpp"
)

add_library(${PROJECT_NAME} ${KOTEK_DEVELOPMENT_TYPE}
	${to_include_game_files}
	${to_include_game_resource_management_files}
	${to_include_module_files}
	${to_include_game_session_editor_files}
	${to_include_game_session_game_files}
)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine")

source_group("Game/Session/Interface" FILES ${to_include_game_session_interface_files})
source_group("Game/Session/Game" FILES ${to_include_game_session_game_files})
source_group("Game/Scene" FILES ${to_include_game_scene_files})
source_group("Game/Resource Management" FILES ${to_include_game_resource_management_files})
source_group("Game/Core" FILES ${to_include_game_files})
source_group("Module" FILES ${to_include_module_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

target_link_libraries(${PROJECT_NAME} PUBLIC zircon.render)
target_link_libraries(${PROJECT_NAME} PUBLIC zircon.editor)
target_link_libraries(${PROJECT_NAME} PUBLIC kotek.ui)

set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES 
        OUTPUT_NAME "game"
        SUFFIX ".ktk"
)

if ("${KOTEK_DEPS_FOLDER}" STREQUAL "vcpkg")
else()
endif()