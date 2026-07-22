cmake_minimum_required(VERSION 3.21)

project(zircon.game VERSION 1 LANGUAGES CXX)


add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(to_include_module_files 
	"zircon_game.h"
	"main_game_dll.cpp"
)

kotek_add_library(${PROJECT_NAME}
	${to_include_module_files}
	"zircon_asset_manager.h"
	"zircon_asset_manager.cpp"
)

# TODO: add if when game.ktk builds as shared
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine/game")
if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Module" FILES ${to_include_module_files})
source_group("Session" FILES ${to_include_session_files})

kotek_target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.ecs)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.world)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.game.session)