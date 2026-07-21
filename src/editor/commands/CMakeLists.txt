cmake_minimum_required(VERSION 3.21)

project(zircon.editor.commands VERSION 1 LANGUAGES CXX)


add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(to_include_module_files 
	"main_editor_dll.cpp"
)

set(to_include_manager_files
	"zircon_command_history.h"
	"zircon_command_history.cpp"
)

set(to_include_commands_files
	"zircon_command_add_component_to_entity.h"
	"zircon_command_add_component_to_entity.cpp"
	"zircon_command_delete_component_from_entity.h"
	"zircon_command_delete_component_from_entity.cpp"
	"zircon_command_create_entity.h"
	"zircon_command_create_entity.cpp"
	"zircon_command_delete_entity.h"
	"zircon_command_delete_entity.cpp"
)

kotek_add_library(${PROJECT_NAME}
	${to_include_module_files}
	${to_include_manager_files}
	${to_include_commands_files}
	"zircon_command_definitions.h"
)

# TODO: add if when game.ktk builds as shared
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine/editor")
if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Module" FILES ${to_include_module_files})
source_group("Manager" FILES ${to_include_manager_files})
source_group("Commands" FILES ${to_include_commands_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
target_link_libraries(${PROJECT_NAME} PUBLIC zircon.ecs)