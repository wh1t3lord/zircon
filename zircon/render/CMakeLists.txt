cmake_minimum_required(VERSION 3.19.3)

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

set(to_include_opengles3_game_passes_files
	"gles3/zircon_render_graph_pass_imgui.h"
	"gles3/zircon_render_graph_pass_imgui.cpp"
	"gles3/zircon_render_graph_pass_present.h"
	"gles3/zircon_render_graph_pass_present.cpp"
	"gles3/zircon_render_graph_pass_model_static.cpp"
	"gles3/zircon_render_graph_pass_model_static.h"
	"gles3/zircon_render_graph_pass_terrain.h"
	"gles3/zircon_render_graph_pass_terrain.cpp"
	"gles3/zircon_render_graph_pass_debug.h"
	"gles3/zircon_render_graph_pass_debug.cpp"
)

set(to_include_opengles3_editor_passes_files
	"gles3/zircon_render_graph_pass_editor_imgui.h"
	"gles3/zircon_render_graph_pass_editor_imgui.cpp"
	"gles3/zircon_render_graph_pass_editor_present.h"
	"gles3/zircon_render_graph_pass_editor_present.cpp"
	"gles3/zircon_render_graph_pass_editor_model_static.cpp"
	"gles3/zircon_render_graph_pass_editor_model_static.h"
	"gles3/zircon_render_graph_pass_editor_debug.h"
	"gles3/zircon_render_graph_pass_editor_debug.cpp"
	"gles3/zircon_render_graph_pass_editor_grid.h"
	"gles3/zircon_render_graph_pass_editor_grid.cpp"
	"gles3/zircon_render_graph_pass_editor_camera.h"
	"gles3/zircon_render_graph_pass_editor_camera.cpp"
)


### OpenGL ES

### shaders
set(to_include_shaders_gles
	"../../game_data/shaders/glsl/simple_geometry.vert"
	"../../game_data/shaders/glsl/simple_geometry.frag"
)

### shaders

### os
set(to_include_os
	"os/zircon_render_graph_pass_console.h"
	"os/zircon_render_graph_pass_console.cpp"
)
###

set(to_include_module_files 
	"zircon_render.h"
	"main_render_dll.cpp"
)

add_library(${PROJECT_NAME} STATIC
#	${to_include_vulkan_files}
#	${to_include_vulkan_passes_files}
	${to_include_shaders_gles}
	${to_include_opengles3_game_passes_files}
	${to_include_opengles3_editor_passes_files}
	${to_include_opengles3_implementation_files}
	${to_include_render_ecs_shared}
	${to_include_render_ecs_private}
	${to_include_module_files}
	${to_include_os}
)

# TODO: add if when game.ktk builds as shared

if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

# TODO: restore Vulkan when Vulkan development comes
#source_group("Vulkan/Implementation" FILES ${to_include_vulkan_files})
#source_group("Vulkan/Passes" FILES ${to_include_vulkan_passes_files})
source_group("OpenGL/3.3/Implementation" FILES ${to_include_opengl3_3_implementation_files})
source_group("OpenGL/3.3/Passes" FILES ${to_include_opengl3_3_passes_files})
source_group("OpenGL ES/3/Implementation" FILES ${to_include_opengles3_implementation_files})
source_group("OpenGL ES/3/Passes/Game" FILES ${to_include_opengles3_game_passes_files})
source_group("OpenGL ES/3/Passes/Editor" FILES ${to_include_opengles3_editor_passes_files})
source_group("OpenGL ES/3/Shaders" FILES ${to_include_shaders_gles})
source_group("OpenGL ES/3/Passes/OS" FILES ${to_include_os})
source_group("Module" FILES ${to_include_module_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
target_link_libraries(${PROJECT_NAME} PUBLIC zircon.ecs.components)
target_link_libraries(${PROJECT_NAME} PUBLIC kotek.render)
