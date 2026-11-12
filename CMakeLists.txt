cmake_minimum_required(VERSION 3.28)
project(EmberEngine LANGUAGES C CXX)

# Header records (2026-11-11): MSVC writes its /showIncludes notes in the system's language, here Portuguese ("Observação: incluindo
# arquivo:"), in UTF-8; CMake's detection stored that line mis-encoded ("Observa├º├úo"), so ninja never matched it and recorded no
# header for any object: a changed header recompiled nothing, and build.bat's check_build_deps.py then set every object aside. The
# line as the compiler writes it (only on a Portuguese compiler; no English messages are installed here, VSLANG=1033 changes nothing).
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja" AND CMAKE_CL_SHOWINCLUDES_PREFIX MATCHES "^Observa")
  set(CMAKE_CL_SHOWINCLUDES_PREFIX "Observação: incluindo arquivo: ")
endif()

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
