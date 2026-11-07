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

# Headless weather/clock regression: does not link the engine or initialize graphics.
add_executable(TempoCheck TempoCheck.cpp)
target_include_directories(TempoCheck PRIVATE ${CMAKE_SOURCE_DIR}/engine)
target_link_libraries(TempoCheck PRIVATE nlohmann_json)
add_executable(TurnClockCheck TurnClockCheck.cpp)
target_link_libraries(TurnClockCheck PRIVATE EmberEngine)
add_executable(UploadQueueCheck UploadQueueCheck.cpp)
target_link_libraries(UploadQueueCheck PRIVATE EmberEngine)
# Headless Action Point rules check (world/ActionPoints.h) on a real battle file; run from the project root.
add_executable(ActionPointsCheck ActionPointsCheck.cpp)
target_link_libraries(ActionPointsCheck PRIVATE EmberEngine)
if(MSVC)
  target_compile_options(ActionPointsCheck PRIVATE /utf-8 /W4)
endif()
if(MSVC)
  target_compile_options(TempoCheck PRIVATE /utf-8 /W4)
  target_compile_options(TurnClockCheck PRIVATE /utf-8 /W4)
endif()

if(MSVC)
  target_compile_options(EmberTools PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()

add_executable(ModelVisibilityCheck ModelVisibilityCheck.cpp)
target_link_libraries(ModelVisibilityCheck PRIVATE EmberEngine)

add_executable(EditorSceneCheck EditorSceneCheck.cpp)
target_link_libraries(EditorSceneCheck PRIVATE EmberEngine)


add_executable(DeepAshenCheck DeepAshenCheck.cpp)
target_link_libraries(DeepAshenCheck PRIVATE EmberEngine)
if(MSVC)
  target_compile_options(DeepAshenCheck PRIVATE /utf-8 /W4)
endif()
