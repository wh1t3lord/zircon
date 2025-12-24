cmake_minimum_required(VERSION 3.21)

project(zircon.render VERSION 1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

### VULKAN
set(to_include_vulkan_files
	"vk/zircon_renderer.cpp"
	"vk/zircon_renderer.h"
	"vk/zircon_render_resource_manager.h"
	"vk/zircon_render_resource_manager.cpp")

set(to_include_vulkan_passes_files
	"vk/zircon_render_graph_pass_imgui.h"
	"vk/zircon_render_graph_pass_imgui.cpp"
	"vk/zircon_render_graph_pass_mesh.h"
	"vk/zircon_render_graph_pass_mesh.cpp"
	"vk/zircon_render_graph_pass_present.h"
	"vk/zircon_render_graph_pass_present.cpp")
### VULKAN

### OPENGL
set(to_include_opengl3_3_implementation_files
	"gl3_3/zircon_renderer.h"
	"gl3_3/zircon_renderer.cpp"
	"gl3_3/zircon_render_resource_manager.h"
	"gl3_3/zircon_render_resource_manager.cpp")

set(to_include_opengl3_3_passes_files
	"gl3_3/zircon_render_graph_pass_imgui.h"
	"gl3_3/zircon_render_graph_pass_imgui.cpp"
	"gl3_3/zircon_render_graph_pass_present.h"
	"gl3_3/zircon_render_graph_pass_present.cpp"
	"gl3_3/zircon_render_graph_pass_model.cpp"
	"gl3_3/zircon_render_graph_pass_model.h"
	"gl3_3/zircon_render_graph_pass_terrain.h"
	"gl3_3/zircon_render_graph_pass_terrain.cpp"
)
### OPENGL

### OpenGL ES
set(to_include_opengles3_implementation_files
	"gles3/zircon_renderer.h"
	"gles3/zircon_renderer.cpp"
	"gles3/zircon_render_resource_manager.h"
	"gles3/zircon_render_resource_manager.cpp"
)

set(to_include_opengles3_game_passes_no_streaming_files
	"gles3/passes/no_streaming/zircon_render_graph_pass_imgui.h"
	"gles3/passes/no_streaming/zircon_render_graph_pass_imgui.cpp"
	"gles3/passes/no_streaming/zircon_render_graph_pass_present.h"
	"gles3/passes/no_streaming/zircon_render_graph_pass_present.cpp"
	"gles3/passes/no_streaming/zircon_render_graph_pass_model_static.cpp"
	"gles3/passes/no_streaming/zircon_render_graph_pass_model_static.h"
	"gles3/passes/no_streaming/zircon_render_graph_pass_terrain.h"
	"gles3/passes/no_streaming/zircon_render_graph_pass_terrain.cpp"
	"gles3/passes/no_streaming/zircon_render_graph_pass_debug.h"
	"gles3/passes/no_streaming/zircon_render_graph_pass_debug.cpp"
)

set(to_include_opengles3_editor_passes_no_streaming_files
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_imgui.h"
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_imgui.cpp"
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_present.h"
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_present.cpp"
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_model_static.cpp"
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_model_static.h"
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_debug.h"
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_debug.cpp"
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_grid.h"
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_grid.cpp"
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_camera.h"
	"gles3/passes/no_streaming/zircon_render_graph_pass_editor_camera.cpp"
)

set(to_include_opengles3_editor_pass_interface
	"gles3/zircon_render_graph_pass_editor.h"
	"gles3/zircon_render_graph_pass_editor.cpp"
)

set(to_include_opengles3_game_pass_interface
	"gles3/zircon_render_graph_pass.h"
	"gles3/zircon_render_graph_pass.cpp"
)


### OpenGL ES

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
file(GLOB to_include_shaders_gles
	"../../data_game/shaders/glsl/gles3/*.vert"
	"../../data_game/shaders/glsl/gles3/*.frag"
)

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

set(to_include_module_files 
	"zircon_render.h"
	"main_render_dll.cpp"
)

set(to_include_passes_files_opengles3 
	${to_include_opengles3_game_passes_no_streaming_files}
	${to_include_opengles3_editor_passes_no_streaming_files}
)

set(to_include_passes_vulkan
	"todo: include and merge other like I did to_include_passes_files_opengles3"
)

set(to_include_bgfx
	${to_include_bgfx_implementation_files}
	${to_include_bgfx_game_passes_no_streaming_files}
	${to_include_bgfx_game_pass_interface}
	${to_include_bgfx_editor_passes_no_streaming_files}
	${to_include_bgfx_editor_pass_interface}
	${to_include_shaders_bgfx}
)

set(to_include_opengles3
	${to_include_opengles3_game_pass_interface}
	${to_include_opengles3_editor_pass_interface}
	${to_include_opengles3_implementation_files}
	${to_include_passes_files_opengles3}
)

add_library(${PROJECT_NAME} STATIC
#	${to_include_vulkan_files}
#	${to_include_vulkan_passes_files}
	${to_include_shaders_gles}
	${to_include_opengles3}
	${to_include_bgfx}
	${to_include_module_files}
	${to_include_shared_os}
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


# TODO: restore Vulkan when Vulkan development comes
#source_group("Vulkan/Implementation" FILES ${to_include_vulkan_files})
#source_group("Vulkan/Passes" FILES ${to_include_vulkan_passes_files})
#source_group("Vulkan/Passes/RTX" FILES ${to_include_vulkan_editor_passes_rtx_no_streaming_files})
source_group("OpenGL/3.3/Implementation" FILES ${to_include_opengl3_3_implementation_files})
source_group("OpenGL/3.3/Passes" FILES ${to_include_opengl3_3_passes_files})
source_group("OpenGL ES/3/Implementation" FILES ${to_include_opengles3_implementation_files})
source_group("OpenGL ES/3/Passes/Game/No Streaming" FILES ${to_include_opengles3_game_passes_no_streaming_files})
source_group("OpenGL ES/3/Passes/Game/Interfaces" FILES ${to_include_opengles3_game_pass_interface})
source_group("OpenGL ES/3/Passes/Editor/No Streaming" FILES ${to_include_opengles3_editor_passes_no_streaming_files})
source_group("OpenGL ES/3/Passes/Editor/Interfaces" FILES ${to_include_opengles3_editor_pass_interface})
source_group("OpenGL ES/3/Shaders" FILES ${to_include_shaders_gles})
source_group("OpenGL ES/3/Passes/OS" FILES ${to_include_shared_os})

source_group("BGFX/Implementation" FILES ${to_include_bgfx_implementation_files})
source_group("BGFX/Passes/Game/No Streaming" FILES ${to_include_bgfx_game_passes_no_streaming_files})
source_group("BGFX/Passes/Game/Interfaces" FILES ${to_include_bgfx_game_pass_interface})
source_group("BGFX/Passes/Editor/No Streaming" FILES ${to_include_bgfx_editor_passes_no_streaming_files})
source_group("BGFX/Passes/Editor/Interfaces" FILES ${to_include_bgfx_editor_pass_interface})
source_group("BGFX/Shaders" FILES ${to_include_shaders_bgfx})
source_group("BGFX/Passes/OS" FILES ${to_include_shared_os})

source_group("Module" FILES ${to_include_module_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
target_link_libraries(${PROJECT_NAME} PUBLIC zircon.ecs)
target_link_libraries(${PROJECT_NAME} PUBLIC zircon.world)
target_link_libraries(${PROJECT_NAME} PUBLIC zircon.editor)
target_link_libraries(${PROJECT_NAME} PUBLIC zircon.game)
target_link_libraries(${PROJECT_NAME} PUBLIC kotek.render)
