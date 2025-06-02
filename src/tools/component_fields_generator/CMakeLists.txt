cmake_minimum_required(VERSION 3.15)
project(AutoHeaderGenerator)

# Find Python and required components
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

# Configure paths
set(GEN_HEADER ${CMAKE_BINARY_DIR}/generated/zircon_fields.h)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)

# Execute Python parser during configuration
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate_zircon.py --src ${CMAKE_CURRENT_SOURCE_DIR}/../../ecs/components/ --kotek_src ${CMAKE_CURRENT_SOURCE_DIR}/../../../kotek/engine/kotek.core.defines_dependent.ecs/include/ --output ${GEN_HEADER}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE result
    ERROR_VARIABLE error_output
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "Header generation failed:\n${error_output}")
endif()

# Include generated header
include_directories(${CMAKE_BINARY_DIR}/generated)
add_executable(main main.cpp)