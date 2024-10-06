cmake_minimum_required(VERSION 3.19.3)

project(zircon.ecs.components VERSION 1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(to_include_module_files 
	"zircon_ecs_components.h"
	"main_ecs_dll.cpp"
)

set(to_include_game_ecs_files
	"zircon_component_camera.cpp"
	"zircon_component_camera.h"
	"zircon_component_input.cpp"
	"zircon_component_input.h"
	"zircon_component_transform.cpp"
	"zircon_component_transform.h"
	"zircon_component_visibility.cpp"
	"zircon_component_visibility.h"
	"zircon_component_geometry.h"
	"zircon_component_geometry.cpp"
	"zircon_component_model.h"
	"zircon_component_model.cpp"
	"zircon_component_actor.h"
	"zircon_component_terrain.h"
	"zircon_component_terrain.cpp"
	"zircon_component_terrain_impl_cbt.h"
	"zircon_component_terrain_impl_cbt.cpp"
	"zircon_component_frustum.h"
	"zircon_component_frustum.cpp"
	"zircon_component_bounding_sphere.h"
	"zircon_component_bounding_sphere.cpp"
	"zircon_component_ui_surface.h"
	"zircon_component_ui_surface.cpp"
	"zircon_component_ui_camera.h"
	"zircon_component_ui_camera.cpp"
	"zircon_component_interface.h"
)

set(to_include_game_factory_files 
	"zircon_factory.h"
	"zircon_factory.cpp"
)

set(to_include_sdk_ecs_files
	"zircon_component_sdk_scene_name.h"
	"zircon_component_sdk_scene_name.cpp"
	"zircon_component_sdk_camera.h"
	"zircon_component_sdk_camera.cpp"
	"zircon_component_sdk_input.h"
	"zircon_component_sdk_input.cpp"
)


add_library(${PROJECT_NAME} STATIC
	${to_include_sdk_ecs_files}
	${to_include_game_ecs_files}
	${to_include_module_files}
	${to_include_game_factory_files}
)

# TODO: add if when game.ktk builds as shared

if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Game/ECS" FILES ${to_include_game_ecs_files})
source_group("Game/Factory" FILES ${to_include_game_factory_files})
source_group("SDK/ECS" FILES ${to_include_sdk_ecs_files})
source_group("Module" FILES ${to_include_module_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)