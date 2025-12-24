cmake_minimum_required(VERSION 3.21)

project(zircon.game.session VERSION 1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(to_include_module_files 
	"zircon_game.h"
	"main_game_dll.cpp"
)

set(to_include_session_files
	"zircon_session_game.h"
	"zircon_session_game.cpp"
	"zircon_session_game_manager.h"
	"zircon_session_game_manager.cpp"
)


add_library(${PROJECT_NAME} STATIC
	${to_include_module_files}
	${to_include_session_files}
)

# TODO: add if when game.ktk builds as shared
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine/game")
if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Module" FILES ${to_include_module_files})
source_group("Session" FILES ${to_include_session_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
target_link_libraries(${PROJECT_NAME} PUBLIC zircon.world)