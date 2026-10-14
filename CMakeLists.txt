cmake_minimum_required(VERSION 3.28)
project(EmberEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Vulkan REQUIRED COMPONENTS dxc)

include(cmake/Dependencies.cmake)
include(cmake/Shaders.cmake)

add_subdirectory(engine)
add_subdirectory(tools)
add_subdirectory(apps/sandbox)
add_subdirectory(apps/cooker)
