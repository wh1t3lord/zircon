cmake_minimum_required(VERSION 3.21)

project(zircon.editor.ui VERSION 1 LANGUAGES CXX)


add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(to_include_module_files 
	"main_editor_dll.cpp"
)

set(to_include_windows_files
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
)

set(to_include_manager_files
	"zircon_editor_ui_state.h"
	"zircon_editor_ui_state.cpp"
)

kotek_add_library(${PROJECT_NAME}
	${to_include_module_files}
	${to_include_manager_files}
	${to_include_windows_files}
)

# TODO: add if when game.ktk builds as shared
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine/editor")
if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Module" FILES ${to_include_module_files})
source_group("Manager" FILES ${to_include_manager_files})
source_group("Windows" FILES ${to_include_windows_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
target_link_libraries(${PROJECT_NAME} PUBLIC zircon.ecs)