cmake_minimum_required(VERSION 3.15)
project(zircon_shaderpack)

# Host tool (same structural pattern as
# src/tools/cpp_generators/ecs_fields_generator): configured STANDALONE via a
# nested cmake invocation at engine BUILD time (cmake/zircon_slang_shaders.cmake),
# so it must set its own standard — it does not inherit the engine root's
# compiler globals. std is allowed in tools (AGENTS.md §2.1 exemption); no
# kotek/bgfx includes are needed — the container writer is self-contained.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} zircon_shaderpack.cpp)
