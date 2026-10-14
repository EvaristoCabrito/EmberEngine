# Third-party libraries, pinned to exact releases. Only libraries, never an engine.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Window, input, gamepads
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.18
  GIT_SHALLOW TRUE)

# Vulkan function loader
FetchContent_Declare(volk
  GIT_REPOSITORY https://github.com/zeux/volk.git
  GIT_TAG vulkan-sdk-1.4.363.0
  GIT_SHALLOW TRUE)

# GPU memory allocator (header only; SOURCE_SUBDIR keeps its own CMake out of the build)
FetchContent_Declare(vma
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG v3.4.0
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-build)

# Math
FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.3
  GIT_SHALLOW TRUE)

# Image read/write for tools and frame capture
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
  SOURCE_SUBDIR do-not-build)

# Debug overlay and editor UI (docking branch: the editor needs dockable panels)
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.93.0-docking
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-build)

# Archive compression
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zstd
  GIT_REPOSITORY https://github.com/facebook/zstd.git
  GIT_TAG v1.5.7
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR build/cmake)

FetchContent_MakeAvailable(SDL3 volk vma glm stb imgui zstd)

add_library(vma INTERFACE)
target_include_directories(vma SYSTEM INTERFACE ${vma_SOURCE_DIR}/include)

add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
# The Vulkan backend loads its functions through volk, like the rest of the engine.
target_compile_definitions(imgui PUBLIC VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_USE_VOLK IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_link_libraries(imgui PUBLIC SDL3::SDL3 volk)

add_library(zstd ALIAS libzstd_static)
