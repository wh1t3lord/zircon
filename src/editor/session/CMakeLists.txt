cmake_minimum_required(VERSION 3.21)

project(zircon.editor.session VERSION 1 LANGUAGES CXX)


add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(to_include_module_files 
	"main_editor_dll.cpp"
)

set(to_include_session_files
	"zircon_session_editor.h"
	"zircon_session_editor.cpp"
	"zircon_session_editor_manager.h"
	"zircon_session_editor_manager.cpp"
)


kotek_add_library(${PROJECT_NAME}
	${to_include_module_files}
	${to_include_session_files}
)

# TODO: add if when game.ktk builds as shared
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine/editor")
if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Module" FILES ${to_include_module_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.world)