cmake_minimum_required(VERSION 3.21)

project(zircon.world VERSION 1 LANGUAGES CXX)


add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(to_include_module_files 
	"main_world_dll.cpp"
)

set(to_include_world_files
	"zircon_world.h"
	"zircon_world.cpp"
	"zircon_world_manager.h"
	"zircon_world_manager.cpp"
)


kotek_add_library(${PROJECT_NAME}
	${to_include_module_files}
	${to_include_world_files}
)

# TODO: add if when game.ktk builds as shared
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine/world")
if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Module" FILES ${to_include_module_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.ecs)