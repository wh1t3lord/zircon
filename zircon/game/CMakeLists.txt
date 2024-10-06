cmake_minimum_required(VERSION 3.19.3)
project(zircon.game VERSION 1 LANGUAGES CXX)

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

set(to_include_imgui_files 	
	"ui/ImGui/zircon_ui_top_bar.h"
	"ui/ImGui/zircon_ui_top_bar.cpp"
	"ui/ImGui/zircon_ui_object_list.cpp"
	"ui/ImGui/zircon_ui_object_list.h"
	"ui/ImGui/zircon_ui_window_game.h"
	"ui/ImGui/zircon_ui_window_game.cpp"
	"ui/ImGui/zircon_ui_window_animation.h"
	"ui/ImGui/zircon_ui_window_animation.cpp"
	"ui/ImGui/zircon_ui_window_sound.h"
	"ui/ImGui/zircon_ui_window_sound.cpp"
	"ui/ImGui/zircon_ui_window_log.h"
	"ui/ImGui/zircon_ui_window_log.cpp"
	"ui/ImGui/zircon_ui_window_prefab.h"
	"ui/ImGui/zircon_ui_window_prefab.cpp"
	"ui/ImGui/zircon_ui_window_prefab_browser.h"
	"ui/ImGui/zircon_ui_window_prefab_browser.cpp"
	"ui/ImGui/zircon_ui_component_inspector.h"
	"ui/ImGui/zircon_ui_component_inspector.cpp"
	"ui/ImGui/zircon_ui_window_history_command_log.h"
	"ui/ImGui/zircon_ui_window_history_command_log.cpp"
	"ui/ImGui/zircon_ui_window_render_system.h"
	"ui/ImGui/zircon_ui_window_render_system.cpp"
	"ui/ImGui/zircon_ui_window_settings.h"
	"ui/ImGui/zircon_ui_window_settings.cpp"
)

set(to_include_sdk_files_commands)

set(to_include_sdk_files_commands_history
	"zircon_command_history.h"
	"zircon_command_history.cpp"
	"zircon_command_add_component_to_entity.h"
	"zircon_command_add_component_to_entity.cpp"
	"zircon_command_delete_component_from_entity.h"
	"zircon_command_delete_component_from_entity.cpp"
	"zircon_command_create_entity.h"
	"zircon_command_create_entity.cpp"
	"zircon_command_delete_entity.h"
	"zircon_command_delete_entity.cpp"
	"zircon_command_definitions.h"
)

set(to_include_game_scene_files
	"zircon_scene.h"
	"zircon_scene.cpp"
	"zircon_scene_manager.h"
	"zircon_scene_manager.cpp"
)

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

# Session filter folder
set(to_include_game_session_interface_files
	"zircon_session.h"
	"zircon_session.cpp"
)

set(to_include_game_session_game_files
	"zircon_session_game.h"
	"zircon_session_game.cpp"
)

set(to_include_game_session_editor_files
	"zircon_session_editor.h"
	"zircon_session_editor.cpp"
)

set(to_include_module_files 
	"zircon_game.h"
	"main_game_dll.cpp"
)

add_library(${PROJECT_NAME} ${KOTEK_DEVELOPMENT_TYPE}
	${to_include_imgui_files}
	${to_include_game_files}
	${to_include_game_scene_files}
	${to_include_game_resource_management_files}
	${to_include_module_files}
	${to_include_game_session_interface_files}
	${to_include_game_session_editor_files}
	${to_include_game_session_game_files}
	${to_include_sdk_files_command_interface}
	${to_include_sdk_files_commands_history}
)

source_group("Game/Session/Interface" FILES ${to_include_game_session_interface_files})
source_group("Game/Session/Editor" FILES ${to_include_game_session_editor_files})
source_group("Game/Session/Game" FILES ${to_include_game_session_game_files})
source_group("Game/Scene" FILES ${to_include_game_scene_files})
source_group("Game/Resource Management" FILES ${to_include_game_resource_management_files})
source_group("Game/Core" FILES ${to_include_game_files})

source_group("SDK/ui/ImGui" FILES ${to_include_imgui_files})
source_group("SDK/commands/" FILES ${to_include_sdk_files_commands})
source_group("SDK/commands/history" FILES ${to_include_sdk_files_commands_history})

source_group("Module" FILES ${to_include_module_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

target_link_libraries(${PROJECT_NAME} PUBLIC zircon.render)
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