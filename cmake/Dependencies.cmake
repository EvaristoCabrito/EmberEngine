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

# SVG rasteriser for the UI's lucide icons (content/ui/lucide), drawn at the exact pixel size on screen. plutovg comes
# in as lunasvg's own submodule; no text in the icons, so no system fonts are read.
set(LUNASVG_DISABLE_LOAD_SYSTEM_FONTS ON CACHE BOOL "" FORCE)
set(PLUTOVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(lunasvg
  GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
  GIT_TAG v3.5.0
  GIT_SHALLOW TRUE)

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

# Films in AV1 + Opus (WebM; media/VideoPlayer, the user 2026-10-25: our own decoders, every PC plays them the same, AV1 about
# half of H.264's size). dav1d (VideoLAN's AV1 decoder) builds with Meson upstream, so it is compiled here by our own target
# (cmake/Dav1d.cmake); libwebm gives only its Matroska reader; Opus builds with its own CMake.
FetchContent_Declare(dav1d
  GIT_REPOSITORY https://code.videolan.org/videolan/dav1d.git
  GIT_TAG 1.5.4
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-build)
FetchContent_Declare(libwebm
  GIT_REPOSITORY https://chromium.googlesource.com/webm/libwebm
  GIT_TAG libwebm-1.0.0.32
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-build)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(opus
  GIT_REPOSITORY https://github.com/xiph/opus.git
  GIT_TAG v1.6.1
  GIT_SHALLOW TRUE)

# --- Cooking tools only (never linked into the game) ---------------------------------------------

# BC7 encoder: bc7e's pure C++ port inside Basis Universal (only those two files are built)
FetchContent_Declare(basisu
  GIT_REPOSITORY https://github.com/BinomialLLC/basis_universal.git
  GIT_TAG v2_50
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-build)

# BC7 decoder, to measure compression quality at cook time (only bc7decomp is built)
FetchContent_Declare(bc7enc_rdo
  GIT_REPOSITORY https://github.com/richgel999/bc7enc_rdo.git
  GIT_TAG b9438627eef73a1157e84201b6fa6eb2ffd6d9f0
  SOURCE_SUBDIR do-not-build)

# JSON for source manifests (header only)
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-build)

# Mesh cooking: meshlets, cluster LOD, simplification, tangents (only the library is built; clusterlod.h
# is a single-header add-on in demo/)
set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(meshoptimizer
  GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
  GIT_TAG v1.3
  GIT_SHALLOW TRUE)

# glTF / GLB reader (single header)
FetchContent_Declare(cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG v1.15
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-build)

FetchContent_MakeAvailable(SDL3 volk vma glm stb imgui lunasvg zstd dav1d libwebm opus basisu bc7enc_rdo nlohmann_json meshoptimizer cgltf)
include(${CMAKE_CURRENT_LIST_DIR}/Dav1d.cmake)

# libwebm: only the Matroska / WebM reader (the muxer, the tools and the tests stay out)
add_library(webm_reader STATIC
  ${libwebm_SOURCE_DIR}/mkvparser/mkvparser.cc
  ${libwebm_SOURCE_DIR}/mkvparser/mkvreader.cc)
target_include_directories(webm_reader SYSTEM PUBLIC ${libwebm_SOURCE_DIR})
if(MSVC)
  target_compile_options(webm_reader PRIVATE /W0)
endif()

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

add_library(bc7 STATIC
  ${basisu_SOURCE_DIR}/encoder/basisu_bc7e_scalar.cpp
  ${bc7enc_rdo_SOURCE_DIR}/bc7decomp.cpp
  ${bc7enc_rdo_SOURCE_DIR}/rgbcx.cpp)
target_include_directories(bc7 SYSTEM PUBLIC ${basisu_SOURCE_DIR}/encoder ${bc7enc_rdo_SOURCE_DIR})
if(MSVC)
  target_compile_options(bc7 PRIVATE /W0)
endif()

add_library(cgltf INTERFACE)
target_include_directories(cgltf SYSTEM INTERFACE ${cgltf_SOURCE_DIR})

add_library(clusterlod INTERFACE)
target_include_directories(clusterlod SYSTEM INTERFACE ${meshoptimizer_SOURCE_DIR}/demo)
target_link_libraries(clusterlod INTERFACE meshoptimizer)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json SYSTEM INTERFACE ${nlohmann_json_SOURCE_DIR}/single_include)
