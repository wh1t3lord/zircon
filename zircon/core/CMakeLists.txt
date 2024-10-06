cmake_minimum_required(VERSION 3.19.3)

project(zircon.core VERSION 1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(to_include_module_files 
	"zircon_core.h"
	"main_core_dll.cpp"
)

add_library(${PROJECT_NAME} STATIC
	${to_include_module_files}
	"zircon_config.h"
	"zircon_config.cpp"
	"zircon_sdk_ui.h"
	"zircon_sdk_ui.cpp"
)

# TODO: add if when game.ktk builds as shared

if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Module" FILES ${to_include_module_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

target_link_libraries(${PROJECT_NAME} PUBLIC kotek.core)