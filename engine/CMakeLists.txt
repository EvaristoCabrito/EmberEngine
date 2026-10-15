add_library(EmberEngine STATIC
  core/Jobs.cpp
  core/Log.cpp
  core/Memory.cpp
  core/StbImpl.cpp
  platform/IoQueue.cpp
  platform/Window.cpp
  assets/Pak.cpp
  assets/AssetManager.cpp
  rhi/Vma.cpp
  rhi/VulkanContext.cpp
  rhi/Swapchain.cpp
  rhi/Image.cpp
  rhi/Bindless.cpp
  rhi/Pipeline.cpp
  rhi/GpuTimer.cpp
  rhi/UploadQueue.cpp
  render/Renderer.cpp
  render/DebugUi.cpp
  render/UiArt.cpp
  render/SpriteSystem.cpp
  render/MeshScene.cpp
  render/BoardGround.cpp
  world/Board.cpp
  world/FogOfWar.cpp
  render/FogLooks.cpp
  world/GroundMesh.cpp
  world/SceneData.cpp
  world/Battle.cpp
  world/Combat.cpp
  world/BattleTurns.cpp
  world/Spells.cpp
  debug/PerfOverlay.cpp
)

target_include_directories(EmberEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(EmberEngine PUBLIC SDL3::SDL3 volk glm::glm vma stb imgui PRIVATE zstd nlohmann_json)
target_compile_definitions(EmberEngine PUBLIC
  VK_NO_PROTOTYPES
  VMA_STATIC_VULKAN_FUNCTIONS=0
  VMA_DYNAMIC_VULKAN_FUNCTIONS=1
  GLM_FORCE_DEPTH_ZERO_TO_ONE
  GLM_FORCE_RADIANS
  NOMINMAX
  WIN32_LEAN_AND_MEAN)

if(MSVC)
  target_compile_options(EmberEngine PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
  set_source_files_properties(rhi/Vma.cpp core/StbImpl.cpp PROPERTIES COMPILE_OPTIONS "/W0")
endif()
