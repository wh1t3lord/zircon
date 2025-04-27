cmake_minimum_required(VERSION 3.19.3)
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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(
		-DKOTEK_DEBUG
	)
endif()

if ("${KOTEK_UNICODE}" STREQUAL "")
	set(KOTEK_UNICODE "ON")
endif()

message("[${PROJECT_NAME}]: UNICODE is '${KOTEK_UNICODE}'")

if("${KOTEK_UNICODE}" STREQUAL "ON")
	add_definitions(-DUNICODE -D_UNICODE -DKOTEK_UNICODE)
else()
endif()

if ("${KOTEK_TESTS}" STREQUAL "")
	set(KOTEK_TESTS "ON")
endif()

message("[${PROJECT_NAME}]: TESTS is '${KOTEK_TESTS}'")
if ("${KOTEK_TESTS}" STREQUAL "ON")
	add_definitions(-DKOTEK_TESTS)
elseif("${KOTEK_TESTS}" STREQUAL "OFF")
	set(KOTEK_TESTS_RUNTIME "OFF")
endif()

if ("${KOTEK_TESTS_RUNTIME}" STREQUAL "")
	set(KOTEK_TESTS_RUNTIME "ON")
endif()

message("[${PROJECT_NAME}]: TESTS use in runtime is '${KOTEK_TESTS_RUNTIME}'")

if ("${KOTEK_TESTS}" STREQUAL "ON")
	if ("${KOTEK_TESTS_RUNTIME}" STREQUAL "ON")
		add_definitions(-DKOTEK_TESTS_RUNTIME)
	endif()
endif()

if ("${KOTEK_CPU_PROFILER}" STREQUAL "")
	if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		set(KOTEK_CPU_PROFILER "OFF")
	else()
		set(KOTEK_CPU_PROFILER "ON")
	endif()
endif()

message("[${PROJECT_NAME}]: CPU Profiler is '${KOTEK_CPU_PROFILER}'")
if ("${KOTEK_CPU_PROFILER}" STREQUAL "ON")
	add_definitions(-DKOTEK_CPU_PROFILER)
	add_definitions(-DTRACY_ENABLE)
	add_definitions(-DTRACY_DELAYED_INIT)
	add_definitions(-DTRACY_MANUAL_LIFETIME)
endif()

if ("${KOTEK_GPU_PROFILER}" STREQUAL "")
	if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		set(KOTEK_GPU_PROFILER "OFF")
	else()
		set(KOTEK_GPU_PROFILER "ON")
	endif()
endif()

message("[${PROJECT_NAME}]: GPU Profiler is '${KOTEK_GPU_PROFILER}'")
if ("${KOTEK_GPU_PROFILER}" STREQUAL "ON")
	add_definitions(-DKOTEK_GPU_PROFILER)
endif()

if ("${KOTEK_MATH_LIBRARY}" STREQUAL "")
	if (WIN32)
		set(KOTEK_MATH_LIBRARY "DXM")
	else()
		set(KOTEK_MATH_LIBRARY "GLM")
	endif()
endif()

if ("${KOTEK_MATH_LIBRARY}" STREQUAL "GLM")
	add_definitions(-DKOTEK_MATH_LIBRARY_GLM)
	set(glm_DIR "../../${KOTEK_ENGINE_ROOT_FOLDER_NAME}/${KOTEK_DEPS_FOLDER}/glm/cmake/glm/")
elseif("${KOTEK_MATH_LIBRARY}" STREQUAL "DXM")
	add_definitions(-DKOTEK_MATH_LIBRARY_DXM)
elseif("${KOTEK_MATH_LIBRARY}" STREQUAL "EIGEN")
	add_definitions(-DKOTEK_MATH_LIBRARY_EIGEN)
endif()

message("[${PROJECT_NAME}]: math library is '${KOTEK_MATH_LIBRARY}'")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message("[${PROJECT_NAME}]: output dir is '${CMAKE_BINARY_DIR}'")

include_directories(${KOTEK_ENGINE_ROOT_FOLDER_NAME}/engine)

add_subdirectory("${KOTEK_ENGINE_ROOT_FOLDER_NAME}")
add_subdirectory("zircon/core")
add_subdirectory("zircon/ecs")
add_subdirectory("zircon/game")
add_subdirectory("zircon/game/session")
add_subdirectory("zircon/editor")
add_subdirectory("zircon/editor/commands")
add_subdirectory("zircon/editor/ui")
add_subdirectory("zircon/editor/session")
add_subdirectory("zircon/engine")
add_subdirectory("zircon/render")
add_subdirectory("zircon/world")
add_dependencies(${KOTEK_ENGINE_ROOT_FOLDER_NAME} zircon)