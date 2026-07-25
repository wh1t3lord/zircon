cmake_minimum_required(VERSION 3.21)

project(zircon.render VERSION 1 LANGUAGES CXX)


add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

# render backends: bgfx (raster) is the only built-in one; NRI (dx12/vk +
# ray tracing) is planned and gets its own folder with the same two-project
# split (passes + executor). gles3/vk were removed (2026-07-22, owner
# decision).

### BGFX

set(to_include_bgfx_implementation_files
	"bgfx/zircon_renderer.h"
	"bgfx/zircon_renderer.cpp"
	"bgfx/zircon_render_resource_manager.h"
	"bgfx/zircon_render_resource_manager.cpp"
)

set(to_include_bgfx_editor_pass_interface
	"bgfx/zircon_render_graph_pass_editor.h"
	"bgfx/zircon_render_graph_pass_editor.cpp"
)

set(to_include_bgfx_game_pass_interface
	"bgfx/zircon_render_graph_pass.h"
	"bgfx/zircon_render_graph_pass.cpp"
)

set(to_include_bgfx_game_passes_no_streaming_files
	"bgfx/passes/no_streaming/zircon_render_graph_pass_imgui.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_imgui.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_present.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_present.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_model_static.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_model_static.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_terrain.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_terrain.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_debug.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_debug.cpp"
)

set(to_include_bgfx_editor_passes_no_streaming_files
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_imgui.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_imgui.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_present.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_present.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_model_static.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_model_static.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_debug.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_debug.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_grid.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_grid.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_camera.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_camera.cpp"
)

### BGFX

### shaders
file(GLOB to_include_shaders_bgfx
	"../../data_game/shaders/bgfx/*.frag"
	"../../data_game/shaders/bgfx/*.vert"
)

### shaders

### os
set(to_include_shared_os
	"os/zircon_render_graph_pass_console.h"
	"os/zircon_render_graph_pass_console.cpp"
)
###

### NRI (task K11/Z5 phase 1: minimal renderer over the kotek.render.nri
### backend; the passes split is the deferred Z5 work)
set(to_include_nri
	"nri/zircon_renderer_nri.h"
	"nri/zircon_renderer_nri.cpp"
)
###

set(to_include_module_files
	"zircon_render.h"
	"main_render_dll.cpp"
)

set(to_include_bgfx
	${to_include_bgfx_implementation_files}
	${to_include_bgfx_game_passes_no_streaming_files}
	${to_include_bgfx_game_pass_interface}
	${to_include_bgfx_editor_passes_no_streaming_files}
	${to_include_bgfx_editor_pass_interface}
	${to_include_shaders_bgfx}
)

kotek_add_library(${PROJECT_NAME}
	${to_include_bgfx}
	${to_include_module_files}
	${to_include_shared_os}
	${to_include_nri}
)

# TODO: add if when game.ktk builds as shared

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine/render")

if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()


add_custom_command(
	TARGET ${PROJECT_NAME}
	PRE_BUILD
	COMMAND ${CMAKE_COMMAND}
		-S ${CMAKE_SOURCE_DIR}/src/tools/cpp_generators/ecs_fields_generator
		-B ${CMAKE_BINARY_DIR}/zircon_tools
		-DKOTEK_DEPS_FOLDER=${KOTEK_DEPS_FOLDER}
		-DKOTEK_DEPS_FOLDER_ENABLE=${KOTEK_DEPS_FOLDER_ENABLE}
		-DKOTEK_ENGINE_ROOT_FOLDER_NAME=${KOTEK_ENGINE_ROOT_FOLDER_NAME}
		-DKOTEK_NUGET_PLATFORM_NAME=${KOTEK_NUGET_PLATFORM_NAME}
		-DKOTEK_NUGET_COMPILER_NAME=${KOTEK_NUGET_COMPILER_NAME}
		-DKOTEK_NUGET_ARCH_BIT_NUMBER=${KOTEK_NUGET_ARCH_BIT_NUMBER}
	COMMAND ${CMAKE_COMMAND}
		--build ${CMAKE_BINARY_DIR}/zircon_tools
		--config $<CONFIG>
	COMMAND "${CMAKE_BINARY_DIR}/zircon_tools/$<CONFIG>/zircon_generator" --type 2 ${CMAKE_SOURCE_DIR}/src/render/bgfx/passes/no_streaming
    COMMENT "generating render = no_streaming"
)


source_group("BGFX/Implementation" FILES ${to_include_bgfx_implementation_files})
source_group("BGFX/Passes/Game/No Streaming" FILES ${to_include_bgfx_game_passes_no_streaming_files})
source_group("BGFX/Passes/Game/Interfaces" FILES ${to_include_bgfx_game_pass_interface})
source_group("BGFX/Passes/Editor/No Streaming" FILES ${to_include_bgfx_editor_passes_no_streaming_files})
source_group("BGFX/Passes/Editor/Interfaces" FILES ${to_include_bgfx_editor_pass_interface})
source_group("BGFX/Shaders" FILES ${to_include_shaders_bgfx})
source_group("BGFX/Passes/OS" FILES ${to_include_shared_os})
source_group("NRI" FILES ${to_include_nri})

source_group("Module" FILES ${to_include_module_files})

kotek_target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.ecs)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.world)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.editor)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.game)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC kotek.render)
# render glue shared with the bgfx backend (pass bases, simplified graph) —
# static leaf, linked directly even in PLUGIN mode
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC kotek.render.shared.bgfx)
