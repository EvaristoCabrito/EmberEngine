# Cooking library: turns source files into the engine's cooked formats. Shared by the Cooker app
# and (later) the editor's hot reload. Never linked into the shipped game.
add_library(EmberTools STATIC
  PakWriter.cpp
  SpriteSource.cpp
  SpriteCooker.cpp
  TextureCooker.cpp
  MeshCooker.cpp
  ArchitectureCooker.cpp
  GroundCheck.cpp
  BattleCheck.cpp
  GroundCooker.cpp
  ProceduralTextures.cpp
)

target_include_directories(EmberTools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(EmberTools PUBLIC EmberEngine PRIVATE zstd bc7 nlohmann_json meshoptimizer clusterlod cgltf stb)

if(MSVC)
  target_compile_options(EmberTools PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()
