# HLSL -> SPIR-V with the Vulkan SDK's DXC. Output: <bin>/shaders/<name>.<entry>.spv
set(EMBER_SHADER_OUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)

# ember_shaders(<target> FILE <hlsl> STAGES vs:VSMain ps:PSMain ...)
function(ember_shaders target)
  cmake_parse_arguments(ARG "" "FILE" "STAGES" ${ARGN})
  get_filename_component(name ${ARG_FILE} NAME_WE)
  set(outputs)
  foreach(stage_entry ${ARG_STAGES})
    string(REPLACE ":" ";" parts ${stage_entry})
    list(GET parts 0 stage)
    list(GET parts 1 entry)
    set(out ${EMBER_SHADER_OUT}/${name}.${entry}.spv)
    add_custom_command(
      OUTPUT ${out}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${EMBER_SHADER_OUT}
      COMMAND Vulkan::dxc_exe -spirv -T ${stage}_6_7 -E ${entry} -HV 2021
              -fspv-target-env=vulkan1.3 -fvk-use-scalar-layout -Zpr -O3
              -I ${CMAKE_SOURCE_DIR}/shaders -Fo ${out} ${ARG_FILE}
      DEPENDS ${ARG_FILE} ${CMAKE_SOURCE_DIR}/shaders/common.hlsli
      COMMENT "Shader ${name}.${entry}"
      VERBATIM)
    list(APPEND outputs ${out})
  endforeach()
  target_sources(${target} PRIVATE ${outputs})
endfunction()
