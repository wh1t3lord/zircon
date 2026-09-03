cmake_minimum_required(VERSION 3.21)

project(zircon.core VERSION 1 LANGUAGES CXX)


add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(to_include_module_files 
	"zircon_core.h"
	"main_core_dll.cpp"
)

kotek_add_library(${PROJECT_NAME}
	${to_include_module_files}
	"zircon_config.h"
	"zircon_config.cpp"
	"zircon_cancel_arbiter.h"
	"zircon_cancel_arbiter.cpp"
	"zircon_editor_ui_state_interface.h"
	"zircon_editor_ui_state_interface.cpp"
	"zircon_session.h"
	"zircon_session.cpp"
	"zircon_asset_manager_interface.h"
	"zircon_console.h"
	"zircon_defs.h"
	"zircon_gltf_loader.h"
	"zircon_gltf_loader.cpp"
)

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
	COMMAND "${CMAKE_BINARY_DIR}/zircon_tools/$<CONFIG>/zircon_generator" --type 1 --sdk_src ${CMAKE_SOURCE_DIR}/src/editor/ui --output ${CMAKE_CURRENT_SOURCE_DIR}/zircon_editor_enums.h
    COMMENT "Generating zircon_editor_enums.h using zircon_generator"
)

# TODO: add if when game.ktk builds as shared
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine/core")
if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Module" FILES ${to_include_module_files})

target_compile_definitions(${PROJECT_NAME} PUBLIC -DKOTEK_MODULE_GAME)

kotek_target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

kotek_target_link_libraries(${PROJECT_NAME} PUBLIC kotek.core)