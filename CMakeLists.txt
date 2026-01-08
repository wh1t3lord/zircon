cmake_minimum_required(VERSION 3.21)
project(zircon)

set(KOTEK_ENGINE_ROOT_FOLDER_NAME "kotek")

# TODO: when CMake will fix that we can restore but for now we can't use this due to breaking USE_FOLDERS feature...
# TODO2: Rename cmake default filter folder CMakePredefinedTargets as just plain 'targets' word
#set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "${KOTEK_ENGINE_ROOT_FOLDER_NAME}")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

get_property(is_enabled GLOBAL PROPERTY USE_FOLDERS)

if(is_enabled)
else()
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
	set(CMAKE_BUILD_TYPE Debug)
endif()

message("[${PROJECT_NAME}]: build type is '${CMAKE_BUILD_TYPE}'")

message("[${PROJECT_NAME}]: CMAKE_SYSTEM_VERSION='${CMAKE_SYSTEM_VERSION}'")

if ("${KOTEK_DEPS_FOLDER}" STREQUAL "")
else()
	message("[${PROJECT_NAME}]: your dependency folder is '${KOTEK_DEPS_FOLDER}'")
endif()

include_directories(${KOTEK_ENGINE_ROOT_FOLDER_NAME}/src)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_compile_definitions(
		KOTEK_DEBUG
	)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message("[${PROJECT_NAME}]: output dir is '${CMAKE_BINARY_DIR}'")

add_subdirectory("${KOTEK_ENGINE_ROOT_FOLDER_NAME}")

message("[${PROJECT_NAME}]: KOTEK_UNICODE=${KOTEK_UNICODE}")
message("[${PROJECT_NAME}]: KOTEK_TESTS_RUNTIME=${KOTEK_TESTS_RUNTIME}")
message("[${PROJECT_NAME}]: KOTEK_TESTS=${KOTEK_TESTS}")
message("[${PROJECT_NAME}]: KOTEK_CPU_PROFILER=${KOTEK_CPU_PROFILER}")
message("[${PROJECT_NAME}]: KOTEK_GPU_PROFILER=${KOTEK_GPU_PROFILER}")
message("[${PROJECT_NAME}]: math library is KOTEK_MATH_LIBRARY=${KOTEK_MATH_LIBRARY}")


add_subdirectory("src/core")
add_subdirectory("src/ecs")
add_subdirectory("src/game")
add_subdirectory("src/game/session")
add_subdirectory("src/editor")
add_subdirectory("src/editor/commands")
add_subdirectory("src/editor/ui")
add_subdirectory("src/editor/session")
add_subdirectory("src/engine")
add_subdirectory("src/render")
add_subdirectory("src/world")
add_dependencies(${KOTEK_ENGINE_ROOT_FOLDER_NAME} zircon)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${KOTEK_PROJECT_NAME_OVERRIDE_KOTEK})