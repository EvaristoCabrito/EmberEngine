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
  render/SpellEffects.cpp
  render/SpellEffectsEmber.cpp
  render/vfx/MagicMissileV2.cpp
  render/vfx/Fireball.cpp
  render/vfx/Bless.cpp
  render/vfx/HolyLight.cpp
  render/vfx/FrostLine.cpp
  render/vfx/IceStorm.cpp
  render/vfx/SpellBolts.cpp
  render/vfx/Lightning.cpp
  render/vfx/CombatFx.cpp
  render/vfx/CausticVenom.cpp
  render/vfx/AcidCloud.cpp
  render/vfx/ElementSprites.cpp
  render/vfx/PhantasmalForce.cpp
  render/vfx/WebOfDreams.cpp
  render/vfx/DreamWeb.cpp
  render/vfx/ParticleEmitter.cpp
  render/vfx/BurningHands.cpp
  render/vfx/Sweeps.cpp
  render/vfx/WarriorFx.cpp
  render/vfx/ArmsFx.cpp
  render/vfx/RangerFx.cpp
  render/vfx/LancerFx.cpp
  render/vfx/HealerFx.cpp
  render/vfx/CrusaderFx.cpp
  render/vfx/ConjurerFx.cpp
  render/vfx/StatusFx.cpp
  render/vfx/DaggerFx.cpp
  render/vfx/MageFx.cpp
  render/vfx/TurnUndead.cpp
  render/MeshScene.cpp
  render/MeshBuild.cpp
  render/BoardGround.cpp
  world/Board.cpp
  world/FogOfWar.cpp
  render/FogLooks.cpp
  render/Volumetrics.cpp
  render/Volumetrics.cpp
  world/GroundMesh.cpp
  world/SceneData.cpp
  world/SceneBuilder.cpp
  world/MapDressing.cpp
  world/ThreeGeometry.cpp
  world/Architecture.cpp
  world/ProbeBake.cpp
  world/Water3D.cpp
  world/Battle.cpp
  world/CutsceneScript.cpp
  world/Interlude.cpp
  world/BattleExport.cpp
  world/Progression.cpp
  world/BattleSave.cpp
  world/Combat.cpp
  world/BattleTurns.cpp
  world/Spells.cpp
  world/Campaign.cpp
  world/Locais.cpp
  world/Overworld.cpp
  world/Inn.cpp
  world/Quests.cpp
  world/Companions.cpp
  world/FieldSpells.cpp
  world/Loadout.cpp
  media/VideoPlayer.cpp
  media/AudioMixer.cpp
  debug/PerfOverlay.cpp
)

target_include_directories(EmberEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(EmberEngine PUBLIC SDL3::SDL3 volk glm::glm vma stb imgui nlohmann_json PRIVATE zstd)
target_link_libraries(EmberEngine PRIVATE meshoptimizer clusterlod lunasvg::lunasvg)
# Films (media/VideoPlayer): AV1 + Opus in WebM through our own decoders (dav1d, libopus, libwebm's reader); H.264 MP4s
# through Windows Media Foundation.
target_link_libraries(EmberEngine PRIVATE dav1d opus webm_reader mfplat mfreadwrite mfuuid ole32)
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
