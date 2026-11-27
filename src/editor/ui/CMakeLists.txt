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
	"zircon_ui_csg.h"
	"zircon_ui_csg.cpp"
	"zircon_ui_window_history_command_log.h"
	"zircon_ui_window_history_command_log.cpp"
	"zircon_ui_window_render_stats.h"
	"zircon_ui_window_render_stats.cpp"
	"zircon_ui_window_settings.h"
	"zircon_ui_window_settings.cpp"
	"zircon_ui_render_passes.h"
	"zircon_ui_render_passes.cpp"
	"zircon_ui_window_debug_input.h"
	"zircon_ui_window_debug_input.cpp"
	"zircon_ui_gizmo_overlay.h"
	"zircon_ui_gizmo_overlay.cpp"
	"zircon_ui_gizmo_imguizmo.h"
	"zircon_ui_gizmo_imguizmo.cpp"
	# ImGuizmo is used only by this host window. Compiling it here keeps the
	# editor UI independent of the concrete pass library in DLL builds.
	"../../render/bgfx/passes/imguizmo/ImGuizmo.h"
	"../../render/bgfx/passes/imguizmo/ImGuizmo.cpp"
	"../../render/bgfx/passes/imguizmo/LICENSE"
)

set(to_include_manager_files
	"zircon_editor_ui_state.h"
	"zircon_editor_ui_state.cpp"
	# task Z17: the UI-press test harness (the frame-stepping injector +
	# the widget rect registry) and the test catalog (the constexpr step
	# tables + the engine-contract probes)
	"zircon_ui_test_harness.h"
	"zircon_ui_test_harness.cpp"
	"zircon_ui_test_catalog.h"
	"zircon_ui_test_catalog.cpp"
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

# ImGuizmo.cpp includes imgui_internal.h, which requires the math operators
# before imgui.h; this must be target-wide because the PCH includes imgui.
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
kotek_target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.ecs)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.world)
# concrete CSG commands are placement-created by the CSG window; session
# and renderer access itself goes through zircon.core interfaces (Z9)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.editor.commands)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC kotek.render.shared.bgfx)
# only a real editor UI DLL needs its own imgui + stats imports; static twins
# get them through game.ktk's closure (duplicate links break gfxdev exports)
get_target_property(_zircon_editor_ui_target_type ${PROJECT_NAME} TYPE)
if ("${_zircon_editor_ui_target_type}" STREQUAL "SHARED_LIBRARY")
	kotek_target_link_libraries(${PROJECT_NAME} PUBLIC kotek.ui.imgui)
	kotek_target_link_libraries(${PROJECT_NAME} PUBLIC kotek.render.shared)
endif()

# the ImGuizmo gizmo host window (Z3 P2f) includes the vendored
# ImGuizmo.h, whose quoted "imgui.h" include predates kotek's
# virtual-path layout — the raw module include dir for this target only
target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_SOURCE_DIR}/${KOTEK_ENGINE_ROOT_FOLDER_NAME}/src/kotek.ui.imgui/include")