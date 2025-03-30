cmake_minimum_required(VERSION 3.19.3)

project(zircon.editor.ui VERSION 1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(to_include_module_files 
	"zircon_sdk_ui.h"
	"zircon_sdk_ui.cpp"
	"zircon_ui_top_bar.h"
	"zircon_ui_top_bar.cpp"
	"zircon_ui_object_list.cpp"
	"zircon_ui_object_list.h"
	"zircon_ui_window_game.h"
	"zircon_ui_window_game.cpp"
	"zircon_ui_window_animation.h"
	"zircon_ui_window_animation.cpp"
	"zircon_ui_window_sound.h"
	"zircon_ui_window_sound.cpp"
	"zircon_ui_window_log.h"
	"zircon_ui_window_log.cpp"
	"zircon_ui_window_prefab.h"
	"zircon_ui_window_prefab.cpp"
	"zircon_ui_window_prefab_browser.h"
	"zircon_ui_window_prefab_browser.cpp"
	"zircon_ui_component_inspector.h"
	"zircon_ui_component_inspector.cpp"
	"zircon_ui_window_history_command_log.h"
	"zircon_ui_window_history_command_log.cpp"
	"zircon_ui_window_render_stats.h"
	"zircon_ui_window_render_stats.cpp"
	"zircon_ui_window_settings.h"
	"zircon_ui_window_settings.cpp"
	"zircon_ui_window_debug_input.h"
	"zircon_ui_window_debug_input.cpp"
	"main_editor_dll.cpp"
)


add_library(${PROJECT_NAME} STATIC
	${to_include_module_files}
)

# TODO: add if when game.ktk builds as shared
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine/editor")
if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Module" FILES ${to_include_module_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
target_link_libraries(${PROJECT_NAME} PUBLIC zircon.ecs.components)