add_executable(Sandbox main.cpp)
target_link_libraries(Sandbox PRIVATE EmberEngine)
if(MSVC)
  target_compile_options(Sandbox PRIVATE /W4 /permissive- /utf-8)
endif()

ember_shaders(Sandbox FILE ${CMAKE_SOURCE_DIR}/shaders/triangle.hlsl STAGES vs:VSMain ps:PSMain)
ember_shaders(Sandbox FILE ${CMAKE_SOURCE_DIR}/shaders/tonemap.hlsl STAGES vs:VSMain ps:PSMain)
